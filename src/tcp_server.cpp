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
    
    Serial.printf("[TCP] Initialized on port %u with intelligent throttling\n", port);
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
    clientStats.clear();  // Clear stats map
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
        
        // ═══════════════════════════════════════════════════════════════
        // NOUVEAU: Initialiser les stats pour ce client
        // ═══════════════════════════════════════════════════════════════
        ClientStats stats;
        stats.lastSend = millis();
        stats.failedSends = 0;
        stats.totalSent = 0;
        stats.totalSkipped = 0;
        clientStats[client] = stats;
        
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
        // ═══════════════════════════════════════════════════════════════
        // NOUVEAU: Afficher les stats avant de retirer le client
        // ═══════════════════════════════════════════════════════════════
        auto statsIt = clientStats.find(client);
        if (statsIt != clientStats.end()) {
            const ClientStats& stats = statsIt->second;
            Serial.printf("[TCP] Client %s stats: sent=%u, skipped=%u, fails=%u\n",
                         client->remoteIP().toString().c_str(),
                         stats.totalSent, stats.totalSkipped, stats.failedSends);
            clientStats.erase(statsIt);
        }
        
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
    
    // ═══════════════════════════════════════════════════════════════
    // Prepare data with CRLF if not already present
    // ═══════════════════════════════════════════════════════════════
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
    
    // ═══════════════════════════════════════════════════════════════
    // NOUVEAU: Système de throttling intelligent
    // ═══════════════════════════════════════════════════════════════
    uint32_t now = millis();
    size_t sentCount = 0;
    size_t skippedCount = 0;
    size_t errorCount = 0;
    
    for (auto it = clients.begin(); it != clients.end(); ) {
        AsyncClient* client = *it;
        
        // Check if client is still connected
        if (!client || !client->connected()) {
            Serial.printf("[TCP] Removing disconnected client during broadcast\n");
            
            // Clean up stats
            auto statsIt = clientStats.find(client);
            if (statsIt != clientStats.end()) {
                clientStats.erase(statsIt);
            }
            
            it = clients.erase(it);
            continue;
        }
        
        // Get client stats
        auto& stats = clientStats[client];
        
        // ═══════════════════════════════════════════════════════════════
        // NOUVEAU: Vérifier si le client peut recevoir
        // ═══════════════════════════════════════════════════════════════
        if (!client->canSend()) {
            stats.failedSends++;
            stats.totalSkipped++;
            skippedCount++;
            
            // ═══════════════════════════════════════════════════════════════
            // Politique de déconnexion intelligente:
            // 1. Plus de 100 échecs consécutifs → déconnexion
            // 2. Plus de 10 secondes bloqué ET > 10 échecs → déconnexion
            // 3. Plus de 30 secondes bloqué → déconnexion inconditionnelle
            // ═══════════════════════════════════════════════════════════════
            uint32_t timeSinceLastSend = now - stats.lastSend;
            
            bool shouldDisconnect = false;
            const char* reason = "";
            
            if (stats.failedSends > 100) {
                shouldDisconnect = true;
                reason = "too many consecutive failures (>100)";
            } else if (timeSinceLastSend > 30000) {
                shouldDisconnect = true;
                reason = "blocked for >30 seconds";
            } else if (timeSinceLastSend > 10000 && stats.failedSends > 10) {
                shouldDisconnect = true;
                reason = "blocked for >10s with failures";
            }
            
            if (shouldDisconnect) {
                Serial.printf("[TCP] Disconnecting %s: %s (fails=%u, blocked=%ums)\n", 
                             client->remoteIP().toString().c_str(),
                             reason,
                             stats.failedSends,
                             timeSinceLastSend);
                
                client->close();
                clientStats.erase(client);
                it = clients.erase(it);
                errorCount++;
                continue;
            }
            
            // Sinon, skip ce message pour ce client
            ++it;
            continue;
        }
        
        // ═══════════════════════════════════════════════════════════════
        // Client prêt - envoyer les données
        // ═══════════════════════════════════════════════════════════════
        size_t written = client->write(buffer, sendLen);
        
        if (written == sendLen) {
            // Succès complet
            stats.failedSends = 0;  // Reset compteur d'échecs
            stats.lastSend = now;
            stats.totalSent++;
            sentCount++;
        } else {
            // Écriture partielle ou échec
            Serial.printf("[TCP] ⚠️  Partial write to %s (%u/%u bytes)\n", 
                         client->remoteIP().toString().c_str(), written, sendLen);
            stats.failedSends++;
            errorCount++;
        }
        
        ++it;
    }
    
    // ═══════════════════════════════════════════════════════════════
    // NOUVEAU: Logging périodique intelligent (évite le spam)
    // ═══════════════════════════════════════════════════════════════
    static uint32_t lastLog = 0;
    static uint32_t broadcastCount = 0;
    static uint32_t totalSent = 0;
    static uint32_t totalSkipped = 0;
    static uint32_t totalErrors = 0;
    
    broadcastCount++;
    totalSent += sentCount;
    totalSkipped += skippedCount;
    totalErrors += errorCount;
    
    // Log toutes les 30 secondes
    if (now - lastLog > 30000) {
        if (clients.size() > 0) {
            Serial.println("\n[TCP] ═══════ Broadcast Stats (30s) ═══════");
            Serial.printf("[TCP] Messages: %u broadcasts to %d clients\n", 
                         broadcastCount, clients.size());
            Serial.printf("[TCP] Sent: %u (%.1f%%)\n", 
                         totalSent, 
                         (float)totalSent / (broadcastCount * clients.size()) * 100.0f);
            
            if (totalSkipped > 0) {
                Serial.printf("[TCP] ⚠️  Skipped: %u (%.1f%%) - clients too slow\n", 
                             totalSkipped,
                             (float)totalSkipped / (broadcastCount * clients.size()) * 100.0f);
            }
            
            if (totalErrors > 0) {
                Serial.printf("[TCP] ❌ Errors: %u\n", totalErrors);
            }
            
            // Stats détaillées par client
            for (auto client : clients) {
                auto& stats = clientStats[client];
                uint32_t age = now - stats.lastSend;
                
                if (stats.totalSkipped > 0 || age > 5000) {
                    Serial.printf("[TCP]   Client %s: sent=%u, skipped=%u, age=%ums",
                                 client->remoteIP().toString().c_str(),
                                 stats.totalSent, stats.totalSkipped, age);
                    
                    if (stats.failedSends > 0) {
                        Serial.printf(", current_fails=%u", stats.failedSends);
                    }
                    Serial.println();
                }
            }
            
            Serial.println("[TCP] ════════════════════════════════════\n");
        }
        
        lastLog = now;
        broadcastCount = 0;
        totalSent = 0;
        totalSkipped = 0;
        totalErrors = 0;
    }
    
    xSemaphoreGive(clientsMutex);
}

size_t TCPServer::getClientCount() {
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    size_t count = clients.size();
    xSemaphoreGive(clientsMutex);
    return count;
}

// ═══════════════════════════════════════════════════════════════
// NOUVEAU: Méthode pour obtenir les stats d'un client
// ═══════════════════════════════════════════════════════════════
bool TCPServer::getClientStats(AsyncClient* client, ClientStats& stats) {
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    
    auto it = clientStats.find(client);
    if (it != clientStats.end()) {
        stats = it->second;
        xSemaphoreGive(clientsMutex);
        return true;
    }
    
    xSemaphoreGive(clientsMutex);
    return false;
}