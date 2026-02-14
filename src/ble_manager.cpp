#include "ble_manager.h"
#include <ArduinoJson.h>

// ============================================================
// BLE Server Callbacks Implementation
// ============================================================

BLEServerCallbacks::BLEServerCallbacks(BLEManager* manager) : bleManager(manager) {}

void BLEServerCallbacks::onConnect(BLEServer* pServer) {
    bleManager->connectedDevices++;
    Serial.printf("[BLE] Device connected (total: %u)\n", bleManager->connectedDevices);
}

void BLEServerCallbacks::onDisconnect(BLEServer* pServer) {
    if (bleManager->connectedDevices > 0) {
        bleManager->connectedDevices--;
    }
    Serial.printf("[BLE] Device disconnected (remaining: %u)\n", bleManager->connectedDevices);
    
    // Restart advertising if still enabled
    if (bleManager->config.enabled && bleManager->connectedDevices < BLE_MAX_CONNECTIONS) {
        delay(500);  // Brief delay before restarting advertising
        bleManager->startAdvertising();
    }
}

// ============================================================
// Autopilot Command Callbacks Implementation
// ============================================================

AutopilotCommandCallbacks::AutopilotCommandCallbacks(BLEManager* manager) : bleManager(manager) {}

void AutopilotCommandCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    
    if (value.length() == 0) {
        return;
    }
    
    Serial.printf("[BLE] Autopilot command received: %s\n", value.c_str());
    
    // Parse JSON command
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, value.c_str());
    
    if (error) {
        Serial.printf("[BLE] JSON parse error: %s\n", error.c_str());
        return;
    }
    
    const char* cmd = doc["command"] | "";
    
    AutopilotCommand command;
    command.timestamp = millis();
    
    if (strcmp(cmd, "enable") == 0) {
        command.type = AutopilotCommand::ENABLE;
    } else if (strcmp(cmd, "disable") == 0) {
        command.type = AutopilotCommand::DISABLE;
    } else if (strcmp(cmd, "adjust+10") == 0) {
        command.type = AutopilotCommand::ADJUST_PLUS_10;
    } else if (strcmp(cmd, "adjust-10") == 0) {
        command.type = AutopilotCommand::ADJUST_MINUS_10;
    } else if (strcmp(cmd, "adjust+1") == 0) {
        command.type = AutopilotCommand::ADJUST_PLUS_1;
    } else if (strcmp(cmd, "adjust-1") == 0) {
        command.type = AutopilotCommand::ADJUST_MINUS_1;
    } else {
        Serial.printf("[BLE] Unknown command: %s\n", cmd);
        return;
    }
    
    // Store command
    xSemaphoreTake(bleManager->commandMutex, portMAX_DELAY);
    bleManager->pendingCommand = command;
    xSemaphoreGive(bleManager->commandMutex);
    
    Serial.printf("[BLE] Autopilot command queued: %d\n", command.type);
}

// ============================================================
// BLE Security Callbacks Implementation
// ============================================================

BLESecurityCallbacks::BLESecurityCallbacks(BLEManager* manager) : bleManager(manager) {}

uint32_t BLESecurityCallbacks::onPassKeyRequest() {
    Serial.println("[BLE] PassKey request");
    return atoi(bleManager->config.pin_code);
}

void BLESecurityCallbacks::onPassKeyNotify(uint32_t pass_key) {
    Serial.printf("[BLE] PassKey notify: %06u\n", pass_key);
}

bool BLESecurityCallbacks::onConfirmPIN(uint32_t pass_key) {
    Serial.printf("[BLE] Confirm PIN: %06u\n", pass_key);
    return true;
}

bool BLESecurityCallbacks::onSecurityRequest() {
    Serial.println("[BLE] Security request");
    return true;
}

void BLESecurityCallbacks::onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) {
    if (auth_cmpl.success) {
        Serial.println("[BLE] ✓ Authentication successful");
    } else {
        Serial.printf("[BLE] ✗ Authentication failed (reason: %d)\n", auth_cmpl.fail_reason);
    }
}

// ============================================================
// BLE Manager Implementation
// ============================================================

BLEManager::BLEManager() 
    : pServer(nullptr), pAdvertising(nullptr),
      pNavigationService(nullptr), pNavDataChar(nullptr),
      pWindService(nullptr), pWindDataChar(nullptr),
      pAutopilotService(nullptr), pAutopilotDataChar(nullptr), pAutopilotCmdChar(nullptr),
      serverCallbacks(nullptr), autopilotCmdCallbacks(nullptr), securityCallbacks(nullptr),
      boatState(nullptr), initialized(false), advertising(false), 
      connectedDevices(0), updateTaskHandle(nullptr) {
    
    commandMutex = xSemaphoreCreateMutex();
}

BLEManager::~BLEManager() {
    stop();
    
    if (commandMutex) {
        vSemaphoreDelete(commandMutex);
    }
    
    if (serverCallbacks) delete serverCallbacks;
    if (autopilotCmdCallbacks) delete autopilotCmdCallbacks;
    if (securityCallbacks) delete securityCallbacks;
}

void BLEManager::init(const BLEConfig& cfg, BoatState* state) {
    if (initialized) {
        return;
    }
    
    config = cfg;
    boatState = state;
    
    Serial.println("[BLE] Initializing BLE Manager");
    Serial.printf("[BLE] Device Name: %s\n", config.device_name);
    Serial.printf("[BLE] PIN Code: %s\n", config.pin_code);
    Serial.printf("[BLE] Enabled: %s\n", config.enabled ? "Yes" : "No");
    
    // Initialize BLE Device
    BLEDevice::init(config.device_name);
    
    // Create BLE Server
    pServer = BLEDevice::createServer();
    serverCallbacks = new BLEServerCallbacks(this);
    pServer->setCallbacks(serverCallbacks);
    
    // Setup security
    setupSecurity();
    
    // Setup services
    setupServices();
    
    initialized = true;
    
    Serial.println("[BLE] ✓ Initialization complete");
}

void BLEManager::setupSecurity() {
    if (!BLE_SECURITY_ENABLED) {
        return;
    }
    
    Serial.println("[BLE] Setting up security...");
    
    // Set security callbacks
    securityCallbacks = new BLESecurityCallbacks(this);
    BLEDevice::setSecurityCallbacks(securityCallbacks);
    
    // Configure security parameters
    BLESecurity* pSecurity = new BLESecurity();
    
    uint32_t passkey = atoi(config.pin_code);
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
    
    pSecurity->setCapability(BLE_IO_CAP);
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    
    Serial.println("[BLE] ✓ Security configured");
}

void BLEManager::setupServices() {
    Serial.println("[BLE] Setting up services...");
    
    // ============================================================
    // Navigation Service
    // ============================================================
    pNavigationService = pServer->createService(BLE_SERVICE_NAVIGATION_UUID);
    
    pNavDataChar = pNavigationService->createCharacteristic(
        BLE_CHAR_NAV_DATA_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pNavDataChar->addDescriptor(new BLE2902());
    
    pNavigationService->start();
    Serial.println("[BLE]   ✓ Navigation service created");
    
    // ============================================================
    // Wind Service
    // ============================================================
    pWindService = pServer->createService(BLE_SERVICE_WIND_UUID);
    
    pWindDataChar = pWindService->createCharacteristic(
        BLE_CHAR_WIND_DATA_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pWindDataChar->addDescriptor(new BLE2902());
    
    pWindService->start();
    Serial.println("[BLE]   ✓ Wind service created");
    
    // ============================================================
    // Autopilot Service
    // ============================================================
    pAutopilotService = pServer->createService(BLE_SERVICE_AUTOPILOT_UUID);
    
    // Data characteristic (read/notify)
    pAutopilotDataChar = pAutopilotService->createCharacteristic(
        BLE_CHAR_AUTOPILOT_DATA_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pAutopilotDataChar->addDescriptor(new BLE2902());
    
    // Command characteristic (write)
    pAutopilotCmdChar = pAutopilotService->createCharacteristic(
        BLE_CHAR_AUTOPILOT_CMD_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    autopilotCmdCallbacks = new AutopilotCommandCallbacks(this);
    pAutopilotCmdChar->setCallbacks(autopilotCmdCallbacks);
    
    pAutopilotService->start();
    Serial.println("[BLE]   ✓ Autopilot service created");
    
    Serial.println("[BLE] ✓ All services configured");
}

void BLEManager::start() {
    if (!initialized || !config.enabled) {
        return;
    }
    
    Serial.println("[BLE] Starting BLE Manager...");
    
    // Start advertising
    startAdvertising();
    
    // Start update task
    xTaskCreate(updateTask, "BLE_Update", BLE_TASK_STACK_SIZE, this, BLE_TASK_PRIORITY, &updateTaskHandle);
    
    Serial.println("[BLE] ✓ BLE Manager started");
}

void BLEManager::stop() {
    if (!initialized) {
        return;
    }
    
    Serial.println("[BLE] Stopping BLE Manager...");
    
    // Stop update task
    if (updateTaskHandle) {
        vTaskDelete(updateTaskHandle);
        updateTaskHandle = nullptr;
    }
    
    // Stop advertising
    stopAdvertising();
    
    Serial.println("[BLE] ✓ BLE Manager stopped");
}

void BLEManager::startAdvertising() {
    if (advertising) {
        return;
    }
    
    pAdvertising = BLEDevice::getAdvertising();
    
    // Add services to advertising
    pAdvertising->addServiceUUID(BLE_SERVICE_NAVIGATION_UUID);
    pAdvertising->addServiceUUID(BLE_SERVICE_WIND_UUID);
    pAdvertising->addServiceUUID(BLE_SERVICE_AUTOPILOT_UUID);
    
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    
    BLEDevice::startAdvertising();
    advertising = true;
    
    Serial.println("[BLE] ✓ Advertising started");
}

void BLEManager::stopAdvertising() {
    if (!advertising) {
        return;
    }
    
    BLEDevice::stopAdvertising();
    advertising = false;
    
    Serial.println("[BLE] Advertising stopped");
}

void BLEManager::setEnabled(bool enabled) {
    config.enabled = enabled;
    
    if (enabled) {
        start();
    } else {
        stop();
    }
}

void BLEManager::setDeviceName(const char* name) {
    strncpy(config.device_name, name, sizeof(config.device_name) - 1);
    config.device_name[sizeof(config.device_name) - 1] = '\0';
    
    // Restart BLE if already running
    if (config.enabled) {
        stop();
        BLEDevice::deinit(true);
        delay(500);
        init(config, boatState);
        start();
    }
}

void BLEManager::setPinCode(const char* pin) {
    strncpy(config.pin_code, pin, sizeof(config.pin_code) - 1);
    config.pin_code[sizeof(config.pin_code) - 1] = '\0';
}

void BLEManager::updateTask(void* parameter) {
    BLEManager* manager = static_cast<BLEManager*>(parameter);
    
    Serial.println("[BLE Task] Started");
    
    while (true) {
        if (manager->connectedDevices > 0) {
            manager->updateNavigationData();
            manager->updateWindData();
            manager->updateAutopilotData();
        }
        
        vTaskDelay(pdMS_TO_TICKS(BLE_UPDATE_INTERVAL_MS));
    }
}

void BLEManager::updateNavigationData() {
    if (!pNavDataChar || !boatState) {
        return;
    }
    
    String json = buildNavigationJSON();
    pNavDataChar->setValue(json.c_str());
    pNavDataChar->notify();
}

void BLEManager::updateWindData() {
    if (!pWindDataChar || !boatState) {
        return;
    }
    
    String json = buildWindJSON();
    pWindDataChar->setValue(json.c_str());
    pWindDataChar->notify();
}

void BLEManager::updateAutopilotData() {
    if (!pAutopilotDataChar || !boatState) {
        return;
    }
    
    String json = buildAutopilotJSON();
    pAutopilotDataChar->setValue(json.c_str());
    pAutopilotDataChar->notify();
}

String BLEManager::buildNavigationJSON() {
    JsonDocument doc;
    
    GPSData gps = boatState->getGPS();
    SpeedData speed = boatState->getSpeed();
    HeadingData heading = boatState->getHeading();
    DepthData depth = boatState->getDepth();
    
    // Position
    if (gps.position.lat.valid && !gps.position.lat.isStale()) {
        doc["lat"] = gps.position.lat.value;
        doc["lon"] = gps.position.lon.value;
    }
    
    // Speed
    if (gps.sog.valid && !gps.sog.isStale()) {
        doc["sog"] = gps.sog.value;
    }
    if (speed.stw.valid && !speed.stw.isStale()) {
        doc["stw"] = speed.stw.value;
    }
    
    // Course
    if (gps.cog.valid && !gps.cog.isStale()) {
        doc["cog"] = gps.cog.value;
    }
    
    // Heading
    if (heading.magnetic.valid && !heading.magnetic.isStale()) {
        doc["hdg"] = heading.magnetic.value;
    }
    
    // Depth
    if (depth.below_transducer.valid && !depth.below_transducer.isStale()) {
        doc["depth"] = depth.below_transducer.value;
    }
    
    // GPS quality
    if (gps.satellites.valid && !gps.satellites.isStale()) {
        doc["sats"] = (int)gps.satellites.value;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

String BLEManager::buildWindJSON() {
    JsonDocument doc;
    
    WindData wind = boatState->getWind();
    
    if (wind.aws.valid && !wind.aws.isStale()) {
        doc["aws"] = wind.aws.value;
    }
    if (wind.awa.valid && !wind.awa.isStale()) {
        doc["awa"] = wind.awa.value;
    }
    if (wind.tws.valid && !wind.tws.isStale()) {
        doc["tws"] = wind.tws.value;
    }
    if (wind.twa.valid && !wind.twa.isStale()) {
        doc["twa"] = wind.twa.value;
    }
    if (wind.twd.valid && !wind.twd.isStale()) {
        doc["twd"] = wind.twd.value;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

String BLEManager::buildAutopilotJSON() {
    JsonDocument doc;
    
    AutopilotData autopilot = boatState->getAutopilot();
    
    if (autopilot.valid && !autopilot.isStale()) {
        doc["mode"] = autopilot.mode;
        doc["status"] = autopilot.status;
        
        if (autopilot.heading_target.valid) {
            doc["target"] = autopilot.heading_target.value;
        }
        if (autopilot.rudder_angle.valid) {
            doc["rudder"] = autopilot.rudder_angle.value;
        }
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool BLEManager::hasAutopilotCommand() {
    xSemaphoreTake(commandMutex, portMAX_DELAY);
    bool hasCmd = (pendingCommand.type != AutopilotCommand::NONE);
    xSemaphoreGive(commandMutex);
    return hasCmd;
}

AutopilotCommand BLEManager::getAutopilotCommand() {
    xSemaphoreTake(commandMutex, portMAX_DELAY);
    AutopilotCommand cmd = pendingCommand;
    pendingCommand.type = AutopilotCommand::NONE;  // Clear after reading
    xSemaphoreGive(commandMutex);
    return cmd;
}

void BLEManager::update() {
    // Called periodically from main loop if needed
    // Currently all updates are handled by the update task
}
