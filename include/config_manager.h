#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Preferences.h>
#include "types.h"

class ConfigManager {
public:
    ConfigManager();
    
    void init();
    
    // WiFi configuration
    bool getWiFiConfig(WiFiConfig& config);
    bool setWiFiConfig(const WiFiConfig& config);
    
    // Serial configuration
    bool getSerialConfig(UARTConfig& config);
    bool setSerialConfig(const UARTConfig& config);
    
    // BLE configuration
    bool getBLEConfig(BLEConfigData& config);
    bool setBLEConfig(const BLEConfigData& config);
    
    // Factory reset
    void factoryReset();
    
private:
    Preferences nvs;
};

#endif // CONFIG_MANAGER_H
