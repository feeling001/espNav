#include "web_server.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "uart_handler.h"
#include "tcp_server.h"
#include "nmea_parser.h"
#include "polar.h"
#include "functions.h"
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// External variables from main.cpp for monitoring
extern volatile uint32_t g_nmeaQueueOverflows;
extern volatile uint32_t g_nmeaQueueFullEvents;
extern QueueHandle_t nmeaQueue;

// ── Constructor ───────────────────────────────────────────────────────────────

WebServer::WebServer(ConfigManager* cm, WiFiManager* wm, TCPServer* tcp, UARTHandler* uart,
                     NMEAParser* nmea, BoatState* bs, BLEManager* ble,
                     SeatalkManager* stMgr)
    : configManager(cm), wifiManager(wm), tcpServer(tcp), uartHandler(uart),
      nmeaParser(nmea), boatState(bs), bleManager(ble),
      seatalkManager(stMgr), running(false),
      otaInProgress(false), otaSuccess(false),
      otaExpectedSize(0), otaBytesWritten(0) {
    server = new AsyncWebServer(WEB_SERVER_PORT);
    wsNMEA = new AsyncWebSocket("/ws/nmea");
}

// ── init ──────────────────────────────────────────────────────────────────────

void WebServer::init() {
    serialPrintf("[Web] Initializing Web Server\n");

    wsNMEA->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                           AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->handleWebSocketEvent(server, client, type, arg, data, len);
    });

    server->addHandler(wsNMEA);
    registerRoutes();
}

// ── start / stop ──────────────────────────────────────────────────────────────

void WebServer::start() {
    if (running) return;
    server->begin();
    running = true;

    serialPrintf("[Web] ═══════════════════════════════════════\n");
    serialPrintf("[Web] Server started on port 80\n");
    serialPrintf("[Web] OTA + Storage endpoints registered\n");
    serialPrintf("[Web] ═══════════════════════════════════════\n");
}

void WebServer::stop() {
    if (!running) return;
    running = false;
    server->end();
    serialPrintf("[Web] Server stopped\n");
}

// ── registerRoutes ────────────────────────────────────────────────────────────

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
        [this](AsyncWebServerRequest* request) {
            if (boatState->polar.isLoaded()) {
                request->send(200, "application/json",
                              "{\"success\":true,\"message\":\"Polar loaded successfully\"}");
            } else {
                request->send(422, "application/json",
                              "{\"success\":false,\"error\":\"Failed to parse polar file\"}");
            }
        },
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

    // ── Autopilot ──────────────────────────────────────────────
    server->on("/api/autopilot/command", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            this->handlePostAutopilotCommand(request, data, len);
        }
    );

    // ── OTA Update ─────────────────────────────────────────────
    server->on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetOTAStatus(request);
    });

    server->on("/api/ota/upload", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            this->handleOTAUploadComplete(request);
        },
        [this](AsyncWebServerRequest* request, const String& filename,
               size_t index, uint8_t* data, size_t len, bool final) {
            this->handleOTAUpload(request, filename, index, data, len, final);
        }
    );

    // ── Storage (LittleFS) ─────────────────────────────────────
    server->on("/api/storage/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetStorageInfo(request);
    });

    server->on("/api/storage/files", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleListFiles(request);
    });

    server->on("/api/storage/delete", HTTP_DELETE,
        [this](AsyncWebServerRequest* request) {
            this->handleDeleteFile(request);
        }
    );

    server->on("/api/storage/format", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            this->handleFormatStorage(request);
        }
    );

    serialPrintf("[Web]   ✓ All API routes registered\n");

    // ── SPA fallback routes ────────────────────────────────────
    const char* spaRoutes[] = {
        "/instruments", "/autopilot", "/performance",
        "/config", "/nmea", nullptr
    };
    for (int i = 0; spaRoutes[i] != nullptr; i++) {
        server->on(spaRoutes[i], HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(LittleFS, "/www/index.html", "text/html");
        });
    }

    // ── Static files ───────────────────────────────────────────
#ifdef WEB_UI_PROGMEM
    registerProgmemRoutes(server);
#else
    server->serveStatic("/", LittleFS, "/www/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=600");
    server->onNotFound([](AsyncWebServerRequest* request) {
        if (request->url().startsWith("/api/") || request->url().startsWith("/ws/")) {
            request->send(404, "text/plain", "Not Found");
            return;
        }
        request->send(LittleFS, "/www/index.html", "text/html");
    });
#endif

    serialPrintf("[Web]   ✓ All routes registered\n");
}

// ── WebSocket ─────────────────────────────────────────────────────────────────

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

// ── OTA Handlers ─────────────────────────────────────────────────────────────

void WebServer::handleGetOTAStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;

    // Running partition info
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* next    = esp_ota_get_next_update_partition(NULL);

    doc["running_partition"] = running ? running->label : "unknown";
    doc["next_partition"]    = next    ? next->label    : "unknown";

    // App description of the running firmware
    esp_app_desc_t appDesc;
    if (esp_ota_get_partition_description(running, &appDesc) == ESP_OK) {
        doc["version"]       = appDesc.version;
        doc["compile_date"]  = appDesc.date;
        doc["compile_time"]  = appDesc.time;
        doc["idf_version"]   = appDesc.idf_ver;
    }

    // OTA partition sizes
    if (running) {
        doc["partition_size"] = (uint32_t)running->size;
    }

    doc["ota_in_progress"]  = otaInProgress;
    doc["last_ota_success"] = otaSuccess;
    if (!otaErrorMessage.isEmpty()) {
        doc["last_ota_error"] = otaErrorMessage;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleOTAUpload(AsyncWebServerRequest* request, const String& filename,
                                 size_t index, uint8_t* data, size_t len, bool final) {
    if (index == 0) {
        // ── Begin OTA ──────────────────────────────────────────
        serialPrintf("[OTA] Upload started: %s  total=%u B\n",
                      filename.c_str(), request->contentLength());

        otaInProgress  = true;
        otaSuccess     = false;
        otaErrorMessage = "";
        otaExpectedSize = request->contentLength();
        otaBytesWritten = 0;

        // Determine update partition
        const esp_partition_t* updatePart = esp_ota_get_next_update_partition(NULL);
        if (!updatePart) {
            otaErrorMessage = "No OTA partition available";
            serialPrintf("[OTA] ✗ %s\n", otaErrorMessage.c_str());
            return;
        }

        serialPrintf("[OTA] Writing to partition: %s (offset=0x%x, size=%u B)\n",
                      updatePart->label,
                      updatePart->address,
                      (uint32_t)updatePart->size);

        // Check firmware fits
        if (otaExpectedSize > 0 && otaExpectedSize > updatePart->size) {
            otaErrorMessage = "Firmware too large for OTA partition";
            serialPrintf("[OTA] ✗ %s\n", otaErrorMessage.c_str());
            Update.abort();
            return;
        }

        if (!Update.begin(otaExpectedSize > 0 ? otaExpectedSize : UPDATE_SIZE_UNKNOWN,
                          U_FLASH)) {
            otaErrorMessage = Update.errorString();
            serialPrintf("[OTA] ✗ Update.begin failed: %s\n", otaErrorMessage.c_str());
            return;
        }

        serialPrintf("[OTA] Update.begin() OK\n");
    }

    // ── Write chunk ────────────────────────────────────────────
    if (otaInProgress && !otaErrorMessage.isEmpty()) {
        // A previous chunk already failed — skip the rest
        return;
    }

    if (Update.isRunning() && len > 0) {
        size_t written = Update.write(data, len);
        otaBytesWritten += written;

        if (written != len) {
            otaErrorMessage = Update.errorString();
            serialPrintf("[OTA] ✗ Write error at byte %u: %s\n",
                          (uint32_t)index, otaErrorMessage.c_str());
            Update.abort();
            otaInProgress = false;
            return;
        }
    }

    // ── Finalise ───────────────────────────────────────────────
    if (final) {
        if (!Update.isRunning()) {
            // Was never started (e.g. partition error above)
            if (otaErrorMessage.isEmpty()) {
                otaErrorMessage = "Update stream was not started";
            }
            otaInProgress = false;
            return;
        }

        if (Update.end(true)) {
            otaSuccess    = true;
            otaInProgress = false;
            serialPrintf("[OTA] ✓ Update complete — %u bytes written\n",
                          (uint32_t)otaBytesWritten);
            serialPrintf("[OTA] Reboot required to activate new firmware\n");
        } else {
            otaErrorMessage = Update.errorString();
            otaInProgress   = false;
            serialPrintf("[OTA] ✗ Update.end() failed: %s\n", otaErrorMessage.c_str());
        }
    }
}

void WebServer::handleOTAUploadComplete(AsyncWebServerRequest* request) {
    // This handler fires after the multipart upload is fully received.
    if (otaSuccess) {
        request->send(200, "application/json",
                      "{\"success\":true,"
                      "\"message\":\"Firmware flashed successfully. Rebooting in 3 seconds...\"}");
        serialPrintf("[OTA] Scheduling reboot in 3 s\n");

        // Reboot after a short delay so the HTTP response has time to be sent
        xTaskCreate([](void*) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_restart();
        }, "OTA_reboot", 2048, NULL, 1, NULL);

    } else {
        JsonDocument doc;
        doc["success"] = false;
        doc["error"]   = otaErrorMessage.isEmpty()
                         ? "Unknown OTA error" : otaErrorMessage;
        String body;
        serializeJson(doc, body);
        request->send(500, "application/json", body);
    }
}

// ── Storage Handlers ──────────────────────────────────────────────────────────

void WebServer::handleGetStorageInfo(AsyncWebServerRequest* request) {
    JsonDocument doc;

    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();

    doc["total_bytes"] = (uint32_t)total;
    doc["used_bytes"]  = (uint32_t)used;
    doc["free_bytes"]  = (uint32_t)(total - used);
    doc["used_pct"]    = total > 0 ? (uint8_t)((used * 100) / total) : 0;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleListFiles(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonArray files = doc["files"].to<JsonArray>();

    // Recursive helper — list all files under a directory
    std::function<void(const char*)> listDir = [&](const char* path) {
        File dir = LittleFS.open(path);
        if (!dir || !dir.isDirectory()) return;

        File entry = dir.openNextFile();
        while (entry) {
            if (entry.isDirectory()) {
                listDir(entry.path());
            } else {
                JsonObject f = files.add<JsonObject>();
                f["path"] = String(entry.path());
                f["size"] = (uint32_t)entry.size();
            }
            entry = dir.openNextFile();
        }
    };

    listDir("/");

    doc["count"] = files.size();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleDeleteFile(AsyncWebServerRequest* request) {
    if (!request->hasParam("path")) {
        request->send(400, "application/json",
                      "{\"error\":\"Missing 'path' parameter\"}");
        return;
    }

    String path = request->getParam("path")->value();

    // Basic safety: do not allow deleting directories or escaping root
    if (path.isEmpty() || path == "/" || path.indexOf("..") >= 0) {
        request->send(400, "application/json",
                      "{\"error\":\"Invalid path\"}");
        return;
    }

    if (!LittleFS.exists(path)) {
        request->send(404, "application/json",
                      "{\"error\":\"File not found\"}");
        return;
    }

    if (LittleFS.remove(path)) {
        serialPrintf("[Storage] Deleted: %s\n", path.c_str());
        request->send(200, "application/json",
                      "{\"success\":true,\"message\":\"File deleted\"}");
    } else {
        request->send(500, "application/json",
                      "{\"error\":\"Failed to delete file\"}");
    }
}

void WebServer::handleFormatStorage(AsyncWebServerRequest* request) {
    serialPrintf("[Storage] Formatting LittleFS...\n");

    // Format runs synchronously and takes a few hundred ms
    bool ok = LittleFS.format();

    if (ok) {
        // Re-mount after format
        LittleFS.begin(false, "/littlefs", 10, "littlefs");
        serialPrintf("[Storage] ✓ LittleFS formatted and remounted\n");
        request->send(200, "application/json",
                      "{\"success\":true,\"message\":\"Storage formatted successfully\"}");
    } else {
        serialPrintf("[Storage] ✗ Format failed\n");
        request->send(500, "application/json",
                      "{\"error\":\"Format failed\"}");
    }
}

// ── Configuration handlers ────────────────────────────────────────────────────

void WebServer::handleGetWiFiConfig(AsyncWebServerRequest* request) {
#ifdef DEBUG_WEB
    serialPrintf("[Web] → GET /api/config/wifi\n");
#endif

    WiFiConfig config;
    configManager->getWiFiConfig(config);

    JsonDocument doc;
    doc["ssid"]            = config.ssid;
    doc["mode"]            = config.mode;
    doc["has_password"]    = (strlen(config.password) > 0);
    doc["ap_ssid"]         = config.ap_ssid;
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

    // ── Firmware version ──────────────────────────────────────
    /*
    const esp_partition_t* runningPart = esp_ota_get_running_partition();
    esp_app_desc_t appDesc;
    if (runningPart && esp_ota_get_partition_description(runningPart, &appDesc) == ESP_OK) {
        doc["version"] = appDesc.version;
    } else {
        doc["version"] = VERSION;
    }
        */
    doc["version"] = VERSION;

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

    // ── NMEA buffer / queue status ────────────────────────────
    JsonObject nmeaBuffer = doc["nmea_buffer"].to<JsonObject>();
    nmeaBuffer["queue_size"]         = NMEA_QUEUE_SIZE;
    nmeaBuffer["overflow_total"]     = g_nmeaQueueOverflows;
    nmeaBuffer["full_events_recent"] = g_nmeaQueueFullEvents;
    nmeaBuffer["has_overflow"]       = (g_nmeaQueueFullEvents > 0);

    // Current number of messages waiting in the queue
    UBaseType_t queueWaiting = 0;
    if (nmeaQueue != NULL) {
        queueWaiting = uxQueueMessagesWaiting(nmeaQueue);
    }
    nmeaBuffer["queue_waiting"]  = (uint32_t)queueWaiting;
    // Load as a percentage of queue capacity (0–100)
    nmeaBuffer["queue_load_pct"] = (NMEA_QUEUE_SIZE > 0)
        ? (uint8_t)((queueWaiting * 100) / NMEA_QUEUE_SIZE)
        : 0;

    JsonObject ble = doc["ble"].to<JsonObject>();
    ble["enabled"]           = bleManager->isEnabled();
    ble["advertising"]       = bleManager->isAdvertising();
    ble["connected_devices"] = bleManager->getConnectedDevices();

    String deviceName = bleManager->getConfig().device_name;
    ble["device_name"] = deviceName;

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

// ── WiFi Scan ─────────────────────────────────────────────────────────────────

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

// ── BLE Configuration ─────────────────────────────────────────────────────────

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

// ── Polar handlers ────────────────────────────────────────────────────────────

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
        bool ok = boatState->polar.loadFromFile(POLAR_FILE_PATH);
        if (ok) {
            boatState->updatePerformance();
            serialPrintf("[Web] ✓ Polar reloaded and performance updated\n");
        } else {
            serialPrintf("[Web] ✗ Polar parse failed after upload\n");
        }
    }
}

// ── Autopilot handler ─────────────────────────────────────────────────────────

void WebServer::handlePostAutopilotCommand(AsyncWebServerRequest* request,
                                            uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (char*)data, len);
    if (err) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const char* command = doc["command"] | "";
    if (command[0] == '\0') {
        request->send(400, "application/json", "{\"error\":\"Missing command field\"}");
        return;
    }

    serialPrintf("[Web] Autopilot command: %s\n", command);

    if (!seatalkManager) {
        request->send(503, "application/json",
                      "{\"success\":false,\"error\":\"SeaTalk manager not initialised\"}");
        return;
    }

    bool ok = seatalkManager->sendAutopilotCommand(command);

    if (ok) {
        JsonDocument resp;
        resp["success"] = true;
        resp["message"] = String("Sent: ") + command;
        String body;
        serializeJson(resp, body);
        request->send(200, "application/json", body);
    } else {
        request->send(500, "application/json",
                      "{\"success\":false,\"error\":\"SeaTalk transmission failed or unknown command\"}");
    }
}

// ── Boat data handlers ────────────────────────────────────────────────────────

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

    auto addDP = [&](const char* key, const DataPoint& dp, const char* unit) {
        if (dp.valid && !dp.isStale()) {
            doc[key]["value"] = dp.value;
            doc[key]["unit"]  = dp.unit;
            doc[key]["age"]   = (millis() - dp.timestamp) / 1000.0;
        } else {
            doc[key]["value"] = nullptr;
            doc[key]["unit"]  = unit;
            doc[key]["age"]   = nullptr;
        }
    };

    addDP("sog",     gps.sog,              "kn");
    addDP("cog",     gps.cog,              "deg");
    addDP("stw",     speed.stw,            "kn");
    addDP("heading", heading.true_heading, "deg");
    addDP("depth",   depth.below_transducer, "m");

    JsonObject quality = doc["gps_quality"].to<JsonObject>();
    if (gps.satellites.valid  && !gps.satellites.isStale())  quality["satellites"]  = (int)gps.satellites.value;  else quality["satellites"]  = nullptr;
    if (gps.fix_quality.valid && !gps.fix_quality.isStale()) quality["fix_quality"] = (int)gps.fix_quality.value; else quality["fix_quality"] = nullptr;
    if (gps.hdop.valid        && !gps.hdop.isStale())        quality["hdop"]        = gps.hdop.value;             else quality["hdop"]        = nullptr;

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
        AISTarget& t      = ais.targets[i];
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
