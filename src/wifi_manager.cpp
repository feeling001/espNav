#include "wifi_manager.h"
#include "config.h"
#include "functions.h"
#include <Arduino.h>
#include "functions.h"
#include <esp_wifi.h>

WiFiManager::WiFiManager() 
    : currentState(WIFI_DISCONNECTED), 
      reconnectAttempts(0), 
      connectStartTime(0),
      scanInProgress(false) {
}

void WiFiManager::init(const WiFiConfig& cfg) {
    config = cfg;
    
    serialPrintf("[WiFi] Initializing WiFi Manager\n");
    serialPrintf("[WiFi] Mode: %s\n", config.mode == 0 ? "STA" : "AP");
    
    if (config.mode == 0) {
        serialPrintf("[WiFi] Target SSID: %s\n", config.ssid);
    } else {
        if (strlen(config.ap_ssid) > 0) {
            serialPrintf("[WiFi] AP SSID: %s\n", config.ap_ssid);
        } else {
            serialPrintf("[WiFi] AP SSID: MarineGateway-XXXXXX (auto)\n");
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
    serialPrintf("[WiFi] Attempting STA connection to '%s'...\n", config.ssid);
   
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);
    
    currentState = WIFI_CONNECTING;
    connectStartTime = millis();
}

void WiFiManager::checkSTAConnection() {
    if (WiFi.status() == WL_CONNECTED) {
        serialPrintf("[WiFi] ✓ STA connected!\n");
        serialPrintf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
        serialPrintf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
        currentState = WIFI_CONNECTED_STA;
        reconnectAttempts = 0;
    } else if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
        serialPrintf("[WiFi] STA connection timeout\n");
        fallbackToAP();
    }
}
/*
void WiFiManager::fallbackToAP() {
    serialPrintf("[WiFi] Starting AP mode...\n");
   
    WiFi.mode(WIFI_OFF); 
    delay(100);
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
        snprintf(apSSID, sizeof(apSSID), "%s-%02X%02X%02X", WIFI_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);
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
    
    serialPrintf("[WiFi] AP Mode Active\n");
    serialPrintf("[WiFi]   SSID: %s\n", apSSID);
    serialPrintf("[WiFi]   Password: %s\n", apPassword);
    serialPrintf("[WiFi]   IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    currentState = WIFI_AP_MODE;
}
*/
void WiFiManager::fallbackToAP() {
    serialPrintf("[WiFi] Starting AP mode...\n");
   
    WiFi.mode(WIFI_OFF); 
    delay(100);
    WiFi.mode(WIFI_AP);
    
    char apSSID[32];
    char apPassword[64];
    
    if (strlen(config.ap_ssid) > 0) {
        strncpy(apSSID, config.ap_ssid, sizeof(apSSID) - 1);
        apSSID[sizeof(apSSID) - 1] = '\0';
    } else {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        snprintf(apSSID, sizeof(apSSID), "%s-%02X%02X%02X", WIFI_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);
    }
    
    if (strlen(config.ap_password) >= 8) {
        strncpy(apPassword, config.ap_password, sizeof(apPassword) - 1);
        apPassword[sizeof(apPassword) - 1] = '\0';
    } else {
        strncpy(apPassword, WIFI_AP_PASSWORD, sizeof(apPassword) - 1);
        apPassword[sizeof(apPassword) - 1] = '\0';
    }
    
    // Canal 6, visible, max 4 clients
    WiFi.softAP(apSSID, apPassword, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CLIENTS);


    // Forcer WPA2-PSK uniquement (évite les problèmes de handshake avec WPA3/SAE)
    wifi_config_t ap_config;
    esp_wifi_get_config(WIFI_IF_AP, &ap_config);
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    
    serialPrintf("[WiFi] AP Mode Active\n");
    serialPrintf("[WiFi]   SSID: %s\n", apSSID);
    serialPrintf("[WiFi]   Password: %s\n", apPassword);
    serialPrintf("[WiFi]   IP: %s\n", WiFi.softAPIP().toString().c_str());
    
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
        serialPrintf("[WiFi] Connection lost, attempting to reconnect...\n");
        currentState = WIFI_RECONNECTING;
        reconnectAttempts = 0;
    }
}

void WiFiManager::handleReconnection() {
    if (reconnectAttempts < WIFI_MAX_RECONNECT) {
        reconnectAttempts++;
        serialPrintf("[WiFi] Reconnection attempt %d/%d\n", 
                     reconnectAttempts, WIFI_MAX_RECONNECT);
        attemptSTAConnection();
    } else {
        serialPrintf("[WiFi] Max reconnect attempts reached, falling back to AP\n");
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
        serialPrintf("[WiFi] Scan already in progress\n");
        return -1;
    }
    
    serialPrintf("[WiFi] Starting WiFi scan...\n");
    scanInProgress = true;
    
    // Start async scan
    int16_t result = WiFi.scanNetworks(true, false, false, 300);
    
    if (result == WIFI_SCAN_RUNNING) {
        serialPrintf("[WiFi] Scan initiated successfully\n");
        return WIFI_SCAN_RUNNING;
    } else if (result == WIFI_SCAN_FAILED) {
        serialPrintf("[WiFi] Scan failed to start\n");
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
        serialPrintf("[WiFi] Scan complete, found %d networks\n", result);
        return true;
    } else if (result == WIFI_SCAN_FAILED) {
        serialPrintf("[WiFi] Scan failed\n");
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
    serialPrintf("[WiFi] Scan results cleared\n");
}
