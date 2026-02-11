#include "web_server.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "uart_handler.h"
#include "tcp_server.h"
#include <ArduinoJson.h>

WebServer::WebServer(ConfigManager* cm, WiFiManager* wm) 
    : configManager(cm), wifiManager(wm), running(false) {
    server = new AsyncWebServer(WEB_SERVER_PORT);
    wsNMEA = new AsyncWebSocket("/ws/nmea");
}

void WebServer::init() {
    Serial.println("[Web] Initializing Web Server");
    
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {  // true = format on failure
        Serial.println("[Web] ✗ LittleFS mount failed");
        return;
    }
    
    Serial.println("[Web] ✓ LittleFS mounted");
    
    // Setup WebSocket
    wsNMEA->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                           AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->handleWebSocketEvent(server, client, type, arg, data, len);
    });
    
    server->addHandler(wsNMEA);
    
    // Register routes
    registerRoutes();
}

void WebServer::start() {
    if (running) {
        return;
    }
    
    server->begin();
    running = true;
    
    Serial.println("[Web] ═══════════════════════════════════════");
    Serial.println("[Web] Server started on port 80");
    Serial.println("[Web] Available endpoints:");
    Serial.println("[Web]   - GET  /api/config/wifi      (Get WiFi Config)");
    Serial.println("[Web]   - POST /api/config/wifi      (Set WiFi Config)");
    Serial.println("[Web]   - GET  /api/config/serial    (Get Serial Config)");
    Serial.println("[Web]   - POST /api/config/serial    (Set Serial Config)");
    Serial.println("[Web]   - GET  /api/status           (System Status)");
    Serial.println("[Web]   - POST /api/restart          (Restart Device)");
    Serial.println("[Web]   - POST /api/wifi/scan        (Start WiFi Scan)");
    Serial.println("[Web]   - GET  /api/wifi/scan        (Get Scan Results)");
    Serial.println("[Web]   - WS   /ws/nmea              (NMEA Stream)");
    Serial.println("[Web] ═══════════════════════════════════════");
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
    // ============================================================
    // Static file serving from LittleFS
    // ============================================================
    Serial.println("[Web]   Registering static file handler: /www/");
    server->serveStatic("/", LittleFS, "/www/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=86400");
    
    // Fallback for root path if /www/ doesn't exist
    server->serveStatic("/", LittleFS, "/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=86400");
    
    // ============================================================
    // REST API endpoints
    // ============================================================
    Serial.println("[Web]   Registering API endpoints...");
    
    // WiFi Configuration
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
    
    // Serial Configuration
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
    
    // System Status
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetStatus(request);
    });
    
    // Restart Device
    server->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleRestart(request);
    });
    
    // WiFi Scan endpoints
    server->on("/api/wifi/scan", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleStartWiFiScan(request);
    });
    
    server->on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetWiFiScanResults(request);
    });
    
    // 404 Handler
    server->onNotFound([](AsyncWebServerRequest* request) {
        Serial.printf("[Web] 404 Not Found: %s\n", request->url().c_str());
        request->send(404, "application/json", "{\"error\":\"Not Found\"}");
    });
    
    Serial.println("[Web]   ✓ All routes registered");
}

// WebSocket event handler
void WebServer::handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WebSocket] Client #%u connected from %s\n", 
                         client->id(), client->remoteIP().toString().c_str());
            break;
            
        case WS_EVT_DISCONNECT:
            Serial.printf("[WebSocket] Client #%u disconnected\n", client->id());
            break;
            
        case WS_EVT_DATA:
            // We don't expect data from clients on NMEA websocket
            Serial.printf("[WebSocket] Unexpected data from client #%u\n", client->id());
            break;
            
        case WS_EVT_PONG:
            Serial.printf("[WebSocket] Pong from client #%u\n", client->id());
            break;
            
        case WS_EVT_ERROR:
            Serial.printf("[WebSocket] Error from client #%u\n", client->id());
            break;
    }
}

// Broadcast NMEA sentence to all WebSocket clients
void WebServer::broadcastNMEA(const char* sentence) {
    if (wsNMEA && running) {
        wsNMEA->textAll(sentence);
    }
}

// ============================================================
// REST API Handlers Implementation
// ============================================================

void WebServer::handleGetWiFiConfig(AsyncWebServerRequest* request) {
    WiFiConfig config;
    configManager->getWiFiConfig(config);
    
    JsonDocument doc;
    doc["ssid"] = config.ssid;
    doc["mode"] = config.mode;
    doc["has_password"] = (strlen(config.password) > 0);
    
    // Include AP configuration
    doc["ap_ssid"] = config.ap_ssid;
    doc["ap_has_password"] = (strlen(config.ap_password) >= 8);
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

void WebServer::handlePostWiFiConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (char*)data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    WiFiConfig config;
    
    // STA configuration
    strncpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid) - 1);
    config.ssid[sizeof(config.ssid) - 1] = '\0';
    
    strncpy(config.password, doc["password"] | "", sizeof(config.password) - 1);
    config.password[sizeof(config.password) - 1] = '\0';
    
    config.mode = doc["mode"] | 0;
    
    // AP configuration
    strncpy(config.ap_ssid, doc["ap_ssid"] | "", sizeof(config.ap_ssid) - 1);
    config.ap_ssid[sizeof(config.ap_ssid) - 1] = '\0';
    
    strncpy(config.ap_password, doc["ap_password"] | "", sizeof(config.ap_password) - 1);
    config.ap_password[sizeof(config.ap_password) - 1] = '\0';
    
    // Save to NVS
    configManager->setWiFiConfig(config);
    
    request->send(200, "application/json", 
                 "{\"success\":true,\"message\":\"WiFi config saved. Restart to apply.\"}");
    
    Serial.println("[Web] WiFi config updated, restart required");
}

void WebServer::handleGetSerialConfig(AsyncWebServerRequest* request) {
    UARTConfig config;
    configManager->getSerialConfig(config);
    
    JsonDocument doc;
    doc["baudRate"] = config.baudRate;
    doc["dataBits"] = config.dataBits;
    doc["parity"] = config.parity;
    doc["stopBits"] = config.stopBits;
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

void WebServer::handlePostSerialConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (char*)data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    UARTConfig config;
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
    JsonDocument doc;
    
    doc["uptime"] = millis() / 1000;
    
    JsonObject heap = doc["heap"].to<JsonObject>();
    heap["free"] = ESP.getFreeHeap();
    heap["total"] = ESP.getHeapSize();
    heap["min_free"] = ESP.getMinFreeHeap();
    
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    
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

// ============================================================
// WiFi Scan Handlers
// ============================================================

void WebServer::handleStartWiFiScan(AsyncWebServerRequest* request) {
    Serial.println("[Web] WiFi scan requested");
    
    int16_t result = wifiManager->startScan();
    
    if (result == WIFI_SCAN_RUNNING) {
        request->send(200, "application/json",
                     "{\"success\":true,\"message\":\"WiFi scan started\"}");
    } else if (result == -1) {
        request->send(500, "application/json",
                     "{\"success\":false,\"error\":\"Failed to start scan\"}");
    } else {
        // Scan completed immediately (unlikely but possible)
        request->send(200, "application/json",
                     "{\"success\":true,\"message\":\"WiFi scan completed\"}");
    }
}

void WebServer::handleGetWiFiScanResults(AsyncWebServerRequest* request) {
    // Check if scan is complete
    if (!wifiManager->isScanComplete()) {
        request->send(202, "application/json",
                     "{\"scanning\":true,\"networks\":[]}");
        return;
    }
    
    // Get scan results
    std::vector<WiFiScanResult> results = wifiManager->getScanResults();
    
    JsonDocument doc;
    doc["scanning"] = false;
    
    JsonArray networks = doc["networks"].to<JsonArray>();
    
    for (const auto& result : results) {
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = result.ssid;
        network["rssi"] = result.rssi;
        network["channel"] = result.channel;
        network["encryption"] = result.encryption;
        
        // Calculate signal quality percentage (0-100)
        int quality = 0;
        if (result.rssi >= -50) {
            quality = 100;
        } else if (result.rssi >= -60) {
            quality = 90;
        } else if (result.rssi >= -70) {
            quality = 75;
        } else if (result.rssi >= -80) {
            quality = 50;
        } else if (result.rssi >= -90) {
            quality = 25;
        } else {
            quality = 10;
        }
        network["quality"] = quality;
        
        // Encryption type string
        const char* encStr = "Unknown";
        switch (result.encryption) {
            case 0: encStr = "Open"; break;
            case 1: encStr = "WEP"; break;
            case 2: encStr = "WPA"; break;
            case 3: encStr = "WPA2"; break;
            case 4: encStr = "WPA/WPA2"; break;
            case 5: encStr = "WPA2-Enterprise"; break;
            case 6: encStr = "WPA3"; break;
        }
        network["encryption_type"] = encStr;
    }
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
    
    // Clear scan results after sending
    wifiManager->clearScanResults();
}
