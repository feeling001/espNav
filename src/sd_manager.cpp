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
        serialPrintf("[SD] Card type: %s  total: %llu MB\n",
                      SD.cardType() == CARD_SDHC ? "SDHC/SDXC" :
                      SD.cardType() == CARD_SD   ? "SDSC"       : "Unknown",
                      (unsigned long long)(SD.totalBytes() / (1024ULL * 1024ULL)));
    } else {
        mounted = false;
    }

    unlock();
    return ok;
}

void SDManager::unmount() {
    if (!lock()) return;
    if (mounted) {
        SD.end();
        mounted = false;
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
        case CARD_SD:   info.cardType = "SDSC";  break;
        case CARD_SDHC: info.cardType = "SDHC";  break;
        case CARD_NONE: info.cardType = "None";  break;
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
    if (!mounted) return File();
    // Intentionally not locking here: the caller manages the File lifetime.
    // Concurrent reads are safe on FAT; concurrent write + read would need
    // external coordination.
    return SD.open(path, FILE_READ);
}

File SDManager::openForWrite(const char* path, bool append) {
    if (!mounted) return File();
    return SD.open(path, append ? FILE_APPEND : FILE_WRITE);
}

bool SDManager::format() {
    if (!lock()) return false;

    serialPrintf("[SD] Formatting SD card (FAT32)...\n");

    // The Arduino SD library does not expose a format function directly.
    // We use the ESP-IDF sdmmc_full_erase + ff_mkfs approach via the
    // FATFS layer.  Because that requires esp-idf headers not always
    // included in the Arduino environment, we fall back to the simplest
    // portable approach: remove all top-level entries.
    //
    // For a true low-level format the user should use a PC tool (e.g.
    // SD Association Formatter, mkfs.fat, or Disk Utility).

    // Remove every entry at the root level.
    File root = SD.open("/");
    if (!root) {
        serialPrintf("[SD] Format: cannot open root\n");
        unlock();
        return false;
    }

    // Collect names first to avoid iterator invalidation during deletion.
    std::vector<String> entries;
    std::vector<bool>   areDirs;
    File entry = root.openNextFile();
    while (entry) {
        entries.push_back(String(entry.path()));
        areDirs.push_back(entry.isDirectory());
        entry.close();
        entry = root.openNextFile();
    }
    root.close();

    for (size_t i = 0; i < entries.size(); i++) {
        if (areDirs[i]) {
            // Release mutex for the recursive deleteDir call.
            unlock();
            deleteDir(entries[i].c_str());
            if (!lock()) return false;
        } else {
            SD.remove(entries[i].c_str());
        }
    }

    serialPrintf("[SD] ✓ SD card formatted (all files removed)\n");
    unlock();
    return true;
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
