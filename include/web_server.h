#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "types.h"

// Forward declarations
class ConfigManager;
class WiFiManager;
class UARTHandler;
class TCPServer;

class WebServer {
public:
    WebServer();
    ~WebServer();
    
    void init(ConfigManager* configMgr, WiFiManager* wifiMgr, 
              UARTHandler* uartHandler, TCPServer* tcpServer);
    void start();
    void stop();
    
    void broadcastNMEA(const char* sentence);
    
private:
    // REST API handlers
    void handleGetWiFiConfig(AsyncWebServerRequest* request);
    void handlePostWiFiConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetSerialConfig(AsyncWebServerRequest* request);
    void handlePostSerialConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleRestart(AsyncWebServerRequest* request);
    
    // WebSocket handlers
    void onWSEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                   AwsEventType type, void* arg, uint8_t* data, size_t len);
    
    void registerRoutes();
    
    AsyncWebServer* server;
    AsyncWebSocket* wsNMEA;
    
    ConfigManager* configManager;
    WiFiManager* wifiManager;
    UARTHandler* uartHandler;
    TCPServer* tcpServer;
    
    bool initialized;
    bool running;
};

#endif // WEB_SERVER_H
