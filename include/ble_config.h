#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

#include <Arduino.h>

// BLE Device Configuration
#define BLE_DEVICE_NAME          "MarineGateway"
#define BLE_DEFAULT_PIN          "123456"
#define BLE_UPDATE_INTERVAL_MS   1000    // 1Hz update rate

// BLE Service UUIDs (Custom UUIDs for Marine Gateway)
// Base UUID: 0000XXXX-0000-1000-8000-00805F9B34FB (Bluetooth SIG base)
// Custom base: 4D475743-XXXX-4E41-5649-474154494F4E (MGWC = Marine Gateway Custom)

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

// BLE Task Configuration
#define BLE_TASK_STACK_SIZE      8192
#define BLE_TASK_PRIORITY        3
#define BLE_MAX_CONNECTIONS      3

// Security Configuration
#define BLE_SECURITY_ENABLED     true
#define BLE_IO_CAP               ESP_IO_CAP_OUT  // Display only (shows PIN)
#define BLE_REQUIRE_BONDING      true
#define BLE_REQUIRE_MITM         true  // Man-in-the-Middle protection

#endif // BLE_CONFIG_H
