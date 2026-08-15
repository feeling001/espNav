/**
 * @file sd_manager.cpp
 * @brief SD card manager implementation for the Marine Gateway.
 *
 * Uses the Arduino SD library on top of a dedicated SPI bus instance.
 * All operations are mutex-guarded so they can be called safely from
 * multiple FreeRTOS tasks (web server, logging task, etc.).
 *
 * Filesystem format: FAT32 (handled by the SD library / ESP-IDF VFS layer).
 * FAT32 is chosen because:
 *   - Universally readable on Windows, macOS, and Linux without extra tools.
 *   - Suitable for the large sequential CSV / NMEA log files this gateway
 *     will produce.
 *   - Supported natively by the Arduino SD library.
 *
 * FIX: After a failed SD.begin(), SD.end() is now always called to ensure
 * the SPI driver and CS pin are released cleanly. Without this, a failed
 * mount leaves the SPI bus in a partially-initialised state which blocks
 * FreeRTOS mutex acquisitions under load — causing POST requests (which
 * involve body parsing and more CPU cycles) to time out while GET requests
 * (fast path) still succeed.
 */

#include "sd_manager.h"
#include "functions.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

SDManager::SDManager()
    : mounted(false), spi(nullptr) {
    mutex = xSemaphoreCreateMutex();
}

SDManager::~SDManager() {
    unmount();
    if (spi) {
        delete spi;
        spi = nullptr;
    }
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void SDManager::init() {
    serialPrintf("[SD] Initialising SD card manager\n");
    serialPrintf("[SD]   MOSI=%d  MISO=%d  SCK=%d  CS=%d  freq=%u Hz\n",
                  (int)SD_MOSI_PIN, (int)SD_MISO_PIN,
                  (int)SD_SCK_PIN,  (int)SD_CS_PIN,
                  (unsigned)SD_SPI_FREQ_HZ);

    // Create a dedicated SPI instance so we do not conflict with other
    // peripherals that might use VSPI/HSPI.
    spi = new SPIClass(FSPI);
    spi->begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    // Drive CS high explicitly before attempting mount so the SD card
    // sees a clean idle state on the bus.
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    delay(10);

    if (mount()) {
        serialPrintf("[SD] ✓ Card mounted at boot\n");
    } else {
        serialPrintf("[SD] ⚠  No SD card detected (optional — continuing without it)\n");
    }
}

bool SDManager::mount() {
    if (!lock()) return false;

    if (mounted) {
        unlock();
        return true;
    }

    // SD.begin() accepts (cs, spi, freq)
    bool ok = SD.begin(SD_CS_PIN, *spi, SD_SPI_FREQ_HZ);

    if (ok) {
        mounted = true;

        // Populate the info cache while we still hold the mutex.
        // SD.usedBytes() can be slow on a large card but this runs on a
        // background task (see WebServer::startSDJob), never on async_tcp.
        uint64_t total = SD.totalBytes();
        uint64_t used  = SD.usedBytes();
        m_cachedInfo.totalBytes = total;
        m_cachedInfo.usedBytes  = used;
        m_cachedInfo.freeBytes  = (total > used) ? (total - used) : 0;
        m_cachedInfo.usedPct    = (total > 0) ? (uint8_t)((used * 100ULL) / total) : 0;
        switch (SD.cardType()) {
            case CARD_SD:   m_cachedInfo.cardType = "SDSC";    break;
            case CARD_SDHC: m_cachedInfo.cardType = "SDHC";    break;
            case CARD_NONE: m_cachedInfo.cardType = "None";    break;
            default:        m_cachedInfo.cardType = "Unknown"; break;
        }
        m_cachedInfo.mounted = true;

        // Notify any listeners (e.g., LogManager) that the SD card is now mounted
        if (onMountedCallback) {
            onMountedCallback();
        }

        serialPrintf("[SD] Card type: %s  total: %llu MB  used: %llu MB\n",
                      m_cachedInfo.cardType.c_str(),
                      (unsigned long long)(total / (1024ULL * 1024ULL)),
                      (unsigned long long)(used  / (1024ULL * 1024ULL)));
    } else {
        mounted = false;

        // ── KEY FIX ──────────────────────────────────────────────────────────
        // Always call SD.end() after a failed SD.begin().
        // A failed begin() leaves the SD library's internal SDFS object in a
        // partially-initialised state: the SPI driver is installed, the mutex
        // inside the VFS layer may be held, and the CS pin may be asserted low.
        // This manifests as FreeRTOS mutex timeouts under load — GET requests
        // succeed (fast path, no body parsing) while POST requests fail because
        // they spend more time in the task scheduler and hit the blocked mutex.
        // Calling SD.end() tears down the driver cleanly and releases the CS pin.
        // ─────────────────────────────────────────────────────────────────────
        SD.end();

        // Release CS pin back to idle-high so it doesn't interfere with the
        // SPI bus if other devices share the same bus in the future.
        pinMode(SD_CS_PIN, OUTPUT);
        digitalWrite(SD_CS_PIN, HIGH);
    }

    unlock();
    return ok;
}

void SDManager::unmount() {
    if (!lock()) return;
    if (mounted) {
        SD.end();
        mounted = false;
        m_cachedInfo = SDStorageInfo();  // clear cached stats

        // Return CS to idle-high.
        pinMode(SD_CS_PIN, OUTPUT);
        digitalWrite(SD_CS_PIN, HIGH);

        serialPrintf("[SD] Card unmounted\n");
    }
    unlock();
}

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────

SDStorageInfo SDManager::getStorageInfo() {
    SDStorageInfo info;
    info.mounted = mounted;

    if (!mounted || !lock()) return info;

    uint64_t total = SD.totalBytes();
    uint64_t used  = SD.usedBytes();

    info.totalBytes = total;
    info.usedBytes  = used;
    info.freeBytes  = (total > used) ? (total - used) : 0;
    info.usedPct    = (total > 0) ? (uint8_t)((used * 100ULL) / total) : 0;

    switch (SD.cardType()) {
        case CARD_SD:   info.cardType = "SDSC";    break;
        case CARD_SDHC: info.cardType = "SDHC";    break;
        case CARD_NONE: info.cardType = "None";    break;
        default:        info.cardType = "Unknown"; break;
    }

    unlock();
    return info;
}

// ─────────────────────────────────────────────────────────────────────────────
// File operations
// ─────────────────────────────────────────────────────────────────────────────

std::vector<SDFileInfo> SDManager::listFiles(const char* dirPath,
                                              uint8_t maxDepth) {
    std::vector<SDFileInfo> result;

    if (!mounted || !lock()) return result;

    File root = SD.open(dirPath);
    if (!root || !root.isDirectory()) {
        serialPrintf("[SD] listFiles: cannot open dir '%s'\n", dirPath);
        unlock();
        return result;
    }

    listDir(root, result, 0, maxDepth);
    root.close();

    unlock();
    return result;
}

void SDManager::listDir(File& dir, std::vector<SDFileInfo>& out,
                         uint8_t depth, uint8_t maxDepth) {
    File entry = dir.openNextFile();
    while (entry) {
        SDFileInfo info;
        info.path  = String(entry.path());
        info.size  = entry.isDirectory() ? 0 : entry.size();
        info.isDir = entry.isDirectory();
        out.push_back(info);

        if (entry.isDirectory() && depth < maxDepth) {
            listDir(entry, out, depth + 1, maxDepth);
        }
        entry.close();
        entry = dir.openNextFile();
    }
}

bool SDManager::deleteFile(const char* path) {
    if (!mounted || !lock()) return false;

    bool ok = false;
    if (SD.exists(path)) {
        ok = SD.remove(path);
        serialPrintf("[SD] delete '%s' → %s\n", path, ok ? "OK" : "FAILED");
    } else {
        serialPrintf("[SD] deleteFile: path not found '%s'\n", path);
    }

    unlock();
    return ok;
}

bool SDManager::deleteDir(const char* path) {
    if (!mounted || !lock()) return false;

    // Recursively remove all entries before removing the directory itself.
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        unlock();
        return false;
    }

    File entry = dir.openNextFile();
    while (entry) {
        String entryPath = String(entry.path());
        bool   isDir     = entry.isDirectory();
        entry.close();

        if (isDir) {
            // Must release mutex before recursive call to avoid deadlock.
            unlock();
            deleteDir(entryPath.c_str());
            if (!lock()) return false;
        } else {
            SD.remove(entryPath.c_str());
        }
        entry = dir.openNextFile();
    }
    dir.close();

    bool ok = SD.rmdir(path);
    serialPrintf("[SD] deleteDir '%s' → %s\n", path, ok ? "OK" : "FAILED");

    unlock();
    return ok;
}

File SDManager::openForRead(const char* path) {
    if (!mounted || !lock()) return File();
    File f = SD.open(path, FILE_READ);
    unlock();
    return f;
}

File SDManager::openForWrite(const char* path, bool append) {
    if (!mounted || !lock()) return File();
    File f = SD.open(path, append ? FILE_APPEND : FILE_WRITE);
    if (!f) {
        serialPrintf("[SD] ❌ Failed to open '%s' for %s\n", path, append ? "append" : "write");
    }
    unlock();
    return f;
}

bool SDManager::exists(const char* path) {
    if (!mounted || !lock()) return false;
    bool ok = SD.exists(path);
    unlock();
    return ok;
}

bool SDManager::mkdir(const char* path) {
    if (!mounted || !lock()) return false;
    bool ok = SD.mkdir(path);
    serialPrintf("[SD] mkdir '%s' → %s\n", path, ok ? "OK" : "FAILED");
    unlock();
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

bool SDManager::lock(TickType_t timeout) {
    if (!mutex) return true; // no RTOS yet (pre-scheduler call)
    return xSemaphoreTake(mutex, timeout) == pdTRUE;
}

void SDManager::unlock() {
    if (mutex) xSemaphoreGive(mutex);
}