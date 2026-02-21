#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

// ============================================================
// NimBLE-Arduino 2.x API
//
// Breaking changes vs 1.4.x:
//  - NimBLESecurity.h removed → security configured via NimBLEDevice static methods
//  - NimBLESecurityCallbacks removed → use NimBLEServerCallbacks::onAuthenticationComplete
//  - onConnect/onDisconnect now receive NimBLEConnInfo&
//  - onWrite now receives NimBLEConnInfo&
//  - onAuthenticationComplete receives NimBLEConnInfo&
// ============================================================

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "ble_config.h"
#include "boat_state.h"

// ============================================================
// BLE Configuration
// ============================================================
struct BLEConfig {
    bool enabled;
    char device_name[32];
    char pin_code[7];   // 6 digits + null terminator

    BLEConfig() : enabled(false) {
        strncpy(device_name, BLE_DEVICE_NAME, sizeof(device_name) - 1);
        device_name[sizeof(device_name) - 1] = '\0';
        strncpy(pin_code, BLE_DEFAULT_PIN, sizeof(pin_code) - 1);
        pin_code[sizeof(pin_code) - 1] = '\0';
    }
};

// ============================================================
// Autopilot command
// ============================================================
struct AutopilotCommand {
    enum Type {
        NONE = 0, ENABLE, DISABLE,
        ADJUST_PLUS_10, ADJUST_MINUS_10,
        ADJUST_PLUS_1,  ADJUST_MINUS_1
    };
    Type     type;
    uint32_t timestamp;
    AutopilotCommand() : type(NONE), timestamp(0) {}
};

// ============================================================
// Forward declaration
// ============================================================
class BLEManager;

// ============================================================
// Server callbacks — NimBLE 2.x API
//
// onConnect / onDisconnect now receive NimBLEConnInfo&.
// onAuthenticationComplete replaces NimBLESecurityCallbacks.
// ============================================================
class MarineServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit MarineServerCallbacks(BLEManager* mgr) : manager(mgr) {}

    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override;

private:
    BLEManager* manager;
};

// ============================================================
// Characteristic write callback — NimBLE 2.x API
// onWrite now receives NimBLEConnInfo&
// ============================================================
class AutopilotCmdCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit AutopilotCmdCallbacks(BLEManager* mgr) : manager(mgr) {}

    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override;

private:
    BLEManager* manager;
};

// ============================================================
// BLEManager
// ============================================================
class BLEManager {
public:
    BLEManager();
    ~BLEManager();

    void init(const BLEConfig& config, BoatState* state);
    void start();
    void stop();
    void update();

    bool      isEnabled()           const { return config.enabled; }
    void      setEnabled(bool en);
    void      setDeviceName(const char* name);
    void      setPinCode(const char* pin);
    BLEConfig getConfig()           const { return config; }
    bool      isAdvertising()       const { return advertising; }
    uint32_t  getConnectedDevices() const { return connectedDevices; }

    bool             hasAutopilotCommand();
    AutopilotCommand getAutopilotCommand();

    friend class MarineServerCallbacks;
    friend class AutopilotCmdCallbacks;

private:
    // NimBLE objects
    NimBLEServer*         pServer;
    NimBLEAdvertising*    pAdvertising;

    NimBLEService*        pNavService;
    NimBLECharacteristic* pNavDataChar;

    NimBLEService*        pWindService;
    NimBLECharacteristic* pWindDataChar;

    NimBLEService*        pAutopilotService;
    NimBLECharacteristic* pAutopilotDataChar;
    NimBLECharacteristic* pAutopilotCmdChar;

    MarineServerCallbacks*  serverCallbacks;
    AutopilotCmdCallbacks*  autopilotCmdCallbacks;

    // State
    BLEConfig  config;
    BoatState* boatState;
    bool       initialized;
    bool       advertising;
    uint32_t   connectedDevices;

    // Autopilot queue
    AutopilotCommand  pendingCommand;
    SemaphoreHandle_t commandMutex;

    // FreeRTOS update task
    TaskHandle_t   updateTaskHandle;
    static void    updateTask(void* param);

    // Internal helpers
    void setupSecurity();
    void setupServices();
    void startAdvertising();
    void stopAdvertising();

    void updateNavData();
    void updateWindData();
    void updateAutopilotData();

    String buildNavJSON();
    String buildWindJSON();
    String buildAutopilotJSON();
};

#endif // BLE_MANAGER_H
