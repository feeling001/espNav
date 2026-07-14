#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

// WiFi configuration structure
struct WiFiConfig {
    char ssid[32];
    char password[64];
    uint8_t mode;  // 0=STA, 1=AP
    int channel;
    int maxconn;
    
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

// BLE configuration structure
struct BLEConfigData {
    bool enabled;
    char device_name[32];
    char pin_code[7];  // 6 digits + null terminator
    
    BLEConfigData() : enabled(false) {
        strcpy(device_name, "MarineGateway");
        strcpy(pin_code, "123456");
    }
};

// Alarm configuration structure
struct AlarmConfig {
    bool     alarms_enabled;

    bool     depth_enabled;
    float    depth_threshold_m;

    bool     ais_enabled;
    float    ais_distance_nm;
    uint32_t own_mmsi;

    bool     gps_lost_enabled;
    uint16_t gps_lost_timeout_s;

    AlarmConfig()
        : alarms_enabled(true),
          depth_enabled(true),      depth_threshold_m(2.0f),
          ais_enabled(true),        ais_distance_nm(1.0f), own_mmsi(0),
          gps_lost_enabled(true),   gps_lost_timeout_s(10) {}
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

// ── Bus Conversion Configuration ─────────────────────────────────────────────
// Number and ordering of conversion rules (must match CONV_RULE_DEFS in
// seatalk_manager.cpp and the front-end ConversionsConfig.jsx)
#define CONV_GPS_COG_TO_ST     0   // NMEA GPS COG → SeaTalk 0x53
#define CONV_GPS_SOG_TO_ST     1   // NMEA GPS SOG → SeaTalk 0x52
#define CONV_GPS_POS_TO_ST     2   // NMEA GPS LAT/LON → SeaTalk 0x50/0x51
#define CONV_DEPTH_TO_ST       3   // NMEA Depth → SeaTalk 0x00
#define CONV_STW_TO_ST         4   // NMEA STW → SeaTalk 0x20
#define CONV_AWA_TO_ST         5   // NMEA AWA → SeaTalk 0x10  (⚠ normally native on SeaTalk)
#define CONV_AWS_TO_ST         6   // NMEA AWS → SeaTalk 0x11  (⚠ normally native on SeaTalk)
#define CONV_WATER_TEMP_TO_ST  7   // NMEA Water Temp → SeaTalk 0x27
#define CONV_HDG_TO_ST         8   // NMEA Heading → SeaTalk 0x9C
#define CONV_TW_TO_NMEA        9   // Calculated True Wind (TWA+TWS) → NMEA0183 MWV(T)
#define CONV_COUNT             10

struct ConversionRule {
    bool    enabled;
    uint8_t interval_s;   // transmission interval in seconds (1–60)

    ConversionRule() : enabled(false), interval_s(1) {}
};

struct ConversionConfig {
    ConversionRule rules[CONV_COUNT];

    ConversionConfig() {}
};

#endif // TYPES_H
