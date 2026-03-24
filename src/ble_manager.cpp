#include "ble_manager.h"
#include "functions.h"
#include <ArduinoJson.h>

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
    AutopilotCommand command;
    command.timestamp = millis();

    if      (strcmp(cmd, "enable")    == 0) command.type = AutopilotCommand::ENABLE;
    else if (strcmp(cmd, "disable")   == 0) command.type = AutopilotCommand::DISABLE;
    else if (strcmp(cmd, "adjust+10") == 0) command.type = AutopilotCommand::ADJUST_PLUS_10;
    else if (strcmp(cmd, "adjust-10") == 0) command.type = AutopilotCommand::ADJUST_MINUS_10;
    else if (strcmp(cmd, "adjust+1")  == 0) command.type = AutopilotCommand::ADJUST_PLUS_1;
    else if (strcmp(cmd, "adjust-1")  == 0) command.type = AutopilotCommand::ADJUST_MINUS_1;
    else {
        serialPrintf("[BLE] Unknown command: %s\n", cmd);
        return;
    }

    xSemaphoreTake(manager->commandMutex, portMAX_DELAY);
    manager->pendingCommand = command;
    xSemaphoreGive(manager->commandMutex);
    serialPrintf("[BLE] Command queued: type=%d\n", command.type);
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
      serverCallbacks(nullptr), autopilotCmdCallbacks(nullptr),
      boatState(nullptr), initialized(false), advertising(false),
      connectedDevices(0), updateTaskHandle(nullptr) {

    commandMutex = xSemaphoreCreateMutex();
}

BLEManager::~BLEManager() {
    stop();
    if (commandMutex)           vSemaphoreDelete(commandMutex);
    if (serverCallbacks)        delete serverCallbacks;
    if (autopilotCmdCallbacks)  delete autopilotCmdCallbacks;
}

// ============================================================
// init
// ============================================================

void BLEManager::init(const BLEConfig& cfg, BoatState* state) {
    if (initialized) return;

    config    = cfg;
    boatState = state;

    serialPrintf("[BLE] Initializing NimBLE 2.x stack");
    serialPrintf("[BLE]   Device name : %s\n", config.device_name);
    serialPrintf("[BLE]   PIN code    : %s\n", config.pin_code);
    serialPrintf("[BLE]   Enabled     : %s\n", config.enabled ? "yes" : "no");

    NimBLEDevice::init(config.device_name);
    NimBLEDevice::setPower(9);

    setupSecurity();

    pServer = NimBLEDevice::createServer();
    serverCallbacks = new MarineServerCallbacks(this);
    pServer->setCallbacks(serverCallbacks);

    setupServices();

    initialized = true;
    serialPrintf("[BLE] ✓ NimBLE 2.x initialization complete");
}

// ============================================================
// start / stop
// ============================================================

void BLEManager::start() {
    if (!initialized) { serialPrintf("[BLE] ✗ Cannot start — not initialized"); return; }
    if (!config.enabled) { serialPrintf("[BLE] Not starting — disabled in config"); return; }

    serialPrintf("[BLE] Starting...");
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

    serialPrintf("[BLE] ✓ Started");
}

void BLEManager::stop() {
    if (updateTaskHandle) { vTaskDelete(updateTaskHandle); updateTaskHandle = nullptr; }
    stopAdvertising();
    serialPrintf("[BLE] Stopped");
}

// ============================================================
// update — called by FreeRTOS task at 1 Hz
// ============================================================

void BLEManager::update() {
    if (!initialized || !config.enabled || connectedDevices == 0) return;

    // Watchdog: restart advertising if it dropped unexpectedly
    if (connectedDevices < BLE_MAX_CONNECTIONS && config.enabled && !advertising) {
        serialPrintf("[BLE] Watchdog: restarting advertising");
        pServer->startAdvertising();
        advertising = true;
    }

    updateNavData();
    updateWindData();
    updateAutopilotData();
    updatePerformanceData();
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

    serialPrintf("[BLE] Configuring security (NimBLE 2.x)...");

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
    serialPrintf("[BLE] Creating GATT services...");

    // ── Navigation ─────────────────────────────────────────────
    pNavService  = pServer->createService(BLE_SERVICE_NAVIGATION_UUID);
    pNavDataChar = pNavService->createCharacteristic(
        BLE_CHAR_NAV_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    serialPrintf("[BLE]   ✓ Navigation service");

    // ── Wind ───────────────────────────────────────────────────
    pWindService  = pServer->createService(BLE_SERVICE_WIND_UUID);
    pWindDataChar = pWindService->createCharacteristic(
        BLE_CHAR_WIND_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    serialPrintf("[BLE]   ✓ Wind service");

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
    serialPrintf("[BLE]   ✓ Autopilot service");

    // ── Sail Performance ───────────────────────────────────────
    pPerformanceService  = pServer->createService(BLE_SERVICE_PERFORMANCE_UUID);
    pPerformanceDataChar = pPerformanceService->createCharacteristic(
        BLE_CHAR_PERFORMANCE_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    serialPrintf("[BLE]   ✓ Sail Performance service");

    serialPrintf("[BLE] ✓ All services created");
}

// ============================================================
// Advertising — NimBLE 2.x API
// ============================================================

void BLEManager::startAdvertising() {
    if (!initialized || !config.enabled || advertising) return;

    serialPrintf("[BLE] Starting advertising...");

    pAdvertising = NimBLEDevice::getAdvertising();

    NimBLEAdvertisementData advData;
    advData.setFlags(0x06);
    advData.setName(config.device_name);

    NimBLEAdvertisementData scanData;
    scanData.addServiceUUID(BLE_SERVICE_NAVIGATION_UUID);
    // Also advertise the performance service UUID so scanners can filter on it
    scanData.addServiceUUID(BLE_SERVICE_PERFORMANCE_UUID);

    pAdvertising->setAdvertisementData(advData);
    pAdvertising->setScanResponseData(scanData);
    pAdvertising->setMinInterval(0x30);
    pAdvertising->setMaxInterval(0x60);

    NimBLEDevice::startAdvertising();
    advertising = true;

    serialPrintf("[BLE] ✓ Advertising");
}

void BLEManager::stopAdvertising() {
    if (!advertising) return;
    NimBLEDevice::stopAdvertising();
    advertising = false;
    serialPrintf("[BLE] Advertising stopped");
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
        init(config, boatState);
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
// Autopilot command queue
// ============================================================

bool BLEManager::hasAutopilotCommand() {
    bool has = false;
    xSemaphoreTake(commandMutex, portMAX_DELAY);
    has = (pendingCommand.type != AutopilotCommand::NONE);
    xSemaphoreGive(commandMutex);
    return has;
}

AutopilotCommand BLEManager::getAutopilotCommand() {
    AutopilotCommand cmd;
    xSemaphoreTake(commandMutex, portMAX_DELAY);
    cmd = pendingCommand;
    pendingCommand.type = AutopilotCommand::NONE;
    xSemaphoreGive(commandMutex);
    return cmd;
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

/**
 * @brief Build the Sail Performance JSON payload.
 *
 * Fields:
 *   vmg         float | null   VMG in knots. Positive = upwind, negative = downwind.
 *   polar_pct   float | null   Current STW as % of polar target. null if no polar loaded.
 *   target_stw  float | null   Polar target STW in knots.        null if no polar loaded.
 *   polar_loaded bool          True when a polar file is loaded on the device.
 *
 * Example (polar loaded, sailing upwind at 85 % efficiency):
 *   {"vmg":4.2,"polar_pct":85.3,"target_stw":7.1,"polar_loaded":true}
 *
 * Example (no polar loaded):
 *   {"vmg":4.2,"polar_pct":null,"target_stw":null,"polar_loaded":false}
 */
String BLEManager::buildPerformanceJSON() {
    JsonDocument doc;

    PerformanceData perf = boatState->getPerformance();
    WindData        wind = boatState->getWind();

    // ── VMG ────────────────────────────────────────────────────
    SET_JSON_DP(doc, "vmg", perf.vmg);

    // ── Polar % ────────────────────────────────────────────────
    SET_JSON_DP(doc, "polar_pct", perf.polarPct);

    // ── Target STW ─────────────────────────────────────────────
    // Recompute directly from the polar so we always have the raw target
    // alongside the percentage, without storing it as a separate DataPoint.
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
