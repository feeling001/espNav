#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <LittleFS.h>
#include "config_manager.h"
#include "wifi_manager.h"

class WebServer {
public:
    WebServer(ConfigManager* cm, WiFiManager* wm);
    
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
    bool running;
};

#endif // WEB_SERVER_H
