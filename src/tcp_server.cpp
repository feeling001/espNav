#include "tcp_server.h"

TCPServer::TCPServer() 
    : server(NULL), clientsMutex(NULL), port(0), 
      initialized(false), running(false) {
}

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
    
    if (clientsMutex == NULL) {
        Serial.println("[TCP] ❌ Failed to create mutex!");
        return;
    }
    
    initialized = true;
    
    Serial.printf("[TCP] Initialized on port %u\n", port);
}

void TCPServer::start() {
    if (!initialized || running) {
        return;
    }
    
    server = new AsyncServer(port);
    
    if (server == NULL) {
        Serial.println("[TCP] ❌ Failed to create server!");
        return;
    }
    
    // Set up new client handler
    server->onClient([](void* arg, AsyncClient* client) {
        TCPServer* srv = static_cast<TCPServer*>(arg);
        srv->onConnect(client);
    }, this);
    
    server->begin();
    running = true;
    
    Serial.printf("[TCP] ✓ Server started on port %u\n", port);
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
        if (client && client->connected()) {
            client->close();
        }
    }
    clients.clear();
    xSemaphoreGive(clientsMutex);
    
    Serial.println("[TCP] Server stopped");
}

void TCPServer::onConnect(AsyncClient* client) {
    if (!client) {
        return;
    }
    
    Serial.printf("[TCP] New client connected: %s:%u\n", 
                  client->remoteIP().toString().c_str(),
                  client->remotePort());
    
    // Set up client callbacks
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
    
    client->onTimeout([](void* arg, AsyncClient* c, uint32_t time) {
        Serial.printf("[TCP] Client timeout: %s (time: %u ms)\n", 
                     c->remoteIP().toString().c_str(), time);
    }, this);
    
    addClient(client);
    
    // Send welcome message
    const char* welcome = "$PMAR,Marine Gateway Connected*00\r\n";
    if (client->canSend()) {
        client->write(welcome, strlen(welcome));
    }
}

void TCPServer::onDisconnect(AsyncClient* client) {
    if (!client) {
        return;
    }
    
    Serial.printf("[TCP] Client disconnected: %s:%u\n", 
                  client->remoteIP().toString().c_str(),
                  client->remotePort());
    removeClient(client);
}

void TCPServer::onData(AsyncClient* client, void* data, size_t len) {
    // TCP server is transmit-only for NMEA data
    // Ignore any incoming data (could log it if needed)
    Serial.printf("[TCP] Received %u bytes from %s (ignored)\n", 
                  len, client->remoteIP().toString().c_str());
}

void TCPServer::onError(AsyncClient* client, int8_t error) {
    if (!client) {
        return;
    }
    
    Serial.printf("[TCP] Client error: %s, error code: %d\n", 
                  client->remoteIP().toString().c_str(), error);
    removeClient(client);
}

void TCPServer::addClient(AsyncClient* client) {
    if (!client) {
        return;
    }
    
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    
    if (clients.size() < TCP_MAX_CLIENTS) {
        clients.push_back(client);
        Serial.printf("[TCP] Client added, total clients: %d/%d\n", 
                     clients.size(), TCP_MAX_CLIENTS);
    } else {
        Serial.printf("[TCP] ⚠️  Max clients reached (%d), rejecting connection from %s\n", 
                     TCP_MAX_CLIENTS, client->remoteIP().toString().c_str());
        
        // Send rejection message before closing
        const char* msg = "$PMAR,Server Full*00\r\n";
        if (client->canSend()) {
            client->write(msg, strlen(msg));
        }
        
        client->close();
    }
    
    xSemaphoreGive(clientsMutex);
}

void TCPServer::removeClient(AsyncClient* client) {
    if (!client) {
        return;
    }
    
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    
    auto it = std::find(clients.begin(), clients.end(), client);
    if (it != clients.end()) {
        clients.erase(it);
        Serial.printf("[TCP] Client removed, remaining clients: %d\n", clients.size());
    }
    
    xSemaphoreGive(clientsMutex);
}

void TCPServer::broadcast(const char* data) {
    if (!data) {
        return;
    }
    broadcast(data, strlen(data));
}

void TCPServer::broadcast(const char* data, size_t len) {
    if (!running || !data || len == 0) {
        return;
    }
    
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    
    if (clients.empty()) {
        xSemaphoreGive(clientsMutex);
        return;
    }
    
    // Prepare data with CRLF if not already present
    char buffer[NMEA_MAX_LENGTH + 3];  // +3 for \r\n\0
    size_t sendLen = len;
    
    // Check if data already has CRLF
    bool hasCRLF = (len >= 2 && data[len-2] == '\r' && data[len-1] == '\n');
    
    if (len < sizeof(buffer) - 2) {
        memcpy(buffer, data, len);
        
        if (!hasCRLF) {
            buffer[len++] = '\r';
            buffer[len++] = '\n';
        }
        buffer[len] = '\0';
        sendLen = len;
    } else {
        // Data too long, just use as-is
        memcpy(buffer, data, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        sendLen = sizeof(buffer) - 1;
    }
    
    // Send to all connected clients
    size_t sentCount = 0;
    size_t errorCount = 0;
    
    for (auto it = clients.begin(); it != clients.end(); ) {
        AsyncClient* client = *it;
        
        // Check if client is still connected
        if (!client || !client->connected()) {
            Serial.printf("[TCP] Removing disconnected client during broadcast\n");
            it = clients.erase(it);
            continue;
        }
        
        // Check if client can send
        if (!client->canSend()) {
            Serial.printf("[TCP] ⚠️  Client %s buffer full, closing connection\n", 
                         client->remoteIP().toString().c_str());
            client->close();
            it = clients.erase(it);
            errorCount++;
            continue;
        }
        
        // Send data
        size_t written = client->write(buffer, sendLen);
        if (written == sendLen) {
            sentCount++;
        } else {
            Serial.printf("[TCP] ⚠️  Partial write to %s (%u/%u bytes)\n", 
                         client->remoteIP().toString().c_str(), written, sendLen);
            errorCount++;
        }
        
        ++it;
    }
    
    // Debug output (can be disabled for production)
    if (sentCount > 0) {
        // Only log occasionally to avoid spam
        static uint32_t lastLog = 0;
        static uint32_t broadcastCount = 0;
        
        broadcastCount++;
        
        if (millis() - lastLog > 5000) {  // Log every 5 seconds
            Serial.printf("[TCP] Broadcast stats: %u messages sent to %d clients (errors: %u)\n", 
                         broadcastCount, sentCount, errorCount);
            lastLog = millis();
            broadcastCount = 0;
        }
    }
    
    xSemaphoreGive(clientsMutex);
}

size_t TCPServer::getClientCount() {
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    size_t count = clients.size();
    xSemaphoreGive(clientsMutex);
    return count;
}