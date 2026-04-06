#include "config_manager.h"
#include "functions.h"
#include <Arduino.h>

ConfigManager::ConfigManager() {
}

void ConfigManager::init() {
    serialPrintf("[Config] Initializing Config Manager\n");
    
    // Open NVS namespace in read-write mode
    if (!nvs.begin("marine_gw", false)) {
        serialPrintf("[Config] ✗ Failed to open NVS\n");
        return;
    }
    
    serialPrintf("[Config] ✓ NVS initialized\n");
}

bool ConfigManager::getWiFiConfig(WiFiConfig& config) {
    // Load STA configuration
    String ssid     = nvs.isKey("wifi_ssid") ? nvs.getString("wifi_ssid", "") : "";
    String password = nvs.isKey("wifi_pass") ? nvs.getString("wifi_pass", "") : "";

    uint8_t mode = nvs.getUChar("wifi_mode", 0);
    
    strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid) - 1);
    config.ssid[sizeof(config.ssid) - 1] = '\0';
    
    strncpy(config.password, password.c_str(), sizeof(config.password) - 1);
    config.password[sizeof(config.password) - 1] = '\0';
    
    config.mode = mode;
    
    // Load AP configuration
    String apSSID       = nvs.isKey("wifi_ap_ssid") ? nvs.getString("wifi_ap_ssid", "") : "";
    String apPassword   = nvs.isKey("wifi_ap_pass") ? nvs.getString("wifi_ap_pass", "") : "";
    
    strncpy(config.ap_ssid, apSSID.c_str(), sizeof(config.ap_ssid) - 1);
    config.ap_ssid[sizeof(config.ap_ssid) - 1] = '\0';
    
    strncpy(config.ap_password, apPassword.c_str(), sizeof(config.ap_password) - 1);
    config.ap_password[sizeof(config.ap_password) - 1] = '\0';
    
    serialPrintf("[Config] WiFi config loaded from NVS\n");
    serialPrintf("[Config]   Mode: %s\n", mode == 0 ? "STA" : "AP");
    if (mode == 0 && strlen(config.ssid) > 0) {
        serialPrintf("[Config]   STA SSID: %s\n", config.ssid);
    }
    if (strlen(config.ap_ssid) > 0) {
        serialPrintf("[Config]   AP SSID: %s\n", config.ap_ssid);
    }
    
    return true;
}

bool ConfigManager::setWiFiConfig(const WiFiConfig& config) {
    serialPrintf("[Config] Saving WiFi config to NVS\n");
    
    // Save STA configuration
    nvs.putString("wifi_ssid", config.ssid);
    nvs.putString("wifi_pass", config.password);
    nvs.putUChar("wifi_mode", config.mode);
    
    // Save AP configuration
    nvs.putString("wifi_ap_ssid", config.ap_ssid);
    nvs.putString("wifi_ap_pass", config.ap_password);
    
    serialPrintf("[Config]   Mode: %s\n", config.mode == 0 ? "STA" : "AP");
    if (config.mode == 0 && strlen(config.ssid) > 0) {
        serialPrintf("[Config]   STA SSID: %s\n", config.ssid);
    }
    if (strlen(config.ap_ssid) > 0) {
        serialPrintf("[Config]   AP SSID: %s\n", config.ap_ssid);
    }
    if (strlen(config.ap_password) > 0) {
        serialPrintf("[Config]   AP Password: %s\n", 
                     strlen(config.ap_password) >= 8 ? "***" : "[too short, will use default]");
    }
    
    serialPrintf("[Config] ✓ WiFi config saved\n");
    return true;
}

bool ConfigManager::getSerialConfig(UARTConfig& config) {
    config.baudRate = nvs.getUInt("serial_baud", 38400);
    config.dataBits = nvs.getUChar("serial_data", 8);
    config.parity = nvs.getUChar("serial_parity", 0);
    config.stopBits = nvs.getUChar("serial_stop", 1);
    
    #ifdef DEBUG
    serialPrintf("[Config] Serial config loaded from NVS\n");
    serialPrintf("[Config]   Baud: %u\n", config.baudRate);
    serialPrintf("[Config]   Data: %u\n", config.dataBits);
    serialPrintf("[Config]   Parity: %u\n", config.parity);
    serialPrintf("[Config]   Stop: %u\n", config.stopBits);
    #endif
    
    return true;
}

bool ConfigManager::setSerialConfig(const UARTConfig& config) {
    #ifdef DEBUG
    serialPrintf("[Config] Saving Serial config to NVS\n");
    #endif

    nvs.putUInt("serial_baud", config.baudRate);
    nvs.putUChar("serial_data", config.dataBits);
    nvs.putUChar("serial_parity", config.parity);
    nvs.putUChar("serial_stop", config.stopBits);
    
    #ifdef DEBUG
    serialPrintf("[Config]   Baud: %u\n", config.baudRate);
    serialPrintf("[Config]   Data: %u\n", config.dataBits);
    serialPrintf("[Config]   Parity: %u\n", config.parity);
    serialPrintf("[Config]   Stop: %u\n", config.stopBits);
    
    serialPrintf("[Config] ✓ Serial config saved\n");
    #endif

    return true;
}



bool ConfigManager::getBLEConfig(BLEConfigData& config) {
    config.enabled = nvs.getBool("ble_enabled", false);

    
    String deviceName = nvs.isKey("ble_name") ? nvs.getString("ble_name", "MarineGateway") : "MarineGateway";
    String pinCode    = nvs.isKey("ble_pin")  ? nvs.getString("ble_pin",  "123456")        : "123456";

    
    strncpy(config.device_name, deviceName.c_str(), sizeof(config.device_name) - 1);
    config.device_name[sizeof(config.device_name) - 1] = '\0';
    
    
    strncpy(config.pin_code, pinCode.c_str(), sizeof(config.pin_code) - 1);
    config.pin_code[sizeof(config.pin_code) - 1] = '\0';
    
    serialPrintf("[Config] BLE config loaded from NVS\n");
    serialPrintf("[Config]   Enabled: %s\n", config.enabled ? "Yes" : "No");
    serialPrintf("[Config]   Device Name: %s\n", config.device_name);
    serialPrintf("[Config]   PIN Code: %s\n", config.pin_code);
    
    return true;
}

bool ConfigManager::setBLEConfig(const BLEConfigData& config) {
    serialPrintf("[Config] Saving BLE config to NVS\n");
    
    nvs.putBool("ble_enabled", config.enabled);
    nvs.putString("ble_name", config.device_name);
    nvs.putString("ble_pin", config.pin_code);
    
    serialPrintf("[Config]   Enabled: %s\n", config.enabled ? "Yes" : "No");
    serialPrintf("[Config]   Device Name: %s\n", config.device_name);
    serialPrintf("[Config]   PIN Code: %s\n", config.pin_code);
    
    serialPrintf("[Config] ✓ BLE config saved\n");
    return true;
}

void ConfigManager::factoryReset() {
    serialPrintf("[Config] Performing factory reset...\n");
    
    nvs.clear();
    
    // Set default values
    WiFiConfig defaultWiFi;
    UARTConfig defaultSerial;
    BLEConfigData defaultBLE;
    
    setWiFiConfig(defaultWiFi);
    setSerialConfig(defaultSerial);
    setBLEConfig(defaultBLE);
    
    serialPrintf("[Config] ✓ Factory reset complete\n");
}
