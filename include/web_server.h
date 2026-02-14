#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <LittleFS.h>
#include "config_manager.h"
#include "wifi_manager.h"
#include "ble_manager.h"  // Include complet au lieu de forward declaration

// Forward declarations
class TCPServer;
class UARTHandler;
class NMEAParser;
class BoatState;

class WebServer {
public:
    // CONSTRUCTEUR AVEC BOATSTATE
    WebServer(ConfigManager* cm, WiFiManager* wm, TCPServer* tcp, UARTHandler* uart, NMEAParser* nmea, BoatState* bs, BLEManager* ble);
    
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
    
    // Boat data handlers - AJOUTÉ
    void handleGetNavigation(AsyncWebServerRequest* request);
    void handleGetWind(AsyncWebServerRequest* request);
    void handleGetAIS(AsyncWebServerRequest* request);
    void handleGetBoatState(AsyncWebServerRequest* request);
    
    // WiFi scan handlers
    void handleStartWiFiScan(AsyncWebServerRequest* request);
    void handleGetWiFiScanResults(AsyncWebServerRequest* request);
    
    // WebSocket handlers
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                             AwsEventType type, void* arg, uint8_t* data, size_t len);
    
    // Membres privés
    AsyncWebServer* server;
    AsyncWebSocket* wsNMEA;
    ConfigManager* configManager;
    WiFiManager* wifiManager;
    TCPServer* tcpServer;
    UARTHandler* uartHandler;
    NMEAParser* nmeaParser;
    BoatState* boatState;  // AJOUTÉ
    BLEManager* bleManager;
    bool running;
};

#endif // WEB_SERVER_H
