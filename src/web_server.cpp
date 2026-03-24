#include "web_server.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "uart_handler.h"
#include "tcp_server.h"
#include "nmea_parser.h"
#include "polar.h"
#include "functions.h"
#include <ArduinoJson.h>

// External variables from main.cpp for monitoring
extern volatile uint32_t g_nmeaQueueOverflows;
extern volatile uint32_t g_nmeaQueueFullEvents;


WebServer::WebServer(ConfigManager* cm, WiFiManager* wm, TCPServer* tcp, UARTHandler* uart, NMEAParser* nmea, BoatState* bs, BLEManager* ble)
    : configManager(cm), wifiManager(wm), tcpServer(tcp), uartHandler(uart),
      nmeaParser(nmea), boatState(bs), bleManager(ble), running(false) {
    server  = new AsyncWebServer(WEB_SERVER_PORT);
    wsNMEA  = new AsyncWebSocket("/ws/nmea");
}


void WebServer::init() {
    serialPrintf("[Web] Initializing Web Server\n");
    serialPrintf("[Web] Using already-mounted LittleFS\n");
    
    wsNMEA->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                           AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->handleWebSocketEvent(server, client, type, arg, data, len);
    });
    
    server->addHandler(wsNMEA);
    registerRoutes();
}

void WebServer::start() {
    if (running) return;
    server->begin();
    running = true;
    
    serialPrintf("[Web] ═══════════════════════════════════════\n");
    serialPrintf("[Web] Server started on port 80\n");
    serialPrintf("[Web] Available endpoints:\n");
    serialPrintf("[Web]   Configuration:\n");
    serialPrintf("[Web]   - GET  /api/config/wifi\n");
    serialPrintf("[Web]   - POST /api/config/wifi\n");
    serialPrintf("[Web]   - GET  /api/config/serial\n");
    serialPrintf("[Web]   - POST /api/config/serial\n");
    serialPrintf("[Web]   - GET  /api/status\n");
    serialPrintf("[Web]   - POST /api/restart\n");
    serialPrintf("[Web]   - POST /api/wifi/scan\n");
    serialPrintf("[Web]   - GET  /api/wifi/scan\n");
    serialPrintf("[Web]   Polar:\n");
    serialPrintf("[Web]   - GET  /api/polar/status\n");
    serialPrintf("[Web]   - POST /api/polar/upload\n");
    serialPrintf("[Web]   Boat Data:\n");
    serialPrintf("[Web]   - GET  /api/boat/navigation\n");
    serialPrintf("[Web]   - GET  /api/boat/wind\n");
    serialPrintf("[Web]   - GET  /api/boat/ais\n");
    serialPrintf("[Web]   - GET  /api/boat/state\n");
    serialPrintf("[Web]   - GET  /api/boat/performance\n");
    serialPrintf("[Web]   WebSocket:\n");
    serialPrintf("[Web]   - WS   /ws/nmea\n");
    serialPrintf("[Web] ═══════════════════════════════════════\n");
}

void WebServer::stop() {
    if (!running) return;
    running = false;
    server->end();
    serialPrintf("[Web] Server stopped\n");
}

void WebServer::registerRoutes() {
    serialPrintf("[Web]   Registering API endpoints...\n");
    
    // ── Configuration ──────────────────────────────────────────
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

    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetStatus(request);
    });

    server->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleRestart(request);
    });

    server->on("/api/wifi/scan", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleStartWiFiScan(request);
    });
    server->on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetWiFiScanResults(request);
    });

    // ── Polar ──────────────────────────────────────────────────
    server->on("/api/polar/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetPolarStatus(request);
    });

    server->on("/api/polar/upload", HTTP_POST,
        // Completion callback — send response after upload handler ran
        [this](AsyncWebServerRequest* request) {
            if (boatState->polar.isLoaded()) {
                request->send(200, "application/json",
                              "{\"success\":true,\"message\":\"Polar loaded successfully\"}");
            } else {
                request->send(422, "application/json",
                              "{\"success\":false,\"error\":\"Failed to parse polar file\"}");
            }
        },
        // File upload callback
        [this](AsyncWebServerRequest* request, const String& filename,
               size_t index, uint8_t* data, size_t len, bool final) {
            this->handleUploadPolar(request, filename, index, data, len, final);
        }
    );

    // ── Boat Data ──────────────────────────────────────────────
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
    server->on("/api/boat/performance", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetPerformance(request);
    });

    serialPrintf("[Web]   ✓ All API routes registered\n");


    server->on("/instruments", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/www/index.html", "text/html");
    });
    server->on("/performance", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/www/index.html", "text/html");
    });
    server->on("/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/www/index.html", "text/html");
    });
    server->on("/nmea", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/www/index.html", "text/html");
    });

    // ── Static files ───────────────────────────────────────────
    server->serveStatic("/", LittleFS, "/www/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=600");

    // ── 404 / SPA fallback ─────────────────────────────────────
    server->onNotFound([](AsyncWebServerRequest* request) {
        if (request->url().startsWith("/api/") || request->url().startsWith("/ws/")) {
            request->send(404, "text/plain", "Not Found");
            return;
        }
        request->send(LittleFS, "/www/index.html", "text/html");
    });

    serialPrintf("[Web]   ✓ All routes registered\n");
}

// ============================================================
// WebSocket
// ============================================================

void WebServer::handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            serialPrintf("[WebSocket] Client #%u connected from %s\n",
                          client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            serialPrintf("[WebSocket] Client #%u disconnected\n", client->id());
            break;
        default:
            break;
    }
}

void WebServer::broadcastNMEA(const char* sentence) {
    if (!wsNMEA || !running) return;

    static uint32_t lastCleanup = 0;
    uint32_t now = millis();
    if (now - lastCleanup >= 5000) {
        wsNMEA->cleanupClients();
        lastCleanup = now;
    }

    if (wsNMEA->count() == 0) return;

    static uint32_t lastSend = 0;
    static const uint32_t minInterval = 1000 / WS_MAX_RATE_HZ;
    if (now - lastSend < minInterval) return;
    lastSend = now;

    wsNMEA->textAll(sentence);
}

// ============================================================
// Configuration handlers
// ============================================================

void WebServer::handleGetWiFiConfig(AsyncWebServerRequest* request) {
    #ifdef DEBUG_WEB
    serialPrintf("[Web] → GET /api/config/wifi\n");
    #endif

    WiFiConfig config;
    configManager->getWiFiConfig(config);

    JsonDocument doc;
    doc["ssid"]           = config.ssid;
    doc["mode"]           = config.mode;
    doc["has_password"]   = (strlen(config.password) > 0);
    doc["ap_ssid"]        = config.ap_ssid;
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
    strncpy(config.ssid,        doc["ssid"]        | "", sizeof(config.ssid) - 1);
    strncpy(config.password,    doc["password"]    | "", sizeof(config.password) - 1);
    strncpy(config.ap_ssid,     doc["ap_ssid"]     | "", sizeof(config.ap_ssid) - 1);
    strncpy(config.ap_password, doc["ap_password"] | "", sizeof(config.ap_password) - 1);
    config.mode = doc["mode"] | 0;

    configManager->setWiFiConfig(config);
    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"WiFi config saved. Restart to apply.\"}");
}

void WebServer::handleGetSerialConfig(AsyncWebServerRequest* request) {
    UARTConfig config;
    configManager->getSerialConfig(config);

    JsonDocument doc;
    doc["baudRate"] = config.baudRate;
    doc["dataBits"] = config.dataBits;
    doc["parity"]   = config.parity;
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
    config.parity   = doc["parity"]   | 0;
    config.stopBits = doc["stopBits"] | 1;

    configManager->setSerialConfig(config);
    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"Serial config saved. Restart to apply.\"}");
}

void WebServer::handleGetStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;

    doc["uptime"] = millis() / 1000;

    JsonObject heap = doc["heap"].to<JsonObject>();
    heap["free"]     = ESP.getFreeHeap();
    heap["total"]    = ESP.getHeapSize();
    heap["min_free"] = ESP.getMinFreeHeap();

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    WiFiState state    = wifiManager->getState();
    wl_status_t wlStat = WiFi.status();
    const char* modeStr = "Unknown";
    String ssid = "";
    int8_t rssi = 0;
    IPAddress ip(0, 0, 0, 0);
    size_t clients = 0;

    if (wlStat == WL_CONNECTED || state == WIFI_CONNECTED_STA) {
        modeStr = "STA";
        ssid    = WiFi.SSID();
        rssi    = WiFi.RSSI();
        ip      = WiFi.localIP();
    } else if (WiFi.getMode() == WIFI_AP || state == WIFI_AP_MODE) {
        modeStr = "AP";
        ssid    = wifiManager->getSSID();
        ip      = WiFi.softAPIP();
        clients = WiFi.softAPgetStationNum();
    } else {
        switch (state) {
            case WIFI_DISCONNECTED:  modeStr = "Disconnected";  break;
            case WIFI_CONNECTING:    modeStr = "Connecting";    break;
            case WIFI_RECONNECTING:  modeStr = "Reconnecting";  break;
            default: break;
        }
    }

    wifi["mode"]    = modeStr;
    wifi["ssid"]    = ssid;
    wifi["rssi"]    = rssi;
    wifi["ip"]      = ip.toString();
    wifi["clients"] = clients;

    JsonObject tcp = doc["tcp"].to<JsonObject>();
    tcp["clients"] = tcpServer->getClientCount();
    tcp["port"]    = TCP_PORT;

    JsonObject uart = doc["uart"].to<JsonObject>();
    uart["sentences_received"] = uartHandler->getSentencesReceived();
    uart["errors"]             = nmeaParser->getInvalidSentences();
    UARTConfig serialConfig;
    configManager->getSerialConfig(serialConfig);
    uart["baud"] = serialConfig.baudRate;

    JsonObject nmeaBuffer = doc["nmea_buffer"].to<JsonObject>();
    nmeaBuffer["queue_size"]         = NMEA_QUEUE_SIZE;
    nmeaBuffer["overflow_total"]     = g_nmeaQueueOverflows;
    nmeaBuffer["full_events_recent"] = g_nmeaQueueFullEvents;
    nmeaBuffer["has_overflow"]       = (g_nmeaQueueFullEvents > 0);

    // CPU estimate
    JsonObject cpu = doc["cpu"].to<JsonObject>();
    static uint32_t lastCheckTime    = 0;
    static uint32_t lastSentenceCount = 0;
    static uint32_t lastHeapFree     = 0;
    uint32_t now                     = millis();
    uint32_t currentSentences        = uartHandler->getSentencesReceived();
    uint32_t currentHeapFree         = ESP.getFreeHeap();

    if (lastCheckTime > 0 && (now - lastCheckTime) >= 5000) {
        uint32_t deltaTime      = now - lastCheckTime;
        uint32_t deltaSentences = currentSentences - lastSentenceCount;
        float sentPerSec        = (float)(deltaSentences * 1000) / deltaTime;

        uint32_t cpuEst = 0;
        if      (sentPerSec < 2.0f)  cpuEst = (uint32_t)(sentPerSec * 8);
        else if (sentPerSec < 5.0f)  cpuEst = 16 + (uint32_t)((sentPerSec - 2.0f) * 8);
        else if (sentPerSec < 10.0f) cpuEst = 40 + (uint32_t)((sentPerSec - 5.0f) * 6);
        else                          cpuEst = 70 + (uint32_t)(min((sentPerSec - 10.0f) * 3, 30.0f));

        cpuEst += tcpServer->getClientCount() * 2;
        if (bleManager->isEnabled() && bleManager->getConnectedDevices() > 0) cpuEst += 5;
        cpuEst = min(cpuEst, (uint32_t)100);

        cpu["usage_percent"]    = cpuEst;
        cpu["sentences_per_sec"] = (uint32_t)(sentPerSec * 10) / 10.0f;
        cpu["method"]           = "composite";

        lastSentenceCount = currentSentences;
        lastHeapFree      = currentHeapFree;
        lastCheckTime     = now;
    } else {
        if (lastCheckTime == 0) {
            lastCheckTime     = now;
            lastSentenceCount = currentSentences;
            lastHeapFree      = currentHeapFree;
        }
        cpu["usage_percent"]    = 0;
        cpu["sentences_per_sec"] = 0;
        cpu["method"]           = "initializing";
    }

    JsonObject ble = doc["ble"].to<JsonObject>();
    ble["enabled"]           = bleManager->isEnabled();
    ble["advertising"]       = bleManager->isAdvertising();
    ble["connected_devices"] = bleManager->getConnectedDevices();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleRestart(AsyncWebServerRequest* request) {
    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"Restarting in 2 seconds\"}");
    serialPrintf("[Web] Restarting...\n");
    delay(2000);
    ESP.restart();
}

// ============================================================
// WiFi Scan
// ============================================================

void WebServer::handleStartWiFiScan(AsyncWebServerRequest* request) {
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
    if (!wifiManager->isScanComplete()) {
        request->send(202, "application/json", "{\"scanning\":true,\"networks\":[]}");
        return;
    }

    std::vector<WiFiScanResult> results = wifiManager->getScanResults();
    JsonDocument doc;
    doc["scanning"] = false;
    JsonArray networks = doc["networks"].to<JsonArray>();

    for (const auto& r : results) {
        JsonObject network    = networks.add<JsonObject>();
        network["ssid"]       = r.ssid;
        network["rssi"]       = r.rssi;
        network["channel"]    = r.channel;
        network["encryption"] = r.encryption;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// ============================================================
// BLE Configuration
// ============================================================

void WebServer::handleGetBLEConfig(AsyncWebServerRequest* request) {
    BLEConfig config = bleManager->getConfig();
    JsonDocument doc;
    doc["enabled"]           = config.enabled;
    doc["device_name"]       = config.device_name;
    doc["pin_code"]          = config.pin_code;
    doc["advertising"]       = bleManager->isAdvertising();
    doc["connected_devices"] = bleManager->getConnectedDevices();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handlePostBLEConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (char*)data, len);
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    BLEConfig config;
    config.enabled = doc["enabled"] | false;
    strncpy(config.device_name, doc["device_name"] | "MarineGateway",
            sizeof(config.device_name) - 1);
    strncpy(config.pin_code, doc["pin_code"] | "123456",
            sizeof(config.pin_code) - 1);

    if (strlen(config.pin_code) != 6) {
        request->send(400, "application/json",
                      "{\"error\":\"PIN code must be exactly 6 digits\"}");
        return;
    }
    for (int i = 0; i < 6; i++) {
        if (!isdigit(config.pin_code[i])) {
            request->send(400, "application/json",
                          "{\"error\":\"PIN code must contain only digits\"}");
            return;
        }
    }

    BLEConfigData bleConfigData;
    bleConfigData.enabled = config.enabled;
    strncpy(bleConfigData.device_name, config.device_name, sizeof(bleConfigData.device_name) - 1);
    strncpy(bleConfigData.pin_code,    config.pin_code,    sizeof(bleConfigData.pin_code) - 1);
    configManager->setBLEConfig(bleConfigData);

    bleManager->setEnabled(config.enabled);
    if (strcmp(bleManager->getConfig().device_name, config.device_name) != 0) {
        bleManager->setDeviceName(config.device_name);
    }
    bleManager->setPinCode(config.pin_code);

    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"BLE config saved and applied\"}");
}

// ============================================================
// Polar handlers
// ============================================================

/**
 * GET /api/polar/status
 * Returns whether a polar is loaded and basic metadata.
 */
void WebServer::handleGetPolarStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;
    bool loaded      = boatState->polar.isLoaded();
    doc["loaded"]    = loaded;
    doc["file_size"] = (int)boatState->polar.fileSize();
    doc["file_exists"] = LittleFS.exists(POLAR_FILE_PATH);

    if (loaded) {
        doc["tws_count"] = boatState->polar.twsCount();
        doc["twa_count"] = boatState->polar.twaCount();
        doc["tws_list"]  = boatState->polar.twsString();
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

/**
 * POST /api/polar/upload  (multipart file upload)
 * Writes the incoming file to LittleFS and immediately reloads the polar.
 */
void WebServer::handleUploadPolar(AsyncWebServerRequest* request,
                                   const String& filename,
                                   size_t index, uint8_t* data,
                                   size_t len, bool final) {
    static File uploadFile;

    if (index == 0) {
        serialPrintf("[Web] Polar upload started: %s\n", filename.c_str());
        if (LittleFS.exists(POLAR_FILE_PATH)) {
            LittleFS.remove(POLAR_FILE_PATH);
        }
        uploadFile = LittleFS.open(POLAR_FILE_PATH, "w");
        if (!uploadFile) {
            serialPrintf("[Web] ✗ Failed to open polar file for writing\n");
            return;
        }
    }

    if (uploadFile && len > 0) {
        uploadFile.write(data, len);
    }

    if (final) {
        if (uploadFile) {
            uploadFile.close();
            serialPrintf("[Web] Polar upload complete: %u bytes\n", index + len);
        }
        // Reload polar into memory immediately
        bool ok = boatState->polar.loadFromFile(POLAR_FILE_PATH);
        if (ok) {
            // Recompute performance with the newly loaded polar
            boatState->updatePerformance();
            serialPrintf("[Web] ✓ Polar reloaded and performance updated\n");
        } else {
            serialPrintf("[Web] ✗ Polar parse failed after upload\n");
        }
    }
}

// ============================================================
// Boat data handlers
// ============================================================

void WebServer::handleGetNavigation(AsyncWebServerRequest* request) {
    if (!boatState) {
        request->send(500, "application/json", "{\"error\":\"BoatState not available\"}");
        return;
    }

    JsonDocument doc;
    GPSData     gps     = boatState->getGPS();
    SpeedData   speed   = boatState->getSpeed();
    HeadingData heading = boatState->getHeading();
    DepthData   depth   = boatState->getDepth();

    // Position
    JsonObject position = doc["position"].to<JsonObject>();
    if (gps.position.lat.valid && !gps.position.lat.isStale()) {
        position["latitude"]  = gps.position.lat.value;
        position["longitude"] = gps.position.lon.value;
        position["age"]       = (millis() - gps.position.lat.timestamp) / 1000.0;
    } else {
        position["latitude"]  = nullptr;
        position["longitude"] = nullptr;
        position["age"]       = nullptr;
    }

    // SOG
    if (gps.sog.valid && !gps.sog.isStale()) {
        doc["sog"]["value"] = gps.sog.value;
        doc["sog"]["unit"]  = gps.sog.unit;
        doc["sog"]["age"]   = (millis() - gps.sog.timestamp) / 1000.0;
    } else { doc["sog"]["value"] = nullptr; doc["sog"]["unit"] = "kn"; doc["sog"]["age"] = nullptr; }

    // COG
    if (gps.cog.valid && !gps.cog.isStale()) {
        doc["cog"]["value"] = gps.cog.value;
        doc["cog"]["unit"]  = gps.cog.unit;
        doc["cog"]["age"]   = (millis() - gps.cog.timestamp) / 1000.0;
    } else { doc["cog"]["value"] = nullptr; doc["cog"]["unit"] = "deg"; doc["cog"]["age"] = nullptr; }

    // STW
    if (speed.stw.valid && !speed.stw.isStale()) {
        doc["stw"]["value"] = speed.stw.value;
        doc["stw"]["unit"]  = speed.stw.unit;
        doc["stw"]["age"]   = (millis() - speed.stw.timestamp) / 1000.0;
    } else { doc["stw"]["value"] = nullptr; doc["stw"]["unit"] = "kn"; doc["stw"]["age"] = nullptr; }

    // Heading
    if (heading.true_heading.valid && !heading.true_heading.isStale()) {
        doc["heading"]["value"] = heading.true_heading.value;
        doc["heading"]["unit"]  = heading.true_heading.unit;
        doc["heading"]["age"]   = (millis() - heading.true_heading.timestamp) / 1000.0;
    } else { doc["heading"]["value"] = nullptr; doc["heading"]["unit"] = "deg"; doc["heading"]["age"] = nullptr; }

    // Depth
    if (depth.below_transducer.valid && !depth.below_transducer.isStale()) {
        doc["depth"]["value"] = depth.below_transducer.value;
        doc["depth"]["unit"]  = depth.below_transducer.unit;
        doc["depth"]["age"]   = (millis() - depth.below_transducer.timestamp) / 1000.0;
    } else { doc["depth"]["value"] = nullptr; doc["depth"]["unit"] = "m"; doc["depth"]["age"] = nullptr; }

    // GPS quality
    JsonObject quality = doc["gps_quality"].to<JsonObject>();
    if (gps.satellites.valid  && !gps.satellites.isStale())  quality["satellites"]  = (int)gps.satellites.value;  else quality["satellites"]  = nullptr;
    if (gps.fix_quality.valid && !gps.fix_quality.isStale()) quality["fix_quality"] = (int)gps.fix_quality.value; else quality["fix_quality"] = nullptr;
    if (gps.hdop.valid        && !gps.hdop.isStale())        quality["hdop"]        = gps.hdop.value;             else quality["hdop"]        = nullptr;

    // Trip / Total
    if (speed.trip.valid  && !speed.trip.isStale())  { doc["trip"]["value"]  = speed.trip.value;  doc["trip"]["unit"]  = speed.trip.unit; }
    else                                              { doc["trip"]["value"]  = nullptr;            doc["trip"]["unit"]  = "nm"; }
    if (speed.total.valid && !speed.total.isStale())  { doc["total"]["value"] = speed.total.value; doc["total"]["unit"] = speed.total.unit; }
    else                                              { doc["total"]["value"] = nullptr;            doc["total"]["unit"] = "nm"; }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleGetWind(AsyncWebServerRequest* request) {
    if (!boatState) {
        request->send(500, "application/json", "{\"error\":\"BoatState not available\"}");
        return;
    }

    JsonDocument doc;
    WindData wind = boatState->getWind();

    auto addWind = [&](const char* key, const DataPoint& dp, const char* defaultUnit) {
        if (dp.valid && !dp.isStale()) {
            doc[key]["value"] = dp.value;
            doc[key]["unit"]  = dp.unit;
            doc[key]["age"]   = (millis() - dp.timestamp) / 1000.0;
        } else {
            doc[key]["value"] = nullptr;
            doc[key]["unit"]  = defaultUnit;
            doc[key]["age"]   = nullptr;
        }
    };

    addWind("aws", wind.aws, "kn");
    addWind("awa", wind.awa, "deg");
    addWind("tws", wind.tws, "kn");
    addWind("twa", wind.twa, "deg");
    addWind("twd", wind.twd, "deg");

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleGetAIS(AsyncWebServerRequest* request) {
    if (!boatState) {
        request->send(500, "application/json", "{\"error\":\"BoatState not available\"}");
        return;
    }

    JsonDocument doc;
    AISData ais = boatState->getAIS();
    doc["target_count"] = ais.targetCount;
    JsonArray targets = doc["targets"].to<JsonArray>();

    for (int i = 0; i < ais.targetCount; i++) {
        AISTarget& t   = ais.targets[i];
        unsigned long age = (millis() - t.timestamp) / 1000;
        if (age > DATA_TIMEOUT_AIS / 1000) continue;

        JsonObject obj = targets.add<JsonObject>();
        obj["mmsi"] = t.mmsi;
        obj["name"] = t.name;
        JsonObject pos = obj["position"].to<JsonObject>();
        pos["latitude"]  = t.lat;
        pos["longitude"] = t.lon;
        obj["cog"]     = t.cog;
        obj["sog"]     = t.sog;
        obj["heading"] = t.heading;
        JsonObject prox = obj["proximity"].to<JsonObject>();
        prox["distance"]      = t.distance;
        prox["distance_unit"] = "nm";
        prox["bearing"]       = t.bearing;
        prox["bearing_unit"]  = "deg";
        prox["cpa"]           = t.cpa;
        prox["cpa_unit"]      = "nm";
        prox["tcpa"]          = t.tcpa;
        prox["tcpa_unit"]     = "min";
        obj["age"] = age;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleGetBoatState(AsyncWebServerRequest* request) {
    if (!boatState) {
        request->send(500, "application/json", "{\"error\":\"BoatState not available\"}");
        return;
    }
    request->send(200, "application/json", boatState->toJSON());
}

/**
 * GET /api/boat/performance
 * Returns VMG and polar efficiency percentage.
 */
void WebServer::handleGetPerformance(AsyncWebServerRequest* request) {
    if (!boatState) {
        request->send(500, "application/json", "{\"error\":\"BoatState not available\"}");
        return;
    }

    PerformanceData perf = boatState->getPerformance();
    JsonDocument doc;

    if (perf.vmg.valid && !perf.vmg.isStale()) {
        doc["vmg"]["value"] = perf.vmg.value;
        doc["vmg"]["unit"]  = perf.vmg.unit;
        doc["vmg"]["age"]   = (millis() - perf.vmg.timestamp) / 1000.0;
    } else {
        doc["vmg"]["value"] = nullptr;
        doc["vmg"]["unit"]  = "kn";
        doc["vmg"]["age"]   = nullptr;
    }

    if (perf.polarPct.valid && !perf.polarPct.isStale()) {
        doc["polar_pct"]["value"] = perf.polarPct.value;
        doc["polar_pct"]["unit"]  = perf.polarPct.unit;
        doc["polar_pct"]["age"]   = (millis() - perf.polarPct.timestamp) / 1000.0;
    } else {
        doc["polar_pct"]["value"] = nullptr;
        doc["polar_pct"]["unit"]  = "%";
        doc["polar_pct"]["age"]   = nullptr;
    }

    doc["polar_loaded"] = boatState->polar.isLoaded();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}
