#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "types.h"
#include "config.h"

class WiFiManager {
public:
    WiFiManager();
    
    void init(const WiFiConfig& config);
    void start();
    void reconnect();
    
    WiFiState getState() const { return currentState; }
    int8_t getRSSI() const;
    IPAddress getIP() const;
    size_t getConnectedClients() const;
    String getSSID() const;
    
    void update();  // Called periodically to monitor connection
    
private:
    void attemptSTAConnection();
    void checkSTAConnection();
    void monitorSTAConnection();
    void handleReconnection();
    void fallbackToAP();
    
    WiFiConfig config;
    WiFiState currentState;
    uint32_t connectStartTime;
    uint8_t reconnectAttempts;
    bool initialized;
};

#endif // WIFI_MANAGER_H
