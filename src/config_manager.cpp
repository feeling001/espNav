#include "config_manager.h"

ConfigManager::ConfigManager() : initialized(false) {}

ConfigManager::~ConfigManager() {
    if (initialized) {
        preferences.end();
    }
}

void ConfigManager::init() {
    if (initialized) {
        return;
    }
    
    preferences.begin(NVS_NAMESPACE, false);
    initialized = true;
    
    Serial.println("[Config] Configuration manager initialized");
}

bool ConfigManager::getWiFiConfig(WiFiConfig& config) {
    if (!initialized) {
        Serial.println("[Config] Error: Not initialized");
        return false;
    }
    
    String ssid = preferences.getString("wifi_ssid", "");
    String pass = preferences.getString("wifi_pass", "");
    uint8_t mode = preferences.getUChar("wifi_mode", 0);
    
    strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid) - 1);
    config.ssid[sizeof(config.ssid) - 1] = '\0';
    
    strncpy(config.password, pass.c_str(), sizeof(config.password) - 1);
    config.password[sizeof(config.password) - 1] = '\0';
    
    config.mode = mode;
    
    Serial.printf("[Config] Loaded WiFi config: SSID='%s', Mode=%d\n", 
                  config.ssid, config.mode);
    
    return true;
}

bool ConfigManager::setWiFiConfig(const WiFiConfig& config) {
    if (!initialized) {
        Serial.println("[Config] Error: Not initialized");
        return false;
    }
    
    preferences.putString("wifi_ssid", config.ssid);
    preferences.putString("wifi_pass", config.password);
    preferences.putUChar("wifi_mode", config.mode);
    
    Serial.printf("[Config] Saved WiFi config: SSID='%s', Mode=%d\n", 
                  config.ssid, config.mode);
    
    return true;
}

bool ConfigManager::getSerialConfig(SerialConfig& config) {
    if (!initialized) {
        Serial.println("[Config] Error: Not initialized");
        return false;
    }
    
    config.baudRate = preferences.getUInt("serial_baud", 38400);
    config.dataBits = preferences.getUChar("serial_data", 8);
    config.parity = preferences.getUChar("serial_parity", 0);
    config.stopBits = preferences.getUChar("serial_stop", 1);
    
    Serial.printf("[Config] Loaded Serial config: Baud=%u, Data=%u, Parity=%u, Stop=%u\n",
                  config.baudRate, config.dataBits, config.parity, config.stopBits);
    
    return true;
}

bool ConfigManager::setSerialConfig(const SerialConfig& config) {
    if (!initialized) {
        Serial.println("[Config] Error: Not initialized");
        return false;
    }
    
    preferences.putUInt("serial_baud", config.baudRate);
    preferences.putUChar("serial_data", config.dataBits);
    preferences.putUChar("serial_parity", config.parity);
    preferences.putUChar("serial_stop", config.stopBits);
    
    Serial.printf("[Config] Saved Serial config: Baud=%u, Data=%u, Parity=%u, Stop=%u\n",
                  config.baudRate, config.dataBits, config.parity, config.stopBits);
    
    return true;
}

void ConfigManager::factoryReset() {
    if (!initialized) {
        Serial.println("[Config] Error: Not initialized");
        return;
    }
    
    preferences.clear();
    
    // Set defaults
    WiFiConfig defaultWiFi;
    SerialConfig defaultSerial;
    
    setWiFiConfig(defaultWiFi);
    setSerialConfig(defaultSerial);
    
    Serial.println("[Config] Factory reset complete");
}
