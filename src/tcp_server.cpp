#include "tcp_server.h"

TCPServer::TCPServer() 
    : server(NULL), clientsMutex(NULL), port(0), 
      initialized(false), running(false) {}

TCPServer::~TCPServer() {
    stop();
    
    if (clientsMutex) {
        vSemaphoreDelete(clientsMutex);
    }
    
    if (server) {
        delete server;
    }
}

void TCPServer::init(uint16_t p) {
    if (initialized) {
        return;
    }
    
    port = p;
    clientsMutex = xSemaphoreCreateMutex();
    
    initialized = true;
    
    Serial.printf("[TCP] Server initialized on port %u\n", port);
}

void TCPServer::start() {
    if (!initialized || running) {
        return;
    }
    
    server = new AsyncServer(port);
    
    server->onClient([](void* arg, AsyncClient* client) {
        TCPServer* srv = static_cast<TCPServer*>(arg);
        srv->onConnect(client);
    }, this);
    
    server->begin();
    running = true;
    
    Serial.printf("[TCP] Server started on port %u\n", port);
}

void TCPServer::stop() {
    if (!running) {
        return;
    }
    
    running = false;
    
    if (server) {
        server->end();
    }
    
    // Close all clients
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    for (auto client : clients) {
        client->close();
    }
    clients.clear();
    xSemaphoreGive(clientsMutex);
    
    Serial.println("[TCP] Server stopped");
}

void TCPServer::onConnect(AsyncClient* client) {
    Serial.printf("[TCP] Client connected: %s\n", client->remoteIP().toString().c_str());
    
    // Set up callbacks
    client->onDisconnect([](void* arg, AsyncClient* c) {
        TCPServer* srv = static_cast<TCPServer*>(arg);
        srv->onDisconnect(c);
    }, this);
    
    client->onData([](void* arg, AsyncClient* c, void* data, size_t len) {
        TCPServer* srv = static_cast<TCPServer*>(arg);
        srv->onData(c, data, len);
    }, this);
    
    client->onError([](void* arg, AsyncClient* c, int8_t error) {
        TCPServer* srv = static_cast<TCPServer*>(arg);
        srv->onError(c, error);
    }, this);
    
    addClient(client);
}

void TCPServer::onDisconnect(AsyncClient* client) {
    Serial.printf("[TCP] Client disconnected: %s\n", client->remoteIP().toString().c_str());
    removeClient(client);
}

void TCPServer::onData(AsyncClient* client, void* data, size_t len) {
    // TCP server is receive-only for NMEA, ignore incoming data
    // Could be used for future bidirectional communication
}

void TCPServer::onError(AsyncClient* client, int8_t error) {
    Serial.printf("[TCP] Client error: %s, error=%d\n", 
                  client->remoteIP().toString().c_str(), error);
}

void TCPServer::addClient(AsyncClient* client) {
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    
    if (clients.size() < TCP_MAX_CLIENTS) {
        clients.push_back(client);
        Serial.printf("[TCP] Client added, total: %d\n", clients.size());
    } else {
        Serial.printf("[TCP] Max clients reached (%d), rejecting connection\n", TCP_MAX_CLIENTS);
        client->close();
    }
    
    xSemaphoreGive(clientsMutex);
}

void TCPServer::removeClient(AsyncClient* client) {
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    
    auto it = std::find(clients.begin(), clients.end(), client);
    if (it != clients.end()) {
        clients.erase(it);
        Serial.printf("[TCP] Client removed, total: %d\n", clients.size());
    }
    
    xSemaphoreGive(clientsMutex);
}

void TCPServer::broadcast(const char* data, size_t len) {
    if (!running) {
        return;
    }
    
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    
    // Prepare data with CRLF if not already present
    char buffer[256];
    size_t dataLen = len;
    
    if (len < sizeof(buffer) - 3) {
        memcpy(buffer, data, len);
        if (len < 2 || buffer[len-2] != '\r' || buffer[len-1] != '\n') {
            buffer[len++] = '\r';
            buffer[len++] = '\n';
        }
        dataLen = len;
    }
    
    // Send to all connected clients
    for (auto it = clients.begin(); it != clients.end(); ) {
        AsyncClient* client = *it;
        
        if (!client->connected()) {
            it = clients.erase(it);
            continue;
        }
        
        if (!client->canSend()) {
            Serial.printf("[TCP] Client %s buffer full, dropping\n", 
                         client->remoteIP().toString().c_str());
            client->close();
            it = clients.erase(it);
            continue;
        }
        
        client->write(buffer, dataLen);
        ++it;
    }
    
    xSemaphoreGive(clientsMutex);
}

size_t TCPServer::getClientCount() {
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    size_t count = clients.size();
    xSemaphoreGive(clientsMutex);
    return count;
}
