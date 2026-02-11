#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "types.h"
#include "config.h"

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    
    void init();
    
    // WiFi configuration
    bool getWiFiConfig(WiFiConfig& config);
    bool setWiFiConfig(const WiFiConfig& config);
    
    // Serial configuration
    bool getSerialConfig(UARTConfig& config);
    bool setSerialConfig(const UARTConfig& config);
    
    // Factory reset
    void factoryReset();
    
private:
    Preferences preferences;
    bool initialized;
};

#endif // CONFIG_MANAGER_H
