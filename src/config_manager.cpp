#include "config_manager.h"
#include <Arduino.h>

ConfigManager::ConfigManager() {
}

void ConfigManager::init() {
    Serial.println("[Config] Initializing Config Manager");
    
    // Open NVS namespace in read-write mode
    if (!nvs.begin("marine_gw", false)) {
        Serial.println("[Config] ✗ Failed to open NVS");
        return;
    }
    
    Serial.println("[Config] ✓ NVS initialized");
}

bool ConfigManager::getWiFiConfig(WiFiConfig& config) {
    // Load STA configuration
    String ssid = nvs.getString("wifi_ssid", "");
    String password = nvs.getString("wifi_pass", "");
    uint8_t mode = nvs.getUChar("wifi_mode", 0);
    
    strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid) - 1);
    config.ssid[sizeof(config.ssid) - 1] = '\0';
    
    strncpy(config.password, password.c_str(), sizeof(config.password) - 1);
    config.password[sizeof(config.password) - 1] = '\0';
    
    config.mode = mode;
    
    // Load AP configuration
    String apSSID = nvs.getString("wifi_ap_ssid", "");
    String apPassword = nvs.getString("wifi_ap_pass", "");
    
    strncpy(config.ap_ssid, apSSID.c_str(), sizeof(config.ap_ssid) - 1);
    config.ap_ssid[sizeof(config.ap_ssid) - 1] = '\0';
    
    strncpy(config.ap_password, apPassword.c_str(), sizeof(config.ap_password) - 1);
    config.ap_password[sizeof(config.ap_password) - 1] = '\0';
    
    Serial.println("[Config] WiFi config loaded from NVS");
    Serial.printf("[Config]   Mode: %s\n", mode == 0 ? "STA" : "AP");
    if (mode == 0 && strlen(config.ssid) > 0) {
        Serial.printf("[Config]   STA SSID: %s\n", config.ssid);
    }
    if (strlen(config.ap_ssid) > 0) {
        Serial.printf("[Config]   AP SSID: %s\n", config.ap_ssid);
    }
    
    return true;
}

bool ConfigManager::setWiFiConfig(const WiFiConfig& config) {
    Serial.println("[Config] Saving WiFi config to NVS");
    
    // Save STA configuration
    nvs.putString("wifi_ssid", config.ssid);
    nvs.putString("wifi_pass", config.password);
    nvs.putUChar("wifi_mode", config.mode);
    
    // Save AP configuration
    nvs.putString("wifi_ap_ssid", config.ap_ssid);
    nvs.putString("wifi_ap_pass", config.ap_password);
    
    Serial.printf("[Config]   Mode: %s\n", config.mode == 0 ? "STA" : "AP");
    if (config.mode == 0 && strlen(config.ssid) > 0) {
        Serial.printf("[Config]   STA SSID: %s\n", config.ssid);
    }
    if (strlen(config.ap_ssid) > 0) {
        Serial.printf("[Config]   AP SSID: %s\n", config.ap_ssid);
    }
    if (strlen(config.ap_password) > 0) {
        Serial.printf("[Config]   AP Password: %s\n", 
                     strlen(config.ap_password) >= 8 ? "***" : "[too short, will use default]");
    }
    
    Serial.println("[Config] ✓ WiFi config saved");
    return true;
}

bool ConfigManager::getSerialConfig(UARTConfig& config) {
    config.baudRate = nvs.getUInt("serial_baud", 38400);
    config.dataBits = nvs.getUChar("serial_data", 8);
    config.parity = nvs.getUChar("serial_parity", 0);
    config.stopBits = nvs.getUChar("serial_stop", 1);
    
    #ifdef DEBUG
    Serial.println("[Config] Serial config loaded from NVS");
    Serial.printf("[Config]   Baud: %u\n", config.baudRate);
    Serial.printf("[Config]   Data: %u\n", config.dataBits);
    Serial.printf("[Config]   Parity: %u\n", config.parity);
    Serial.printf("[Config]   Stop: %u\n", config.stopBits);
    #endif
    
    return true;
}

bool ConfigManager::setSerialConfig(const UARTConfig& config) {
    #ifdef DEBUG
    Serial.println("[Config] Saving Serial config to NVS");
    #endif

    nvs.putUInt("serial_baud", config.baudRate);
    nvs.putUChar("serial_data", config.dataBits);
    nvs.putUChar("serial_parity", config.parity);
    nvs.putUChar("serial_stop", config.stopBits);
    
    #ifdef DEBUG
    Serial.printf("[Config]   Baud: %u\n", config.baudRate);
    Serial.printf("[Config]   Data: %u\n", config.dataBits);
    Serial.printf("[Config]   Parity: %u\n", config.parity);
    Serial.printf("[Config]   Stop: %u\n", config.stopBits);
    
    Serial.println("[Config] ✓ Serial config saved");
    #endif

    return true;
}

bool ConfigManager::getBLEConfig(BLEConfigData& config) {
    config.enabled = nvs.getBool("ble_enabled", false);
    
    String deviceName = nvs.getString("ble_name", "MarineGateway");
    strncpy(config.device_name, deviceName.c_str(), sizeof(config.device_name) - 1);
    config.device_name[sizeof(config.device_name) - 1] = '\0';
    
    String pinCode = nvs.getString("ble_pin", "123456");
    strncpy(config.pin_code, pinCode.c_str(), sizeof(config.pin_code) - 1);
    config.pin_code[sizeof(config.pin_code) - 1] = '\0';
    
    Serial.println("[Config] BLE config loaded from NVS");
    Serial.printf("[Config]   Enabled: %s\n", config.enabled ? "Yes" : "No");
    Serial.printf("[Config]   Device Name: %s\n", config.device_name);
    Serial.printf("[Config]   PIN Code: %s\n", config.pin_code);
    
    return true;
}

bool ConfigManager::setBLEConfig(const BLEConfigData& config) {
    Serial.println("[Config] Saving BLE config to NVS");
    
    nvs.putBool("ble_enabled", config.enabled);
    nvs.putString("ble_name", config.device_name);
    nvs.putString("ble_pin", config.pin_code);
    
    Serial.printf("[Config]   Enabled: %s\n", config.enabled ? "Yes" : "No");
    Serial.printf("[Config]   Device Name: %s\n", config.device_name);
    Serial.printf("[Config]   PIN Code: %s\n", config.pin_code);
    
    Serial.println("[Config] ✓ BLE config saved");
    return true;
}

void ConfigManager::factoryReset() {
    Serial.println("[Config] Performing factory reset...");
    
    nvs.clear();
    
    // Set default values
    WiFiConfig defaultWiFi;
    UARTConfig defaultSerial;
    BLEConfigData defaultBLE;
    
    setWiFiConfig(defaultWiFi);
    setSerialConfig(defaultSerial);
    setBLEConfig(defaultBLE);
    
    Serial.println("[Config] ✓ Factory reset complete");
}
