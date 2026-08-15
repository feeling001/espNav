#ifndef WEB_SERVER_H
#define WEB_SERVER_H

/**
 * @file web_server.h
 * @brief Async HTTP server + WebSocket server for the Marine Gateway dashboard.
 *
 * Exposes:
 *   - REST API  (/api/*)        — configuration, status, boat data, OTA, storage
 *   - WebSocket (/ws/nmea)     — real-time NMEA sentence stream
 *   - Static files             — React SPA served from LittleFS or PROGMEM
 *
 * SD card endpoints are available under /api/sd/* when an SDManager
 * instance is provided.  All SD endpoints gracefully return 503 when no
 * card is mounted.
 */

#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <Preferences.h>
#include <functional>
#include <atomic>
#include "config_manager.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "seatalk_manager.h"
#include "sd_manager.h"
#include "alarm_manager.h"

// Forward declarations
class TCPServer;
class UARTHandler;
class NMEAParser;
class BoatState;

#ifdef WEB_UI_PROGMEM
class AsyncWebServer;
void registerProgmemRoutes(AsyncWebServer* server);
#endif

class WebServer {
public:
    /**
     * @param cm      Configuration manager (NVS)
     * @param wm      WiFi manager
     * @param tcp     TCP server (NMEA stream)
     * @param uart    UART handler
     * @param nmea    NMEA parser
     * @param bs      Boat state
     * @param ble     BLE manager
     * @param stMgr   SeaTalk manager (autopilot commands)
     * @param sdMgr   SD card manager — may be nullptr if SD is not used
     * @param alarmMgr Alarm manager — may be nullptr if alarms are not used
     */
    WebServer(ConfigManager* cm, WiFiManager* wm, TCPServer* tcp, UARTHandler* uart,
              NMEAParser* nmea, BoatState* bs, BLEManager* ble,
              SeatalkManager* stMgr, LogManager* logManager ,SDManager* sdMgr = nullptr,
              AlarmManager* alarmMgr = nullptr, DataSourceManager* dataSourceManager = nullptr);

    void init();
    void start();
    void stop();
    void broadcastNMEA(const char* sentence);
    void broadcastBoatState();
    /** Push any new debug-log lines (see DebugLog) to connected /ws/debug clients. */
    void broadcastDebugLog();

    /**
     * @brief Record the outcome of a completed background SD job (see
     *        startSDJob). Public because it is invoked from the
     *        FreeRTOS task entry point, which is a free function.
     */
    void finishSDJob(bool ok, const String& successMsg, const String& failMsg);

    /**
     * @brief Record the result of a completed background SD file-listing job.
     *        Called from sdListTaskEntry (a free function) so it must be public.
     */
    void finishSDListJob(const String& json);

private:
    void registerRoutes();
    String buildBoatStateJSON();

    // ── Configuration handlers ────────────────────────────────────────────────
    void handleGetWiFiConfig(AsyncWebServerRequest* request);
    void handlePostWiFiConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetSerialConfig(AsyncWebServerRequest* request);
    void handlePostSerialConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleRestart(AsyncWebServerRequest* request);
    void handlePostSeatalkExtra(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // ── Conversion handlers ───────────────────────────────────────────────────
    void handleGetConversionConfig(AsyncWebServerRequest* request);
    void handlePostConversionConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // ── Data source selection handlers ────────────────────────────────────────
    void handleGetDataSourceConfig(AsyncWebServerRequest* request);
    void handlePostDataSourceConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // ── BLE handlers ──────────────────────────────────────────────────────────
    void handleGetBLEConfig(AsyncWebServerRequest* request);
    void handlePostBLEConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // ── Polar handlers ────────────────────────────────────────────────────────
    void handleGetPolarStatus(AsyncWebServerRequest* request);
    void handleUploadPolar(AsyncWebServerRequest* request, const String& filename,
                           size_t index, uint8_t* data, size_t len, bool final);

    // ── LogBook handlers ────────────────────────────────────────────────────
    void handlePostLogNewSession(AsyncWebServerRequest* request);
    void handleGetLogStatus(AsyncWebServerRequest* request);
    void handlePostLogConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetLogConfig(AsyncWebServerRequest* request);

    // ── Boat data handlers ────────────────────────────────────────────────────
    void handleGetNavigation(AsyncWebServerRequest* request);
    void handleGetWind(AsyncWebServerRequest* request);
    void handleGetAIS(AsyncWebServerRequest* request);
    void handleGetBoatState(AsyncWebServerRequest* request);
    void handleGetPerformance(AsyncWebServerRequest* request);
    void handleGetPerformanceConfig(AsyncWebServerRequest* request);
    void handlePostPerformanceConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // ── Alarm handlers ──────────────────────────────────────────
    void handleGetAlarmsConfig(AsyncWebServerRequest* request);
    void handlePostAlarmsConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetAlarmsStatus(AsyncWebServerRequest* request);
    void handlePostAlarmsAck(AsyncWebServerRequest* request);
    void handlePostAlarmsBeepOn(AsyncWebServerRequest* request);
    void handlePostAlarmsBeepOff(AsyncWebServerRequest* request);

    // ── AIS handlers ──────────────────────────────────────────
    void handleGetAISConfig(AsyncWebServerRequest* request);
    void handlePostAISConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // ── WiFi scan handlers ────────────────────────────────────────────────────
    void handleStartWiFiScan(AsyncWebServerRequest* request);
    void handleGetWiFiScanResults(AsyncWebServerRequest* request);

    // ── Autopilot handler ─────────────────────────────────────────────────────
    void handlePostAutopilotCommand(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // ── OTA handlers ──────────────────────────────────────────────────────────
    void handleGetOTAStatus(AsyncWebServerRequest* request);
    void handleOTAUpload(AsyncWebServerRequest* request, const String& filename,
                         size_t index, uint8_t* data, size_t len, bool final);
    void handleOTAUploadComplete(AsyncWebServerRequest* request);

    // ── LittleFS storage handlers ─────────────────────────────────────────────
    void handleGetStorageInfo(AsyncWebServerRequest* request);
    void handleListFiles(AsyncWebServerRequest* request);
    void handleDeleteFile(AsyncWebServerRequest* request);
    void handleFormatStorage(AsyncWebServerRequest* request);

    // ── SD card handlers ──────────────────────────────────────────────────────

    /** GET /api/sd/status — mount status + storage statistics */
    void handleGetSDStatus(AsyncWebServerRequest* request);

    /** GET /api/sd/files — recursive file listing */
    void handleListSDFiles(AsyncWebServerRequest* request);

    /** GET /api/sd/download?path=<file> — stream a file to the browser */
    void handleDownloadSDFile(AsyncWebServerRequest* request);

    /** DELETE /api/sd/delete?path=<file> — delete a single file */
    void handleDeleteSDFile(AsyncWebServerRequest* request);

    /** POST /api/sd/mkdir — create a directory  body: {"path":"/logs"} */
    void handleMkdirSD(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    /** POST /api/sd/mount — (re-)mount the SD card */
    void handleMountSD(AsyncWebServerRequest* request);

    /** POST /api/sd/unmount — safely unmount the SD card */
    void handleUnmountSD(AsyncWebServerRequest* request);

    /**
     * @brief Run a blocking SD/SPI operation on a dedicated FreeRTOS task
     *        while keeping the calling task (async_tcp) fed to the task
     *        watchdog.
     *
     * Used only for short operations (e.g. unmount). For anything that can
     * take an unbounded amount of time (mount, format), use startSDJob()
     * instead — see its documentation for why a wait loop is not safe.
     *
     * @param fn Blocking function to execute; its return value is
     *           returned by this helper.
     */
    bool runBlockingSD(std::function<bool()> fn);

    /**
     * @brief Kick off a long-running SD/SPI operation on a background task
     *        and return immediately (fire-and-forget).
     *
     * AsyncWebServer route handlers execute directly on the "async_tcp"
     * task, which is registered with the Task Watchdog Timer. The
     * "async_tcp" task is created with no fixed core affinity, so it can
     * end up sharing a core with any helper task we spawn. Operations like
     * SD.begin() (no card / bad wiring) or formatting a card with many
     * files perform many short blocking SPI transactions back-to-back; if
     * our own polling/wait loop happens to run on the same core it can be
     * starved long enough to miss the watchdog deadline. The only fully
     * safe approach is for the request handler to never wait on the
     * result at all: it starts the job and returns immediately, and the
     * frontend polls /api/sd/status (busy/last_job_*) for completion.
     *
     * @param type        Short identifier stored in sdJobType ("mount", "format", …).
     * @param fn          Blocking function to execute on the background task.
     * @param successMsg  Message stored in sdJobMessage when fn() returns true.
     * @param failMsg     Message stored in sdJobMessage when fn() returns false.
     * @return true if the job was started; false if another job is already
     *         running or the task could not be created.
     */
    bool startSDJob(const char* type, std::function<bool()> fn,
                     const char* successMsg, const char* failMsg);

    /**
     * @brief Kick off a background file-listing job.
     *
     * sdManager->listFiles() holds the SDManager mutex for the entire duration
     * of a recursive directory scan, which can take many seconds on a large
     * card — enough to trigger the Task Watchdog if run directly on async_tcp.
     * This helper spawns the listing on a background task (core 0) and caches
     * the result as a JSON string in sdListJson.  handleListSDFiles() returns
     * the cache immediately, or HTTP 202 while the job is running.
     *
     * @param dir Directory to list (e.g. "/").
     * @return true if the job was started; false if already running.
     */
    bool startSDListJob(const String& dir);

    // ── WebSocket handlers ────────────────────────────────────────────────────
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                              AwsEventType type, void* arg, uint8_t* data, size_t len);

        
    // ── Members ───────────────────────────────────────────────────────────────
    AsyncWebServer* server;
    AsyncWebSocket* wsNMEA;
    AsyncWebSocket* wsBoatState;
    AsyncWebSocket* wsDebug;
    uint32_t        debugLogLastSeq = 0; ///< Last DebugLog sequence number already broadcast

    // ── SD background job state (see startSDJob) ──────────────────────────────
    std::atomic<bool> sdJobBusy{false};
    std::atomic<bool> sdJobSuccess{false};
    String             sdJobType;      ///< "mount" | "format" | ""
    String             sdJobMessage;   ///< Result message of the last completed job

    // ── SD file-listing background job state (see startSDListJob) ───────────────
    std::atomic<bool> sdListBusy{false};
    String             sdListJson;  ///< Cached JSON from last listFiles run
    String             sdListDir;   ///< Directory of last listing
    ConfigManager*  configManager;
    WiFiManager*    wifiManager;
    TCPServer*      tcpServer;
    UARTHandler*    uartHandler;
    NMEAParser*     nmeaParser;
    BoatState*      boatState;
    BLEManager*     bleManager;
    SeatalkManager* seatalkManager;
    SDManager*      sdManager;      ///< May be nullptr when SD is disabled
    LogManager*     logManager;
    AlarmManager*   alarmManager;   ///< May be nullptr when alarms are disabled
    DataSourceManager* dataSourceManager; ///< May be nullptr when not wired in
    bool            running;

    // ── OTA state ─────────────────────────────────────────────────────────────
    Preferences  perfNvs;         ///< NVS namespace for performance config

    // ── OTA state ─────────────────────────────────────────────────────────────
    bool     otaInProgress;
    bool     otaSuccess;
    String   otaErrorMessage;
    size_t   otaExpectedSize;
    size_t   otaBytesWritten;
};

#endif // WEB_SERVER_H
