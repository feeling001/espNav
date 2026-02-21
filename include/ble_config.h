#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

#include <Arduino.h>

// ============================================================
// BLE Device Configuration
// ============================================================
#define BLE_DEVICE_NAME         "MarineGateway"
#define BLE_DEFAULT_PIN         "123456"
#define BLE_UPDATE_INTERVAL_MS  1000    // 1 Hz update rate

// ============================================================
// Service UUIDs  (custom base: 4D475743-xxxx-4E41-5649-474154494F4E)
// ============================================================
#define BLE_SERVICE_NAVIGATION_UUID     "4d475743-0001-4e41-5649-474154494f4e"
#define BLE_CHAR_NAV_DATA_UUID          "4d475743-0101-4e41-5649-474154494f4e"

#define BLE_SERVICE_WIND_UUID           "4d475743-0002-4e41-5649-474154494f4e"
#define BLE_CHAR_WIND_DATA_UUID         "4d475743-0201-4e41-5649-474154494f4e"

#define BLE_SERVICE_AUTOPILOT_UUID      "4d475743-0003-4e41-5649-474154494f4e"
#define BLE_CHAR_AUTOPILOT_DATA_UUID    "4d475743-0301-4e41-5649-474154494f4e"
#define BLE_CHAR_AUTOPILOT_CMD_UUID     "4d475743-0302-4e41-5649-474154494f4e"

// ============================================================
// Limits & task config
// ============================================================
#define BLE_MAX_CONNECTIONS     3
#define BLE_SECURITY_ENABLED    true
#define BLE_TASK_STACK_SIZE     4096
#define BLE_TASK_PRIORITY       3

#endif // BLE_CONFIG_H
