#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <LittleFS.h>

#include "config.h"
#include "types.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "uart_handler.h"
#include "nmea_parser.h"
#include "tcp_server.h"
#include "web_server.h"

// ============================================================
// Global instances
// ============================================================
ConfigManager configManager;
WiFiManager wifiManager;
UARTHandler uartHandler;
NMEAParser nmeaParser;
TCPServer tcpServer;
WebServer webServer(&configManager, &wifiManager);

// Message queue for NMEA sentences
QueueHandle_t nmeaQueue;

// Task handles
TaskHandle_t nmeaTaskHandle;
TaskHandle_t wifiTaskHandle;

// ============================================================
// Forward declarations
// ============================================================
void nmeaTask(void* parameter);
void wifiTask(void* parameter);

// ============================================================
// Utility function to list LittleFS files
// ============================================================
void listLittleFSFiles(const char* dirname, uint8_t levels) {
    Serial.printf("[LittleFS] Listing directory: %s\n", dirname);
    
    File root = LittleFS.open(dirname);
    if (!root) {
        Serial.println("[LittleFS] Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("[LittleFS] Not a directory");
        return;
    }
    
    File file = root.openNextFile();
    int fileCount = 0;
    size_t totalSize = 0;
    
    while (file) {
        if (file.isDirectory()) {
            Serial.printf("  [DIR]  %s\n", file.name());
            if (levels) {
                listLittleFSFiles(file.path(), levels - 1);
            }
        } else {
            Serial.printf("  [FILE] %s (%zu bytes)\n", file.name(), file.size());
            fileCount++;
            totalSize += file.size();
        }
        file = root.openNextFile();
    }
    
    Serial.printf("[LittleFS] Total: %d files, %zu bytes\n", fileCount, totalSize);
}

// ============================================================
// Setup
// ============================================================
void setup() {
    // Initialize Serial for debugging
    Serial.begin(115200);
    delay(1000);  // Give serial time to initialize
    
    Serial.println("\n\n======================================");
    Serial.println("   Marine Gateway - ESP32-S3");
    Serial.println("   Version: " VERSION);
    Serial.println("======================================\n");
    
    // ============================================================
    // CRITICAL: Mount LittleFS FIRST before anything else
    // ============================================================
    Serial.println("[LittleFS] Initializing filesystem...");
    
    // Try to mount LittleFS
    if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
        Serial.println("[LittleFS] ❌ MOUNT FAILED!");
        Serial.println("[LittleFS] Possible reasons:");
        Serial.println("  1. Filesystem not formatted");
        Serial.println("  2. Partition table incorrect");
        Serial.println("  3. Flash memory issue");
        Serial.println("  4. Files not uploaded");
        Serial.println("");
        Serial.println("[LittleFS] Attempting to format and mount...");
        
        // Try to format and mount
        if (LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
            Serial.println("[LittleFS] ✓ Formatted and mounted successfully");
        } else {
            Serial.println("[LittleFS] ❌ Format and mount FAILED!");
            Serial.println("[LittleFS] System will continue but web interface will not work");
        }
    } else {
        Serial.println("[LittleFS] ✓ Mounted successfully");
    }
    
    // List LittleFS contents
    Serial.println("\n[LittleFS] Contents:");
    listLittleFSFiles("/", 2);
    
    // Check for web dashboard files
    Serial.println("\n[LittleFS] Checking for web dashboard...");
    if (!LittleFS.exists("/www/index.html")) {
        Serial.println("[LittleFS] ⚠️  Web dashboard not found!");
        Serial.println("[LittleFS] Web interface will not work.");
        Serial.println("[LittleFS] To fix: ");
        Serial.println("[LittleFS]   1. cd web-dashboard && npm run build");
        Serial.println("[LittleFS]   2. pio run -t uploadfs");
    } else {
        Serial.println("[LittleFS] ✓ Web dashboard files present");
    }
    
    Serial.println("\n======================================\n");
    
    // ============================================================
    // Initialize all components
    // ============================================================
    
    // Initialize configuration manager
    Serial.println("[Config] Initializing configuration manager...");
    configManager.init();
    
    // Load WiFi configuration
    WiFiConfig wifiConfig;
    configManager.getWiFiConfig(wifiConfig);
    Serial.printf("[Config] WiFi SSID: %s\n", wifiConfig.ssid);
    Serial.printf("[Config] WiFi Mode: %s\n", wifiConfig.mode == 0 ? "Station" : "Access Point");
    
    // Load Serial configuration
    UARTConfig serialConfig;
    configManager.getSerialConfig(serialConfig);
    Serial.printf("[Config] Serial Baud: %u\n", serialConfig.baudRate);
    
    // Initialize WiFi manager
    Serial.println("\n[WiFi] Initializing WiFi manager...");
    wifiManager.init(wifiConfig);
    wifiManager.start();
    
    // Initialize UART handler
    Serial.println("\n[UART] Initializing UART handler...");
    uartHandler.init(serialConfig);
    uartHandler.start();
    
    // Initialize TCP server
    Serial.println("\n[TCP] Initializing TCP server...");
    tcpServer.init(TCP_PORT);
    
    // Initialize Web server (LittleFS already mounted)
    Serial.println("\n[Web] Initializing web server...");
    webServer.init();
    
    // Create NMEA message queue
    Serial.println("\n[NMEA] Creating message queue...");
    nmeaQueue = xQueueCreate(NMEA_QUEUE_SIZE, sizeof(NMEASentence));
    if (nmeaQueue == NULL) {
        Serial.println("[NMEA] ❌ Failed to create queue!");
    } else {
        Serial.println("[NMEA] ✓ Queue created successfully");
    }
    
    // Create FreeRTOS tasks
    Serial.println("\n[Tasks] Creating FreeRTOS tasks...");
    
    BaseType_t nmeaTaskResult = xTaskCreate(
        nmeaTask, 
        "NMEA", 
        TASK_STACK_NMEA, 
        NULL, 
        TASK_PRIORITY_NMEA, 
        &nmeaTaskHandle
    );
    
    BaseType_t wifiTaskResult = xTaskCreate(
        wifiTask, 
        "WiFi", 
        TASK_STACK_WIFI, 
        NULL, 
        TASK_PRIORITY_WIFI, 
        &wifiTaskHandle
    );
    
    if (nmeaTaskResult == pdPASS) {
        Serial.println("[Tasks] ✓ NMEA task created");
    } else {
        Serial.println("[Tasks] ❌ NMEA task creation failed!");
    }
    
    if (wifiTaskResult == pdPASS) {
        Serial.println("[Tasks] ✓ WiFi task created");
    } else {
        Serial.println("[Tasks] ❌ WiFi task creation failed!");
    }
    
    // Wait for WiFi to connect
    Serial.println("\n[WiFi] Waiting for connection...");
    delay(5000);
    
    // Start TCP server
    Serial.println("\n[TCP] Starting TCP server...");
    tcpServer.start();
    
    // Start Web server
    Serial.println("\n[Web] Starting web server...");
    webServer.start();
    
    Serial.println("\n======================================");
    Serial.println("✓ Initialization complete!");
    Serial.println("======================================\n");
    
    // Print connection info
    Serial.println("Connection Information:");
    Serial.println("----------------------");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Web Interface: http://%s/\n", WiFi.localIP().toString().c_str());
        Serial.printf("TCP Server: %s:%d\n", WiFi.localIP().toString().c_str(), TCP_PORT);
    } else {
        Serial.println("WiFi: Not connected (check AP mode)");
        if (wifiManager.getState() == WIFI_AP_MODE) {
            Serial.printf("AP Mode SSID: %s\n", wifiManager.getSSID().c_str());
            Serial.printf("AP IP Address: %s\n", wifiManager.getIP().toString().c_str());
            Serial.printf("Web Interface: http://%s/\n", wifiManager.getIP().toString().c_str());
        }
    }
    Serial.println("----------------------\n");
}

// ============================================================
// Main loop (empty - tasks handle everything)
// ============================================================
void loop() {
    // Main loop can be empty (tasks handle everything)
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ============================================================
// NMEA Task Implementation
// ============================================================
void nmeaTask(void* parameter) {
    char lineBuffer[NMEA_MAX_LENGTH];
    NMEASentence sentence;
    
    Serial.println("[NMEA Task] Started and ready to read UART");
    Serial.printf("[NMEA Task] Waiting for data on UART%d (RX: GPIO%d, TX: GPIO%d)...\n", 
                  UART_NUM, UART_RX_PIN, UART_TX_PIN);
    
    uint32_t lastStatsTime = millis();
    
    while (true) {
        // Try to read a line from UART with 1 second timeout
        if (uartHandler.readLine(lineBuffer, sizeof(lineBuffer), pdMS_TO_TICKS(1000))) {
            Serial.printf("[NMEA Task] Received: %s\n", lineBuffer);
            
            // Parse the NMEA sentence
            if (nmeaParser.parseLine(lineBuffer, sentence)) {
                Serial.printf("[NMEA Task] ✓ Valid %s sentence\n", sentence.type);
                
                // Send to queue for potential future use
                if (nmeaQueue != NULL) {
                    if (xQueueSend(nmeaQueue, &sentence, 0) != pdTRUE) {
                        Serial.println("[NMEA Task] ⚠️  Queue full, sentence dropped");
                    }
                }
                
                // Broadcast via TCP server
                tcpServer.broadcast(sentence.raw);
                
                // Broadcast via WebSocket
                webServer.broadcastNMEA(sentence.raw);
                
            } else {
                Serial.printf("[NMEA Task] ❌ Invalid sentence: %s\n", lineBuffer);
            }
        }
        
        // Print statistics every 10 seconds
        if (millis() - lastStatsTime > 10000) {
            Serial.println("\n[NMEA Task] === Statistics ===");
            Serial.printf("[NMEA Task] UART received: %u sentences\n", uartHandler.getSentencesReceived());
            Serial.printf("[NMEA Task] UART errors: %u\n", uartHandler.getErrors());
            Serial.printf("[NMEA Task] Valid sentences: %u\n", nmeaParser.getValidSentences());
            Serial.printf("[NMEA Task] Invalid sentences: %u\n", nmeaParser.getInvalidSentences());
            Serial.printf("[NMEA Task] TCP clients: %u\n", tcpServer.getClientCount());
            Serial.println("[NMEA Task] ==================\n");
            lastStatsTime = millis();
        }
        
        // Small delay to prevent CPU hogging
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ============================================================
// WiFi Task Implementation
// ============================================================
void wifiTask(void* parameter) {
    Serial.println("[WiFi Task] Started");
    
    while (true) {
        // WiFi manager handles reconnection automatically
        // This task can be used for monitoring or periodic checks
        
        // Check WiFi status periodically
        static WiFiState lastState = WIFI_DISCONNECTED;
        WiFiState currentState = wifiManager.getState();
        
        if (currentState != lastState) {
            Serial.printf("[WiFi Task] State changed: %d -> %d\n", lastState, currentState);
            
            if (currentState == WIFI_CONNECTED_STA) {
                Serial.printf("[WiFi Task] ✓ Connected to %s\n", wifiManager.getSSID().c_str());
                Serial.printf("[WiFi Task] IP: %s\n", wifiManager.getIP().toString().c_str());
                Serial.printf("[WiFi Task] RSSI: %d dBm\n", wifiManager.getRSSI());
            } else if (currentState == WIFI_AP_MODE) {
                Serial.printf("[WiFi Task] ✓ AP Mode: %s\n", wifiManager.getSSID().c_str());
                Serial.printf("[WiFi Task] IP: %s\n", wifiManager.getIP().toString().c_str());
            }
            
            lastState = currentState;
        }
        
        // Sleep for 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}