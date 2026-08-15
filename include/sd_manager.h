#ifndef SD_MANAGER_H
#define SD_MANAGER_H

/**
 * @file sd_manager.h
 * @brief Optional SD card manager for the Marine Gateway.
 *
 * Provides mount/unmount, file listing, deletion, download helpers,
 * and storage statistics for a FAT32-formatted SD card connected via SPI.
 *
 * The SD card is entirely optional.  All public methods are safe to call
 * when no card is present — they return false / empty results and log a
 * warning rather than crashing.
 *
 * Thread safety:
 *   A FreeRTOS mutex serialises all SD operations so that the web server
 *   task and any future logging task can share the card safely.
 */

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

// ─────────────────────────────────────────────────────────────────────────────
// Data structures
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Metadata for a single file on the SD card.
 */
struct SDFileInfo {
    String   path;      ///< Full absolute path, e.g. "/logs/nmea_2024.csv"
    uint32_t size;      ///< File size in bytes
    bool     isDir;     ///< True if this entry is a directory

    SDFileInfo() : size(0), isDir(false) {}
};

/**
 * @brief SD card storage statistics.
 */
struct SDStorageInfo {
    uint64_t totalBytes; ///< Total card capacity in bytes
    uint64_t usedBytes;  ///< Bytes used (totalBytes - freeBytes)
    uint64_t freeBytes;  ///< Free bytes
    uint8_t  usedPct;    ///< Used percentage (0–100)
    String   cardType;   ///< "SDSC", "SDHC", "SDXC", or "Unknown"
    bool     mounted;    ///< True when a card is mounted

    SDStorageInfo()
        : totalBytes(0), usedBytes(0), freeBytes(0),
          usedPct(0), mounted(false) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// SDManager
// ─────────────────────────────────────────────────────────────────────────────

class SDManager {
public:
    SDManager();
    ~SDManager();

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * @brief Initialise the SPI bus and attempt to mount the SD card.
     *
     * Uses the pins defined in config.h (SD_MOSI_PIN, SD_MISO_PIN,
     * SD_SCK_PIN, SD_CS_PIN).  No card present is not an error.
     */
    void init();

    /**
     * @brief Attempt to (re-)mount the SD card.
     * @return true if a card was found and mounted.
     */
    bool mount();

    /**
     * @brief Unmount the SD card cleanly.
     */
    void unmount();

    // ── State ─────────────────────────────────────────────────────────────────

    /** @return true when a card is currently mounted and accessible. */
    bool isMounted() const { return mounted; }

    /**
     * @brief Return storage statistics.
     * @return SDStorageInfo struct; mounted=false when no card is present.
     * @note Acquires the SPI mutex and calls SD.usedBytes() — may block for
     *       several seconds on a large card.  Do NOT call from the
     *       AsyncWebServer (async_tcp) task; use getLastKnownInfo() instead.
     */
    SDStorageInfo getStorageInfo();

    /**
     * @brief Return the cached storage statistics from the last successful
     *        mount.  Completely non-blocking and safe to call from any
     *        context, including the async_tcp web-server task.
     *
     * The cache is populated by mount() (which runs on a background task)
     * and cleared by unmount().  It returns a zeroed SDStorageInfo with
     * mounted=false when no card has been successfully mounted yet.
     */
    SDStorageInfo getLastKnownInfo() const { return m_cachedInfo; }

    // ── File operations ───────────────────────────────────────────────────────

    /**
     * @brief Recursively list all files under @p dirPath.
     *
     * @param dirPath  Absolute directory path, e.g. "/" or "/logs".
     * @param maxDepth Maximum recursion depth (default 4).
     * @return Vector of SDFileInfo entries; empty if unmounted or path invalid.
     */
    std::vector<SDFileInfo> listFiles(const char* dirPath = "/",
                                       uint8_t maxDepth = 4);

    /**
     * @brief Delete a single file by path.
     * @param path  Absolute file path.
     * @return true on success.
     */
    bool deleteFile(const char* path);

    /**
     * @brief Delete a directory and all its contents recursively.
     * @param path  Absolute directory path.
     * @return true on success.
     */
    bool deleteDir(const char* path);

    /**
     * @brief Open a file for reading.
     *
     * Caller is responsible for closing the returned File object.
     * Returns an invalid File (operator bool() == false) on error.
     *
     * @param path Absolute file path.
     */
    File openForRead(const char* path);

    /**
     * @brief Open a file for writing (creates it if it does not exist).
     *
     * @param path   Absolute file path.
     * @param append If true, data is appended; if false, the file is truncated.
     * @return File object; invalid on error.
     */
    File openForWrite(const char* path, bool append = false);

    /**
     * @brief Check whether a path exists on the SD card.
     * @param path Absolute path.
     * @return true if the path exists.
     */
    bool exists(const char* path);

    /**
     * @brief Create a directory (and any missing parents).
     * @param path Absolute directory path.
     * @return true on success.
     */
    bool mkdir(const char* path);

    /**
     * @brief Register a callback to be invoked when mount() completes successfully.
     * Used by LogManager to retry file opens when the SD card becomes available.
     */
    void setOnMountedCallback(void (*cb)(void)) { onMountedCallback = cb; }

private:
    bool               mounted;
    SPIClass*          spi;
    SemaphoreHandle_t  mutex;
    SDStorageInfo      m_cachedInfo;  ///< Populated by mount(); read by getLastKnownInfo()
    void (*onMountedCallback)(void) = nullptr; ///< Callback when mount() succeeds

    // Internal recursive listing helper
    void listDir(File& dir, std::vector<SDFileInfo>& out, uint8_t depth,
                 uint8_t maxDepth);

    // Acquire / release the mutex (with timeout)
    bool lock(TickType_t timeout = pdMS_TO_TICKS(2000));
    void unlock();
};

#endif // SD_MANAGER_H
