#include "web_server.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "uart_handler.h"
#include "tcp_server.h"

WebServer::WebServer() 
    : server(NULL), wsNMEA(NULL), configManager(NULL), wifiManager(NULL),
      uartHandler(NULL), tcpServer(NULL), initialized(false), running(false) {}

WebServer::~WebServer() {
    stop();
    
    if (wsNMEA) {
        delete wsNMEA;
    }
    
    if (server) {
        delete server;
    }
}

void WebServer::init(ConfigManager* configMgr, WiFiManager* wifiMgr, 
                     UARTHandler* uart, TCPServer* tcp) {
    if (initialized) {
        return;
    }
    
    configManager = configMgr;
    wifiManager = wifiMgr;
    uartHandler = uart;
    tcpServer = tcp;
    
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("[Web] LittleFS mount failed");
        // Continue anyway, API will still work
    } else {
        Serial.println("[Web] LittleFS mounted");
    }
    
    server = new AsyncWebServer(WEB_SERVER_PORT);
    wsNMEA = new AsyncWebSocket("/ws/nmea");
    
    // Set up WebSocket
    wsNMEA->onEvent([this](AsyncWebSocket* srv, AsyncWebSocketClient* client,
                           AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->onWSEvent(srv, client, type, arg, data, len);
    });
    
    server->addHandler(wsNMEA);
    
    registerRoutes();
    
    initialized = true;
    
    Serial.println("[Web] Server initialized");
}

void WebServer::start() {
    if (!initialized || running) {
        return;
    }
    
    server->begin();
    running = true;
    
    Serial.printf("[Web] Server started on port %d\n", WEB_SERVER_PORT);
}

void WebServer::stop() {
    if (!running) {
        return;
    }
    
    running = false;
    server->end();
    
    Serial.println("[Web] Server stopped");
}

void WebServer::registerRoutes() {
    // Serve static files from LittleFS
    server->serveStatic("/", LittleFS, "/www/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=86400");
    
    // REST API endpoints
    server->on("/api/config/wifi", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetWiFiConfig(request);
    });
    
    server->on("/api/config/wifi", HTTP_POST, 
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, 
               size_t index, size_t total) {
            this->handlePostWiFiConfig(request, data, len);
        }
    );
    
    server->on("/api/config/serial", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetSerialConfig(request);
    });
    
    server->on("/api/config/serial", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            this->handlePostSerialConfig(request, data, len);
        }
    );
    
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetStatus(request);
    });
    
    server->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleRestart(request);
    });
    
    // 404 handler
    server->onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not found");
    });
}

void WebServer::handleGetWiFiConfig(AsyncWebServerRequest* request) {
    WiFiConfig config;
    configManager->getWiFiConfig(config);
    
    StaticJsonDocument<256> doc;
    doc["ssid"] = config.ssid;
    doc["mode"] = config.mode;
    doc["has_password"] = (strlen(config.password) > 0);
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

void WebServer::handlePostWiFiConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, (char*)data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    WiFiConfig config;
    strncpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid) - 1);
    config.ssid[sizeof(config.ssid) - 1] = '\0';
    
    strncpy(config.password, doc["password"] | "", sizeof(config.password) - 1);
    config.password[sizeof(config.password) - 1] = '\0';
    
    config.mode = doc["mode"] | 0;
    
    configManager->setWiFiConfig(config);
    
    request->send(200, "application/json", 
                 "{\"success\":true,\"message\":\"WiFi config saved. Restart to apply.\"}");
    
    Serial.println("[Web] WiFi config updated, restart required");
}

void WebServer::handleGetSerialConfig(AsyncWebServerRequest* request) {
    SerialConfig config;
    configManager->getSerialConfig(config);
    
    StaticJsonDocument<256> doc;
    doc["baudRate"] = config.baudRate;
    doc["dataBits"] = config.dataBits;
    doc["parity"] = config.parity;
    doc["stopBits"] = config.stopBits;
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

void WebServer::handlePostSerialConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, (char*)data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    SerialConfig config;
    config.baudRate = doc["baudRate"] | 38400;
    config.dataBits = doc["dataBits"] | 8;
    config.parity = doc["parity"] | 0;
    config.stopBits = doc["stopBits"] | 1;
    
    configManager->setSerialConfig(config);
    
    request->send(200, "application/json",
                 "{\"success\":true,\"message\":\"Serial config saved. Restart to apply.\"}");
    
    Serial.println("[Web] Serial config updated, restart required");
}

void WebServer::handleGetStatus(AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(1024);
    
    doc["uptime"] = millis() / 1000;
    
    JsonObject heap = doc.createNestedObject("heap");
    heap["free"] = ESP.getFreeHeap();
    heap["total"] = ESP.getHeapSize();
    heap["min_free"] = ESP.getMinFreeHeap();
    
    JsonObject wifi = doc.createNestedObject("wifi");
    
    const char* modeStr = "Unknown";
    switch (wifiManager->getState()) {
        case WIFI_DISCONNECTED: modeStr = "Disconnected"; break;
        case WIFI_CONNECTING: modeStr = "Connecting"; break;
        case WIFI_CONNECTED_STA: modeStr = "STA"; break;
        case WIFI_RECONNECTING: modeStr = "Reconnecting"; break;
        case WIFI_AP_MODE: modeStr = "AP"; break;
    }
    
    wifi["mode"] = modeStr;
    wifi["ssid"] = wifiManager->getSSID();
    wifi["rssi"] = wifiManager->getRSSI();
    wifi["ip"] = wifiManager->getIP().toString();
    wifi["clients"] = wifiManager->getConnectedClients();
    
    JsonObject tcp = doc.createNestedObject("tcp");
    tcp["clients"] = tcpServer->getClientCount();
    tcp["port"] = TCP_PORT;
    
    JsonObject uart = doc.createNestedObject("uart");
    uart["baud"] = 38400;  // TODO: Get from config
    uart["sentences_received"] = uartHandler->getSentencesReceived();
    uart["errors"] = uartHandler->getErrors();
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

void WebServer::handleRestart(AsyncWebServerRequest* request) {
    request->send(200, "application/json",
                 "{\"success\":true,\"message\":\"Restarting in 2 seconds\"}");
    
    Serial.println("[Web] Restart requested");
    
    // Restart after 2 seconds
    delay(2000);
    ESP.restart();
}

void WebServer::onWSEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[Web] WebSocket client connected: %u\n", client->id());
            break;
            
        case WS_EVT_DISCONNECT:
            Serial.printf("[Web] WebSocket client disconnected: %u\n", client->id());
            break;
            
        case WS_EVT_DATA:
            // Handle incoming WebSocket data if needed
            break;
            
        case WS_EVT_ERROR:
            Serial.printf("[Web] WebSocket error: %u\n", client->id());
            break;
            
        default:
            break;
    }
}

void WebServer::broadcastNMEA(const char* sentence) {
    if (running && wsNMEA) {
        wsNMEA->textAll(sentence);
    }
}
