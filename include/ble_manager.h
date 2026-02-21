#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

// ============================================================
// NimBLE-Arduino 1.4.x
//
// NOTE: This file targets the 1.4.x API which is what PlatformIO
// installs despite requesting ^2.3.7. To force 2.x:
//   1. Delete .pio/libdeps/esp32s3/NimBLE-Arduino
//   2. Run: pio pkg install --library "h2zero/NimBLE-Arduino@2.3.7"
//   3. pio run --target clean && pio run
//
// Key advantage over Bluedroid: onDisconnect() fires reliably
// even after Android gatt.close() without prior disconnect(),
// eliminating the zombie connection problem.
// ============================================================

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLESecurity.h>
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
// Server callbacks — NimBLE 1.4.x API
//
// In 1.4.x the callbacks do NOT receive NimBLEConnInfo.
// Access peer list via pServer->getPeerDevices() if needed.
//
// IMPORTANT: NimBLE 1.4.x DOES fire onDisconnect() reliably
// even when Android drops the link without a proper disconnect.
// This is the core fix for the zombie connection problem.
// ============================================================
class MarineServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit MarineServerCallbacks(BLEManager* mgr) : manager(mgr) {}

    // 1.4.x signature — no NimBLEConnInfo parameter
    void onConnect(NimBLEServer* pServer) override;
    void onDisconnect(NimBLEServer* pServer) override;

private:
    BLEManager* manager;
};

// ============================================================
// Characteristic write callback — NimBLE 1.4.x API
// ============================================================
class AutopilotCmdCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit AutopilotCmdCallbacks(BLEManager* mgr) : manager(mgr) {}

    // 1.4.x signature — no NimBLEConnInfo parameter
    void onWrite(NimBLECharacteristic* pChar) override;

private:
    BLEManager* manager;
};

// ============================================================
// Security callbacks — NimBLE 1.4.x API
//
// In 1.4.x NimBLESecurityCallbacks still exists (removed in 2.x).
// Signatures match the 1.4.x virtual methods exactly.
// ============================================================
class MarineSecurityCallbacks : public NimBLESecurityCallbacks {
public:
    explicit MarineSecurityCallbacks(BLEManager* mgr) : manager(mgr) {}

    // Called to get the static passkey to display/use
    uint32_t onPassKeyRequest() override;

    // Called when the peer sends its passkey — log only
    void onPassKeyNotify(uint32_t pass_key) override;

    // Called for numeric comparison — return true to confirm
    bool onConfirmPIN(uint32_t pass_key) override;

    // Called when a security request arrives from the client
    bool onSecurityRequest() override;

    // Called when pairing completes (success or failure)
    void onAuthenticationComplete(ble_gap_conn_desc* desc) override;

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
    friend class MarineSecurityCallbacks;

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

    MarineServerCallbacks*   serverCallbacks;
    AutopilotCmdCallbacks*   autopilotCmdCallbacks;
    MarineSecurityCallbacks* securityCallbacks;

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
