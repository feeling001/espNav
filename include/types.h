#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

// WiFi configuration structure
struct WiFiConfig {
    char ssid[32];
    char password[64];
    uint8_t mode;  // 0=STA, 1=AP
    
    WiFiConfig() : mode(0) {
        ssid[0] = '\0';
        password[0] = '\0';
    }
};

// Serial configuration structure
struct SerialConfig {
    uint32_t baudRate;
    uint8_t dataBits;  // 5-8
    uint8_t parity;    // 0=None, 1=Even, 2=Odd
    uint8_t stopBits;  // 1-2
    
    SerialConfig() : baudRate(38400), dataBits(8), parity(0), stopBits(1) {}
};

// NMEA sentence structure
struct NMEASentence {
    char raw[128];
    char type[8];
    uint8_t checksum;
    bool valid;
    uint32_t timestamp;
    
    NMEASentence() : checksum(0), valid(false), timestamp(0) {
        raw[0] = '\0';
        type[0] = '\0';
    }
};

// WiFi state enumeration
enum WiFiState {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED_STA,
    WIFI_RECONNECTING,
    WIFI_AP_MODE
};

#endif // TYPES_H
