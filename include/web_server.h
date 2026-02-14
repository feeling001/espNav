#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <LittleFS.h>
#include "config_manager.h"
#include "wifi_manager.h"
#include "boat_state.h"

// Forward declarations
class TCPServer;
class UARTHandler;
class NMEAParser;

class WebServer {
public:
    WebServer(ConfigManager* cm, WiFiManager* wm, TCPServer* tcp, UARTHandler* uart, NMEAParser* nmea, BoatState* bs);
    
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
    
    // WiFi scan handlers
    void handleStartWiFiScan(AsyncWebServerRequest* request);
    void handleGetWiFiScanResults(AsyncWebServerRequest* request);
    
    // REST API handlers - BoatState (NEW!)
    void handleGetNavigation(AsyncWebServerRequest* request);
    void handleGetWind(AsyncWebServerRequest* request);
    void handleGetAIS(AsyncWebServerRequest* request);
    void handleGetBoatState(AsyncWebServerRequest* request);  // All data
    
    // WebSocket handlers
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                             AwsEventType type, void* arg, uint8_t* data, size_t len);
    
    AsyncWebServer* server;
    AsyncWebSocket* wsNMEA;
    ConfigManager* configManager;
    WiFiManager* wifiManager;
    TCPServer* tcpServer;
    UARTHandler* uartHandler;
    NMEAParser* nmeaParser;
    BoatState* boatState;
    bool running;
};

#endif // WEB_SERVER_H
