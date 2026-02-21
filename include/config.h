#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Version
#define VERSION "1.0.0"

#define DEBUG

#ifdef DEBUG
//#define DEBUG_WEB
#define DEBUG_BLE
#define DEBUG_UART
#endif

// WiFi Configuration
#define WIFI_CONNECT_TIMEOUT_MS  30000
#define WIFI_AP_SSID_PREFIX      "MarineGateway"
#define WIFI_AP_PASSWORD         "marine123"
#define WIFI_MAX_RECONNECT       3

// UART Configuration
#define UART_NUM                 UART_NUM_1
#define UART_RX_PIN              GPIO_NUM_6
#define UART_TX_PIN              GPIO_NUM_5
#define UART_BUFFER_SIZE         1024      // should be enough
#define UART_DEFAULT_BAUD        38400

// TCP Server
#define TCP_PORT                 10110
#define TCP_MAX_CLIENTS          5

// Web Server
#define WEB_SERVER_PORT          80

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

#endif // CONFIG_H