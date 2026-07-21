/**
 * @file web_server.cpp
 * @brief Async HTTP + WebSocket server implementation for the Marine Gateway.
 *
 * All API endpoints are registered in registerRoutes().
 * SD card endpoints (/api/sd/*) delegate to SDManager and gracefully
 * return HTTP 503 when no card is present or SDManager is nullptr.
 */

#include "web_server.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "uart_handler.h"
#include "tcp_server.h"
#include "nmea_parser.h"
#include "polar.h"
#include "functions.h"
#include "log_manager.h"
#include "debug_log.h"

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
                     SeatalkManager* stMgr, LogManager* logManager, SDManager* sdMgr,
                     AlarmManager* alarmMgr)
    : configManager(cm), wifiManager(wm), tcpServer(tcp), uartHandler(uart),
      nmeaParser(nmea), boatState(bs), bleManager(ble),
      seatalkManager(stMgr), logManager(logManager), sdManager(sdMgr),
      alarmManager(alarmMgr), running(false),
      otaInProgress(false), otaSuccess(false),
      otaExpectedSize(0), otaBytesWritten(0) {
    server = new AsyncWebServer(WEB_SERVER_PORT);
    wsNMEA = new AsyncWebSocket("/ws/nmea");
    wsBoatState = new AsyncWebSocket("/ws/boatstate");
    wsDebug = new AsyncWebSocket("/ws/debug");
}

// ── init ──────────────────────────────────────────────────────────────────────

void WebServer::init() {
    serialPrintf("[Web] Initializing Web Server\n");

    // Load performance config from NVS and apply to BoatState
    perfNvs.begin("perf_cfg", false);
    float tau = perfNvs.getFloat("damping_tau", 0.0f);
    if (boatState) boatState->setDampingTau(tau);
    serialPrintf("[Web] Performance damping tau: %.1f s\n", tau);

    wsNMEA->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                           AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->handleWebSocketEvent(server, client, type, arg, data, len);
    });

    wsBoatState->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                                AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->handleWebSocketEvent(server, client, type, arg, data, len);
    });

    wsDebug->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                            AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->handleWebSocketEvent(server, client, type, arg, data, len);
    });

    server->addHandler(wsNMEA);
    server->addHandler(wsBoatState);
    server->addHandler(wsDebug);
    registerRoutes();
}

// ── start / stop ──────────────────────────────────────────────────────────────

void WebServer::start() {
    if (running) return;
    server->begin();
    running = true;

    serialPrintf("[Web] ═══════════════════════════════════════\n");
    serialPrintf("[Web] Server started on port 80\n");
    serialPrintf("[Web] OTA + LittleFS + SD card endpoints registered\n");
    serialPrintf("[Web] SD manager: %s\n", sdManager ? "present" : "not configured");
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

    server->on("/api/config/conversions", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetConversionConfig(request);
    });
    server->on("/api/config/conversions", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            this->handlePostConversionConfig(request, data, len);
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

    // ── LogBook ────────────────────────────────────────────────
    server->on("/api/log/config", HTTP_GET, [this](AsyncWebServerRequest* r) {
        this->handleGetLogConfig(r);
    });

    server->on("/api/log/config", HTTP_POST, [](AsyncWebServerRequest*) {}, NULL, [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t, size_t) { 
        this->handlePostLogConfig(r, d, l); }
    );

    server->on("/api/log/status", HTTP_GET, [this](AsyncWebServerRequest* r) {
        this->handleGetLogStatus(r);
    });
 
    server->on("/api/log/new", HTTP_POST, [this](AsyncWebServerRequest* r) {
        this->handlePostLogNewSession(r);
    });
        
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
    server->on("/api/performance/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetPerformanceConfig(request);
    });
    server->on("/api/performance/config", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            this->handlePostPerformanceConfig(request, data, len);
        }
    );

    // ── Autopilot ──────────────────────────────────────────────
    server->on("/api/autopilot/command", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            this->handlePostAutopilotCommand(request, data, len);
        }
    );
    server->on("/api/seatalk/extra", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
                size_t index, size_t total) {
            this->handlePostSeatalkExtra(request, data, len);
        }
    );

    // ── Alarms ─────────────────────────────────────────────────
    server->on("/api/alarms/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetAlarmsConfig(request);
    });
    server->on("/api/alarms/config", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            this->handlePostAlarmsConfig(request, data, len);
        }
    );
    server->on("/api/alarms/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetAlarmsStatus(request);
    });
    server->on("/api/alarms/ack", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handlePostAlarmsAck(request);
    });
    server->on("/api/alarms/beep_on", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handlePostAlarmsBeepOn(request);
    });
    server->on("/api/alarms/beep_off", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handlePostAlarmsBeepOff(request);
    });

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

    // ── LittleFS Storage ───────────────────────────────────────
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

    // ── SD Card ────────────────────────────────────────────────
    server->on("/api/sd/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetSDStatus(request);
    });

    server->on("/api/sd/files", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleListSDFiles(request);
    });

    server->on("/api/sd/download", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleDownloadSDFile(request);
    });

    server->on("/api/sd/delete", HTTP_DELETE,
        [this](AsyncWebServerRequest* request) {
            this->handleDeleteSDFile(request);
        }
    );


    server->on("/api/sd/mkdir", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            this->handleMkdirSD(request, data, len);
        }
    );

    server->on("/api/sd/format", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleFormatSD(request);
    });

    server->on("/api/sd/mount", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleMountSD(request);
    });

    server->on("/api/sd/unmount", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleUnmountSD(request);
    });

    serialPrintf("[Web]   ✓ All API routes registered\n");

    // ── SPA fallback routes (LittleFS only) ────────────────────
#ifndef WEB_UI_PROGMEM
    const char* spaRoutes[] = {
        "/instruments", "/autopilot", "/performance",
        "/config", "/nmea", nullptr
    };
    for (int i = 0; spaRoutes[i] != nullptr; i++) {
        server->on(spaRoutes[i], HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(LittleFS, "/www/index.html", "text/html");
        });
    }
#endif

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
            // Send the existing debug-log backlog to newly connected debug
            // console clients so they immediately see recent history rather
            // than waiting for the next line to be printed.
            if (server == wsDebug) {
                String backlog = DebugLog::instance().collectAll();
                if (!backlog.isEmpty()) client->text(backlog);
            }
            break;
        case WS_EVT_DISCONNECT:
            serialPrintf("[WebSocket] Client #%u disconnected\n", client->id());
            break;
        default:
            break;
    }
}

// ── /ws/debug: streams serialPrintf() output captured in DebugLog ───────────

void WebServer::broadcastDebugLog() {
    if (!wsDebug || !running) return;

    static uint32_t lastCleanup = 0;
    uint32_t now = millis();
    if (now - lastCleanup >= 5000) {
        wsDebug->cleanupClients();
        lastCleanup = now;
    }

    if (wsDebug->count() == 0) {
        // No viewers: still advance the cursor so we don't build up and dump
        // a huge backlog the moment someone connects mid-session (they get
        // the ring-buffer backlog via collectAll() on connect instead).
        debugLogLastSeq = DebugLog::instance().currentSeq();
        return;
    }

    String newLines = DebugLog::instance().collectSince(debugLogLastSeq);
    if (newLines.isEmpty()) return;

    wsDebug->textAll(newLines);
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

// ── /ws/boatstate: periodic push of navigation + wind + ais + alarms ─────────

void WebServer::broadcastBoatState() {
    if (!wsBoatState || !running || !boatState) return;

    static uint32_t lastCleanup = 0;
    uint32_t now = millis();
    if (now - lastCleanup >= 5000) {
        wsBoatState->cleanupClients();
        lastCleanup = now;
    }

    if (wsBoatState->count() == 0) return;

    static uint32_t lastSend = 0;
    static const uint32_t minInterval = 1000 / WS_BOATSTATE_RATE_HZ;
    if (now - lastSend < minInterval) return;
    lastSend = now;

    wsBoatState->textAll(buildBoatStateJSON());
}

String WebServer::buildBoatStateJSON() {
    JsonDocument doc;
    doc["timestamp"] = millis();

    // ── Navigation ────────────────────────────────────────────────
    {
        GPSData     gps     = boatState->getGPS();
        SpeedData   speed   = boatState->getSpeed();
        HeadingData heading = boatState->getHeading();
        DepthData   depth   = boatState->getDepth();

        // GPS-derived UTC timestamp (Unix epoch, from RMC/ZDA date+time).
        // 0 when no GPS fix has provided date/time yet — same semantics as
        // the "datetime" field of GET /api/status.
        doc["gpstimestamp"] = gps.datetime.getTimestamp();

        JsonObject nav = doc["navigation"].to<JsonObject>();

        JsonObject position = nav["position"].to<JsonObject>();
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
                nav[key]["value"] = dp.value;
                nav[key]["unit"]  = dp.unit;
                nav[key]["age"]   = (millis() - dp.timestamp) / 1000.0;
            } else {
                nav[key]["value"] = nullptr;
                nav[key]["unit"]  = unit;
                nav[key]["age"]   = nullptr;
            }
        };

        addDP("sog",     gps.sog,                "kn");
        addDP("cog",     gps.cog,                "deg");
        addDP("stw",     speed.stw,              "kn");
        addDP("heading", heading.true_heading,   "deg");
        addDP("depth",   depth.below_transducer, "m");

        JsonObject quality = nav["gps_quality"].to<JsonObject>();
        if (gps.satellites.valid  && !gps.satellites.isStale())  quality["satellites"]  = (int)gps.satellites.value;  else quality["satellites"]  = nullptr;
        if (gps.fix_quality.valid && !gps.fix_quality.isStale()) quality["fix_quality"] = (int)gps.fix_quality.value; else quality["fix_quality"] = nullptr;
        if (gps.hdop.valid        && !gps.hdop.isStale())        quality["hdop"]        = gps.hdop.value;             else quality["hdop"]        = nullptr;

        if (speed.trip.valid  && !speed.trip.isStale())  { nav["trip"]["value"]  = speed.trip.value;  nav["trip"]["unit"]  = speed.trip.unit; }
        else                                              { nav["trip"]["value"]  = nullptr;            nav["trip"]["unit"]  = "nm"; }
        if (speed.total.valid && !speed.total.isStale())  { nav["total"]["value"] = speed.total.value; nav["total"]["unit"] = speed.total.unit; }
        else                                              { nav["total"]["value"] = nullptr;            nav["total"]["unit"] = "nm"; }
    }

    // ── Wind ──────────────────────────────────────────────────────
    {
        WindData wind = boatState->getWind();
        JsonObject windObj = doc["wind"].to<JsonObject>();

        auto addWind = [&](const char* key, const DataPoint& dp, const char* defaultUnit) {
            if (dp.valid && !dp.isStale()) {
                windObj[key]["value"] = dp.value;
                windObj[key]["unit"]  = dp.unit;
                windObj[key]["age"]   = (millis() - dp.timestamp) / 1000.0;
            } else {
                windObj[key]["value"] = nullptr;
                windObj[key]["unit"]  = defaultUnit;
                windObj[key]["age"]   = nullptr;
            }
        };

        addWind("aws", wind.aws, "kn");
        addWind("awa", wind.awa, "deg");
        addWind("tws", wind.tws, "kn");
        addWind("twa", wind.twa, "deg");
        addWind("twd", wind.twd, "deg");
    }

    // ── AIS ───────────────────────────────────────────────────────
    {
        AISData ais = boatState->getAIS();
        JsonObject aisObj = doc["ais"].to<JsonObject>();
        aisObj["target_count"] = ais.targetCount;
        JsonArray targets = aisObj["targets"].to<JsonArray>();

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
    }

    // ── Alarms ────────────────────────────────────────────────────
    if (alarmManager) {
        AlarmState state = boatState->getAlarmState();
        JsonObject alarmsObj = doc["alarms"].to<JsonObject>();

        auto addAlarm = [&](const char* key, const AlarmItemState& item) {
            JsonObject obj = alarmsObj[key].to<JsonObject>();
            obj["active"]       = item.active;
            obj["acknowledged"] = item.acknowledged;
            if (item.active) {
                obj["age"] = (millis() - item.since) / 1000.0;
            } else {
                obj["age"] = nullptr;
            }
        };

        addAlarm("depth",    state.depth);
        addAlarm("ais",      state.ais);
        addAlarm("gps_lost", state.gps_lost);

        alarmsObj["ais_trigger_mmsi"] = state.ais_trigger_mmsi;
        alarmsObj["any_active"]       = state.anyActive();
        alarmsObj["any_unacked"]      = state.anyUnacked();
    }

    // ── Autopilot ─────────────────────────────────────────────────
    {
        AutopilotData ap = boatState->getAutopilot();
        JsonObject apObj = doc["autopilot"].to<JsonObject>();

        if (ap.valid && !ap.isStale()) {
            apObj["mode"]   = ap.mode;
            apObj["status"] = ap.status;
            apObj["alarm"]  = ap.alarm;
            apObj["age"]    = (millis() - ap.timestamp) / 1000.0;

            auto addAP = [&](const char* key, const DataPoint& dp, const char* unit) {
                if (dp.valid && !dp.isStale()) {
                    apObj[key]["value"] = dp.value;
                    apObj[key]["unit"]  = dp.unit;
                    apObj[key]["age"]   = (millis() - dp.timestamp) / 1000.0;
                } else {
                    apObj[key]["value"] = nullptr;
                    apObj[key]["unit"]  = unit;
                    apObj[key]["age"]   = nullptr;
                }
            };

            addAP("heading_target",    ap.heading_target,    "deg");
            addAP("wind_angle_target", ap.wind_angle_target, "deg");
            addAP("rudder_angle",      ap.rudder_angle,      "deg");
            addAP("xte",               ap.xte,               "nm");
        } else {
            apObj["mode"]   = nullptr;
            apObj["status"] = nullptr;
            apObj["alarm"]  = nullptr;
            apObj["age"]    = nullptr;
        }
    }

    // ── Performance (polar-based) ────────────────────────────────
    {
        PerformanceData perf = boatState->getPerformance();
        JsonObject perfObj = doc["performance"].to<JsonObject>();

        auto addPerf = [&](const char* key, const DataPoint& dp, const char* unit) {
            if (dp.valid && !dp.isStale()) {
                perfObj[key]["value"] = dp.value;
                perfObj[key]["unit"]  = dp.unit;
                perfObj[key]["age"]   = (millis() - dp.timestamp) / 1000.0;
            } else {
                perfObj[key]["value"] = nullptr;
                perfObj[key]["unit"]  = unit;
                perfObj[key]["age"]   = nullptr;
            }
        };

        addPerf("vmg",       perf.vmg,      "kn");
        addPerf("polar_pct", perf.polarPct, "%");
        perfObj["polar_loaded"] = boatState->polar.isLoaded();
    }

    // ── System status (lightweight) ──────────────────────────────
    {
        JsonObject statusObj = doc["status"].to<JsonObject>();
        statusObj["uptime"] = millis() / 1000;

        JsonObject heap = statusObj["heap"].to<JsonObject>();
        heap["free"]  = ESP.getFreeHeap();
        heap["total"] = ESP.getHeapSize();

        JsonObject wifi = statusObj["wifi"].to<JsonObject>();
        wl_status_t wlStat = WiFi.status();
        const char* modeStr = "Unknown";
        if (wlStat == WL_CONNECTED) {
            modeStr = "STA";
            wifi["rssi"] = WiFi.RSSI();
        } else if (WiFi.getMode() == WIFI_AP) {
            modeStr = "AP";
            wifi["clients"] = WiFi.softAPgetStationNum();
        }
        wifi["mode"] = modeStr;

        statusObj["tcp_clients"]  = tcpServer ? tcpServer->getClientCount() : 0;
        statusObj["ble_connected"] = bleManager ? bleManager->getConnectedDevices() : 0;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

// ── OTA Handlers ─────────────────────────────────────────────────────────────

void WebServer::handleGetOTAStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;

    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* next    = esp_ota_get_next_update_partition(NULL);

    doc["running_partition"] = running ? running->label : "unknown";
    doc["next_partition"]    = next    ? next->label    : "unknown";

    esp_app_desc_t appDesc;
    if (esp_ota_get_partition_description(running, &appDesc) == ESP_OK) {
        doc["version"]       = appDesc.version;
        doc["compile_date"]  = appDesc.date;
        doc["compile_time"]  = appDesc.time;
        doc["idf_version"]   = appDesc.idf_ver;
    }

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
        serialPrintf("[OTA] Upload started: %s  total=%u B\n",
                      filename.c_str(), request->contentLength());

        otaInProgress  = true;
        otaSuccess     = false;
        otaErrorMessage = "";
        otaExpectedSize = request->contentLength();
        otaBytesWritten = 0;

        const esp_partition_t* updatePart = esp_ota_get_next_update_partition(NULL);
        if (!updatePart) {
            otaErrorMessage = "No OTA partition available";
            serialPrintf("[OTA] ✗ %s\n", otaErrorMessage.c_str());
            return;
        }

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
    }

    if (otaInProgress && !otaErrorMessage.isEmpty()) return;

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

    if (final) {
        if (!Update.isRunning()) {
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
        } else {
            otaErrorMessage = Update.errorString();
            otaInProgress   = false;
            serialPrintf("[OTA] ✗ Update.end() failed: %s\n", otaErrorMessage.c_str());
        }
    }
}

void WebServer::handleOTAUploadComplete(AsyncWebServerRequest* request) {
    if (otaSuccess) {
        request->send(200, "application/json",
                      "{\"success\":true,"
                      "\"message\":\"Firmware flashed successfully. Rebooting in 3 seconds...\"}");
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

// ── LittleFS Storage Handlers ─────────────────────────────────────────────────

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

    if (path.isEmpty() || path == "/" || path.indexOf("..") >= 0) {
        request->send(400, "application/json", "{\"error\":\"Invalid path\"}");
        return;
    }

    if (!LittleFS.exists(path)) {
        request->send(404, "application/json", "{\"error\":\"File not found\"}");
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

    bool ok = LittleFS.format();

    if (ok) {
        LittleFS.begin(false, "/littlefs", 10, "littlefs");
        serialPrintf("[Storage] ✓ LittleFS formatted and remounted\n");
        request->send(200, "application/json",
                      "{\"success\":true,\"message\":\"Storage formatted successfully\"}");
    } else {
        serialPrintf("[Storage] ✗ Format failed\n");
        request->send(500, "application/json", "{\"error\":\"Format failed\"}");
    }
}

// ── SD Card Handlers ──────────────────────────────────────────────────────────

/**
 * Helper: return a 503 response when the SD subsystem is unavailable.
 */
static inline void sdNotAvailable(AsyncWebServerRequest* request,
                                   const char* reason = "SD card not mounted") {
    JsonDocument doc;
    doc["error"]   = reason;
    doc["mounted"] = false;
    String body;
    serializeJson(doc, body);
    request->send(503, "application/json", body);
}

void WebServer::handleGetSDStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;

    if (!sdManager) {
        doc["enabled"] = false;
        doc["mounted"] = false;
        doc["error"]   = "SD manager not configured";
        String body;
        serializeJson(doc, body);
        request->send(200, "application/json", body);
        return;
    }

    SDStorageInfo info = sdManager->getStorageInfo();

    doc["enabled"]      = true;
    doc["mounted"]      = info.mounted;
    doc["card_type"]    = info.cardType;
    doc["total_bytes"]  = (uint32_t)(info.totalBytes & 0xFFFFFFFF);
    doc["used_bytes"]   = (uint32_t)(info.usedBytes  & 0xFFFFFFFF);
    doc["free_bytes"]   = (uint32_t)(info.freeBytes   & 0xFFFFFFFF);
    doc["used_pct"]     = info.usedPct;
    // Also expose high 32 bits for cards > 4 GB
    doc["total_mb"]     = (uint32_t)(info.totalBytes / (1024ULL * 1024ULL));
    doc["free_mb"]      = (uint32_t)(info.freeBytes  / (1024ULL * 1024ULL));

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleListSDFiles(AsyncWebServerRequest* request) {
    if (!sdManager || !sdManager->isMounted()) {
        sdNotAvailable(request);
        return;
    }

    const char* dir = "/";
    if (request->hasParam("dir")) {
        dir = request->getParam("dir")->value().c_str();
    }

    std::vector<SDFileInfo> files = sdManager->listFiles(dir, 4);

    JsonDocument doc;
    doc["dir"]   = dir;
    doc["count"] = files.size();
    JsonArray arr = doc["files"].to<JsonArray>();

    for (const auto& f : files) {
        JsonObject obj = arr.add<JsonObject>();
        obj["path"]  = f.path;
        obj["size"]  = f.size;
        obj["isDir"] = f.isDir;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleDownloadSDFile(AsyncWebServerRequest* request) {
    if (!sdManager || !sdManager->isMounted()) {
        sdNotAvailable(request);
        return;
    }

    if (!request->hasParam("path")) {
        request->send(400, "application/json",
                      "{\"error\":\"Missing 'path' parameter\"}");
        return;
    }

    String path = request->getParam("path")->value();

    // Basic path sanitation
    if (path.isEmpty() || path.indexOf("..") >= 0) {
        request->send(400, "application/json", "{\"error\":\"Invalid path\"}");
        return;
    }

    if (!sdManager->exists(path.c_str())) {
        request->send(404, "application/json", "{\"error\":\"File not found\"}");
        return;
    }

    File f = sdManager->openForRead(path.c_str());
    if (!f || f.isDirectory()) {
        request->send(400, "application/json", "{\"error\":\"Not a file\"}");
        return;
    }

    // Determine MIME type from extension
    String ext = path.substring(path.lastIndexOf('.') + 1);
    ext.toLowerCase();
    const char* mime = "application/octet-stream";
    if      (ext == "csv")  mime = "text/csv";
    else if (ext == "txt")  mime = "text/plain";
    else if (ext == "nmea") mime = "text/plain";
    else if (ext == "json") mime = "application/json";
    else if (ext == "log")  mime = "text/plain";

    // Extract filename for Content-Disposition
    String filename = path.substring(path.lastIndexOf('/') + 1);

    AsyncWebServerResponse* response =
        request->beginResponse(SD, path, mime);
    response->addHeader("Content-Disposition",
                        String("attachment; filename=\"") + filename + "\"");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);

    serialPrintf("[SD] Download: %s (%u B)\n", path.c_str(), (uint32_t)f.size());
    f.close();
}

void WebServer::handleDeleteSDFile(AsyncWebServerRequest* request) {
    if (!sdManager || !sdManager->isMounted()) {
        sdNotAvailable(request);
        return;
    }

    if (!request->hasParam("path")) {
        request->send(400, "application/json",
                      "{\"error\":\"Missing 'path' parameter\"}");
        return;
    }

    String path = request->getParam("path")->value();

    if (path.isEmpty() || path == "/" || path.indexOf("..") >= 0) {
        request->send(400, "application/json", "{\"error\":\"Invalid path\"}");
        return;
    }

    bool ok = sdManager->deleteFile(path.c_str());

    if (ok) {
        request->send(200, "application/json",
                      "{\"success\":true,\"message\":\"File deleted\"}");
    } else {
        request->send(500, "application/json",
                      "{\"success\":false,\"error\":\"Delete failed or file not found\"}");
    }
}

void WebServer::handleMkdirSD(AsyncWebServerRequest* request,
                               uint8_t* data, size_t len) {
    if (!sdManager || !sdManager->isMounted()) {
        sdNotAvailable(request);
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, (char*)data, len)) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const char* path = doc["path"] | "";
    if (path[0] == '\0' || String(path).indexOf("..") >= 0) {
        request->send(400, "application/json", "{\"error\":\"Invalid path\"}");
        return;
    }

    bool ok = sdManager->mkdir(path);

    JsonDocument resp;
    resp["success"] = ok;
    resp["path"]    = path;
    if (!ok) resp["error"] = "Failed to create directory";
    String body;
    serializeJson(resp, body);
    request->send(ok ? 200 : 500, "application/json", body);
}

void WebServer::handleFormatSD(AsyncWebServerRequest* request) {
    if (!sdManager) {
        sdNotAvailable(request, "SD manager not configured");
        return;
    }
    if (!sdManager->isMounted()) {
        sdNotAvailable(request);
        return;
    }

    serialPrintf("[SD] Format requested via web API\n");
    bool ok = sdManager->format();

    if (ok) {
        request->send(200, "application/json",
                      "{\"success\":true,\"message\":\"SD card formatted (all files removed)\"}");
    } else {
        request->send(500, "application/json",
                      "{\"success\":false,\"error\":\"Format failed\"}");
    }
}

void WebServer::handleMountSD(AsyncWebServerRequest* request) {
    if (!sdManager) {
        sdNotAvailable(request, "SD manager not configured");
        return;
    }

    bool ok = sdManager->mount();

    JsonDocument doc;
    doc["success"] = ok;
    doc["mounted"] = sdManager->isMounted();
    doc["message"] = ok ? "SD card mounted" : "No SD card found";
    String body;
    serializeJson(doc, body);
    request->send(ok ? 200 : 503, "application/json", body);
}

void WebServer::handleUnmountSD(AsyncWebServerRequest* request) {
    if (!sdManager) {
        sdNotAvailable(request, "SD manager not configured");
        return;
    }

    sdManager->unmount();
    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"SD card unmounted safely\"}");
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
    GPSData     gps     = boatState->getGPS();

    doc["version"] = VERSION;
    doc["uptime"]  = millis() / 1000;
    
    doc["datetime"]  = gps.datetime.getTimestamp();

    JsonObject heap  = doc["heap"].to<JsonObject>();
    heap["free"]     = ESP.getFreeHeap();
    heap["total"]    = ESP.getHeapSize();
    heap["min_free"] = ESP.getMinFreeHeap();

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    WiFiState   state   = wifiManager->getState();
    wl_status_t wlStat  = WiFi.status();
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

    UBaseType_t queueWaiting = 0;
    if (nmeaQueue != NULL) {
        queueWaiting = uxQueueMessagesWaiting(nmeaQueue);
    }
    nmeaBuffer["queue_waiting"]  = (uint32_t)queueWaiting;
    nmeaBuffer["queue_load_pct"] = (NMEA_QUEUE_SIZE > 0)
        ? (uint8_t)((queueWaiting * 100) / NMEA_QUEUE_SIZE)
        : 0;

    JsonObject ble = doc["ble"].to<JsonObject>();
    ble["enabled"]           = bleManager->isEnabled();
    ble["advertising"]       = bleManager->isAdvertising();
    ble["connected_devices"] = bleManager->getConnectedDevices();
    ble["device_name"]       = bleManager->getConfig().device_name;

    // ── Internal chip temperature ─────────────────────────────
    {
        float chip_temp = temperatureRead();  // Arduino ESP32 built-in
        JsonObject temp = doc["chip_temp"].to<JsonObject>();
        temp["celsius"]   = chip_temp;
        temp["available"] = true;
    }

    // ── SD card summary ───────────────────────────────────────
    JsonObject sd = doc["sd"].to<JsonObject>();
    if (sdManager) {
        SDStorageInfo sdInfo = sdManager->getStorageInfo();
        sd["enabled"]    = true;
        sd["mounted"]    = sdInfo.mounted;
        sd["card_type"]  = sdInfo.cardType;
        sd["total_mb"]   = (uint32_t)(sdInfo.totalBytes / (1024ULL * 1024ULL));
        sd["free_mb"]    = (uint32_t)(sdInfo.freeBytes  / (1024ULL * 1024ULL));
        sd["used_pct"]   = sdInfo.usedPct;
    } else {
        sd["enabled"] = false;
        sd["mounted"] = false;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::handleRestart(AsyncWebServerRequest* request) {
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Restarting in 2 seconds\"}");
    xTaskCreate([](void*) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }, "restart_task", 2048, nullptr, 1, nullptr);
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

    addDP("sog",     gps.sog,                "kn");
    addDP("cog",     gps.cog,                "deg");
    addDP("stw",     speed.stw,              "kn");
    addDP("heading", heading.true_heading,   "deg");
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

// GET /api/performance/config
void WebServer::handleGetPerformanceConfig(AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["damping_tau"] = boatState ? boatState->getDampingTau() : 0.0f;
    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

// POST /api/performance/config
void WebServer::handlePostPerformanceConfig(AsyncWebServerRequest* request,
                                             uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, (char*)data, len)) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    if (!doc["damping_tau"].is<float>() && !doc["damping_tau"].is<int>()) {
        request->send(400, "application/json", "{\"error\":\"damping_tau required (number)\"}");
        return;
    }

    float tau = doc["damping_tau"].as<float>();
    if (tau < 0.0f)  tau = 0.0f;
    if (tau > 60.0f) tau = 60.0f;

    perfNvs.putFloat("damping_tau", tau);
    if (boatState) boatState->setDampingTau(tau);

    serialPrintf("[Web] Performance damping tau set to %.1f s\n", tau);

    JsonDocument resp;
    resp["success"]     = true;
    resp["damping_tau"] = tau;
    String body;
    serializeJson(resp, body);
    request->send(200, "application/json", body);
}


void WebServer::handleGetLogConfig(AsyncWebServerRequest* request) {
    if (!logManager) {
        request->send(503, "application/json",
                      "{\"error\":\"Log manager not configured\"}");
        return;
    }

    LogConfig cfg = logManager->getConfig();
    JsonDocument doc;
    doc["nmea_enabled"]      = cfg.nmeaEnabled;
    doc["seatalk_enabled"]   = cfg.seatalkEnabled;
    doc["csv_enabled"]       = cfg.csvEnabled;
    doc["csv_interval_min"]  = cfg.csvIntervalMin;

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

// POST /api/log/config
void WebServer::handlePostLogConfig(AsyncWebServerRequest* request,
                                     uint8_t* data, size_t len) {
    if (!logManager) {
        request->send(503, "application/json",
                      "{\"error\":\"Log manager not configured\"}");
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, (char*)data, len)) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    LogConfig cfg = logManager->getConfig();
    if (doc["nmea_enabled"].is<bool>())     cfg.nmeaEnabled    = doc["nmea_enabled"];
    if (doc["seatalk_enabled"].is<bool>())  cfg.seatalkEnabled = doc["seatalk_enabled"];
    if (doc["csv_enabled"].is<bool>())      cfg.csvEnabled     = doc["csv_enabled"];
    if (doc["csv_interval_min"].is<int>()) {
        int ivl = doc["csv_interval_min"];
        if (ivl < 1)    ivl = 1;
        if (ivl > 1440) ivl = 1440;
        cfg.csvIntervalMin = (uint16_t)ivl;
    }

    logManager->setConfig(cfg);
    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"Log config saved\"}");
}

// GET /api/log/status
void WebServer::handleGetLogStatus(AsyncWebServerRequest* request) {
    if (!logManager) {
        request->send(503, "application/json",
                      "{\"error\":\"Log manager not configured\"}");
        return;
    }

    LogStats  st  = logManager->getStats();
    LogConfig cfg = logManager->getConfig();

    JsonDocument doc;
    doc["running"]          = logManager->isRunning();
    doc["has_open_files"]   = logManager->hasOpenFiles();
    doc["open_files"]       = logManager->openFilePaths();
    doc["session_name"]     = st.sessionName;
    doc["session_uptime_s"] = (millis() - st.sessionStartMs) / 1000;
    doc["nmea_lines"]       = st.nmeaLines;
    doc["seatalk_lines"]    = st.seatalkLines;
    doc["csv_snapshots"]    = st.csvSnapshots;
    doc["dropped_entries"]  = st.droppedEntries;

    // Also mirror config for convenience (dashboard needs one request).
    doc["nmea_enabled"]     = cfg.nmeaEnabled;
    doc["seatalk_enabled"]  = cfg.seatalkEnabled;
    doc["csv_enabled"]      = cfg.csvEnabled;
    doc["csv_interval_min"] = cfg.csvIntervalMin;

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

// POST /api/log/new
void WebServer::handlePostLogNewSession(AsyncWebServerRequest* request) {
    if (!logManager) {
        request->send(503, "application/json",
                      "{\"error\":\"Log manager not configured\"}");
        return;
    }

    logManager->newSession();
    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"New log session started\"}");
}


void WebServer::handlePostSeatalkExtra(AsyncWebServerRequest* request,
                                        uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, (char*)data, len)) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    const char* command = doc["command"] | "";
    if (command[0] == '\0') {
        request->send(400, "application/json", "{\"error\":\"Missing command field\"}");
        return;
    }
    if (!seatalkManager) {
        request->send(503, "application/json",
                      "{\"success\":false,\"error\":\"SeaTalk manager not initialised\"}");
        return;
    }
    bool ok = seatalkManager->sendExtraCommand(command);
    if (ok) {
        JsonDocument resp;
        resp["success"] = true;
        resp["message"] = String("Sent: ") + command;
        String body; serializeJson(resp, body);
        request->send(200, "application/json", body);
    } else {
        request->send(500, "application/json",
                      "{\"success\":false,\"error\":\"Transmission failed or unknown command\"}");
    }
}

// ── Conversion config rule descriptors (must match types.h index constants) ──
static const char* CONV_RULE_IDS[CONV_COUNT] = {
    "gps_cog_to_st",    // 0
    "gps_sog_to_st",    // 1
    "gps_pos_to_st",    // 2
    "depth_to_st",      // 3
    "stw_to_st",        // 4
    "awa_to_st",        // 5
    "aws_to_st",        // 6
    "water_temp_to_st", // 7
    "hdg_to_st",        // 8
    "tw_to_nmea",       // 9
};

// GET /api/config/conversions
void WebServer::handleGetConversionConfig(AsyncWebServerRequest* request) {
    ConversionConfig cfg;
    if (configManager) configManager->getConversionConfig(cfg);

    JsonDocument doc;
    JsonArray rules = doc["rules"].to<JsonArray>();
    for (int i = 0; i < CONV_COUNT; i++) {
        JsonObject rule = rules.add<JsonObject>();
        rule["id"]         = CONV_RULE_IDS[i];
        rule["enabled"]    = cfg.rules[i].enabled;
        rule["interval_s"] = cfg.rules[i].interval_s;
    }

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

// POST /api/config/conversions
void WebServer::handlePostConversionConfig(AsyncWebServerRequest* request,
                                           uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, (char*)data, len)) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    ConversionConfig cfg;
    if (configManager) configManager->getConversionConfig(cfg);

    JsonArray rules = doc["rules"].as<JsonArray>();
    if (!rules.isNull()) {
        for (JsonObject rule : rules) {
            const char* id = rule["id"] | "";
            for (int i = 0; i < CONV_COUNT; i++) {
                if (strcmp(id, CONV_RULE_IDS[i]) == 0) {
                    if (rule["enabled"].is<bool>())
                        cfg.rules[i].enabled = rule["enabled"].as<bool>();
                    if (rule["interval_s"].is<int>()) {
                        int iv = rule["interval_s"].as<int>();
                        if (iv < 1)  iv = 1;
                        if (iv > 60) iv = 60;
                        cfg.rules[i].interval_s = (uint8_t)iv;
                    }
                    break;
                }
            }
        }
    }

    if (configManager)  configManager->setConversionConfig(cfg);
    if (seatalkManager) seatalkManager->setConversionConfig(cfg);

    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"Conversion config saved\"}");
}

// ── Alarm handlers ─────────────────────────────────────────────────────────────

// GET /api/alarms/config
void WebServer::handleGetAlarmsConfig(AsyncWebServerRequest* request) {
    if (!alarmManager) {
        request->send(503, "application/json",
                      "{\"error\":\"Alarm manager not initialised\"}");
        return;
    }

    AlarmConfig cfg = alarmManager->getConfig();
    JsonDocument doc;
    doc["alarms_enabled"]       = cfg.alarms_enabled;
    doc["depth_enabled"]        = cfg.depth_enabled;
    doc["depth_threshold_m"]    = cfg.depth_threshold_m;
    doc["ais_enabled"]          = cfg.ais_enabled;
    doc["ais_distance_nm"]      = cfg.ais_distance_nm;
    doc["own_mmsi"]             = cfg.own_mmsi;
    doc["gps_lost_enabled"]     = cfg.gps_lost_enabled;
    doc["gps_lost_timeout_s"]   = cfg.gps_lost_timeout_s;

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

// POST /api/alarms/config
void WebServer::handlePostAlarmsConfig(AsyncWebServerRequest* request,
                                        uint8_t* data, size_t len) {
    if (!alarmManager) {
        request->send(503, "application/json",
                      "{\"error\":\"Alarm manager not initialised\"}");
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, (char*)data, len)) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    AlarmConfig cfg = alarmManager->getConfig();

    if (doc["alarms_enabled"].is<bool>())     cfg.alarms_enabled     = doc["alarms_enabled"];
    if (doc["depth_enabled"].is<bool>())      cfg.depth_enabled      = doc["depth_enabled"];
    if (doc["depth_threshold_m"].is<float>() || doc["depth_threshold_m"].is<int>()) {
        float th = doc["depth_threshold_m"].as<float>();
        if (th < 0.1f)  th = 0.1f;
        if (th > 999.0f) th = 999.0f;
        cfg.depth_threshold_m = th;
    }
    if (doc["ais_enabled"].is<bool>())        cfg.ais_enabled        = doc["ais_enabled"];
    if (doc["ais_distance_nm"].is<float>() || doc["ais_distance_nm"].is<int>()) {
        float nm = doc["ais_distance_nm"].as<float>();
        nm = roundf(nm * 10.0f) / 10.0f;   // snap to 0.1 nm steps
        if (nm < 0.1f)  nm = 0.1f;
        if (nm > 99.9f) nm = 99.9f;
        cfg.ais_distance_nm = nm;
    }
    if (doc["own_mmsi"].is<uint32_t>() || doc["own_mmsi"].is<int>() || doc["own_mmsi"].is<long>()) {
        cfg.own_mmsi = doc["own_mmsi"].as<uint32_t>();
    }
    if (doc["gps_lost_enabled"].is<bool>())   cfg.gps_lost_enabled   = doc["gps_lost_enabled"];
    if (doc["gps_lost_timeout_s"].is<int>()) {
        int to = doc["gps_lost_timeout_s"];
        if (to < 1)     to = 1;
        if (to > 3600)  to = 3600;
        cfg.gps_lost_timeout_s = (uint16_t)to;
    }

    alarmManager->setConfig(cfg);

    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"Alarm config saved\"}");
}

// GET /api/alarms/status
void WebServer::handleGetAlarmsStatus(AsyncWebServerRequest* request) {
    if (!boatState || !alarmManager) {
        request->send(503, "application/json",
                      "{\"error\":\"Alarm manager not initialised\"}");
        return;
    }

    AlarmState state = boatState->getAlarmState();
    JsonDocument doc;

    auto addAlarm = [&](const char* key, const AlarmItemState& item) {
        JsonObject obj = doc[key].to<JsonObject>();
        obj["active"]       = item.active;
        obj["acknowledged"] = item.acknowledged;
        if (item.active) {
            obj["age"] = (millis() - item.since) / 1000.0;
        } else {
            obj["age"] = nullptr;
        }
    };

    addAlarm("depth",    state.depth);
    addAlarm("ais",      state.ais);
    addAlarm("gps_lost", state.gps_lost);

    doc["ais_trigger_mmsi"] = state.ais_trigger_mmsi;
    doc["any_active"]       = state.anyActive();
    doc["any_unacked"]      = state.anyUnacked();

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

// POST /api/alarms/ack
void WebServer::handlePostAlarmsAck(AsyncWebServerRequest* request) {
    if (!alarmManager) {
        request->send(503, "application/json",
                      "{\"error\":\"Alarm manager not initialised\"}");
        return;
    }

    alarmManager->acknowledgeAll();
    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"Alarms acknowledged\"}");
}

// POST /api/alarms/beep_on
void WebServer::handlePostAlarmsBeepOn(AsyncWebServerRequest* request) {
    if (!alarmManager) {
        request->send(503, "application/json",
                      "{\"error\":\"Alarm manager not initialised\"}");
        return;
    }

    bool ok = alarmManager->beepOn();
    if (ok) {
        request->send(200, "application/json",
                      "{\"success\":true,\"message\":\"Beep on\"}");
    } else {
        request->send(500, "application/json",
                      "{\"success\":false,\"error\":\"Transmission failed\"}");
    }
}

// POST /api/alarms/beep_off
void WebServer::handlePostAlarmsBeepOff(AsyncWebServerRequest* request) {
    if (!alarmManager) {
        request->send(503, "application/json",
                      "{\"error\":\"Alarm manager not initialised\"}");
        return;
    }

    bool ok = alarmManager->beepOff();
    if (ok) {
        request->send(200, "application/json",
                      "{\"success\":true,\"message\":\"Beep off\"}");
    } else {
        request->send(500, "application/json",
                      "{\"success\":false,\"error\":\"Transmission failed\"}");
    }
}