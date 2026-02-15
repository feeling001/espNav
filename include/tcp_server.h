#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <AsyncTCP.h>
#include <vector>
#include <map>
#include "config.h"

// ═══════════════════════════════════════════════════════════════
// Structure pour tracker les statistiques par client - NOUVEAU
// ═══════════════════════════════════════════════════════════════
struct ClientStats {
    uint32_t lastSend;          // Timestamp du dernier envoi réussi
    uint32_t failedSends;       // Nombre d'échecs consécutifs actuels
    uint32_t totalSent;         // Total messages envoyés avec succès
    uint32_t totalSkipped;      // Total messages skippés (buffer plein)
};

class TCPServer {
public:
    TCPServer();
    ~TCPServer();
    
    void init(uint16_t port);
    void start();
    void stop();
    
    // Broadcast methods
    void broadcast(const char* data);
    void broadcast(const char* data, size_t len);
    
    // Client management
    size_t getClientCount();
    bool getClientStats(AsyncClient* client, ClientStats& stats);
    
private:
    AsyncServer* server;
    std::vector<AsyncClient*> clients;
    std::map<AsyncClient*, ClientStats> clientStats;  // NOUVEAU: Stats par client
    SemaphoreHandle_t clientsMutex;
    
    uint16_t port;
    bool initialized;
    bool running;
    
    // Client event handlers
    void onConnect(AsyncClient* client);
    void onDisconnect(AsyncClient* client);
    void onData(AsyncClient* client, void* data, size_t len);
    void onError(AsyncClient* client, int8_t error);
    
    // Internal methods
    void addClient(AsyncClient* client);
    void removeClient(AsyncClient* client);
};

#endif // TCP_SERVER_H