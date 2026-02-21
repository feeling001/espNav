#include "ble_manager.h"
#include <ArduinoJson.h>

// ============================================================
// Helper macro — ArduinoJson v7 compatible null/value assignment.
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
    Serial.printf("[BLE] Device connected addr=%s (total=%u)\n",
                  connInfo.getAddress().toString().c_str(),
                  manager->connectedDevices);

    if (manager->connectedDevices >= BLE_MAX_CONNECTIONS) {
        manager->stopAdvertising();
    }
}

void MarineServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    if (manager->connectedDevices > 0) manager->connectedDevices--;

    Serial.printf("[BLE] Device disconnected addr=%s reason=%d (remaining=%u)\n",
                  connInfo.getAddress().toString().c_str(),
                  reason,
                  manager->connectedDevices);

    // Restart advertising so new clients can connect
    if (manager->config.enabled &&
        manager->connectedDevices < BLE_MAX_CONNECTIONS) {
        manager->startAdvertising();
    }
}

void MarineServerCallbacks::onAuthenticationComplete(NimBLEConnInfo& connInfo) {
    // In NimBLE 2.x, NimBLESecurityCallbacks is gone.
    // onAuthenticationComplete is now a method of NimBLEServerCallbacks.
    if (connInfo.isEncrypted()) {
        Serial.printf("[BLE] ✓ Auth complete — addr=%s encrypted=yes bonded=%s\n",
                      connInfo.getAddress().toString().c_str(),
                      connInfo.isBonded() ? "yes" : "no");
    } else {
        Serial.printf("[BLE] ✗ Auth failed — addr=%s\n",
                      connInfo.getAddress().toString().c_str());
    }
}

// ============================================================
// AutopilotCmdCallbacks — NimBLE 2.x signature
// ============================================================

void AutopilotCmdCallbacks::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) {
    // NimBLE 2.x: getValue() returns NimBLEAttVal, cast to std::string
    std::string raw = pChar->getValue();
    if (raw.empty()) return;

    Serial.printf("[BLE] Autopilot command from %s: %s\n",
                  connInfo.getAddress().toString().c_str(),
                  raw.c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, raw.c_str());
    if (err) {
        Serial.printf("[BLE] JSON parse error: %s\n", err.c_str());
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
        Serial.printf("[BLE] Unknown command: %s\n", cmd);
        return;
    }

    xSemaphoreTake(manager->commandMutex, portMAX_DELAY);
    manager->pendingCommand = command;
    xSemaphoreGive(manager->commandMutex);
    Serial.printf("[BLE] Command queued: type=%d\n", command.type);
}

// ============================================================
// Constructor / Destructor
// ============================================================

BLEManager::BLEManager()
    : pServer(nullptr), pAdvertising(nullptr),
      pNavService(nullptr),       pNavDataChar(nullptr),
      pWindService(nullptr),      pWindDataChar(nullptr),
      pAutopilotService(nullptr), pAutopilotDataChar(nullptr), pAutopilotCmdChar(nullptr),
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

    Serial.println("[BLE] Initializing NimBLE 2.x stack");
    Serial.printf("[BLE]   Device name : %s\n", config.device_name);
    Serial.printf("[BLE]   PIN code    : %s\n", config.pin_code);
    Serial.printf("[BLE]   Enabled     : %s\n", config.enabled ? "yes" : "no");

    NimBLEDevice::init(config.device_name);
    NimBLEDevice::setPower(9); // +9 dBm — NimBLE 2.x uses dBm directly

    setupSecurity();

    pServer = NimBLEDevice::createServer();
    serverCallbacks = new MarineServerCallbacks(this);
    pServer->setCallbacks(serverCallbacks);

    setupServices();

    initialized = true;
    Serial.println("[BLE] ✓ NimBLE 2.x initialization complete");
}

// ============================================================
// start / stop
// ============================================================

void BLEManager::start() {
    if (!initialized) { Serial.println("[BLE] ✗ Cannot start — not initialized"); return; }
    if (!config.enabled) { Serial.println("[BLE] Not starting — disabled in config"); return; }

    Serial.println("[BLE] Starting...");
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

    Serial.println("[BLE] ✓ Started");
}

void BLEManager::stop() {
    if (updateTaskHandle) { vTaskDelete(updateTaskHandle); updateTaskHandle = nullptr; }
    stopAdvertising();
    Serial.println("[BLE] Stopped");
}

// ============================================================
// update — called by FreeRTOS task at 1 Hz
// ============================================================

void BLEManager::update() {
    if (!initialized || !config.enabled || connectedDevices == 0) return;
    updateNavData();
    updateWindData();
    updateAutopilotData();
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
//
// NimBLESecurityCallbacks is gone in 2.x.
// onAuthenticationComplete() is now part of NimBLEServerCallbacks.
// Security parameters are still set via NimBLEDevice static methods.
// ============================================================

void BLEManager::setupSecurity() {
    if (!BLE_SECURITY_ENABLED) return;

    Serial.println("[BLE] Configuring security (NimBLE 2.x)...");

    uint32_t passkey = (uint32_t)atoi(config.pin_code);
    NimBLEDevice::setSecurityPasskey(passkey);

    // Secure Connections + MITM + bonding
    NimBLEDevice::setSecurityAuth(true,  // bonding
                                   true,  // MITM
                                   true); // SC

    // Display-only: we show the PIN, client types it
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

    Serial.printf("[BLE] ✓ Security configured (PIN: %s)\n", config.pin_code);
}

// ============================================================
// setupServices
// ============================================================

void BLEManager::setupServices() {
    Serial.println("[BLE] Creating GATT services...");

    // Navigation service
    pNavService  = pServer->createService(BLE_SERVICE_NAVIGATION_UUID);
    pNavDataChar = pNavService->createCharacteristic(
        BLE_CHAR_NAV_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pNavService->start();
    Serial.println("[BLE]   ✓ Navigation service");

    // Wind service
    pWindService  = pServer->createService(BLE_SERVICE_WIND_UUID);
    pWindDataChar = pWindService->createCharacteristic(
        BLE_CHAR_WIND_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pWindService->start();
    Serial.println("[BLE]   ✓ Wind service");

    // Autopilot service
    pAutopilotService  = pServer->createService(BLE_SERVICE_AUTOPILOT_UUID);
    pAutopilotDataChar = pAutopilotService->createCharacteristic(
        BLE_CHAR_AUTOPILOT_DATA_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    autopilotCmdCallbacks = new AutopilotCmdCallbacks(this);
    pAutopilotCmdChar = pAutopilotService->createCharacteristic(
        BLE_CHAR_AUTOPILOT_CMD_UUID,
        NIMBLE_PROPERTY::WRITE);
    pAutopilotCmdChar->setCallbacks(autopilotCmdCallbacks);

    pAutopilotService->start();
    Serial.println("[BLE]   ✓ Autopilot service");

    Serial.println("[BLE] ✓ All services created");
}

// ============================================================
// Advertising — NimBLE 2.x API
// setScanResponse → setScanResponseData
// setMinPreferred → setMinInterval
// setMaxPreferred → setMaxInterval
// ============================================================

void BLEManager::startAdvertising() {
    if (!initialized || !config.enabled || advertising) return;

    Serial.println("[BLE] Starting advertising...");

    // Advertisement principal : nom du device uniquement (payload limité à 31 bytes)
    // 3 UUIDs 128-bit = 48 bytes → trop grand pour l'adv principal
    NimBLEAdvertisementData advData;
    advData.setName(config.device_name);
    advData.setFlags(0x06); // LE General Discoverable + BR/EDR Not Supported

    // Scan response : les UUIDs des services (le client les découvre au scan)
    NimBLEAdvertisementData scanData;
    scanData.addServiceUUID(BLE_SERVICE_NAVIGATION_UUID);
    scanData.addServiceUUID(BLE_SERVICE_WIND_UUID);
    scanData.addServiceUUID(BLE_SERVICE_AUTOPILOT_UUID);

    pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->setScanResponseData(scanData);
    pAdvertising->setMinInterval(0x06);  // 7.5 ms
    pAdvertising->setMaxInterval(0x0C);  // 15 ms

    NimBLEDevice::startAdvertising();
    advertising = true;

    Serial.println("[BLE] ✓ Advertising");
}

void BLEManager::stopAdvertising() {
    if (!advertising) return;
    NimBLEDevice::stopAdvertising();
    advertising = false;
    Serial.println("[BLE] Advertising stopped");
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
// Data push — NimBLE notify() sends to all subscribed clients
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
