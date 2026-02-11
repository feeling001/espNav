#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

// WiFi configuration structure
struct WiFiConfig {
    char ssid[32];
    char password[64];
    uint8_t mode;  // 0=STA, 1=AP
    
    // AP mode configuration
    char ap_ssid[32];      // Custom AP SSID (if empty, use default MarineGateway-XXXXXX)
    char ap_password[64];  // Custom AP password (min 8 chars, if empty use default)
    
    WiFiConfig() : mode(0) {
        ssid[0] = '\0';
        password[0] = '\0';
        ap_ssid[0] = '\0';
        ap_password[0] = '\0';
    }
};

// WiFi scan result structure
struct WiFiScanResult {
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    uint8_t encryption;  // 0=Open, 1=WEP, 2=WPA, 3=WPA2, 4=WPA/WPA2, 5=WPA2-Enterprise, 6=WPA3
    
    WiFiScanResult() : rssi(0), channel(0), encryption(0) {
        ssid[0] = '\0';
    }
};

// Serial configuration structure
// NOTE: Renamed from SerialConfig to UARTConfig to avoid conflict
// with ESP32 Arduino's enum SerialConfig in HardwareSerial.h
struct UARTConfig {
    uint32_t baudRate;
    uint8_t dataBits;  // 5-8
    uint8_t parity;    // 0=None, 1=Even, 2=Odd
    uint8_t stopBits;  // 1-2
    
    UARTConfig() : baudRate(38400), dataBits(8), parity(0), stopBits(1) {}
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
