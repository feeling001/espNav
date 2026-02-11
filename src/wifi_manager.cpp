#include "wifi_manager.h"
#include "config.h"
#include <Arduino.h>

WiFiManager::WiFiManager() 
    : currentState(WIFI_DISCONNECTED), 
      reconnectAttempts(0), 
      connectStartTime(0),
      scanInProgress(false) {
}

void WiFiManager::init(const WiFiConfig& cfg) {
    config = cfg;
    
    Serial.println("[WiFi] Initializing WiFi Manager");
    Serial.printf("[WiFi] Mode: %s\n", config.mode == 0 ? "STA" : "AP");
    
    if (config.mode == 0) {
        Serial.printf("[WiFi] Target SSID: %s\n", config.ssid);
    } else {
        if (strlen(config.ap_ssid) > 0) {
            Serial.printf("[WiFi] AP SSID: %s\n", config.ap_ssid);
        } else {
            Serial.println("[WiFi] AP SSID: MarineGateway-XXXXXX (auto)");
        }
    }
}

void WiFiManager::start() {
    if (config.mode == 0 && strlen(config.ssid) > 0) {
        attemptSTAConnection();
    } else {
        fallbackToAP();
    }
}

void WiFiManager::attemptSTAConnection() {
    Serial.printf("[WiFi] Attempting STA connection to '%s'...\n", config.ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);
    
    currentState = WIFI_CONNECTING;
    connectStartTime = millis();
}

void WiFiManager::checkSTAConnection() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] ✓ STA connected!");
        Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
        currentState = WIFI_CONNECTED_STA;
        reconnectAttempts = 0;
    } else if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
        Serial.println("[WiFi] STA connection timeout");
        fallbackToAP();
    }
}

void WiFiManager::fallbackToAP() {
    Serial.println("[WiFi] Starting AP mode...");
    
    WiFi.mode(WIFI_AP);
    
    char apSSID[32];
    char apPassword[64];
    
    // Use custom AP SSID if provided, otherwise generate default
    if (strlen(config.ap_ssid) > 0) {
        strncpy(apSSID, config.ap_ssid, sizeof(apSSID) - 1);
        apSSID[sizeof(apSSID) - 1] = '\0';
    } else {
        // Generate default SSID with MAC address
        uint8_t mac[6];
        WiFi.macAddress(mac);
        snprintf(apSSID, sizeof(apSSID), "%s-%02X%02X%02X", 
                 WIFI_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);
    }
    
    // Use custom AP password if provided, otherwise use default
    if (strlen(config.ap_password) >= 8) {  // WPA2 requires min 8 chars
        strncpy(apPassword, config.ap_password, sizeof(apPassword) - 1);
        apPassword[sizeof(apPassword) - 1] = '\0';
    } else {
        strncpy(apPassword, WIFI_AP_PASSWORD, sizeof(apPassword) - 1);
        apPassword[sizeof(apPassword) - 1] = '\0';
    }
    
    WiFi.softAP(apSSID, apPassword);
    
    Serial.printf("[WiFi] AP Mode Active\n");
    Serial.printf("[WiFi]   SSID: %s\n", apSSID);
    Serial.printf("[WiFi]   Password: %s\n", apPassword);
    Serial.printf("[WiFi]   IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    currentState = WIFI_AP_MODE;
}

void WiFiManager::update() {
    switch (currentState) {
        case WIFI_CONNECTING:
            checkSTAConnection();
            break;
            
        case WIFI_CONNECTED_STA:
            monitorSTAConnection();
            break;
            
        case WIFI_RECONNECTING:
            handleReconnection();
            break;
            
        default:
            break;
    }
}

void WiFiManager::monitorSTAConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection lost, attempting to reconnect...");
        currentState = WIFI_RECONNECTING;
        reconnectAttempts = 0;
    }
}

void WiFiManager::handleReconnection() {
    if (reconnectAttempts < WIFI_MAX_RECONNECT) {
        reconnectAttempts++;
        Serial.printf("[WiFi] Reconnection attempt %d/%d\n", 
                     reconnectAttempts, WIFI_MAX_RECONNECT);
        attemptSTAConnection();
    } else {
        Serial.println("[WiFi] Max reconnect attempts reached, falling back to AP");
        fallbackToAP();
    }
}

void WiFiManager::reconnect() {
    if (currentState == WIFI_AP_MODE) {
        // Switch back to STA mode
        reconnectAttempts = 0;
        attemptSTAConnection();
    }
}

int8_t WiFiManager::getRSSI() const {
    if (currentState == WIFI_CONNECTED_STA) {
        return WiFi.RSSI();
    }
    return 0;
}

IPAddress WiFiManager::getIP() const {
    if (currentState == WIFI_CONNECTED_STA) {
        return WiFi.localIP();
    } else if (currentState == WIFI_AP_MODE) {
        return WiFi.softAPIP();
    }
    return IPAddress(0, 0, 0, 0);
}

size_t WiFiManager::getConnectedClients() const {
    if (currentState == WIFI_AP_MODE) {
        return WiFi.softAPgetStationNum();
    }
    return 0;
}

String WiFiManager::getSSID() const {
    if (currentState == WIFI_CONNECTED_STA) {
        return WiFi.SSID();
    } else if (currentState == WIFI_AP_MODE) {
        if (strlen(config.ap_ssid) > 0) {
            return String(config.ap_ssid);
        } else {
            uint8_t mac[6];
            WiFi.macAddress(mac);
            char apSSID[32];
            snprintf(apSSID, sizeof(apSSID), "%s-%02X%02X%02X", 
                     WIFI_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);
            return String(apSSID);
        }
    }
    return "";
}

// ============================================================
// WiFi Scan Functionality
// ============================================================

int16_t WiFiManager::startScan() {
    if (scanInProgress) {
        Serial.println("[WiFi] Scan already in progress");
        return -1;
    }
    
    Serial.println("[WiFi] Starting WiFi scan...");
    scanInProgress = true;
    
    // Start async scan
    int16_t result = WiFi.scanNetworks(true, false, false, 300);
    
    if (result == WIFI_SCAN_RUNNING) {
        Serial.println("[WiFi] Scan initiated successfully");
        return WIFI_SCAN_RUNNING;
    } else if (result == WIFI_SCAN_FAILED) {
        Serial.println("[WiFi] Scan failed to start");
        scanInProgress = false;
        return -1;
    }
    
    return result;
}

bool WiFiManager::isScanComplete() {
    if (!scanInProgress) {
        return false;
    }
    
    int16_t result = WiFi.scanComplete();
    
    if (result >= 0) {
        // Scan complete
        scanInProgress = false;
        Serial.printf("[WiFi] Scan complete, found %d networks\n", result);
        return true;
    } else if (result == WIFI_SCAN_FAILED) {
        Serial.println("[WiFi] Scan failed");
        scanInProgress = false;
        return true;  // Complete but with error
    }
    
    // Still running
    return false;
}

std::vector<WiFiScanResult> WiFiManager::getScanResults() {
    std::vector<WiFiScanResult> results;
    
    int16_t networkCount = WiFi.scanComplete();
    
    if (networkCount <= 0) {
        return results;
    }
    
    results.reserve(networkCount);
    
    for (int16_t i = 0; i < networkCount; i++) {
        WiFiScanResult result;
        
        String ssid = WiFi.SSID(i);
        strncpy(result.ssid, ssid.c_str(), sizeof(result.ssid) - 1);
        result.ssid[sizeof(result.ssid) - 1] = '\0';
        
        result.rssi = WiFi.RSSI(i);
        result.channel = WiFi.channel(i);
        
        // Map encryption type
        wifi_auth_mode_t encType = WiFi.encryptionType(i);
        switch (encType) {
            case WIFI_AUTH_OPEN:
                result.encryption = 0;
                break;
            case WIFI_AUTH_WEP:
                result.encryption = 1;
                break;
            case WIFI_AUTH_WPA_PSK:
                result.encryption = 2;
                break;
            case WIFI_AUTH_WPA2_PSK:
                result.encryption = 3;
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                result.encryption = 4;
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                result.encryption = 5;
                break;
            case WIFI_AUTH_WPA3_PSK:
                result.encryption = 6;
                break;
            default:
                result.encryption = 0;
                break;
        }
        
        results.push_back(result);
    }
    
    return results;
}

void WiFiManager::clearScanResults() {
    WiFi.scanDelete();
    scanInProgress = false;
    Serial.println("[WiFi] Scan results cleared");
}
