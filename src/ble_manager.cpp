#include "ble_manager.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "alarm_manager.h"
#include "functions.h"
#include <ArduinoJson.h>
#include <esp_system.h>

// ============================================================
// Helper macro — ArduinoJson v7 null/value assignment.
// ============================================================
#define SET_JSON_DP(doc, key, dp) \
    do { \
        if ((dp).valid && !(dp).isStale()) { \
            (doc)[(key)] = (dp).value; \
        } else { \
            (doc)[(key)] = nullptr; \
        } \
    } while (0)

// ============================================================
// MarineServerCallbacks — NimBLE 2.x signatures
// ============================================================

void MarineServerCallbacks::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    manager->connectedDevices++;
    serialPrintf("[BLE] Device connected addr=%s (total=%u)\n",
                  connInfo.getAddress().toString().c_str(),
                  manager->connectedDevices);

    pServer->updateConnParams(connInfo.getConnHandle(), 0x18, 0x30, 0, 600);

    if (manager->connectedDevices >= BLE_MAX_CONNECTIONS) {
        manager->stopAdvertising();
    }
}

void MarineServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    if (manager->connectedDevices > 0) manager->connectedDevices--;

    serialPrintf("[BLE] Device disconnected addr=%s reason=%d (remaining=%u)\n",
                  connInfo.getAddress().toString().c_str(),
                  reason,
                  manager->connectedDevices);

    if (manager->config.enabled &&
        manager->connectedDevices < BLE_MAX_CONNECTIONS) {
        vTaskDelay(pdMS_TO_TICKS(100));
        pServer->startAdvertising();
        manager->advertising = true;
        serialPrintf("[BLE] Advertising restarted (reason=0x%04X)\n", reason);
    }
}

void MarineServerCallbacks::onAuthenticationComplete(NimBLEConnInfo& connInfo) {
    if (connInfo.isEncrypted()) {
        serialPrintf("[BLE] ✓ Auth complete — addr=%s encrypted=yes bonded=%s\n",
                      connInfo.getAddress().toString().c_str(),
                      connInfo.isBonded() ? "yes" : "no");
    } else {
        serialPrintf("[BLE] ✗ Auth failed — addr=%s\n",
                      connInfo.getAddress().toString().c_str());
    }
}

// ============================================================
// AutopilotCmdCallbacks — NimBLE 2.x signature
// ============================================================

void AutopilotCmdCallbacks::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) {
    std::string raw = pChar->getValue();
    if (raw.empty()) return;

    serialPrintf("[BLE] Autopilot command from %s: %s\n",
                  connInfo.getAddress().toString().c_str(),
                  raw.c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, raw.c_str());
    if (err) {
        serialPrintf("[BLE] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* cmd = doc["command"] | "";
    if (cmd[0] == '\0') {
        serialPrintf("[BLE] Missing command field\n");
        return;
    }

    if (manager->seatalkManager) {
        bool ok = manager->seatalkManager->sendAutopilotCommand(cmd);
        serialPrintf("[BLE] Autopilot cmd '%s' → %s\n", cmd, ok ? "OK" : "FAILED");
    } else {
        serialPrintf("[BLE] SeatalkManager not set — command '%s' dropped\n", cmd);
    }
}

// ============================================================
// AdminCmdCallbacks — NimBLE 2.x signature
//
// Accepted commands:
//   { "command": "restart" }
//   { "command": "wifi_sta", "ssid": "...", "password": "..." }
//   { "command": "wifi_ap",  "ssid": "...", "password": "..." }
// ============================================================

void AdminCmdCallbacks::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) {
    std::string raw = pChar->getValue();
    if (raw.empty()) return;

    serialPrintf("[BLE Admin] Command from %s: %s\n",
                  connInfo.getAddress().toString().c_str(),
                  raw.c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, raw.c_str());
    if (err) {
        serialPrintf("[BLE Admin] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* cmd = doc["command"] | "";
    if (cmd[0] == '\0') {
        serialPrintf("[BLE Admin] Missing command field\n");
        return;
    }

    // ── restart ──────────────────────────────────────────────────────────────
    if (strcmp(cmd, "restart") == 0) {
        serialPrintf("[BLE Admin] Restart requested — rebooting in 2 s\n");
        // Spawn a task so the response has time to be sent before reboot
        xTaskCreate([](void*) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        }, "ble_restart", 2048, nullptr, 1, nullptr);
        return;
    }

    // ── wifi_sta / wifi_ap ───────────────────────────────────────────────────
    if (strcmp(cmd, "wifi_sta") == 0 || strcmp(cmd, "wifi_ap") == 0) {
        if (!manager->configManager || !manager->wifiManager) {
            serialPrintf("[BLE Admin] ConfigManager or WiFiManager not set — WiFi cmd ignored\n");
            return;
        }

        const char* ssid     = doc["ssid"]     | "";
        const char* password = doc["password"] | "";

        if (ssid[0] == '\0') {
            serialPrintf("[BLE Admin] wifi cmd missing ssid — ignored\n");
            return;
        }

        WiFiConfig wifiConfig;
        manager->configManager->getWiFiConfig(wifiConfig);   // load existing values

        bool isAP = (strcmp(cmd, "wifi_ap") == 0);

        if (isAP) {
            wifiConfig.mode = 1;  // AP
            strncpy(wifiConfig.ap_ssid, ssid, sizeof(wifiConfig.ap_ssid) - 1);
            wifiConfig.ap_ssid[sizeof(wifiConfig.ap_ssid) - 1] = '\0';
            strncpy(wifiConfig.ap_password, password, sizeof(wifiConfig.ap_password) - 1);
            wifiConfig.ap_password[sizeof(wifiConfig.ap_password) - 1] = '\0';

            serialPrintf("[BLE Admin] WiFi → AP mode, SSID='%s'\n", wifiConfig.ap_ssid);
        } else {
            wifiConfig.mode = 0;  // STA
            strncpy(wifiConfig.ssid, ssid, sizeof(wifiConfig.ssid) - 1);
            wifiConfig.ssid[sizeof(wifiConfig.ssid) - 1] = '\0';
            strncpy(wifiConfig.password, password, sizeof(wifiConfig.password) - 1);
            wifiConfig.password[sizeof(wifiConfig.password) - 1] = '\0';

            serialPrintf("[BLE Admin] WiFi → STA mode, SSID='%s'\n", wifiConfig.ssid);
        }

        manager->configManager->setWiFiConfig(wifiConfig);

        // Apply immediately by restarting the device — WiFi stack is complex
        // to tear down and re-initialise at runtime; a reboot is safer.
        serialPrintf("[BLE Admin] WiFi config saved — rebooting in 3 s to apply\n");
        xTaskCreate([](void*) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_restart();
        }, "ble_wifi_restart", 2048, nullptr, 1, nullptr);
        return;
    }

    serialPrintf("[BLE Admin] Unknown command: '%s'\n", cmd);
}

// ============================================================
// AlarmCmdCallbacks — NimBLE 2.x signature
//
// Accepted commands:
//   { "command": "set_config", ... }
//   { "command": "ack" }
//   { "command": "beep_on" }
//   { "command": "beep_off" }
// ============================================================

void AlarmCmdCallbacks::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) {
    std::string raw = pChar->getValue();
    if (raw.empty()) return;

    serialPrintf("[BLE Alarm] Command from %s: %s\n",
                  connInfo.getAddress().toString().c_str(),
                  raw.c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, raw.c_str());
    if (err) {
        serialPrintf("[BLE Alarm] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* cmd = doc["command"] | "";
    if (cmd[0] == '\0') {
        serialPrintf("[BLE Alarm] Missing command field\n");
        return;
    }

    if (!manager->alarmManager) {
        serialPrintf("[BLE Alarm] AlarmManager not set — command '%s' dropped\n", cmd);
        return;
    }

    if (strcmp(cmd, "set_config") == 0) {
        AlarmConfig cfg = manager->alarmManager->getConfig();
        if (doc["alarms_enabled"].is<bool>())     cfg.alarms_enabled     = doc["alarms_enabled"];
        if (doc["depth_enabled"].is<bool>())      cfg.depth_enabled      = doc["depth_enabled"];
        if (doc["depth_threshold_m"].is<float>() || doc["depth_threshold_m"].is<int>())
            cfg.depth_threshold_m = doc["depth_threshold_m"].as<float>();
        if (doc["ais_enabled"].is<bool>())        cfg.ais_enabled        = doc["ais_enabled"];
        if (doc["ais_distance_nm"].is<float>() || doc["ais_distance_nm"].is<int>())
            cfg.ais_distance_nm = doc["ais_distance_nm"].as<float>();
        if (doc["own_mmsi"].is<uint32_t>() || doc["own_mmsi"].is<int>() || doc["own_mmsi"].is<long>())
            cfg.own_mmsi = doc["own_mmsi"].as<uint32_t>();
        if (doc["gps_lost_enabled"].is<bool>())   cfg.gps_lost_enabled   = doc["gps_lost_enabled"];
        if (doc["gps_lost_timeout_s"].is<int>())  cfg.gps_lost_timeout_s = (uint16_t)doc["gps_lost_timeout_s"].as<int>();

        manager->alarmManager->setConfig(cfg);
        serialPrintf("[BLE Alarm] Config updated\n");
        return;
    }

    if (strcmp(cmd, "ack") == 0) {
        manager->alarmManager->acknowledgeAll();
        serialPrintf("[BLE Alarm] Alarms acknowledged\n");
        return;
    }

    if (strcmp(cmd, "beep_on") == 0) {
        bool ok = manager->alarmManager->beepOn();
        serialPrintf("[BLE Alarm] beep_on -> %s\n", ok ? "OK" : "FAILED");
        return;
    }

    if (strcmp(cmd, "beep_off") == 0) {
        bool ok = manager->alarmManager->beepOff();
        serialPrintf("[BLE Alarm] beep_off -> %s\n", ok ? "OK" : "FAILED");
        return;
    }

    serialPrintf("[BLE Alarm] Unknown command: '%s'\n", cmd);
}

// ============================================================
// Constructor / Destructor
// ============================================================

BLEManager::BLEManager()
    : pServer(nullptr), pAdvertising(nullptr),
      pNavService(nullptr),         pNavDataChar(nullptr),
      pWindService(nullptr),        pWindDataChar(nullptr),
      pAutopilotService(nullptr),   pAutopilotDataChar(nullptr), pAutopilotCmdChar(nullptr),
      pPerformanceService(nullptr), pPerformanceDataChar(nullptr),
      pAdminService(nullptr),       pAdminDataChar(nullptr),     pAdminCmdChar(nullptr),
      pAlarmService(nullptr),       pAlarmDataChar(nullptr),     pAlarmCmdChar(nullptr),
      pAisService(nullptr),         pAisDataChar(nullptr),
      serverCallbacks(nullptr), autopilotCmdCallbacks(nullptr), adminCmdCallbacks(nullptr),
      alarmCmdCallbacks(nullptr),
      boatState(nullptr), seatalkManager(nullptr),
      configManager(nullptr), wifiManager(nullptr), alarmManager(nullptr),
      initialized(false), advertising(false),
      connectedDevices(0), updateTaskHandle(nullptr) {
}

BLEManager::~BLEManager() {
    stop();
    if (serverCallbacks)        delete serverCallbacks;
    if (autopilotCmdCallbacks)  delete autopilotCmdCallbacks;
    if (adminCmdCallbacks)      delete adminCmdCallbacks;
    if (alarmCmdCallbacks)      delete alarmCmdCallbacks;
}

// ============================================================
// init
// ============================================================

void BLEManager::init(const BLEConfig& cfg, BoatState* state,
                      SeatalkManager* stMgr,
                      ConfigManager*  configMgr,
                      WiFiManager*    wifiMgr,
                      AlarmManager*   alarmMgr) {
    if (initialized) return;

    config         = cfg;
    boatState      = state;
    seatalkManager = stMgr;
    configManager  = configMgr;
    wifiManager    = wifiMgr;
    alarmManager   = alarmMgr;

    serialPrintf("[BLE] Initializing NimBLE 2.x stack\n");
    serialPrintf("[BLE]   Device name    : %s\n", config.device_name);
    serialPrintf("[BLE]   PIN code       : %s\n", config.pin_code);
    serialPrintf("[BLE]   Enabled        : %s\n", config.enabled ? "yes" : "no");
    serialPrintf("[BLE]   SeatalkManager : %s\n", seatalkManager ? "yes" : "no (AP commands disabled)");
    serialPrintf("[BLE]   ConfigManager  : %s\n", configManager  ? "yes" : "no (Admin WiFi cmd disabled)");
    serialPrintf("[BLE]   AlarmManager   : %s\n", alarmManager   ? "yes" : "no (Alarm cmds disabled)");

    NimBLEDevice::init(config.device_name);
    NimBLEDevice::setPower(9);

    setupSecurity();

    pServer = NimBLEDevice::createServer();
    serverCallbacks = new MarineServerCallbacks(this);
    pServer->setCallbacks(serverCallbacks);

    setupServices();

    initialized = true;
    serialPrintf("[BLE] ✓ NimBLE 2.x initialization complete\n");
}

// ============================================================
// start / stop
// ============================================================

void BLEManager::start() {
    if (!initialized) { serialPrintf("[BLE] ✗ Cannot start — not initialized\n"); return; }
    if (!config.enabled) { serialPrintf("[BLE] Not starting — disabled in config\n"); return; }

    serialPrintf("[BLE] Starting...\n");
    startAdvertising();

    xTaskCreatePinnedToCore(
        updateTask,
        "BLE_Update",
        BLE_TASK_STACK_SIZE,
        this,
        BLE_TASK_PRIORITY,
        &updateTaskHandle,
        0
    );

    serialPrintf("[BLE] ✓ Started\n");
}

void BLEManager::stop() {
    if (updateTaskHandle) { vTaskDelete(updateTaskHandle); updateTaskHandle = nullptr; }
    stopAdvertising();
    serialPrintf("[BLE] Stopped\n");
}

// ============================================================
// update — called by FreeRTOS task at 1 Hz
// ============================================================

void BLEManager::update() {
    if (!initialized || !config.enabled || connectedDevices == 0) return;

    // Watchdog: restart advertising if it dropped unexpectedly
    if (connectedDevices < BLE_MAX_CONNECTIONS && config.enabled && !advertising) {
        serialPrintf("[BLE] Watchdog: restarting advertising\n");
        pServer->startAdvertising();
        advertising = true;
    }

    updateNavData();
    updateWindData();
    updateAutopilotData();
    updatePerformanceData();
    updateAdminData();
    updateAlarmData();
    updateAisData();
}

void BLEManager::updateTask(void* param) {
    BLEManager* mgr = static_cast<BLEManager*>(param);
    while (true) {
        mgr->update();
        vTaskDelay(pdMS_TO_TICKS(BLE_UPDATE_INTERVAL_MS));
    }
}

// ============================================================
// setupSecurity — NimBLE 2.x
// ============================================================

void BLEManager::setupSecurity() {
    if (!BLE_SECURITY_ENABLED) return;

    serialPrintf("[BLE] Configuring security (NimBLE 2.x)...\n");

    uint32_t passkey = (uint32_t)atoi(config.pin_code);
    NimBLEDevice::setSecurityPasskey(passkey);
    NimBLEDevice::setSecurityAuth(true, true, true); // bonding, MITM, SC
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

    serialPrintf("[BLE] ✓ Security configured (PIN: %s)\n", config.pin_code);
}

// ============================================================
// setupServices
// ============================================================

void BLEManager::setupServices() {
    serialPrintf("[BLE] Creating GATT services...\n");

    // ── Navigation ─────────────────────────────────────────────
    pNavService  = pServer->createService(BLE_SERVICE_NAVIGATION_UUID);
    pNavDataChar = pNavService->createCharacteristic(
        BLE_CHAR_NAV_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    serialPrintf("[BLE]   ✓ Navigation service\n");

    // ── Wind ───────────────────────────────────────────────────
    pWindService  = pServer->createService(BLE_SERVICE_WIND_UUID);
    pWindDataChar = pWindService->createCharacteristic(
        BLE_CHAR_WIND_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    serialPrintf("[BLE]   ✓ Wind service\n");

    // ── Autopilot ──────────────────────────────────────────────
    pAutopilotService  = pServer->createService(BLE_SERVICE_AUTOPILOT_UUID);
    pAutopilotDataChar = pAutopilotService->createCharacteristic(
        BLE_CHAR_AUTOPILOT_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    autopilotCmdCallbacks = new AutopilotCmdCallbacks(this);
    pAutopilotCmdChar = pAutopilotService->createCharacteristic(
        BLE_CHAR_AUTOPILOT_CMD_UUID,
        NIMBLE_PROPERTY::WRITE);
    pAutopilotCmdChar->setCallbacks(autopilotCmdCallbacks);
    serialPrintf("[BLE]   ✓ Autopilot service\n");

    // ── Sail Performance ───────────────────────────────────────
    pPerformanceService  = pServer->createService(BLE_SERVICE_PERFORMANCE_UUID);
    pPerformanceDataChar = pPerformanceService->createCharacteristic(
        BLE_CHAR_PERFORMANCE_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    serialPrintf("[BLE]   ✓ Sail Performance service\n");

    // ── Admin ──────────────────────────────────────────────────
    pAdminService  = pServer->createService(BLE_SERVICE_ADMIN_UUID);

    pAdminDataChar = pAdminService->createCharacteristic(
        BLE_CHAR_ADMIN_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    adminCmdCallbacks = new AdminCmdCallbacks(this);
    pAdminCmdChar = pAdminService->createCharacteristic(
        BLE_CHAR_ADMIN_CMD_UUID,
        NIMBLE_PROPERTY::WRITE);
    pAdminCmdChar->setCallbacks(adminCmdCallbacks);
    serialPrintf("[BLE]   ✓ Admin service\n");

    // ── Alarm ────────────────────────────────────────
    pAlarmService  = pServer->createService(BLE_SERVICE_ALARM_UUID);

    pAlarmDataChar = pAlarmService->createCharacteristic(
        BLE_CHAR_ALARM_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    alarmCmdCallbacks = new AlarmCmdCallbacks(this);
    pAlarmCmdChar = pAlarmService->createCharacteristic(
        BLE_CHAR_ALARM_CMD_UUID,
        NIMBLE_PROPERTY::WRITE);
    pAlarmCmdChar->setCallbacks(alarmCmdCallbacks);
    serialPrintf("[BLE]   ✓ Alarm service\n");

    // ── AIS ──────────────────────────────────────────
    pAisService  = pServer->createService(BLE_SERVICE_AIS_UUID);
    pAisDataChar = pAisService->createCharacteristic(
        BLE_CHAR_AIS_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    serialPrintf("[BLE]   ✓ AIS service\n");

    serialPrintf("[BLE] ✓ All services created\n");
}

// ============================================================
// Advertising — NimBLE 2.x API
// ============================================================

void BLEManager::startAdvertising() {
    if (!initialized || !config.enabled || advertising) return;

    serialPrintf("[BLE] Starting advertising...\n");

    pAdvertising = NimBLEDevice::getAdvertising();

    NimBLEAdvertisementData advData;
    advData.setFlags(0x06);
    advData.setName(config.device_name);

    NimBLEAdvertisementData scanData;
    scanData.addServiceUUID(BLE_SERVICE_NAVIGATION_UUID);
    scanData.addServiceUUID(BLE_SERVICE_PERFORMANCE_UUID);

    pAdvertising->setAdvertisementData(advData);
    pAdvertising->setScanResponseData(scanData);
    pAdvertising->setMinInterval(0x30);
    pAdvertising->setMaxInterval(0x60);

    NimBLEDevice::startAdvertising();
    advertising = true;

    serialPrintf("[BLE] ✓ Advertising\n");
}

void BLEManager::stopAdvertising() {
    if (!advertising) return;
    NimBLEDevice::stopAdvertising();
    advertising = false;
    serialPrintf("[BLE] Advertising stopped\n");
}

// ============================================================
// Config setters
// ============================================================

void BLEManager::setEnabled(bool en) {
    config.enabled = en;
    if (en && initialized) start();
    else                   stop();
}

void BLEManager::setDeviceName(const char* name) {
    strncpy(config.device_name, name, sizeof(config.device_name) - 1);
    config.device_name[sizeof(config.device_name) - 1] = '\0';

    if (initialized) {
        stop();
        NimBLEDevice::deinit(true);
        initialized = false;
        init(config, boatState, seatalkManager, configManager, wifiManager, alarmManager);
        start();
    }
}

void BLEManager::setPinCode(const char* pin) {
    strncpy(config.pin_code, pin, sizeof(config.pin_code) - 1);
    config.pin_code[sizeof(config.pin_code) - 1] = '\0';

    if (initialized && BLE_SECURITY_ENABLED) {
        uint32_t passkey = (uint32_t)atoi(config.pin_code);
        NimBLEDevice::setSecurityPasskey(passkey);
    }
}

// ============================================================
// Data push helpers
// ============================================================

void BLEManager::updateNavData() {
    if (!pNavDataChar) return;
    String json = buildNavJSON();
    pNavDataChar->setValue(json.c_str());
    pNavDataChar->notify();
}

void BLEManager::updateWindData() {
    if (!pWindDataChar) return;
    String json = buildWindJSON();
    pWindDataChar->setValue(json.c_str());
    pWindDataChar->notify();
}

void BLEManager::updateAutopilotData() {
    if (!pAutopilotDataChar) return;
    String json = buildAutopilotJSON();
    pAutopilotDataChar->setValue(json.c_str());
    pAutopilotDataChar->notify();
}

void BLEManager::updatePerformanceData() {
    if (!pPerformanceDataChar) return;
    String json = buildPerformanceJSON();
    pPerformanceDataChar->setValue(json.c_str());
    pPerformanceDataChar->notify();
}

void BLEManager::updateAdminData() {
    if (!pAdminDataChar) return;
    String json = buildAdminJSON();
    pAdminDataChar->setValue(json.c_str());
    pAdminDataChar->notify();
}

void BLEManager::updateAlarmData() {
    if (!pAlarmDataChar) return;
    String json = buildAlarmJSON();
    pAlarmDataChar->setValue(json.c_str());
    pAlarmDataChar->notify();
}

void BLEManager::updateAisData() {
    if (!pAisDataChar) return;
    String json = buildAisJSON();
    pAisDataChar->setValue(json.c_str());
    pAisDataChar->notify();
}

// ============================================================
// JSON builders — ArduinoJson v7
// ============================================================

String BLEManager::buildNavJSON() {
    JsonDocument doc;
    GPSData     gps     = boatState->getGPS();
    SpeedData   speed   = boatState->getSpeed();
    HeadingData heading = boatState->getHeading();
    DepthData   depth   = boatState->getDepth();

    SET_JSON_DP(doc, "lat",      gps.position.lat);
    SET_JSON_DP(doc, "lon",      gps.position.lon);
    SET_JSON_DP(doc, "sog",      gps.sog);
    SET_JSON_DP(doc, "cog",      gps.cog);
    SET_JSON_DP(doc, "stw",      speed.stw);
    SET_JSON_DP(doc, "hdg_mag",  heading.magnetic);
    SET_JSON_DP(doc, "hdg_true", heading.true_heading);
    SET_JSON_DP(doc, "depth",    depth.below_transducer);

    String out;
    serializeJson(doc, out);
    return out;
}

String BLEManager::buildWindJSON() {
    JsonDocument doc;
    WindData wind = boatState->getWind();

    SET_JSON_DP(doc, "aws", wind.aws);
    SET_JSON_DP(doc, "awa", wind.awa);
    SET_JSON_DP(doc, "tws", wind.tws);
    SET_JSON_DP(doc, "twa", wind.twa);
    SET_JSON_DP(doc, "twd", wind.twd);

    String out;
    serializeJson(doc, out);
    return out;
}

String BLEManager::buildAutopilotJSON() {
    JsonDocument doc;
    AutopilotData ap = boatState->getAutopilot();

    doc["mode"]   = ap.mode;
    doc["status"] = ap.status;
    SET_JSON_DP(doc, "heading_target", ap.heading_target);
    SET_JSON_DP(doc, "wind_target",    ap.wind_angle_target);
    SET_JSON_DP(doc, "rudder",         ap.rudder_angle);
    SET_JSON_DP(doc, "locked_heading", ap.locked_heading);

    String out;
    serializeJson(doc, out);
    return out;
}

String BLEManager::buildPerformanceJSON() {
    JsonDocument doc;

    PerformanceData perf = boatState->getPerformance();
    WindData        wind = boatState->getWind();

    SET_JSON_DP(doc, "vmg", perf.vmg);
    SET_JSON_DP(doc, "polar_pct", perf.polarPct);

    bool polarLoaded = boatState->polar.isLoaded();
    doc["polar_loaded"] = polarLoaded;

    if (polarLoaded &&
        wind.tws.valid && !wind.tws.isStale() &&
        wind.twa.valid && !wind.twa.isStale()) {

        float target = boatState->polar.getTargetSTW(wind.tws.value, wind.twa.value);
        if (target > 0.1f) {
            doc["target_stw"] = target;
        } else {
            doc["target_stw"] = nullptr;
        }
    } else {
        doc["target_stw"] = nullptr;
    }

    String out;
    serializeJson(doc, out);
    return out;
}

String BLEManager::buildAdminJSON() {
    JsonDocument doc;

    // Uptime in seconds
    doc["uptime_s"] = (uint32_t)(millis() / 1000UL);

    // GPS-sourced UTC timestamp (unix epoch), null if not available
    if (boatState) {
        GPSData gps = boatState->getGPS();
        uint64_t ts = gps.datetime.getTimestamp();
        if (ts > 1) {
            // ArduinoJson v7 handles uint64 — store as number
            doc["datetime_utc"] = ts;
        } else {
            doc["datetime_utc"] = nullptr;
        }
    } else {
        doc["datetime_utc"] = nullptr;
    }

    // WiFi state + IP address
    if (configManager) {
        WiFiConfig wifiCfg;
        configManager->getWiFiConfig(wifiCfg);
        bool isSTA = (wifiCfg.mode == 0);
        doc["wifi_mode"] = isSTA ? "sta" : "ap";
        doc["wifi_ssid"] = isSTA ? wifiCfg.ssid : wifiCfg.ap_ssid;

        // Resolve actual IP from the active interface
        IPAddress ip = isSTA ? WiFi.localIP() : WiFi.softAPIP();
        if (ip != IPAddress(0, 0, 0, 0)) {
            doc["ip"] = ip.toString();
        } else {
            doc["ip"] = nullptr;
        }
    } else {
        doc["wifi_mode"] = nullptr;
        doc["wifi_ssid"] = nullptr;
        doc["ip"]        = nullptr;
    }

    // Free heap
    doc["free_heap"] = (uint32_t)ESP.getFreeHeap();

    String out;
    serializeJson(doc, out);
    return out;
}

String BLEManager::buildAlarmJSON() {
    JsonDocument doc;

    if (!alarmManager || !boatState) {
        String out;
        serializeJson(doc, out);
        return out;
    }

    AlarmConfig cfg   = alarmManager->getConfig();
    AlarmState  state = boatState->getAlarmState();

    doc["alarms_enabled"]     = cfg.alarms_enabled;
    doc["depth_enabled"]      = cfg.depth_enabled;
    doc["depth_threshold_m"]  = cfg.depth_threshold_m;
    doc["ais_enabled"]        = cfg.ais_enabled;
    doc["ais_distance_nm"]    = cfg.ais_distance_nm;
    doc["own_mmsi"]           = cfg.own_mmsi;
    doc["gps_lost_enabled"]   = cfg.gps_lost_enabled;
    doc["gps_lost_timeout_s"] = cfg.gps_lost_timeout_s;

    auto addAlarm = [&](const char* key, const AlarmItemState& item) {
        JsonObject obj = doc[key].to<JsonObject>();
        obj["active"]       = item.active;
        obj["acknowledged"] = item.acknowledged;
    };

    addAlarm("depth",    state.depth);
    addAlarm("ais",      state.ais);
    addAlarm("gps_lost", state.gps_lost);

    doc["ais_trigger_mmsi"] = state.ais_trigger_mmsi;
    doc["any_active"]       = state.anyActive();
    doc["any_unacked"]      = state.anyUnacked();

    String out;
    serializeJson(doc, out);
    return out;
}

String BLEManager::buildAisJSON() {
    JsonDocument doc;

    if (!boatState) {
        String out;
        serializeJson(doc, out);
        return out;
    }

    AISData ais = boatState->getAIS();

    // Collect non-stale targets, then keep only the closest ones so the
    // notification stays within the BLE attribute/MTU size limit.
    AISTarget* valid[MAX_AIS_TARGETS];
    int validCount = 0;
    unsigned long now = millis();

    for (int i = 0; i < ais.targetCount; i++) {
        AISTarget& target = ais.targets[i];
        if ((now - target.timestamp) <= DATA_TIMEOUT_AIS) {
            valid[validCount++] = &target;
        }
    }

    // Simple insertion sort by distance (ascending) — target counts are small.
    for (int i = 1; i < validCount; i++) {
        AISTarget* key = valid[i];
        int j = i - 1;
        while (j >= 0 && valid[j]->distance > key->distance) {
            valid[j + 1] = valid[j];
            j--;
        }
        valid[j + 1] = key;
    }

    int sentCount = validCount < BLE_AIS_MAX_TARGETS ? validCount : BLE_AIS_MAX_TARGETS;

    doc["target_count"] = sentCount;
    JsonArray targetsArray = doc["targets"].to<JsonArray>();
    for (int i = 0; i < sentCount; i++) {
        AISTarget* target = valid[i];
        JsonObject targetObj = targetsArray.add<JsonObject>();
        targetObj["mmsi"]     = target->mmsi;
        targetObj["name"]     = target->name;
        targetObj["lat"]      = target->lat;
        targetObj["lon"]      = target->lon;
        targetObj["cog"]      = target->cog;
        targetObj["sog"]      = target->sog;
        targetObj["heading"]  = target->heading;
        targetObj["distance"] = target->distance;
        targetObj["bearing"]  = target->bearing;
        targetObj["cpa"]      = target->cpa;
        targetObj["tcpa"]     = target->tcpa;
        targetObj["age"]      = (uint32_t)((now - target->timestamp) / 1000);
    }

    String out;
    serializeJson(doc, out);
    return out;
}