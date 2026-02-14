#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "ble_config.h"
#include "boat_state.h"

// BLE Configuration structure
struct BLEConfig {
    bool enabled;
    char device_name[32];
    char pin_code[7];  // 6 digits + null terminator
    
    BLEConfig() : enabled(false) {
        strncpy(device_name, BLE_DEVICE_NAME, sizeof(device_name) - 1);
        device_name[sizeof(device_name) - 1] = '\0';
        strncpy(pin_code, BLE_DEFAULT_PIN, sizeof(pin_code) - 1);
        pin_code[sizeof(pin_code) - 1] = '\0';
    }
};

// Autopilot command structure
struct AutopilotCommand {
    enum Type {
        NONE = 0,
        ENABLE,
        DISABLE,
        ADJUST_PLUS_10,
        ADJUST_MINUS_10,
        ADJUST_PLUS_1,
        ADJUST_MINUS_1
    };
    
    Type type;
    uint32_t timestamp;
    
    AutopilotCommand() : type(NONE), timestamp(0) {}
};

// Forward declaration
class BLEManager;

// Custom BLE Server callbacks (renamed to avoid conflict with library class)
class CustomBLEServerCallbacks : public BLEServerCallbacks {
public:
    CustomBLEServerCallbacks(BLEManager* manager);
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
    
private:
    BLEManager* bleManager;
};

// Autopilot command characteristic callback
class AutopilotCommandCallbacks : public BLECharacteristicCallbacks {
public:
    AutopilotCommandCallbacks(BLEManager* manager);
    void onWrite(BLECharacteristic* pCharacteristic) override;
    
private:
    BLEManager* bleManager;
};

// Custom BLE Security callbacks (renamed to avoid conflict with library class)
class CustomBLESecurityCallbacks : public BLESecurityCallbacks {
public:
    CustomBLESecurityCallbacks(BLEManager* manager);
    
    uint32_t onPassKeyRequest() override;
    void onPassKeyNotify(uint32_t pass_key) override;
    bool onConfirmPIN(uint32_t pass_key) override;
    bool onSecurityRequest() override;
    void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) override;
    
private:
    BLEManager* bleManager;
};

class BLEManager {
public:
    BLEManager();
    ~BLEManager();
    
    void init(const BLEConfig& config, BoatState* state);
    void start();
    void stop();
    void update();
    
    // Configuration
    bool isEnabled() const { return config.enabled; }
    void setEnabled(bool enabled);
    void setDeviceName(const char* name);
    void setPinCode(const char* pin);
    BLEConfig getConfig() const { return config; }
    
    // Status
    bool isAdvertising() const { return advertising; }
    uint32_t getConnectedDevices() const { return connectedDevices; }
    
    // Autopilot commands
    bool hasAutopilotCommand();
    AutopilotCommand getAutopilotCommand();
    
    // Friend classes
    friend class CustomBLEServerCallbacks;
    friend class AutopilotCommandCallbacks;
    friend class CustomBLESecurityCallbacks;
    
private:
    // Core BLE objects
    BLEServer* pServer;
    BLEAdvertising* pAdvertising;
    
    // Services and characteristics
    BLEService* pNavigationService;
    BLECharacteristic* pNavDataChar;
    
    BLEService* pWindService;
    BLECharacteristic* pWindDataChar;
    
    BLEService* pAutopilotService;
    BLECharacteristic* pAutopilotDataChar;
    BLECharacteristic* pAutopilotCmdChar;
    
    // Callbacks (using renamed classes)
    CustomBLEServerCallbacks* serverCallbacks;
    AutopilotCommandCallbacks* autopilotCmdCallbacks;
    CustomBLESecurityCallbacks* securityCallbacks;
    
    // State
    BLEConfig config;
    BoatState* boatState;
    bool initialized;
    bool advertising;
    uint32_t connectedDevices;
    
    // Autopilot command queue
    AutopilotCommand pendingCommand;
    SemaphoreHandle_t commandMutex;
    
    // Update task
    TaskHandle_t updateTaskHandle;
    static void updateTask(void* parameter);
    
    // Helper methods
    void setupServices();
    void setupSecurity();
    void startAdvertising();
    void stopAdvertising();
    
    void updateNavigationData();
    void updateWindData();
    void updateAutopilotData();
    
    String buildNavigationJSON();
    String buildWindJSON();
    String buildAutopilotJSON();
};

#endif // BLE_MANAGER_H
