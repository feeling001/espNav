#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <LittleFS.h>

#include "config.h"
#include "types.h"
#include "boat_state.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "uart_handler.h"
// #include "seatalk1_handler.h"
#include "nmea_parser.h"
#include "tcp_server.h"
#include "web_server.h"
#include "ble_manager.h"


// Global instances
ConfigManager configManager;
BoatState boatState;
WiFiManager wifiManager;
UARTHandler uartHandler;
TCPServer tcpServer;
BLEManager bleManager;
NMEAParser nmeaParser(&boatState);
WebServer webServer(&configManager, &wifiManager, &tcpServer, &uartHandler, &nmeaParser, &boatState, &bleManager);

// Message queue for NMEA sentences
QueueHandle_t nmeaQueue;

// Task handles
TaskHandle_t uartReaderTaskHandle;
TaskHandle_t processorTaskHandle;
TaskHandle_t wifiTaskHandle;

// Forward declarations
void uartReaderTask(void* parameter);
void processorTask(void* parameter);
void wifiTask(void* parameter);

// Global variables for system monitoring
volatile uint32_t g_nmeaQueueOverflows = 0;
volatile uint32_t g_nmeaQueueFullEvents = 0;
volatile uint32_t g_messagesRead = 0;
volatile uint32_t g_messagesProcessed = 0;


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

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n======================================");
    Serial.println("   Marine Gateway - ESP32-S3");
    Serial.println("   Version: " VERSION);
    Serial.println("   Dual-Core Optimized");
    Serial.println("======================================\n");
    
    // Mount LittleFS
    Serial.println("[LittleFS] Initializing filesystem...");
    if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
        Serial.println("[LittleFS] Mount failed, attempting format...");
        if (LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
            Serial.println("[LittleFS] ✓ Formatted and mounted");
        } else {
            Serial.println("[LittleFS] ❌ Format failed");
        }
    } else {
        Serial.println("[LittleFS] ✓ Mounted successfully");
    }
    
    listLittleFSFiles("/", 2);
    
    if (!LittleFS.exists("/www/index.html")) {
        Serial.println("[LittleFS] ⚠️  Web dashboard not found");
    } else {
        Serial.println("[LittleFS] ✓ Web dashboard present");
    }
    
    if (!LittleFS.exists("/www/ble-config.html")) {
        Serial.println("[LittleFS] ⚠️  BLE config page not found");
    } else {
        Serial.println("[LittleFS] ✓ BLE config page present");
    }
    
    Serial.println("\n======================================\n");
    
    // Initialize configuration manager
    Serial.println("[Config] Initializing...");
    configManager.init();
    
    // Initialize boat state
    boatState.init();

    // Load WiFi config
    WiFiConfig wifiConfig;
    configManager.getWiFiConfig(wifiConfig);
    Serial.printf("[Config] WiFi: %s (%s mode)\n", 
                  wifiConfig.ssid, 
                  wifiConfig.mode == 0 ? "Station" : "AP");
    
    // Load Serial config
    UARTConfig serialConfig;
    configManager.getSerialConfig(serialConfig);
    Serial.printf("[Config] UART: %u baud\n", serialConfig.baudRate);
    
    // Load BLE config
    BLEConfigData bleConfig;
    configManager.getBLEConfig(bleConfig);
    Serial.printf("[Config] BLE: %s (%s)\n", 
                  bleConfig.device_name,
                  bleConfig.enabled ? "Enabled" : "Disabled");
    
    // Initialize WiFi
    Serial.println("\n[WiFi] Initializing...");
    wifiManager.init(wifiConfig);
    wifiManager.start();
    
    // Initialize UART
    Serial.println("\n[UART] Initializing...");
    uartHandler.init(serialConfig);
    uartHandler.start();
    
    // Initialize TCP server
    Serial.println("\n[TCP] Initializing...");
    tcpServer.init(TCP_PORT);
    
    // Initialize BLE Manager
    Serial.println("\n[BLE] Initializing...");
    BLEConfig bleManagerConfig;
    bleManagerConfig.enabled = bleConfig.enabled;
    strncpy(bleManagerConfig.device_name, bleConfig.device_name, sizeof(bleManagerConfig.device_name) - 1);
    strncpy(bleManagerConfig.pin_code, bleConfig.pin_code, sizeof(bleManagerConfig.pin_code) - 1);
    bleManager.init(bleManagerConfig, &boatState);
    
    if (bleConfig.enabled) {
        bleManager.start();
    }
    
    // Initialize Web server
    Serial.println("\n[Web] Initializing...");
    webServer.init();
    
    // Create NMEA queue
    Serial.println("\n[NMEA] Creating queue...");
    nmeaQueue = xQueueCreate(NMEA_QUEUE_SIZE, sizeof(NMEASentence));
    if (nmeaQueue == NULL) {
        Serial.println("[NMEA] ❌ Queue creation failed!");
    } else {
        Serial.printf("[NMEA] ✓ Queue created (size: %d)\n", NMEA_QUEUE_SIZE);
    }
    
    // ═══════════════════════════════════════════════════════════════
    // Create dual-core tasks
    // ═══════════════════════════════════════════════════════════════
    Serial.println("\n[Tasks] Creating dual-core FreeRTOS tasks...");
    
    // CORE 0: UART Reader - High priority, real-time I/O
    BaseType_t readerResult = xTaskCreatePinnedToCore(
        uartReaderTask,           // Task function
        "UART_Reader",            // Task name
        4096,                     // Stack size (4KB)
        NULL,                     // Parameters
        5,                        // Priority HIGH (5)
        &uartReaderTaskHandle,    // Task handle
        0                         // ⭐ Core 0 - Dedicated to I/O
    );
    
    // CORE 1: Processor - Normal priority, handles broadcasts
    BaseType_t processorResult = xTaskCreatePinnedToCore(
        processorTask,            // Task function
        "Processor",              // Task name
        8192,                     // Stack size (8KB - larger for broadcasts)
        NULL,                     // Parameters
        3,                        // Priority NORMAL (3)
        &processorTaskHandle,     // Task handle
        1                         // ⭐ Core 1 - With WiFi stack
    );
    
    // CORE 1: WiFi Monitor - Low priority
    BaseType_t wifiResult = xTaskCreatePinnedToCore(
        wifiTask,                 // Task function
        "WiFi",                   // Task name
        4096,                     // Stack size (4KB)
        NULL,                     // Parameters
        2,                        // Priority LOW (2)
        &wifiTaskHandle,          // Task handle
        1                         // ⭐ Core 1 - With WiFi stack
    );
    
    if (readerResult == pdPASS) {
        Serial.println("[Tasks] ✓ UART Reader task created (Core 0)");
    } else {
        Serial.println("[Tasks] ❌ UART Reader task failed");
    }
    
    if (processorResult == pdPASS) {
        Serial.println("[Tasks] ✓ Processor task created (Core 1)");
    } else {
        Serial.println("[Tasks] ❌ Processor task failed");
    }
    
    if (wifiResult == pdPASS) {
        Serial.println("[Tasks] ✓ WiFi task created (Core 1)");
    } else {
        Serial.println("[Tasks] ❌ WiFi task failed");
    }
    
    // Wait for WiFi
    Serial.println("\n[WiFi] Waiting for connection...");
    delay(5000);
    
    // Start servers
    Serial.println("\n[TCP] Starting server...");
    tcpServer.start();
    
    Serial.println("\n[Web] Starting server...");
    webServer.start();
    
    Serial.println("\n======================================");
    Serial.println("✓ Initialization complete!");
    Serial.println("  Architecture: Dual-Core");
    Serial.println("  Core 0: UART Reader (High Priority)");
    Serial.println("  Core 1: Processor + WiFi");
    Serial.println("======================================\n");
    
    // Print connection info
    Serial.println("Connection Information:");
    Serial.println("----------------------");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Web: http://%s/\n", WiFi.localIP().toString().c_str());
        Serial.printf("TCP: %s:%d\n", WiFi.localIP().toString().c_str(), TCP_PORT);
    } else {
        Serial.println("WiFi not connected - check configuration");
    }
    Serial.println("----------------------\n");
}

void loop() {
    // Check for autopilot commands from BLE
    if (bleManager.hasAutopilotCommand()) {
        AutopilotCommand cmd = bleManager.getAutopilotCommand();
        
        Serial.printf("[BLE] Autopilot command received: %d\n", cmd.type);
        
        // TODO: Process autopilot command and send to SeaTalk1
        // This will be implemented when SeaTalk1 integration is added
        switch (cmd.type) {
            case AutopilotCommand::ENABLE:
                Serial.println("[Autopilot] Command: Enable");
                break;
            case AutopilotCommand::DISABLE:
                Serial.println("[Autopilot] Command: Disable");
                break;
            case AutopilotCommand::ADJUST_PLUS_10:
                Serial.println("[Autopilot] Command: +10 degrees");
                break;
            case AutopilotCommand::ADJUST_MINUS_10:
                Serial.println("[Autopilot] Command: -10 degrees");
                break;
            case AutopilotCommand::ADJUST_PLUS_1:
                Serial.println("[Autopilot] Command: +1 degree");
                break;
            case AutopilotCommand::ADJUST_MINUS_1:
                Serial.println("[Autopilot] Command: -1 degree");
                break;
            default:
                break;
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
}

// ═══════════════════════════════════════════════════════════════
// CORE 0: UART Reader Task
// High-priority, real-time task dedicated to reading UART
// ═══════════════════════════════════════════════════════════════
void uartReaderTask(void* parameter) {
    char lineBuffer[NMEA_MAX_LENGTH];
    NMEASentence sentence;
    
    Serial.println("[UART Reader] Started on Core 0 - High Priority");
    
    uint32_t lastStatsTime = millis();
    uint32_t sentencesRead = 0;
    uint32_t parseErrors = 0;
    uint32_t queueFullCount = 0;
    
    while (true) {
        // Read line from UART with reasonable timeout
        if (uartHandler.readLine(lineBuffer, sizeof(lineBuffer), pdMS_TO_TICKS(100))) {
            
            // Parse NMEA sentence
            if (nmeaParser.parseLine(lineBuffer, sentence)) {
                sentencesRead++;
                g_messagesRead++;
                
                // ═══════════════════════════════════════════════════════════════
                // Send to queue for processing (non-blocking with short timeout)
                // ═══════════════════════════════════════════════════════════════
                if (nmeaQueue != NULL) {
                    if (xQueueSend(nmeaQueue, &sentence, pdMS_TO_TICKS(5)) != pdTRUE) {
                        queueFullCount++;
                        g_nmeaQueueOverflows++;
                    }
                }
            } else {
                parseErrors++;
            }
            
            // Yield periodically to let other tasks run
            static uint8_t yieldCounter = 0;
            if (++yieldCounter >= 5) {
                yieldCounter = 0;
                taskYIELD();
            }
        }
        
        // Print statistics every 30 seconds
        if (millis() - lastStatsTime > 30000) {
            Serial.println("\n[UART Reader] ════════ Core 0 Stats ════════");
            Serial.printf("[UART Reader] Sentences read: %u\n", sentencesRead);
            Serial.printf("[UART Reader] Parse errors: %u\n", parseErrors);
            
            if (queueFullCount > 0) {
                float dropRate = (float)queueFullCount / sentencesRead * 100.0f;
                Serial.printf("[UART Reader] ⚠️  Queue full events: %u (%.1f%%)\n", 
                             queueFullCount, dropRate);
                g_nmeaQueueFullEvents = queueFullCount;
            } else {
                Serial.println("[UART Reader] ✅ No queue overflows");
                g_nmeaQueueFullEvents = 0;
            }
            
            // Queue health
            UBaseType_t queueLevel = uxQueueMessagesWaiting(nmeaQueue);
            UBaseType_t queueSpaces = uxQueueSpacesAvailable(nmeaQueue);
            Serial.printf("[UART Reader] Queue: %u/%d used (%.1f%% full)\n", 
                         queueLevel, NMEA_QUEUE_SIZE, 
                         (float)queueLevel / NMEA_QUEUE_SIZE * 100.0f);
            
            Serial.println("[UART Reader] ════════════════════════════════\n");
            
            lastStatsTime = millis();
            sentencesRead = 0;
            parseErrors = 0;
            queueFullCount = 0;
        }
        
        // Minimal delay - stay responsive
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ═══════════════════════════════════════════════════════════════
// CORE 1: Processor Task
// Normal-priority task that processes queue and broadcasts
// ═══════════════════════════════════════════════════════════════
void processorTask(void* parameter) {
    NMEASentence sentence;
    
    Serial.println("[Processor] Started on Core 1 - Normal Priority");
    
    uint32_t lastStatsTime = millis();
    uint32_t messagesProcessed = 0;
    uint32_t tcpBroadcasts = 0;
    uint32_t wsBroadcasts = 0;
    uint32_t tcpSkipped = 0;
    
    while (true) {
        // ═══════════════════════════════════════════════════════════════
        // Wait for messages from queue (blocking)
        // ═══════════════════════════════════════════════════════════════
        if (xQueueReceive(nmeaQueue, &sentence, pdMS_TO_TICKS(100)) == pdTRUE) {
            messagesProcessed++;
            g_messagesProcessed++;
            
            // ═══════════════════════════════════════════════════════════════
            // Broadcast to TCP clients (can be slow if clients are slow)
            // ═══════════════════════════════════════════════════════════════
            if (tcpServer.getClientCount() > 0) {
                tcpServer.broadcast(sentence.raw);
                tcpBroadcasts++;
            }
            
            // ═══════════════════════════════════════════════════════════════
            // Broadcast to WebSocket clients (non-blocking by design)
            // ═══════════════════════════════════════════════════════════════
            webServer.broadcastNMEA(sentence.raw);
            wsBroadcasts++;
            
            // Yield every 10 messages to let other tasks run
            static uint8_t yieldCounter = 0;
            if (++yieldCounter >= 10) {
                yieldCounter = 0;
                taskYIELD();
            }
        }
        
        // Print statistics every 30 seconds
        if (millis() - lastStatsTime > 30000) {
            Serial.println("\n[Processor] ════════ Core 1 Stats ════════");
            Serial.printf("[Processor] Messages processed: %u\n", messagesProcessed);
            Serial.printf("[Processor] TCP broadcasts: %u\n", tcpBroadcasts);
            Serial.printf("[Processor] WebSocket broadcasts: %u\n", wsBroadcasts);
            
            if (tcpSkipped > 0) {
                Serial.printf("[Processor] ⚠️  TCP skipped: %u\n", tcpSkipped);
            }
            
            // Overall system health
            Serial.printf("[Processor] TCP clients: %u\n", tcpServer.getClientCount());
            
            if (bleManager.isEnabled()) {
                Serial.printf("[Processor] BLE devices: %u\n", bleManager.getConnectedDevices());
            }
            
            Serial.printf("[Processor] Valid sentences: %u\n", nmeaParser.getValidSentences());
            Serial.printf("[Processor] Invalid sentences: %u\n", nmeaParser.getInvalidSentences());
            
            // Processing rate
            if (messagesProcessed > 0) {
                float rate = messagesProcessed / 30.0f;
                Serial.printf("[Processor] Processing rate: %.1f msg/sec\n", rate);
            }
            
            // Lag detection
            UBaseType_t queueLevel = uxQueueMessagesWaiting(nmeaQueue);
            if (queueLevel > NMEA_QUEUE_SIZE / 2) {
                Serial.printf("[Processor] ⚠️  Queue building up: %u/%d\n", 
                             queueLevel, NMEA_QUEUE_SIZE);
            } else if (queueLevel > 10) {
                Serial.printf("[Processor] 🟡 Queue level: %u/%d\n", 
                             queueLevel, NMEA_QUEUE_SIZE);
            } else {
                Serial.println("[Processor] ✅ Queue healthy");
            }
            
            Serial.println("[Processor] ═══════════════════════════════\n");
            
            lastStatsTime = millis();
            messagesProcessed = 0;
            tcpBroadcasts = 0;
            wsBroadcasts = 0;
            tcpSkipped = 0;
        }
        
        // Short delay
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ═══════════════════════════════════════════════════════════════
// CORE 1: WiFi Task
// Low-priority task for monitoring WiFi connection
// ═══════════════════════════════════════════════════════════════
void wifiTask(void* parameter) {
    Serial.println("[WiFi Task] Started on Core 1 - Low Priority");
    
    WiFiState lastState = WIFI_DISCONNECTED;
    
    while (true) {
        wifiManager.update();
        
        WiFiState currentState = wifiManager.getState();
        
        if (currentState != lastState) {
            switch (currentState) {
                case WIFI_CONNECTED_STA:
                    Serial.printf("[WiFi] ✓ Connected to %s\n", wifiManager.getSSID().c_str());
                    Serial.printf("[WiFi] IP: %s, RSSI: %d dBm\n", 
                                 wifiManager.getIP().toString().c_str(),
                                 wifiManager.getRSSI());
                    break;
                    
                case WIFI_AP_MODE:
                    Serial.printf("[WiFi] ✓ AP Mode: %s\n", wifiManager.getSSID().c_str());
                    Serial.printf("[WiFi] IP: %s\n", wifiManager.getIP().toString().c_str());
                    break;
                    
                case WIFI_DISCONNECTED:
                    Serial.println("[WiFi] Disconnected");
                    break;
                    
                case WIFI_CONNECTING:
                    Serial.println("[WiFi] Connecting...");
                    break;
                    
                case WIFI_RECONNECTING:
                    Serial.println("[WiFi] Reconnecting...");
                    break;
            }
            
            lastState = currentState;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
