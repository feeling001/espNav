#include "web_server.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "uart_handler.h"
#include "tcp_server.h"

WebServer::WebServer() 
    : server(NULL), wsNMEA(NULL), configManager(NULL), wifiManager(NULL),
      uartHandler(NULL), tcpServer(NULL), initialized(false), running(false) {}

WebServer::~WebServer() {
    stop();
    
    if (wsNMEA) {
        delete wsNMEA;
    }
    
    if (server) {
        delete server;
    }
}

void WebServer::init(ConfigManager* configMgr, WiFiManager* wifiMgr, 
                     UARTHandler* uart, TCPServer* tcp) {
    if (initialized) {
        Serial.println("[Web] Already initialized, skipping");
        return;
    }
    
    configManager = configMgr;
    wifiManager = wifiMgr;
    uartHandler = uart;
    tcpServer = tcp;
    
    // ============================================================
    // LittleFS is now mounted in main.cpp BEFORE this init() call
    // We just verify it's accessible
    // ============================================================
    Serial.println("[Web] Verifying LittleFS access...");
    
    // Check if LittleFS is mounted by trying to access it
    File root = LittleFS.open("/");
    if (!root) {
        Serial.println("[Web] ❌ LittleFS not accessible!");
        Serial.println("[Web] ERROR: LittleFS must be mounted before WebServer::init()");
        Serial.println("[Web] API will work but web interface will NOT be served");
    } else {
        Serial.println("[Web] ✓ LittleFS accessible");
        root.close();
        
        // Verify web files exist
        if (LittleFS.exists("/www/index.html") || LittleFS.exists("/index.html")) {
            Serial.println("[Web] ✓ Web files found");
        } else {
            Serial.println("[Web] ⚠ WARNING: Web files not found in LittleFS");
            Serial.println("[Web]   Expected: /www/index.html or /index.html");
            Serial.println("[Web]   Did you run: pio run -t uploadfs ?");
        }
    }
    
    // Create the async web server
    Serial.printf("[Web] Creating AsyncWebServer on port %d...\n", WEB_SERVER_PORT);
    server = new AsyncWebServer(WEB_SERVER_PORT);
    
    // Create WebSocket for NMEA streaming
    Serial.println("[Web] Creating WebSocket endpoint: /ws/nmea");
    wsNMEA = new AsyncWebSocket("/ws/nmea");
    
    // Set up WebSocket event handler
    wsNMEA->onEvent([this](AsyncWebSocket* srv, AsyncWebSocketClient* client,
                           AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->onWSEvent(srv, client, type, arg, data, len);
    });
    
    server->addHandler(wsNMEA);
    Serial.println("[Web] ✓ WebSocket handler registered");
    
    // Register all HTTP routes (API + static files)
    Serial.println("[Web] Registering HTTP routes...");
    registerRoutes();
    
    initialized = true;
    
    Serial.println("[Web] ✓ Server initialized successfully");
}

void WebServer::start() {
    if (!initialized) {
        Serial.println("[Web] ❌ Cannot start: not initialized");
        return;
    }
    
    if (running) {
        Serial.println("[Web] Already running, skipping start");
        return;
    }
    
    server->begin();
    running = true;
    
    Serial.printf("[Web] ✓ Server started on port %d\n", WEB_SERVER_PORT);
    Serial.println("[Web] Endpoints available:");
    Serial.println("[Web]   - GET  /                    (Web Interface)");
    Serial.println("[Web]   - GET  /api/status          (System Status)");
    Serial.println("[Web]   - GET  /api/config/wifi     (WiFi Config)");
    Serial.println("[Web]   - POST /api/config/wifi     (Set WiFi Config)");
    Serial.println("[Web]   - GET  /api/config/serial   (Serial Config)");
    Serial.println("[Web]   - POST /api/config/serial   (Set Serial Config)");
    Serial.println("[Web]   - POST /api/restart         (Restart Device)");
    Serial.println("[Web]   - WS   /ws/nmea             (NMEA Stream)");
}

void WebServer::stop() {
    if (!running) {
        return;
    }
    
    running = false;
    server->end();
    
    Serial.println("[Web] Server stopped");
}

void WebServer::registerRoutes() {
    // ============================================================
    // Static file serving from LittleFS
    // ============================================================
    Serial.println("[Web]   Registering static file handler: /www/");
    server->serveStatic("/", LittleFS, "/www/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=86400");
    
    // Fallback for root path if /www/ doesn't exist
    server->serveStatic("/", LittleFS, "/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=86400");
    
    // ============================================================
    // REST API endpoints
    // ============================================================
    Serial.println("[Web]   Registering API endpoints...");
    
    // WiFi Configuration
    server->on("/api/config/wifi", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetWiFiConfig(request);
    });
    
    server->on("/api/config/wifi", HTTP_POST, 
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, 
               size_t index, size_t total) {
            this->handlePostWiFiConfig(request, data, len);
        }
    );
    
    // Serial Configuration
    server->on("/api/config/serial", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetSerialConfig(request);
    });
    
    server->on("/api/config/serial", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            this->handlePostSerialConfig(request, data, len);
        }
    );
    
    // System Status
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetStatus(request);
    });
    
    // Restart Device
    server->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleRestart(request);
    });
    
    // 404 Handler
    server->onNotFound([](AsyncWebServerRequest* request) {
        Serial.printf("[Web] 404 Not Found: %s\n", request->url().c_str());
        request->send(404, "application/json", "{\"error\":\"Not Found\"}");
    });
    
    Serial.println("[Web]   ✓ All routes registered");
}

// WebSocket event handler
void WebServer::onWSEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WebSocket] Client #%u connected from %s\n", 
                         client->id(), client->remoteIP().toString().c_str());
            break;
            
        case WS_EVT_DISCONNECT:
            Serial.printf("[WebSocket] Client #%u disconnected\n", client->id());
            break;
            
        case WS_EVT_DATA:
            // We don't expect data from clients on NMEA websocket
            Serial.printf("[WebSocket] Unexpected data from client #%u\n", client->id());
            break;
            
        case WS_EVT_PONG:
            Serial.printf("[WebSocket] Pong from client #%u\n", client->id());
            break;
            
        case WS_EVT_ERROR:
            Serial.printf("[WebSocket] Error from client #%u\n", client->id());
            break;
    }
}

// Broadcast NMEA sentence to all WebSocket clients
void WebServer::broadcastNMEA(const char* sentence) {
    if (wsNMEA && running) {
        wsNMEA->textAll(sentence);
    }
}

// REST API Handlers implementation continues here...
// (handleGetWiFiConfig, handlePostWiFiConfig, handleGetSerialConfig, 
//  handlePostSerialConfig, handleGetStatus, handleRestart)
