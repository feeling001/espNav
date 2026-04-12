#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>


// Version (defined in platformio)
#ifndef VERSION
#define VERSION "unknown"
#endif

#define DEBUG

#ifdef DEBUG
#define DEBUG_WEB
#define DEBUG_UART
#define DEBUG_BLE
#define DEBUG_CPU

#endif

#define DEBUG_SERIAL Serial0


// WiFi Configuration
#define WIFI_CONNECT_TIMEOUT_MS  30000
#define WIFI_AP_SSID_PREFIX      "MarineGateway"
#define WIFI_AP_PASSWORD         "marine123"
#define WIFI_AP_MAX_CLIENTS      4
#define WIFI_AP_CHANNEL          4
#define WIFI_MAX_RECONNECT       3


// UART Configuration (NMEA0183_1)
#define UART_NUM                 UART_NUM_1
#define UART_RX_PIN              GPIO_NUM_6
#define UART_TX_PIN              GPIO_NUM_5
#define UART_BUFFER_SIZE         1024      // should be enough
#define UART_DEFAULT_BAUD        38400

// --- Configuration SeaTalk1 ---

#define ST1_RX_PIN          GPIO_NUM_7
#define ST1_TX_PIN          GPIO_NUM_8
#define ST1_RX_CHANNEL      RMT_CHANNEL_4
#define ST1_TX_CHANNEL      RMT_CHANNEL_1
#define ST1_ENABLED         true


// Delays (CDMA/CD)
#define ST1_COLLISION_DELAY 3   // ms [cite: 51]
#define ST1_MAX_TRIES       5   // [cite: 49]

// TCP Server
#define TCP_PORT                 10110
#define TCP_MAX_CLIENTS          5

// Web Server
#define WEB_SERVER_PORT          80

// Websocket configuration
#define WS_MAX_RATE_HZ   10      // max WebSocket frames per second

// NMEA - OPTIMISÉ POUR ÉVITER OVERFLOWS
#define NMEA_MAX_LENGTH          86        // In theory the max is 83 bytes
#define NMEA_QUEUE_SIZE          40        // Is monitored

// NVS
#define NVS_NAMESPACE            "marine_gw"

// Task Priorities (higher number = higher priority)
#define TASK_PRIORITY_UART       5     
#define TASK_PRIORITY_NMEA       4     
#define TASK_PRIORITY_TCP        3
#define TASK_PRIORITY_WEB        2     
#define TASK_PRIORITY_WIFI       2

// Task Stack Sizes - AUGMENTÉS
#define TASK_STACK_UART          4096
#define TASK_STACK_NMEA          4096
#define TASK_STACK_TCP           8192
#define TASK_STACK_WEB           8192
#define TASK_STACK_WIFI          4096

// ─────────────────────────────────────────────────────────────────────────────
// SD Card Configuration (SPI bus, optional)
//
// Available pins on ESP32-S3-Zero (not used by other peripherals):
//   GPIO 1, 2, 9, 10, 11, 12, 13
//
// Pin assignment:
//   SD_MOSI → GPIO 11
//   SD_MISO → GPIO 13
//   SD_SCK  → GPIO 12
//   SD_CS   → GPIO 10
//
// The SD card is mounted at boot but its presence is optional.
// Check SDManager::isMounted() before any file operations.
// ─────────────────────────────────────────────────────────────────────────────
#define SD_MOSI_PIN         GPIO_NUM_11
#define SD_MISO_PIN         GPIO_NUM_13
#define SD_SCK_PIN          GPIO_NUM_12
#define SD_CS_PIN           GPIO_NUM_10
#define SD_SPI_FREQ_HZ      4000000     // 4 MHz — conservative, works with most cards
#define SD_SPI_HOST         SPI3_HOST   // VSPI / SPI3 — leaves HSPI free for other uses
#define SD_MOUNT_POINT      "/sdcard"
#define SD_MAX_FILES        10          // max simultaneously open files on FAT

#endif // CONFIG_H
