#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <LittleFS.h>
#include "config_manager.h"
#include "wifi_manager.h"

// Forward declarations
class TCPServer;
class UARTHandler;
class NMEAParser;
class BLEManager;

class WebServer {
public:
    WebServer(ConfigManager* cm, WiFiManager* wm, TCPServer* tcp, UARTHandler* uart, NMEAParser* nmea, BLEManager* ble);
    
    void init();
    void start();
    void stop();
    void broadcastNMEA(const char* sentence);
    
private:
    void registerRoutes();
    
    // REST API handlers
    void handleGetWiFiConfig(AsyncWebServerRequest* request);
    void handlePostWiFiConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetSerialConfig(AsyncWebServerRequest* request);
    void handlePostSerialConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleRestart(AsyncWebServerRequest* request);
    
    // BLE handlers
    void handleGetBLEConfig(AsyncWebServerRequest* request);
    void handlePostBLEConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    
    // WiFi scan handlers
    void handleStartWiFiScan(AsyncWebServerRequest* request);
    void handleGetWiFiScanResults(AsyncWebServerRequest* request);
    
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
    BLEManager* bleManager;
    bool running;
};

#endif // WEB_SERVER_H
