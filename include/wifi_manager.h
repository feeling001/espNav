#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include "types.h"
#include <vector>

class WiFiManager {
public:
    WiFiManager();
    
    void init(const WiFiConfig& config);
    void start();
    void update();
    void reconnect();
    
    WiFiState getState() const { return currentState; }
    int8_t getRSSI() const;
    IPAddress getIP() const;
    size_t getConnectedClients() const;
    String getSSID() const;
    
    // WiFi scan functionality
    int16_t startScan();  // Returns number of networks found, -1 on error
    bool isScanComplete();
    std::vector<WiFiScanResult> getScanResults();
    void clearScanResults();
    
private:
    void attemptSTAConnection();
    void checkSTAConnection();
    void fallbackToAP();
    void monitorSTAConnection();
    void handleReconnection();
    
    WiFiConfig config;
    WiFiState currentState;
    uint8_t reconnectAttempts;
    unsigned long connectStartTime;
    bool scanInProgress;
};

#endif // WIFI_MANAGER_H
