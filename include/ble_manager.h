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
#include "seatalk_manager.h"

// Forward declarations for Admin service dependencies
class ConfigManager;
class WiFiManager;

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
// AutopilotCommand — kept for backwards compatibility / logging,
// but BLEManager now dispatches directly via SeatalkManager.
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
// Forward declarations
// ============================================================
class BLEManager;

// ============================================================
// Server callbacks — NimBLE 2.x API
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
// Autopilot command write callback — NimBLE 2.x API
// ============================================================
class AutopilotCmdCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit AutopilotCmdCallbacks(BLEManager* mgr) : manager(mgr) {}

    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override;

private:
    BLEManager* manager;
};

// ============================================================
// Admin command write callback — NimBLE 2.x API
//
// Accepted JSON commands:
//   { "command": "restart" }
//   { "command": "wifi_sta", "ssid": "MyNet", "password": "secret" }
//   { "command": "wifi_ap",  "ssid": "MyAP",  "password": "secret" }
// ============================================================
class AdminCmdCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit AdminCmdCallbacks(BLEManager* mgr) : manager(mgr) {}

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

    /**
     * @param config        BLE configuration (name, PIN, enabled flag).
     * @param state         Shared boat state for data notifications.
     * @param stMgr         SeatalkManager for AP commands (may be nullptr).
     * @param configMgr     ConfigManager for WiFi config persistence (may be nullptr).
     * @param wifiMgr       WiFiManager for applying WiFi changes (may be nullptr).
     */
    void init(const BLEConfig& config, BoatState* state,
              SeatalkManager* stMgr      = nullptr,
              ConfigManager*  configMgr  = nullptr,
              WiFiManager*    wifiMgr    = nullptr);
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

    friend class MarineServerCallbacks;
    friend class AutopilotCmdCallbacks;
    friend class AdminCmdCallbacks;

private:
    // ── NimBLE objects ──────────────────────────────────────────

    NimBLEServer*      pServer;
    NimBLEAdvertising* pAdvertising;

    // Navigation service
    NimBLEService*        pNavService;
    NimBLECharacteristic* pNavDataChar;

    // Wind service
    NimBLEService*        pWindService;
    NimBLECharacteristic* pWindDataChar;

    // Autopilot service
    NimBLEService*        pAutopilotService;
    NimBLECharacteristic* pAutopilotDataChar;
    NimBLECharacteristic* pAutopilotCmdChar;

    // Sail Performance service
    NimBLEService*        pPerformanceService;
    NimBLECharacteristic* pPerformanceDataChar;

    // Admin service
    NimBLEService*        pAdminService;
    NimBLECharacteristic* pAdminDataChar;   // READ + NOTIFY
    NimBLECharacteristic* pAdminCmdChar;    // WRITE

    MarineServerCallbacks* serverCallbacks;
    AutopilotCmdCallbacks* autopilotCmdCallbacks;
    AdminCmdCallbacks*     adminCmdCallbacks;

    // ── State ───────────────────────────────────────────────────
    BLEConfig       config;
    BoatState*      boatState;
    SeatalkManager* seatalkManager;
    ConfigManager*  configManager;
    WiFiManager*    wifiManager;

    bool           initialized;
    bool           advertising;
    uint32_t       connectedDevices;

    // ── FreeRTOS update task ────────────────────────────────────
    TaskHandle_t   updateTaskHandle;
    static void    updateTask(void* param);

    // ── Internal helpers ────────────────────────────────────────
    void setupSecurity();
    void setupServices();
    void startAdvertising();
    void stopAdvertising();

    void updateNavData();
    void updateWindData();
    void updateAutopilotData();
    void updatePerformanceData();
    void updateAdminData();

    String buildNavJSON();
    String buildWindJSON();
    String buildAutopilotJSON();
    String buildPerformanceJSON();
    String buildAdminJSON();
};

#endif // BLE_MANAGER_H