#include "wifi_manager.h"

WiFiManager::WiFiManager() 
    : currentState(WIFI_DISCONNECTED), connectStartTime(0), 
      reconnectAttempts(0), initialized(false) {}

void WiFiManager::init(const WiFiConfig& cfg) {
    if (initialized) {
        return;
    }
    
    config = cfg;
    initialized = true;
    
    Serial.println("[WiFi] Manager initialized");
}

void WiFiManager::start() {
    if (!initialized) {
        Serial.println("[WiFi] Error: Not initialized");
        return;
    }
    
    // If no SSID configured, go to AP mode
    if (config.ssid[0] == '\0') {
        Serial.println("[WiFi] No SSID configured, starting AP mode");
        fallbackToAP();
        return;
    }
    
    // Try STA mode first
    attemptSTAConnection();
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
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        currentState = WIFI_CONNECTED_STA;
        reconnectAttempts = 0;
    } else if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
        Serial.println("[WiFi] STA connection timeout");
        fallbackToAP();
    }
}

void WiFiManager::monitorSTAConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection lost");
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
        Serial.println("[WiFi] Max reconnect attempts reached");
        fallbackToAP();
    }
}

void WiFiManager::fallbackToAP() {
    Serial.println("[WiFi] Falling back to AP mode...");
    
    WiFi.mode(WIFI_AP);
    
    // Generate SSID with MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char apSSID[32];
    snprintf(apSSID, sizeof(apSSID), "%s-%02X%02X%02X", 
             WIFI_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);
    
    WiFi.softAP(apSSID, WIFI_AP_PASSWORD);
    
    Serial.printf("[WiFi] AP Mode: SSID='%s', Password='%s'\n", apSSID, WIFI_AP_PASSWORD);
    Serial.printf("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());
    
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
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char apSSID[32];
        snprintf(apSSID, sizeof(apSSID), "%s-%02X%02X%02X", 
                 WIFI_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);
        return String(apSSID);
    }
    return "";
}
