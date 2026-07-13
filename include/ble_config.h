#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

#include <Arduino.h>

// ============================================================
// BLE Device Configuration
// ============================================================
#define BLE_DEVICE_NAME         "MarineGateway"
#define BLE_DEFAULT_PIN         "123456"
#define BLE_UPDATE_INTERVAL_MS  2000    // 0.5 Hz update rate

// ============================================================
// Service UUIDs  (custom base: 4D475743-xxxx-4E41-5649-474154494F4E)
// MGWC = Marine Gateway Custom
// ============================================================

// Navigation Service
#define BLE_SERVICE_NAVIGATION_UUID     "4d475743-0001-4e41-5649-474154494f4e"
#define BLE_CHAR_NAV_DATA_UUID          "4d475743-0101-4e41-5649-474154494f4e"

// Wind Service
#define BLE_SERVICE_WIND_UUID           "4d475743-0002-4e41-5649-474154494f4e"
#define BLE_CHAR_WIND_DATA_UUID         "4d475743-0201-4e41-5649-474154494f4e"

// Autopilot Service
#define BLE_SERVICE_AUTOPILOT_UUID      "4d475743-0003-4e41-5649-474154494f4e"
#define BLE_CHAR_AUTOPILOT_DATA_UUID    "4d475743-0301-4e41-5649-474154494f4e"
#define BLE_CHAR_AUTOPILOT_CMD_UUID     "4d475743-0302-4e41-5649-474154494f4e"

// Sail Performance Service
// Exposes VMG, polar efficiency (%), and polar target STW (kn).
// Requires a polar file to be loaded on the device for polar_pct
// and target_stw to be valid; vmg is always computed when TWA is available.
#define BLE_SERVICE_PERFORMANCE_UUID    "4d475743-0004-4e41-5649-474154494f4e"
#define BLE_CHAR_PERFORMANCE_DATA_UUID  "4d475743-0401-4e41-5649-474154494f4e"

// ============================================================
// Admin Service
// Provides system diagnostics (uptime, datetime) and remote
// administration commands (restart, WiFi configuration).
// ============================================================
#define BLE_SERVICE_ADMIN_UUID          "4d475743-0005-4e41-5649-474154494f4e"

/// READ + NOTIFY — system status: uptime_s, datetime (unix timestamp), wifi state, ip
#define BLE_CHAR_ADMIN_DATA_UUID        "4d475743-0501-4e41-5649-474154494f4e"

/// WRITE — administration commands (JSON):
///   { "command": "restart" }
///   { "command": "wifi_sta", "ssid": "...", "password": "..." }
///   { "command": "wifi_ap",  "ssid": "...", "password": "..." }
#define BLE_CHAR_ADMIN_CMD_UUID         "4d475743-0502-4e41-5649-474154494f4e"

// ============================================================
// Alarm Service
// Exposes alarm configuration + runtime state (depth, AIS proximity,
// GPS lost) and accepts configuration / acknowledge / beep commands.
// ============================================================
#define BLE_SERVICE_ALARM_UUID          "4d475743-0006-4e41-5649-474154494f4e"

/// READ + NOTIFY — alarm config + runtime state (see BLE_Client_Documentation.md)
#define BLE_CHAR_ALARM_DATA_UUID        "4d475743-0601-4e41-5649-474154494f4e"

/// WRITE — alarm commands (JSON):
///   { "command": "set_config", "depth_enabled": true, "depth_threshold_m": 2.0,
///     "ais_enabled": true, "ais_distance_nm": 1.0, "own_mmsi": 123456789,
///     "gps_lost_enabled": true, "gps_lost_timeout_s": 10, "alarms_enabled": true }
///   { "command": "ack" }
///   { "command": "beep_on" }
///   { "command": "beep_off" }
#define BLE_CHAR_ALARM_CMD_UUID         "4d475743-0602-4e41-5649-474154494f4e"

// ============================================================
// Limits & task config
// ============================================================
#define BLE_MAX_CONNECTIONS     3
#define BLE_SECURITY_ENABLED    true
#define BLE_TASK_STACK_SIZE     4096
#define BLE_TASK_PRIORITY       3

#endif // BLE_CONFIG_H