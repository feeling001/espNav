/**
 * @file log_manager.cpp
 * @brief Non-blocking SD card logbook manager — implementation.
 */

#include "log_manager.h"
#include "functions.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

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
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void LogManager::init() {
    if (initialized) return;

    statsMutex = xSemaphoreCreateMutex();

    // Load config and session counter from NVS.
    loadConfig();

    // Create the inter-task queue.
    queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(LogEntry));
    if (!queue) {
        serialPrintf("[Log] ❌ Failed to create log queue\n");
        return;
    }

    initialized = true;
    serialPrintf("[Log] ✓ Initialized (nmea=%d st=%d csv=%d ivl=%umin)\n",
                  config.nmeaEnabled, config.seatalkEnabled,
                  config.csvEnabled, config.csvIntervalMin);
}

void LogManager::start() {
    if (!initialized || running) return;

    stats = LogStats();
    stats.sessionStartMs = millis();

    openFiles();

    xTaskCreatePinnedToCore(
        logTask,
        "LogMgr",
        LOG_TASK_STACK,
        this,
        LOG_TASK_PRIORITY,
        &taskHandle,
        1   // Core 1 — same as the processor task, lowest priority
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

    // Increment persistent session counter.
    sessionCounter++;
    nvs.putUShort("session_ctr", sessionCounter);

    // Reset statistics for the new session.
    if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        stats = LogStats();
        stats.sessionStartMs = millis();
        xSemaphoreGive(statsMutex);
    }

    openFiles();
    serialPrintf("[Log] New session started: %s\n", stats.sessionName);
}

// ─────────────────────────────────────────────────────────────────────────────
// Producer API
// ─────────────────────────────────────────────────────────────────────────────

void LogManager::logNMEA(const char* sentence) {
    if (!initialized || !config.nmeaEnabled || !sentence) return;

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

    LogEntry entry;
    entry.type = LOG_SEATALK;

    // Format as uppercase hex: "52 01 02 FF"
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
        openFiles();
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

    // Clamp interval to [1, 1440].
    if (config.csvIntervalMin < 1)    config.csvIntervalMin = 1;
    if (config.csvIntervalMin > 1440) config.csvIntervalMin = 1440;
}

void LogManager::saveConfig() {
    nvs.putBool("nmea_en",    config.nmeaEnabled);
    nvs.putBool("seatalk_en", config.seatalkEnabled);
    nvs.putBool("csv_en",     config.csvEnabled);
    nvs.putUShort("csv_ivl",  config.csvIntervalMin);
}

void LogManager::buildSessionName(char* out, size_t maxLen) {
    // Attempt to use GPS date/time for a meaningful filename.
    if (boatState) {
        GPSData gps = boatState->getGPS();
        // GPS provides position but no direct date/time in DataPoint —
        // use millis-based fallback.  A real implementation would store
        // the parsed RMC UTC field in BoatState; for now use session counter.
        (void)gps;
    }

    // Fallback: incremental session identifier.
    snprintf(out, maxLen, "session_%04u", (unsigned)sessionCounter);
}

void LogManager::openFiles() {
    if (!sdManager || !sdManager->isMounted()) {
        serialPrintf("[Log] SD not mounted — log files not opened\n");
        return;
    }

    // Ensure /logs directory exists.
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
                // Prefix with a millis timestamp for ordering.
                seatalkFile.printf("%lu %s\n", (unsigned long)millis(), entry.data);
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

void LogManager::writeCSVSnapshot() {
    if (!csvFile || !boatState) return;

    GPSData     gps     = boatState->getGPS();
    SpeedData   speed   = boatState->getSpeed();
    HeadingData heading = boatState->getHeading();
    DepthData   depth   = boatState->getDepth();
    WindData    wind    = boatState->getWind();
    EnvironmentData env = boatState->getEnvironment();

    // Helper lambda: emit float or empty field.
    auto fv = [](const DataPoint& dp) -> String {
        if (dp.valid && !dp.isStale()) return String(dp.value, 4);
        return "";
    };

    // timestamp_ms, lat, lon, sog, cog, stw, hdg_mag, hdg_true,
    // depth, aws, awa, tws, twa, twd, water_temp
    csvFile.printf("%lu,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
        (unsigned long)millis(),
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

    while (true) {
        // Wait up to 500 ms for a queue entry.
        if (xQueueReceive(self->queue, &entry, pdMS_TO_TICKS(500)) == pdTRUE) {
            self->processEntry(entry);
        }

        uint32_t now = millis();

        // Periodic flush — protects against data loss on power cut.
        if (now - self->lastFlushMs >= LOG_FLUSH_INTERVAL_MS) {
            self->flushAll();
        }

        // Periodic CSV snapshot.
        if (self->config.csvEnabled && self->csvFile) {
            uint32_t intervalMs = (uint32_t)self->config.csvIntervalMin * 60000UL;
            if (now - self->lastCsvSnapMs >= intervalMs) {
                self->writeCSVSnapshot();
                self->lastCsvSnapMs = now;
            }
        }

        // Yield explicitly so lower-idle tasks can run.
        taskYIELD();
    }
}
