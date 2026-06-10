/**
 * @file log_manager.cpp
 * @brief Non-blocking SD card logbook manager — implementation.
 *
 * GPS-aware logging: no file is created and no entry is queued until a valid
 * GPS fix is available (datetime.getTimestamp() > 1).  Once the fix is
 * acquired the task opens the log files automatically.
 *
 * File naming: log_YYYYMMDD_HHmm_<session>.<ext>  (UTC from GPS)
 * CSV timestamp column: UTC unix timestamp from GPS instead of millis().
 */

#include "log_manager.h"
#include "functions.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

LogManager::LogManager(SDManager* sdMgr, BoatState* bs)
    : sdManager(sdMgr), boatState(bs),
      queue(nullptr), taskHandle(nullptr), statsMutex(nullptr),
      lastFlushMs(0), lastCsvSnapMs(0),
      sessionCounter(0), initialized(false), running(false) {
}

LogManager::~LogManager() {
    stop();
    if (statsMutex) vSemaphoreDelete(statsMutex);
    if (queue)      vQueueDelete(queue);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helper: GPS fix guard
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Return true when a valid GPS datetime has been received.
 *
 * We use getTimestamp() > 1 as the validity test: the value is 0 when the
 * struct has never been populated, and mktime() can theoretically return -1
 * (cast to uint64, that is a very large number, so we use > 1 to be safe).
 */
bool LogManager::hasGPSFix() const {
    if (!boatState) return false;
    GPSData gps = boatState->getGPS();
    return gps.datetime.getTimestamp() > 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void LogManager::init() {
    if (initialized) return;

    statsMutex = xSemaphoreCreateMutex();

    loadConfig();

    queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(LogEntry));
    if (!queue) {
        serialPrintf("[Log] ❌ Failed to create log queue\n");
        return;
    }

    initialized = true;
    serialPrintf("[Log] ✓ Initialized (nmea=%d st=%d csv=%d ivl=%umin)\n",
                  config.nmeaEnabled, config.seatalkEnabled,
                  config.csvEnabled, config.csvIntervalMin);
    serialPrintf("[Log] Waiting for GPS fix before opening log files...\n");
}

void LogManager::start() {
    if (!initialized || running) return;

    stats = LogStats();
    stats.sessionStartMs = millis();

    // Files are NOT opened here: openFiles() is called from the task once a
    // GPS fix has been acquired (see tryOpenFiles() below).

    xTaskCreatePinnedToCore(
        logTask,
        "LogMgr",
        LOG_TASK_STACK,
        this,
        LOG_TASK_PRIORITY,
        &taskHandle,
        1
    );

    running = true;
    serialPrintf("[Log] ✓ Task started\n");
}

void LogManager::stop() {
    if (!running) return;
    running = false;

    if (taskHandle) {
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }

    flushAll();
    closeFiles();
    serialPrintf("[Log] Stopped and files closed\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Session management
// ─────────────────────────────────────────────────────────────────────────────

void LogManager::newSession() {
    flushAll();
    closeFiles();

    sessionCounter++;
    nvs.putUShort("session_ctr", sessionCounter);

    if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        stats = LogStats();
        stats.sessionStartMs = millis();
        xSemaphoreGive(statsMutex);
    }

    // Open files only if we already have a fix; otherwise tryOpenFiles() will
    // pick it up on the next periodic check in the task.
    if (hasGPSFix()) {
        openFiles();
        serialPrintf("[Log] New session started: %s\n", stats.sessionName);
    } else {
        serialPrintf("[Log] New session requested, waiting for GPS fix...\n");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helper: open files only when a GPS fix is available
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Open log files if not already open and a GPS fix is available.
 * @return true if files are now open (or were already open).
 */
bool LogManager::tryOpenFiles() {
    if (hasOpenFiles()) return true;   // already open — nothing to do
    if (!hasGPSFix())   return false;  // still waiting

    openFiles();
    return hasOpenFiles();
}

// ─────────────────────────────────────────────────────────────────────────────
// Producer API
// ─────────────────────────────────────────────────────────────────────────────

void LogManager::logNMEA(const char* sentence) {
    if (!initialized || !config.nmeaEnabled || !sentence) return;
    if (!hasGPSFix()) return;  // No fix yet — discard

    LogEntry entry;
    entry.type = LOG_NMEA;
    strncpy(entry.data, sentence, sizeof(entry.data) - 1);
    entry.data[sizeof(entry.data) - 1] = '\0';

    if (xQueueSend(queue, &entry, 0) != pdTRUE) {
        if (xSemaphoreTake(statsMutex, 0) == pdTRUE) {
            stats.droppedEntries++;
            xSemaphoreGive(statsMutex);
        }
    }
}

void LogManager::logSeatalk(const uint8_t* data, uint8_t len) {
    if (!initialized || !config.seatalkEnabled || !data || len == 0) return;
    if (!hasGPSFix()) return;  // No fix yet — discard

    LogEntry entry;
    entry.type = LOG_SEATALK;

    size_t pos = 0;
    for (uint8_t i = 0; i < len && pos + 3 < sizeof(entry.data); i++) {
        if (i > 0) entry.data[pos++] = ' ';
        snprintf(entry.data + pos, 3, "%02X", data[i]);
        pos += 2;
    }
    entry.data[pos] = '\0';

    if (xQueueSend(queue, &entry, 0) != pdTRUE) {
        if (xSemaphoreTake(statsMutex, 0) == pdTRUE) {
            stats.droppedEntries++;
            xSemaphoreGive(statsMutex);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Configuration
// ─────────────────────────────────────────────────────────────────────────────

void LogManager::setConfig(const LogConfig& cfg) {
    bool needReopen = (cfg.nmeaEnabled    != config.nmeaEnabled ||
                       cfg.seatalkEnabled != config.seatalkEnabled ||
                       cfg.csvEnabled     != config.csvEnabled);

    config = cfg;
    saveConfig();

    if (needReopen && running) {
        flushAll();
        closeFiles();
        // tryOpenFiles() in the task will reopen when fix is available
        if (hasGPSFix()) openFiles();
    }

    serialPrintf("[Log] Config updated (nmea=%d st=%d csv=%d ivl=%umin)\n",
                  config.nmeaEnabled, config.seatalkEnabled,
                  config.csvEnabled, config.csvIntervalMin);
}

// ─────────────────────────────────────────────────────────────────────────────
// Status
// ─────────────────────────────────────────────────────────────────────────────

LogStats LogManager::getStats() const {
    LogStats copy;
    if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        copy = stats;
        xSemaphoreGive(statsMutex);
    }
    return copy;
}

bool LogManager::hasOpenFiles() const {
    return (bool)nmeaFile || (bool)seatalkFile || (bool)csvFile;
}

String LogManager::openFilePaths() const {
    String result;
    if (nmeaFile)    { if (!result.isEmpty()) result += ','; result += nmeaFile.path(); }
    if (seatalkFile) { if (!result.isEmpty()) result += ','; result += seatalkFile.path(); }
    if (csvFile)     { if (!result.isEmpty()) result += ','; result += csvFile.path(); }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

void LogManager::loadConfig() {
    nvs.begin(LOG_NVS_NAMESPACE, false);

    config.nmeaEnabled    = nvs.getBool("nmea_en",    false);
    config.seatalkEnabled = nvs.getBool("seatalk_en", false);
    config.csvEnabled     = nvs.getBool("csv_en",     false);
    config.csvIntervalMin = nvs.getUShort("csv_ivl",  5);
    sessionCounter        = nvs.getUShort("session_ctr", 0);

    if (config.csvIntervalMin < 1)    config.csvIntervalMin = 1;
    if (config.csvIntervalMin > 1440) config.csvIntervalMin = 1440;
}

void LogManager::saveConfig() {
    nvs.putBool("nmea_en",    config.nmeaEnabled);
    nvs.putBool("seatalk_en", config.seatalkEnabled);
    nvs.putBool("csv_en",     config.csvEnabled);
    nvs.putUShort("csv_ivl",  config.csvIntervalMin);
}

/**
 * @brief Build a session name using GPS date/time when available.
 *
 * Format with GPS fix : log_YYYYMMDD_HHmm   (UTC)
 * Fallback (no fix)   : session_XXXX
 *
 * The fallback should never be reached in practice because openFiles() is
 * only called after hasGPSFix() returns true.
 */
void LogManager::buildSessionName(char* out, size_t maxLen) {
    if (boatState) {
        GPSData gps = boatState->getGPS();
        uint64_t ts = gps.datetime.getTimestamp();

        if (ts > 1) {
            time_t t = (time_t)ts;
            struct tm tmBuf;
            gmtime_r(&t, &tmBuf);  // UTC breakdown

            snprintf(out, maxLen,
                     "log_%04d%02d%02d_%02d%02d",
                     tmBuf.tm_year + 1900,
                     tmBuf.tm_mon  + 1,
                     tmBuf.tm_mday,
                     tmBuf.tm_hour,
                     tmBuf.tm_min);
            return;
        }
    }

    // Fallback — should not be reached under normal conditions
    snprintf(out, maxLen, "session_%04u", (unsigned)sessionCounter);
}

void LogManager::openFiles() {
    if (!sdManager || !sdManager->isMounted()) {
        serialPrintf("[Log] SD not mounted — log files not opened\n");
        return;
    }

    if (!sdManager->exists("/logs")) {
        sdManager->mkdir("/logs");
    }

    char sessionName[32];
    buildSessionName(sessionName, sizeof(sessionName));
    strncpy(stats.sessionName, sessionName, sizeof(stats.sessionName) - 1);

    if (config.nmeaEnabled) {
        char path[64];
        snprintf(path, sizeof(path), "/logs/%s.nmea", sessionName);
        nmeaFile = sdManager->openForWrite(path, true);
        if (nmeaFile) {
            serialPrintf("[Log] NMEA log: %s\n", path);
        } else {
            serialPrintf("[Log] ❌ Failed to open NMEA log: %s\n", path);
        }
    }

    if (config.seatalkEnabled) {
        char path[64];
        snprintf(path, sizeof(path), "/logs/%s.st1", sessionName);
        seatalkFile = sdManager->openForWrite(path, true);
        if (seatalkFile) {
            serialPrintf("[Log] SeaTalk log: %s\n", path);
        } else {
            serialPrintf("[Log] ❌ Failed to open SeaTalk log: %s\n", path);
        }
    }

    if (config.csvEnabled) {
        char path[64];
        snprintf(path, sizeof(path), "/logs/%s.csv", sessionName);
        bool newFile = !sdManager->exists(path);
        csvFile = sdManager->openForWrite(path, true);
        if (csvFile) {
            if (newFile) csvFile.print(LOG_CSV_HEADER);
            serialPrintf("[Log] CSV log: %s\n", path);
        } else {
            serialPrintf("[Log] ❌ Failed to open CSV log: %s\n", path);
        }
    }

    lastFlushMs   = millis();
    lastCsvSnapMs = millis();
}

void LogManager::flushAll() {
    if (nmeaFile)    nmeaFile.flush();
    if (seatalkFile) seatalkFile.flush();
    if (csvFile)     csvFile.flush();
    lastFlushMs = millis();
}

void LogManager::closeFiles() {
    if (nmeaFile)    { nmeaFile.close();    }
    if (seatalkFile) { seatalkFile.close(); }
    if (csvFile)     { csvFile.close();     }
}

void LogManager::processEntry(const LogEntry& entry) {
    switch (entry.type) {
        case LOG_NMEA:
            if (nmeaFile) {
                nmeaFile.println(entry.data);
                if (xSemaphoreTake(statsMutex, 0) == pdTRUE) {
                    stats.nmeaLines++;
                    xSemaphoreGive(statsMutex);
                }
            }
            break;

        case LOG_SEATALK:
            if (seatalkFile) {
                // Use GPS unix timestamp as prefix when available, else millis
                uint64_t ts = 0;
                if (boatState) {
                    ts = boatState->getGPS().datetime.getTimestamp();
                }
                if (ts > 1) {
                    seatalkFile.printf("%llu %s\n", (unsigned long long)ts, entry.data);
                } else {
                    seatalkFile.printf("%lu %s\n", (unsigned long)millis(), entry.data);
                }
                if (xSemaphoreTake(statsMutex, 0) == pdTRUE) {
                    stats.seatalkLines++;
                    xSemaphoreGive(statsMutex);
                }
            }
            break;

        case LOG_CSV_SNAP:
            writeCSVSnapshot();
            break;

        default:
            break;
    }
}

/**
 * @brief Write one CSV row using GPS unix timestamp as the time column.
 *
 * Skips the snapshot silently if no GPS fix is available (should not happen
 * in normal operation since logging only starts after a fix).
 */
void LogManager::writeCSVSnapshot() {
    if (!csvFile || !boatState) return;
    if (!hasGPSFix()) return;  // Guard — do not write rows without a valid time

    GPSData     gps     = boatState->getGPS();
    SpeedData   speed   = boatState->getSpeed();
    HeadingData heading = boatState->getHeading();
    DepthData   depth   = boatState->getDepth();
    WindData    wind    = boatState->getWind();
    EnvironmentData env = boatState->getEnvironment();

    // Use the GPS unix timestamp for the time column
    uint64_t gpsTs = gps.datetime.getTimestamp();

    auto fv = [](const DataPoint& dp) -> String {
        if (dp.valid && !dp.isStale()) return String(dp.value, 4);
        return "";
    };

    csvFile.printf("%llu,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
        (unsigned long long)gpsTs,
        gps.position.lat.valid && !gps.position.lat.isStale()
            ? String(gps.position.lat.value, 6).c_str() : "",
        gps.position.lon.valid && !gps.position.lon.isStale()
            ? String(gps.position.lon.value, 6).c_str() : "",
        fv(gps.sog).c_str(),
        fv(gps.cog).c_str(),
        fv(speed.stw).c_str(),
        fv(heading.magnetic).c_str(),
        fv(heading.true_heading).c_str(),
        fv(depth.below_transducer).c_str(),
        fv(wind.aws).c_str(),
        fv(wind.awa).c_str(),
        fv(wind.tws).c_str(),
        fv(wind.twa).c_str(),
        fv(wind.twd).c_str(),
        fv(env.water_temp).c_str()
    );

    if (xSemaphoreTake(statsMutex, 0) == pdTRUE) {
        stats.csvSnapshots++;
        xSemaphoreGive(statsMutex);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// FreeRTOS task
// ─────────────────────────────────────────────────────────────────────────────

void LogManager::logTask(void* param) {
    LogManager* self = static_cast<LogManager*>(param);
    LogEntry    entry;

    serialPrintf("[Log] Task running on Core %d\n", (int)xPortGetCoreID());

    uint32_t lastFixCheck = 0;  // Tracks when we last attempted to open files

    while (true) {
        // ── Deferred file open: wait for GPS fix ─────────────────────────────
        // Check every 10 seconds until files are open.
        uint32_t now = millis();
        if (!self->hasOpenFiles() && (now - lastFixCheck >= 10000)) {
            lastFixCheck = now;
            if (self->tryOpenFiles()) {
                serialPrintf("[Log] GPS fix acquired — log files opened: %s\n",
                              self->openFilePaths().c_str());
            }
        }

        // ── Drain the queue ──────────────────────────────────────────────────
        if (xQueueReceive(self->queue, &entry, pdMS_TO_TICKS(500)) == pdTRUE) {
            // Ensure files are open before processing (fix may have just arrived)
            if (!self->hasOpenFiles()) self->tryOpenFiles();
            if (self->hasOpenFiles()) self->processEntry(entry);
        }

        now = millis();

        // ── Periodic flush ───────────────────────────────────────────────────
        if (self->hasOpenFiles() && now - self->lastFlushMs >= LOG_FLUSH_INTERVAL_MS) {
            self->flushAll();
        }

        // ── Periodic CSV snapshot ────────────────────────────────────────────
        if (self->config.csvEnabled && self->csvFile && self->hasGPSFix()) {
            uint32_t intervalMs = (uint32_t)self->config.csvIntervalMin * 60000UL;
            if (now - self->lastCsvSnapMs >= intervalMs) {
                self->writeCSVSnapshot();
                self->lastCsvSnapMs = now;
            }
        }

        taskYIELD();
    }
}