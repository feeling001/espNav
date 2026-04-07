#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <LittleFS.h>
#include "config_manager.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "seatalk_rmt.h"

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
    WebServer(ConfigManager* cm, WiFiManager* wm, TCPServer* tcp, UARTHandler* uart,
              NMEAParser* nmea, BoatState* bs, BLEManager* ble, SeatalkRMT* seatalk);

    void init();
    void start();
    void stop();
    void broadcastNMEA(const char* sentence);

private:
    void registerRoutes();

    // REST API handlers - Configuration
    void handleGetWiFiConfig(AsyncWebServerRequest* request);
    void handlePostWiFiConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetSerialConfig(AsyncWebServerRequest* request);
    void handlePostSerialConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleRestart(AsyncWebServerRequest* request);

    // BLE handlers
    void handleGetBLEConfig(AsyncWebServerRequest* request);
    void handlePostBLEConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // Polar handlers
    void handleGetPolarStatus(AsyncWebServerRequest* request);
    void handleUploadPolar(AsyncWebServerRequest* request, const String& filename,
                           size_t index, uint8_t* data, size_t len, bool final);

    // Boat data handlers
    void handleGetNavigation(AsyncWebServerRequest* request);
    void handleGetWind(AsyncWebServerRequest* request);
    void handleGetAIS(AsyncWebServerRequest* request);
    void handleGetBoatState(AsyncWebServerRequest* request);
    void handleGetPerformance(AsyncWebServerRequest* request);

    // WiFi scan handlers
    void handleStartWiFiScan(AsyncWebServerRequest* request);
    void handleGetWiFiScanResults(AsyncWebServerRequest* request);

    // Autopilot handler
    void handlePostAutopilotCommand(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // WebSocket handlers
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                             AwsEventType type, void* arg, uint8_t* data, size_t len);

    // Members
    AsyncWebServer* server;
    AsyncWebSocket* wsNMEA;
    ConfigManager*  configManager;
    WiFiManager*    wifiManager;
    TCPServer*      tcpServer;
    UARTHandler*    uartHandler;
    NMEAParser*     nmeaParser;
    BoatState*      boatState;
    BLEManager*     bleManager;
    SeatalkRMT*     seatalkRMT;
    bool            running;
};

#endif // WEB_SERVER_H
