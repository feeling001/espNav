#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// WiFi Configuration
#define WIFI_CONNECT_TIMEOUT_MS  30000
#define WIFI_AP_SSID_PREFIX      "MarineGateway"
#define WIFI_AP_PASSWORD         "marine123"
#define WIFI_MAX_RECONNECT       3

// UART Configuration
#define UART_NUM                 1
#define UART_RX_PIN              GPIO_NUM_6 // WAS 16 
#define UART_TX_PIN              GPIO_NUM_5 // WAS 17
#define UART_BUFFER_SIZE         2048
#define UART_DEFAULT_BAUD        38400

// TCP Server
#define TCP_PORT                 10110
#define TCP_MAX_CLIENTS          5

// Web Server
#define WEB_SERVER_PORT          80

// NMEA
#define NMEA_MAX_LENGTH          128
#define NMEA_QUEUE_SIZE          50

// NVS
#define NVS_NAMESPACE            "marine_gw"

// Task Priorities (higher number = higher priority)
#define TASK_PRIORITY_UART       5
#define TASK_PRIORITY_NMEA       4
#define TASK_PRIORITY_TCP        3
#define TASK_PRIORITY_WEB        3
#define TASK_PRIORITY_WIFI       2

// Task Stack Sizes
#define TASK_STACK_UART          4096
#define TASK_STACK_NMEA          4096
#define TASK_STACK_TCP           8192
#define TASK_STACK_WEB           8192
#define TASK_STACK_WIFI          4096

#endif // CONFIG_H
