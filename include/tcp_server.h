#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

class TCPServer {
public:
    TCPServer();
    ~TCPServer();
    
    void init(uint16_t port);
    void start();
    void stop();
    
    // Broadcast methods
    void broadcast(const char* data);  // Null-terminated string
    void broadcast(const char* data, size_t len);  // Binary data with length
    
    size_t getClientCount();
    
private:
    // Client event handlers
    void onConnect(AsyncClient* client);
    void onDisconnect(AsyncClient* client);
    void onData(AsyncClient* client, void* data, size_t len);
    void onError(AsyncClient* client, int8_t error);
    
    // Client management
    void addClient(AsyncClient* client);
    void removeClient(AsyncClient* client);
    
    AsyncServer* server;
    std::vector<AsyncClient*> clients;
    SemaphoreHandle_t clientsMutex;
    uint16_t port;
    bool initialized;
    bool running;
};

#endif // TCP_SERVER_H