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
#include <LittleFS.h>
#include <Update.h>
#include "config_manager.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "seatalk_manager.h"
#include "sd_manager.h"

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
     */
    WebServer(ConfigManager* cm, WiFiManager* wm, TCPServer* tcp, UARTHandler* uart,
              NMEAParser* nmea, BoatState* bs, BLEManager* ble,
              SeatalkManager* stMgr, SDManager* sdMgr = nullptr);

    void init();
    void start();
    void stop();
    void broadcastNMEA(const char* sentence);

private:
    void registerRoutes();

    // ── Configuration handlers ────────────────────────────────────────────────
    void handleGetWiFiConfig(AsyncWebServerRequest* request);
    void handlePostWiFiConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetSerialConfig(AsyncWebServerRequest* request);
    void handlePostSerialConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleRestart(AsyncWebServerRequest* request);

    // ── BLE handlers ──────────────────────────────────────────────────────────
    void handleGetBLEConfig(AsyncWebServerRequest* request);
    void handlePostBLEConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // ── Polar handlers ────────────────────────────────────────────────────────
    void handleGetPolarStatus(AsyncWebServerRequest* request);
    void handleUploadPolar(AsyncWebServerRequest* request, const String& filename,
                           size_t index, uint8_t* data, size_t len, bool final);

    // ── Boat data handlers ────────────────────────────────────────────────────
    void handleGetNavigation(AsyncWebServerRequest* request);
    void handleGetWind(AsyncWebServerRequest* request);
    void handleGetAIS(AsyncWebServerRequest* request);
    void handleGetBoatState(AsyncWebServerRequest* request);
    void handleGetPerformance(AsyncWebServerRequest* request);

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

    /** POST /api/sd/format — erase all files on the SD card */
    void handleFormatSD(AsyncWebServerRequest* request);

    /** POST /api/sd/mount — (re-)mount the SD card */
    void handleMountSD(AsyncWebServerRequest* request);

    /** POST /api/sd/unmount — safely unmount the SD card */
    void handleUnmountSD(AsyncWebServerRequest* request);

    // ── WebSocket handlers ────────────────────────────────────────────────────
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                              AwsEventType type, void* arg, uint8_t* data, size_t len);

    // ── Members ───────────────────────────────────────────────────────────────
    AsyncWebServer* server;
    AsyncWebSocket* wsNMEA;
    ConfigManager*  configManager;
    WiFiManager*    wifiManager;
    TCPServer*      tcpServer;
    UARTHandler*    uartHandler;
    NMEAParser*     nmeaParser;
    BoatState*      boatState;
    BLEManager*     bleManager;
    SeatalkManager* seatalkManager;
    SDManager*      sdManager;      ///< May be nullptr when SD is disabled
    bool            running;

    // ── OTA state ─────────────────────────────────────────────────────────────
    bool     otaInProgress;
    bool     otaSuccess;
    String   otaErrorMessage;
    size_t   otaExpectedSize;
    size_t   otaBytesWritten;
};

#endif // WEB_SERVER_H
