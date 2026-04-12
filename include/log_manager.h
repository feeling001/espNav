#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

/**
 * @file log_manager.h
 * @brief Non-blocking SD card logbook manager for the Marine Gateway.
 *
 * Supports three independent logging modes:
 *   1. Raw NMEA  — verbatim copy of incoming NMEA sentences.
 *   2. Raw SeaTalk — ST1 datagrams formatted as hex strings.
 *   3. Structured CSV — periodic snapshot of boatState (excl. AIS).
 *
 * Architecture:
 *   A dedicated FreeRTOS task (logTask) consumes a queue of LogEntry items
 *   written by the NMEA/SeaTalk reception tasks.  The SD filesystem is never
 *   touched from the hot data path — the queue decouples producers from the
 *   (slow) SD I/O.
 *
 *   Files are flushed every FLUSH_INTERVAL_MS milliseconds and synced to
 *   disk.  The flush period is a trade-off between crash safety and SD wear.
 *
 * File naming:
 *   log_YYYYMMDD_HHMM_<type>.{nmea,st1,csv}
 *   If no GPS fix is available, falls back to log_session_<N>_<type>.ext
 *
 * Configuration persisted in NVS under namespace "logmgr":
 *   - nmea_en    (bool)   Raw NMEA logging enabled
 *   - seatalk_en (bool)   Raw SeaTalk logging enabled
 *   - csv_en     (bool)   Structured CSV logging enabled
 *   - csv_ivl    (uint16) CSV snapshot interval in minutes
 */

#include <Arduino.h>
#include <SD.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "boat_state.h"
#include "sd_manager.h"

// ─────────────────────────────────────────────────────────────────────────────
// Compile-time constants
// ─────────────────────────────────────────────────────────────────────────────

#define LOG_QUEUE_SIZE        128   // max pending log entries before drop
#define LOG_TASK_STACK        6144
#define LOG_TASK_PRIORITY     1     // lowest — behind all real-time tasks
#define LOG_FLUSH_INTERVAL_MS 10000 // flush/sync every 10 seconds
#define LOG_NVS_NAMESPACE     "logmgr"
#define LOG_CSV_HEADER \
    "timestamp_ms,lat,lon,sog_kn,cog_deg,stw_kn,hdg_mag_deg,hdg_true_deg," \
    "depth_m,aws_kn,awa_deg,tws_kn,twa_deg,twd_deg,water_temp_c\n"

// ─────────────────────────────────────────────────────────────────────────────
// Public types
// ─────────────────────────────────────────────────────────────────────────────

/** Log entry types pushed onto the queue by producer tasks. */
enum LogEntryType : uint8_t {
    LOG_NMEA     = 0,   ///< Raw NMEA sentence (null-terminated)
    LOG_SEATALK  = 1,   ///< SeaTalk hex string
    LOG_CSV_SNAP = 2,   ///< CSV snapshot trigger (payload unused)
};

/** Fixed-size queue entry — avoids heap allocation in ISR/task context. */
struct LogEntry {
    LogEntryType type;
    char         data[120]; ///< null-terminated payload
};

/** Configuration held in RAM and persisted to NVS. */
struct LogConfig {
    bool    nmeaEnabled;
    bool    seatalkEnabled;
    bool    csvEnabled;
    uint16_t csvIntervalMin; ///< 1–1440

    LogConfig()
        : nmeaEnabled(false), seatalkEnabled(false),
          csvEnabled(false), csvIntervalMin(5) {}
};

/** Logbook session statistics (cleared on new session). */
struct LogStats {
    uint32_t nmeaLines;
    uint32_t seatalkLines;
    uint32_t csvSnapshots;
    uint32_t droppedEntries;
    uint32_t sessionStartMs;
    char     sessionName[32]; ///< human-readable session identifier

    LogStats() { memset(this, 0, sizeof(*this)); }
};

// ─────────────────────────────────────────────────────────────────────────────
// LogManager
// ─────────────────────────────────────────────────────────────────────────────

class LogManager {
public:
    /**
     * @param sdMgr     SD card manager (must outlive LogManager).
     * @param boatState Shared boat state for CSV snapshots.
     */
    LogManager(SDManager* sdMgr, BoatState* boatState);
    ~LogManager();

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /** Load config from NVS, open log files if enabled, start background task. */
    void init();

    /** Start the FreeRTOS logging task. Call after init(). */
    void start();

    /** Flush and close all open log files, stop the task. */
    void stop();

    // ── Session management ────────────────────────────────────────────────────

    /**
     * @brief Close current log files and open new ones (new session).
     *
     * Uses GPS date/time if available, otherwise an incremental session ID.
     */
    void newSession();

    // ── Producer API (called from NMEA / SeaTalk tasks) ───────────────────────

    /**
     * @brief Enqueue a raw NMEA sentence for logging.
     *
     * Non-blocking — drops the entry and increments droppedEntries if the
     * queue is full.  Safe to call from any task/core.
     *
     * @param sentence Null-terminated NMEA sentence (including '$' prefix).
     */
    void logNMEA(const char* sentence);

    /**
     * @brief Enqueue a SeaTalk datagram for logging.
     *
     * The datagram is formatted as uppercase hex bytes separated by spaces,
     * e.g. "52 01 02 FF".
     *
     * @param data Raw bytes of the SeaTalk datagram.
     * @param len  Number of bytes (3–18).
     */
    void logSeatalk(const uint8_t* data, uint8_t len);

    // ── Configuration ─────────────────────────────────────────────────────────

    LogConfig getConfig() const { return config; }
    void      setConfig(const LogConfig& cfg);

    // ── Status ────────────────────────────────────────────────────────────────

    LogStats getStats()     const;
    bool     isRunning()    const { return taskHandle != nullptr; }
    bool     hasOpenFiles() const;

    /**
     * @brief Return comma-separated list of currently open log file paths.
     * Empty string when no files are open.
     */
    String   openFilePaths() const;

private:
    // ── Internal helpers ──────────────────────────────────────────────────────

    void loadConfig();
    void saveConfig();

    /** Determine session name from GPS fix or incremental counter. */
    void buildSessionName(char* out, size_t maxLen);

    /** Open (or re-open) log files according to current config. */
    void openFiles();

    /** Flush + sync all open files. */
    void flushAll();

    /** Close and null-out all file handles. */
    void closeFiles();

    /** Process a single LogEntry dispatched from the queue. */
    void processEntry(const LogEntry& entry);

    /** Write a CSV row for the current boatState snapshot. */
    void writeCSVSnapshot();

    // ── FreeRTOS task ─────────────────────────────────────────────────────────

    static void logTask(void* param);

    // ── Members ───────────────────────────────────────────────────────────────

    SDManager*  sdManager;
    BoatState*  boatState;
    Preferences nvs;

    LogConfig   config;
    LogStats    stats;

    QueueHandle_t   queue;
    TaskHandle_t    taskHandle;
    SemaphoreHandle_t statsMutex;

    // Open file handles (one per type)
    File nmeaFile;
    File seatalkFile;
    File csvFile;

    // Timing
    uint32_t lastFlushMs;
    uint32_t lastCsvSnapMs;

    // Session counter (persisted in NVS across reboots)
    uint16_t sessionCounter;

    bool initialized;
    bool running;
};

#endif // LOG_MANAGER_H
