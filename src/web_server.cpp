#include "web_server.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "uart_handler.h"
#include "tcp_server.h"
#include "nmea_parser.h"
#include <ArduinoJson.h>

// External variables from main.cpp for monitoring
extern volatile uint32_t g_nmeaQueueOverflows;
extern volatile uint32_t g_nmeaQueueFullEvents;


WebServer::WebServer(ConfigManager* cm, WiFiManager* wm, TCPServer* tcp, UARTHandler* uart, NMEAParser* nmea, BoatState* bs, BLEManager* ble) 
    : configManager(cm), wifiManager(wm), tcpServer(tcp), uartHandler(uart), nmeaParser(nmea), boatState(bs), bleManager(ble), running(false) {
    server = new AsyncWebServer(WEB_SERVER_PORT);
    wsNMEA = new AsyncWebSocket("/ws/nmea");
}


void WebServer::init() {
    Serial.println("[Web] Initializing Web Server");
    
    // LittleFS is already mounted in main.cpp - no need to mount again
    Serial.println("[Web] Using already-mounted LittleFS");
    
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
    Serial.println("[Web]   Configuration:");
    Serial.println("[Web]   - GET  /api/config/wifi      (Get WiFi Config)");
    Serial.println("[Web]   - POST /api/config/wifi      (Set WiFi Config)");
    Serial.println("[Web]   - GET  /api/config/serial    (Get Serial Config)");
    Serial.println("[Web]   - POST /api/config/serial    (Set Serial Config)");
    Serial.println("[Web]   - GET  /api/status           (System Status)");
    Serial.println("[Web]   - POST /api/restart          (Restart Device)");
    Serial.println("[Web]   - POST /api/wifi/scan        (Start WiFi Scan)");
    Serial.println("[Web]   - GET  /api/wifi/scan        (Get Scan Results)");
    Serial.println("[Web]   Boat Data:");
    Serial.println("[Web]   - GET  /api/boat/navigation  (GPS, Speed, Depth, Heading)");
    Serial.println("[Web]   - GET  /api/boat/wind        (Apparent & True Wind)");
    Serial.println("[Web]   - GET  /api/boat/ais         (AIS Targets)");
    Serial.println("[Web]   - GET  /api/boat/state       (All Boat Data)");
    Serial.println("[Web]   WebSocket:");
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
    // REST API endpoints (MUST be registered BEFORE static files!)
    // ============================================================
    Serial.println("[Web]   Registering API endpoints...");
    
    // ────────────────────────────────────────────────────────────
    // Configuration Endpoints
    // ────────────────────────────────────────────────────────────
    
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

    // BLE Configuration
    server->on("/api/config/ble", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetBLEConfig(request);
    });
    
    server->on("/api/config/ble", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            this->handlePostBLEConfig(request, data, len);
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
    
    // ────────────────────────────────────────────────────────────
    // Boat State Endpoints (NEW!)
    // ────────────────────────────────────────────────────────────
    
    server->on("/api/boat/navigation", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetNavigation(request);
    });
    
    server->on("/api/boat/wind", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetWind(request);
    });
    
    server->on("/api/boat/ais", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetAIS(request);
    });
    
    server->on("/api/boat/state", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetBoatState(request);
    });
    
    Serial.println("[Web]   ✓ All API routes registered");
    
    // ============================================================
    // Static file serving from LittleFS (AFTER API routes!)
    // ============================================================
    Serial.println("[Web]   Registering static file handler...");
    
    // Serve static files - try /www/ first, fallback to /
    server->serveStatic("/", LittleFS, "/www/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=600");
    
    Serial.println("[Web]   ✓ Static file handler registered");
    
    // ============================================================
    // 404 Handler (LAST!) - Serve index.html for client-side routing
    // ============================================================
    server->onNotFound([](AsyncWebServerRequest* request) {
        // If it's an API request, return 404
        if (request->url().startsWith("/api/") || request->url().startsWith("/ws/")) {
            Serial.printf("[Web] 404 API: %s %s\n", 
                         request->methodToString(), 
                         request->url().c_str());
            request->send(404, "text/plain", "Not Found");
            return;
        }
        
        // For all other routes, serve index.html to support client-side routing
        Serial.printf("[Web] SPA Fallback: %s → index.html\n", request->url().c_str());
        request->send(LittleFS, "/www/index.html", "text/html");
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
        case WS_EVT_PONG:
            // Ignore
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
// Configuration API Handlers
// ============================================================

void WebServer::handleGetWiFiConfig(AsyncWebServerRequest* request) {
    Serial.println("[Web] → GET /api/config/wifi");
    
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
    Serial.println("[Web] → POST /api/config/wifi");
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (char*)data, len);
    
    if (error) {
        Serial.printf("[Web]   JSON error: %s\n", error.c_str());
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
}

void WebServer::handleGetSerialConfig(AsyncWebServerRequest* request) {
    Serial.println("[Web] → GET /api/config/serial");
    
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
    Serial.println("[Web] → POST /api/config/serial");
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (char*)data, len);
    
    if (error) {
        Serial.printf("[Web]   JSON error: %s\n", error.c_str());
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
}

void WebServer::handleGetStatus(AsyncWebServerRequest* request) {
    Serial.println("[Web] → GET /api/status");
    
    JsonDocument doc;
    
    doc["uptime"] = millis() / 1000;
    
    JsonObject heap = doc["heap"].to<JsonObject>();
    heap["free"] = ESP.getFreeHeap();
    heap["total"] = ESP.getHeapSize();
    heap["min_free"] = ESP.getMinFreeHeap();
    
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    
    // Get WiFi state - but also check direct WiFi status as fallback
    WiFiState state = wifiManager->getState();
    wl_status_t wifiStatus = WiFi.status();
    
    const char* modeStr = "Unknown";
    String ssid = "";
    int8_t rssi = 0;
    IPAddress ip(0, 0, 0, 0);
    size_t clients = 0;
    
    // Determine actual mode and get info
    if (wifiStatus == WL_CONNECTED || state == WIFI_CONNECTED_STA) {
        modeStr = "STA";
        ssid = WiFi.SSID();
        rssi = WiFi.RSSI();
        ip = WiFi.localIP();
    } else if (WiFi.getMode() == WIFI_AP || state == WIFI_AP_MODE) {
        modeStr = "AP";
        ssid = wifiManager->getSSID();
        ip = WiFi.softAPIP();
        clients = WiFi.softAPgetStationNum();
    } else {
        switch (state) {
            case WIFI_DISCONNECTED: modeStr = "Disconnected"; break;
            case WIFI_CONNECTING: modeStr = "Connecting"; break;
            case WIFI_RECONNECTING: modeStr = "Reconnecting"; break;
            default: modeStr = "Unknown"; break;
        }
    }
    
    wifi["mode"] = modeStr;
    wifi["ssid"] = ssid;
    wifi["rssi"] = rssi;
    wifi["ip"] = ip.toString();
    wifi["clients"] = clients;
    
    // TCP info
    JsonObject tcp = doc["tcp"].to<JsonObject>();
    tcp["clients"] = tcpServer->getClientCount();
    tcp["port"] = TCP_PORT;
    
    // UART info
    JsonObject uart = doc["uart"].to<JsonObject>();
    uart["sentences_received"] = uartHandler->getSentencesReceived();
    uart["errors"] = nmeaParser->getInvalidSentences();
    
    // Get serial config for baud rate
    UARTConfig serialConfig;
    configManager->getSerialConfig(serialConfig);
    uart["baud"] = serialConfig.baudRate;

    

    // NMEA Buffer info (NOUVEAU!)
    JsonObject nmeaBuffer = doc["nmea_buffer"].to<JsonObject>();
    nmeaBuffer["queue_size"] = NMEA_QUEUE_SIZE;
    nmeaBuffer["overflow_total"] = g_nmeaQueueOverflows;
    nmeaBuffer["full_events_recent"] = g_nmeaQueueFullEvents;
    nmeaBuffer["has_overflow"] = (g_nmeaQueueFullEvents > 0);
    
    // CPU Load info 

// Dans web_server.cpp - handleGetStatus()
JsonObject cpu = doc["cpu"].to<JsonObject>();

// Méthode robuste compatible Arduino
static uint32_t lastCheckTime = 0;
static uint32_t lastSentenceCount = 0;
static uint32_t lastHeapFree = 0;

uint32_t now = millis();
uint32_t currentSentences = uartHandler->getSentencesReceived();
uint32_t currentHeapFree = ESP.getFreeHeap();

if (lastCheckTime > 0 && (now - lastCheckTime) >= 5000) {
    // Mesure sur 5 secondes
    uint32_t deltaTime = now - lastCheckTime;
    uint32_t deltaSentences = currentSentences - lastSentenceCount;
    
    // Calcul des phrases par seconde
    float sentencesPerSec = (float)(deltaSentences * 1000) / deltaTime;
    
    // Calcul de la variation de heap (activité mémoire)
    int32_t heapChange = currentHeapFree - lastHeapFree;
    uint32_t heapChurn = abs(heapChange);
    
    // Estimation CPU basée sur plusieurs facteurs
    uint32_t cpuEstimate = 0;
    
    // Facteur 1: Débit de phrases (principal indicateur)
    if (sentencesPerSec < 2.0f) {
        cpuEstimate = (uint32_t)(sentencesPerSec * 8);  // 0-16%
    } else if (sentencesPerSec < 5.0f) {
        cpuEstimate = 16 + (uint32_t)((sentencesPerSec - 2.0f) * 8);  // 16-40%
    } else if (sentencesPerSec < 10.0f) {
        cpuEstimate = 40 + (uint32_t)((sentencesPerSec - 5.0f) * 6);  // 40-70%
    } else {
        cpuEstimate = 70 + (uint32_t)(min((sentencesPerSec - 10.0f) * 3, 30.0f));  // 70-100%
    }
    
    // Facteur 2: Nombre de clients TCP/WebSocket (overhead réseau)
    uint32_t totalClients = tcpServer->getClientCount();
    cpuEstimate += totalClients * 2;  // +2% par client
    
    // Facteur 3: BLE actif
    if (bleManager->isEnabled() && bleManager->getConnectedDevices() > 0) {
        cpuEstimate += 5;  // +5% pour BLE
    }
    
    // Plafonner à 100%
    cpuEstimate = min(cpuEstimate, (uint32_t)100);
    
    cpu["usage_percent"] = cpuEstimate;
    cpu["sentences_per_sec"] = (uint32_t)(sentencesPerSec * 10) / 10.0f;  // 1 décimale
    cpu["tcp_clients"] = totalClients;
    cpu["method"] = "composite";
    
    lastSentenceCount = currentSentences;
    lastHeapFree = currentHeapFree;
    lastCheckTime = now;
} else {
    // Initialisation
    if (lastCheckTime == 0) {
        lastCheckTime = now;
        lastSentenceCount = currentSentences;
        lastHeapFree = currentHeapFree;
    }
    cpu["usage_percent"] = 0;
    cpu["sentences_per_sec"] = 0;
    cpu["method"] = "initializing";
}


    // BLE info
    JsonObject ble = doc["ble"].to<JsonObject>();
    ble["enabled"] = bleManager->isEnabled();
    ble["advertising"] = bleManager->isAdvertising();
    ble["connected_devices"] = bleManager->getConnectedDevices();
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}


void WebServer::handleRestart(AsyncWebServerRequest* request) {
    Serial.println("[Web] → POST /api/restart");
    
    request->send(200, "application/json",
                 "{\"success\":true,\"message\":\"Restarting in 2 seconds\"}");
    
    Serial.println("[Web]   Restarting...");
    delay(2000);
    ESP.restart();
}

// ============================================================
// WiFi Scan Handlers
// ============================================================

void WebServer::handleStartWiFiScan(AsyncWebServerRequest* request) {
    Serial.println("[Web] → POST /api/wifi/scan");
    
    int16_t result = wifiManager->startScan();
    
    if (result == WIFI_SCAN_RUNNING) {
        request->send(200, "application/json",
                     "{\"success\":true,\"message\":\"WiFi scan started\"}");
    } else if (result == -1) {
        request->send(500, "application/json",
                     "{\"success\":false,\"error\":\"Failed to start scan\"}");
    } else {
        request->send(200, "application/json",
                     "{\"success\":true,\"message\":\"WiFi scan completed\"}");
    }
}

void WebServer::handleGetWiFiScanResults(AsyncWebServerRequest* request) {
    Serial.println("[Web] → GET /api/wifi/scan");
    
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
    }
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

// ════════════════════════════════════════════════════════════
// BOAT STATE API HANDLERS (NEW!)
// ════════════════════════════════════════════════════════════

/**
 * GET /api/boat/navigation
 * 
 * Returns critical navigation data:
 * - GPS position (lat/lon)
 * - Speed Over Ground (SOG)
 * - Course Over Ground (COG)
 * - Speed Through Water (STW)
 * - True Heading
 * - Depth
 * - GPS quality indicators
 */
void WebServer::handleGetNavigation(AsyncWebServerRequest* request) {
    Serial.println("[Web] → GET /api/boat/navigation");
    
    if (!boatState) {
        request->send(500, "application/json", "{\"error\":\"BoatState not available\"}");
        return;
    }
    
    JsonDocument doc;
    
    // Get data from BoatState
    GPSData gps = boatState->getGPS();
    SpeedData speed = boatState->getSpeed();
    HeadingData heading = boatState->getHeading();
    DepthData depth = boatState->getDepth();
    
    // GPS Position
    JsonObject position = doc["position"].to<JsonObject>();
    if (gps.position.lat.valid && !gps.position.lat.isStale()) {
        position["latitude"] = gps.position.lat.value;
        position["longitude"] = gps.position.lon.value;
        position["age"] = (millis() - gps.position.lat.timestamp) / 1000.0;
    } else {
        position["latitude"] = nullptr;
        position["longitude"] = nullptr;
        position["age"] = nullptr;
    }
    
    // Speed Over Ground
    if (gps.sog.valid && !gps.sog.isStale()) {
        doc["sog"]["value"] = gps.sog.value;
        doc["sog"]["unit"] = gps.sog.unit;
        doc["sog"]["age"] = (millis() - gps.sog.timestamp) / 1000.0;
    } else {
        doc["sog"]["value"] = nullptr;
        doc["sog"]["unit"] = "kn";
        doc["sog"]["age"] = nullptr;
    }
    
    // Course Over Ground
    if (gps.cog.valid && !gps.cog.isStale()) {
        doc["cog"]["value"] = gps.cog.value;
        doc["cog"]["unit"] = gps.cog.unit;
        doc["cog"]["age"] = (millis() - gps.cog.timestamp) / 1000.0;
    } else {
        doc["cog"]["value"] = nullptr;
        doc["cog"]["unit"] = "deg";
        doc["cog"]["age"] = nullptr;
    }
    
    // Speed Through Water
    if (speed.stw.valid && !speed.stw.isStale()) {
        doc["stw"]["value"] = speed.stw.value;
        doc["stw"]["unit"] = speed.stw.unit;
        doc["stw"]["age"] = (millis() - speed.stw.timestamp) / 1000.0;
    } else {
        doc["stw"]["value"] = nullptr;
        doc["stw"]["unit"] = "kn";
        doc["stw"]["age"] = nullptr;
    }
    
    // True Heading
    if (heading.true_heading.valid && !heading.true_heading.isStale()) {
        doc["heading"]["value"] = heading.true_heading.value;
        doc["heading"]["unit"] = heading.true_heading.unit;
        doc["heading"]["age"] = (millis() - heading.true_heading.timestamp) / 1000.0;
    } else {
        doc["heading"]["value"] = nullptr;
        doc["heading"]["unit"] = "deg";
        doc["heading"]["age"] = nullptr;
    }
    
    // Depth
    if (depth.below_transducer.valid && !depth.below_transducer.isStale()) {
        doc["depth"]["value"] = depth.below_transducer.value;
        doc["depth"]["unit"] = depth.below_transducer.unit;
        doc["depth"]["age"] = (millis() - depth.below_transducer.timestamp) / 1000.0;
    } else {
        doc["depth"]["value"] = nullptr;
        doc["depth"]["unit"] = "m";
        doc["depth"]["age"] = nullptr;
    }
    
    // GPS Quality
    JsonObject quality = doc["gps_quality"].to<JsonObject>();
    if (gps.satellites.valid && !gps.satellites.isStale()) {
        quality["satellites"] = (int)gps.satellites.value;
    } else {
        quality["satellites"] = nullptr;
    }
    
    if (gps.fix_quality.valid && !gps.fix_quality.isStale()) {
        quality["fix_quality"] = (int)gps.fix_quality.value;
    } else {
        quality["fix_quality"] = nullptr;
    }
    
    if (gps.hdop.valid && !gps.hdop.isStale()) {
        quality["hdop"] = gps.hdop.value;
    } else {
        quality["hdop"] = nullptr;
    }
    
    // Trip & Total distance
    if (speed.trip.valid && !speed.trip.isStale()) {
        doc["trip"]["value"] = speed.trip.value;
        doc["trip"]["unit"] = speed.trip.unit;
    } else {
        doc["trip"]["value"] = nullptr;
        doc["trip"]["unit"] = "nm";
    }
    
    if (speed.total.valid && !speed.total.isStale()) {
        doc["total"]["value"] = speed.total.value;
        doc["total"]["unit"] = speed.total.unit;
    } else {
        doc["total"]["value"] = nullptr;
        doc["total"]["unit"] = "nm";
    }
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

/**
 * GET /api/boat/wind
 * 
 * Returns wind data:
 * - Apparent Wind Speed (AWS)
 * - Apparent Wind Angle (AWA)
 * - True Wind Speed (TWS) - calculated
 * - True Wind Angle (TWA) - calculated
 * - True Wind Direction (TWD) - calculated
 */

 void WebServer::handleGetWind(AsyncWebServerRequest* request) {
    Serial.println("[Web] → GET /api/boat/wind");
    
    if (!boatState) {
        request->send(500, "application/json", "{\"error\":\"BoatState not available\"}");
        return;
    }
    
    JsonDocument doc;
    WindData wind = boatState->getWind();
    
    // AWS
    if (wind.aws.valid && !wind.aws.isStale()) {
        doc["aws"]["value"] = wind.aws.value;
        doc["aws"]["unit"] = wind.aws.unit;
        doc["aws"]["age"] = (millis() - wind.aws.timestamp) / 1000.0;
    } else {
        doc["aws"]["value"] = nullptr;
        doc["aws"]["unit"] = "kn";
        doc["aws"]["age"] = nullptr;
    }
    
    // AWA
    if (wind.awa.valid && !wind.awa.isStale()) {
        doc["awa"]["value"] = wind.awa.value;
        doc["awa"]["unit"] = wind.awa.unit;
        doc["awa"]["age"] = (millis() - wind.awa.timestamp) / 1000.0;
    } else {
        doc["awa"]["value"] = nullptr;
        doc["awa"]["unit"] = "deg";
        doc["awa"]["age"] = nullptr;
    }
    
    // TWS
    if (wind.tws.valid && !wind.tws.isStale()) {
        doc["tws"]["value"] = wind.tws.value;
        doc["tws"]["unit"] = wind.tws.unit;
        doc["tws"]["age"] = (millis() - wind.tws.timestamp) / 1000.0;
    } else {
        doc["tws"]["value"] = nullptr;
        doc["tws"]["unit"] = "kn";
        doc["tws"]["age"] = nullptr;
    }
    
    // TWA
    if (wind.twa.valid && !wind.twa.isStale()) {
        doc["twa"]["value"] = wind.twa.value;
        doc["twa"]["unit"] = wind.twa.unit;
        doc["twa"]["age"] = (millis() - wind.twa.timestamp) / 1000.0;
    } else {
        doc["twa"]["value"] = nullptr;
        doc["twa"]["unit"] = "deg";
        doc["twa"]["age"] = nullptr;
    }
    
    // TWD
    if (wind.twd.valid && !wind.twd.isStale()) {
        doc["twd"]["value"] = wind.twd.value;
        doc["twd"]["unit"] = wind.twd.unit;
        doc["twd"]["age"] = (millis() - wind.twd.timestamp) / 1000.0;
    } else {
        doc["twd"]["value"] = nullptr;
        doc["twd"]["unit"] = "deg";
        doc["twd"]["age"] = nullptr;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}


/**
 * GET /api/boat/ais
 * 
 * Returns AIS targets data:
 * - List of all active AIS targets
 * - Each target contains: MMSI, name, position, COG, SOG, heading
 * - CPA/TCPA calculations
 */
void WebServer::handleGetAIS(AsyncWebServerRequest* request) {
    Serial.println("[Web] → GET /api/boat/ais");
    
    if (!boatState) {
        request->send(500, "application/json", "{\"error\":\"BoatState not available\"}");
        return;
    }
    
    JsonDocument doc;
    
    // Get AIS data from BoatState
    AISData ais = boatState->getAIS();
    
    doc["target_count"] = ais.targetCount;
    
    JsonArray targets = doc["targets"].to<JsonArray>();
    
    for (int i = 0; i < ais.targetCount; i++) {
        AISTarget& target = ais.targets[i];
        unsigned long age = (millis() - target.timestamp) / 1000;
        
        // Only include targets that are not stale (< 60 seconds old)
        if (age <= DATA_TIMEOUT_AIS / 1000) {
            JsonObject t = targets.add<JsonObject>();
            
            t["mmsi"] = target.mmsi;
            t["name"] = target.name;
            
            JsonObject pos = t["position"].to<JsonObject>();
            pos["latitude"] = target.lat;
            pos["longitude"] = target.lon;
            
            t["cog"] = target.cog;
            t["sog"] = target.sog;
            t["heading"] = target.heading;
            
            JsonObject proximity = t["proximity"].to<JsonObject>();
            proximity["distance"] = target.distance;
            proximity["distance_unit"] = "nm";
            proximity["bearing"] = target.bearing;
            proximity["bearing_unit"] = "deg";
            proximity["cpa"] = target.cpa;
            proximity["cpa_unit"] = "nm";
            proximity["tcpa"] = target.tcpa;
            proximity["tcpa_unit"] = "min";
            
            t["age"] = age;
        }
    }
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

/**
 * GET /api/boat/state
 * 
 * Returns complete boat state (all data combined)
 * This is the comprehensive endpoint that returns everything
 */
void WebServer::handleGetBoatState(AsyncWebServerRequest* request) {
    Serial.println("[Web] → GET /api/boat/state");
    
    if (!boatState) {
        request->send(500, "application/json", "{\"error\":\"BoatState not available\"}");
        return;
    }
    
    // Use the built-in toJSON() method from BoatState
    String json = boatState->toJSON();
    
    request->send(200, "application/json", json);
}

void WebServer::handleGetBLEConfig(AsyncWebServerRequest* request) {
    Serial.println("[Web] → GET /api/config/ble");
    
    BLEConfig config = bleManager->getConfig();
    
    JsonDocument doc;
    doc["enabled"] = config.enabled;
    doc["device_name"] = config.device_name;
    doc["pin_code"] = config.pin_code;
    doc["advertising"] = bleManager->isAdvertising();
    doc["connected_devices"] = bleManager->getConnectedDevices();
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

void WebServer::handlePostBLEConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    Serial.println("[Web] → POST /api/config/ble");
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (char*)data, len);
    
    if (error) {
        Serial.printf("[Web]   JSON error: %s\n", error.c_str());
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    BLEConfig config;
    config.enabled = doc["enabled"] | false;
    
    strncpy(config.device_name, doc["device_name"] | "MarineGateway", sizeof(config.device_name) - 1);
    config.device_name[sizeof(config.device_name) - 1] = '\0';
    
    strncpy(config.pin_code, doc["pin_code"] | "123456", sizeof(config.pin_code) - 1);
    config.pin_code[sizeof(config.pin_code) - 1] = '\0';
    
    // Validate PIN code (must be 6 digits)
    if (strlen(config.pin_code) != 6) {
        request->send(400, "application/json", "{\"error\":\"PIN code must be exactly 6 digits\"}");
        return;
    }
    
    for (int i = 0; i < 6; i++) {
        if (!isdigit(config.pin_code[i])) {
            request->send(400, "application/json", "{\"error\":\"PIN code must contain only digits\"}");
            return;
        }
    }
    
    // Save to NVS
    BLEConfigData bleConfigData;
    bleConfigData.enabled = config.enabled;
    strncpy(bleConfigData.device_name, config.device_name, sizeof(bleConfigData.device_name) - 1);
    strncpy(bleConfigData.pin_code, config.pin_code, sizeof(bleConfigData.pin_code) - 1);
    
    configManager->setBLEConfig(bleConfigData);
    
    // Apply configuration
    bleManager->setEnabled(config.enabled);
    if (strcmp(bleManager->getConfig().device_name, config.device_name) != 0) {
        bleManager->setDeviceName(config.device_name);
    }
    bleManager->setPinCode(config.pin_code);
    
    request->send(200, "application/json",
                 "{\"success\":true,\"message\":\"BLE config saved and applied\"}");
}