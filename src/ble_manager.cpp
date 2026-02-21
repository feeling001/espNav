#include "ble_manager.h"
#include <ArduinoJson.h>
#include <esp_gap_ble_api.h>   // esp_ble_gap_disconnect()
#include <esp_gatt_defs.h>     // esp_ble_gatts_cb_param_t

// ============================================================
// BLE Server Callbacks Implementation
// ============================================================

CustomBLEServerCallbacks::CustomBLEServerCallbacks(BLEManager* manager) : bleManager(manager) {}

 void CustomBLEServerCallbacks::onConnect(BLEServer* pServer) {
    bleManager->connectedDevices++;
    bleManager->lastActivityMs = millis();
    Serial.printf("[BLE] Device connected (total: %u)\n", bleManager->connectedDevices);
 
    // Stop advertising once we reach max connections
    if (bleManager->connectedDevices >= BLE_MAX_CONNECTIONS) {
        bleManager->stopAdvertising();
    }
}

void CustomBLEServerCallbacks::onDisconnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
    uint8_t reason = param ? param->disconnect.reason : 0;
    Serial.printf("[BLE] Device disconnected — reason=0x%02X (remaining: %u)\n", reason, bleManager->connectedDevices > 0 ? bleManager->connectedDevices - 1 : 0);

     if (bleManager->connectedDevices > 0) {
         bleManager->connectedDevices--;
     }

     if (bleManager->config.enabled && bleManager->connectedDevices < BLE_MAX_CONNECTIONS) {
        // Use a FreeRTOS software timer instead of delay() to defer advertising
        // restart out of the BLE stack callback context.
         bleManager->startAdvertising();
     }
 }

// Fallback overload for older Arduino BLE versions that don't pass the param.
void CustomBLEServerCallbacks::onDisconnect(BLEServer* pServer) {
    if (bleManager->connectedDevices > 0) {
        bleManager->connectedDevices--;
    }
    Serial.printf("[BLE] Device disconnected — no param (remaining: %u)\n", bleManager->connectedDevices);
    if (bleManager->config.enabled && bleManager->connectedDevices < BLE_MAX_CONNECTIONS) {
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

CustomBLESecurityCallbacks::CustomBLESecurityCallbacks(BLEManager* manager) : bleManager(manager) {}

uint32_t CustomBLESecurityCallbacks::onPassKeyRequest() {
    Serial.println("[BLE] PassKey request");
    return atoi(bleManager->config.pin_code);
}

void CustomBLESecurityCallbacks::onPassKeyNotify(uint32_t pass_key) {
    Serial.printf("[BLE] PassKey notify: %06u\n", pass_key);
}

bool CustomBLESecurityCallbacks::onConfirmPIN(uint32_t pass_key) {
    Serial.printf("[BLE] Confirm PIN: %06u\n", pass_key);
    return true;
}

bool CustomBLESecurityCallbacks::onSecurityRequest() {
    Serial.println("[BLE] Security request");
    return true;
}

void CustomBLESecurityCallbacks::onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) {
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
      connectedDevices(0), lastActivityMs(0), updateTaskHandle(nullptr) {    
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
    serverCallbacks = new CustomBLEServerCallbacks(this);
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
    securityCallbacks = new CustomBLESecurityCallbacks(this);
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
    if (!initialized) {
        Serial.println("[BLE] ✗ Cannot start - not initialized");
        return;
    }
    
    if (!config.enabled) {
        Serial.println("[BLE] Not starting - disabled in config");
        return;
    }
    
    Serial.println("[BLE] Starting BLE services...");
    
    // Start advertising
    startAdvertising();
    
    // Create update task
    xTaskCreatePinnedToCore(
        updateTask,
        "BLE_Update",
        4096,
        this,
        1,
        &updateTaskHandle,
        0
    );
    
    Serial.println("[BLE] ✓ Started");
}

void BLEManager::stop() {
    if (updateTaskHandle) {
        vTaskDelete(updateTaskHandle);
        updateTaskHandle = nullptr;
    }
    
    stopAdvertising();
    
    Serial.println("[BLE] Stopped");
}

void BLEManager::startAdvertising() {
    if (!initialized || !config.enabled) {
        return;
    }
    
    if (advertising) {
        return;
    }
    
    Serial.println("[BLE] Starting advertising...");
    
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_NAVIGATION_UUID);
    pAdvertising->addServiceUUID(BLE_SERVICE_WIND_UUID);
    pAdvertising->addServiceUUID(BLE_SERVICE_AUTOPILOT_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    
    BLEDevice::startAdvertising();
    
    advertising = true;
    Serial.println("[BLE] ✓ Advertising");
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
    
    if (enabled && initialized) {
        start();
    } else {
        stop();
    }
}

void BLEManager::setDeviceName(const char* name) {
    strncpy(config.device_name, name, sizeof(config.device_name) - 1);
    config.device_name[sizeof(config.device_name) - 1] = '\0';
    
    // Restart if running to apply new name
    if (initialized && advertising) {
        stop();
        BLEDevice::deinit(false);
        initialized = false;
        init(config, boatState);
        start();
    }
}

void BLEManager::setPinCode(const char* pin) {
    strncpy(config.pin_code, pin, sizeof(config.pin_code) - 1);
    config.pin_code[sizeof(config.pin_code) - 1] = '\0';
}

bool BLEManager::hasAutopilotCommand() {
    bool hasCommand = false;
    
    xSemaphoreTake(commandMutex, portMAX_DELAY);
    hasCommand = (pendingCommand.type != AutopilotCommand::NONE);
    xSemaphoreGive(commandMutex);
    
    return hasCommand;
}

AutopilotCommand BLEManager::getAutopilotCommand() {
    AutopilotCommand cmd;
    
    xSemaphoreTake(commandMutex, portMAX_DELAY);
    cmd = pendingCommand;
    pendingCommand.type = AutopilotCommand::NONE;
    xSemaphoreGive(commandMutex);
    
    return cmd;
}

void BLEManager::update() {
    if (!initialized || !config.enabled || connectedDevices == 0) {
        return;
    }
    
    // Refresh activity timestamp whenever we successfully push data to a client
    lastActivityMs = millis();
    updateNavigationData();
    updateWindData();
    updateAutopilotData();
    // Check for zombie connections after pushing data
    checkZombieConnections();
}

void BLEManager::updateTask(void* parameter) {
    BLEManager* manager = static_cast<BLEManager*>(parameter);
    
    while (true) {
        manager->update();
        vTaskDelay(pdMS_TO_TICKS(BLE_UPDATE_INTERVAL_MS));
    }
}

void BLEManager::updateNavigationData() {
    if (!pNavDataChar) {
        return;
    }
    
    String json = buildNavigationJSON();
    pNavDataChar->setValue(json.c_str());
    pNavDataChar->notify();
}

void BLEManager::updateWindData() {
    if (!pWindDataChar) {
        return;
    }
    
    String json = buildWindJSON();
    pWindDataChar->setValue(json.c_str());
    pWindDataChar->notify();
}

void BLEManager::updateAutopilotData() {
    if (!pAutopilotDataChar) {
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
    
    doc["lat"] = gps.position.lat.value;
    doc["lon"] = gps.position.lon.value;
    doc["sog"] = gps.sog.value;
    doc["cog"] = gps.cog.value;
    doc["stw"] = speed.stw.value;
    doc["hdg_mag"] = heading.magnetic.value;
    doc["hdg_true"] = heading.true_heading.value;
    doc["depth"] = depth.below_transducer.value;
    
    String output;
    serializeJson(doc, output);
    return output;
}

String BLEManager::buildWindJSON() {
    JsonDocument doc;
    
    WindData wind = boatState->getWind();
    
    doc["aws"] = wind.aws.value;
    doc["awa"] = wind.awa.value;
    doc["tws"] = wind.tws.value;
    doc["twa"] = wind.twa.value;
    doc["twd"] = wind.twd.value;
    
    String output;
    serializeJson(doc, output);
    return output;
}

String BLEManager::buildAutopilotJSON() {
    JsonDocument doc;
    
    AutopilotData ap = boatState->getAutopilot();
    
    doc["mode"] = ap.mode;
    doc["status"] = ap.status;
    doc["heading_target"] = ap.heading_target.value;
    doc["wind_target"] = ap.wind_angle_target.value;
    doc["rudder"] = ap.rudder_angle.value;
    doc["locked_heading"] = ap.locked_heading.value;
    
    String output;
    serializeJson(doc, output);
    return output;
}

// ============================================================
// checkZombieConnections
//
// The ESP32 BLE stack does not always fire onDisconnect when the Android
// client calls gatt.close() without a prior gatt.disconnect(). The result is
// a "zombie" connection: connectedDevices > 0 but no data flows and no new
// client can connect.
//
// Detection: if connectedDevices > 0 but we have received no notify
// acknowledgement for ZOMBIE_TIMEOUT_MS, assume the connection is dead and
// force-close it via esp_ble_gap_disconnect on all known peer addresses.
// ============================================================
void BLEManager::checkZombieConnections() {
    if (connectedDevices == 0) return;
    if (lastActivityMs == 0) return;
    if ((millis() - lastActivityMs) < ZOMBIE_TIMEOUT_MS) return;

    Serial.printf("[BLE] ⚠ Zombie connection detected (%u device(s), no activity for %u ms)\n",
                  connectedDevices, millis() - lastActivityMs);

    // getPeerDevices(true) returns all connected clients as a map of
    // conn_id → conn_status_t. We use the peer BDA address to call
    // esp_ble_gap_disconnect(), which is the correct API-level way to
    // force-close a connection from the server side without needing a
    // gatts_if handle.
    if (pServer) {
        std::map<uint16_t, conn_status_t> peers = pServer->getPeerDevices(true);
        if (peers.empty()) {
            // Table is already empty — counter was just out of sync
            Serial.println("[BLE] Peer table empty — resetting counter");
        } else {
            for (auto& kv : peers) {
                conn_status_t& cs = kv.second;
                // peer_device is a NimBLEConnInfo* / BLEClient* depending on
                // the Arduino BLE version. The safest cross-version approach
                // is to disconnect via the server's disconnect(conn_id) helper.
                uint16_t connId = kv.first;
                Serial.printf("[BLE] Forcing close for conn_id=%u\n", connId);
                pServer->disconnect(connId);
            }
        }
    }

    // Reset regardless — if the stack ignored our disconnect request we still
    // want advertising to restart so a fresh connection is possible.
    connectedDevices = 0;
    lastActivityMs   = 0;

    if (config.enabled) {
        Serial.println("[BLE] Restarting advertising after zombie cleanup");
        // Small yield to let the stack process the disconnect before advertising
        vTaskDelay(pdMS_TO_TICKS(200));
        startAdvertising();
    }
}