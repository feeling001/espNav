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
#include "nmea_parser.h"
#include "tcp_server.h"
#include "web_server.h"
#include "ble_manager.h"

// Global instances
ConfigManager configManager;
BoatState boatState;
WiFiManager wifiManager;
UARTHandler uartHandler;
NMEAParser nmeaParser;
TCPServer tcpServer;
BLEManager bleManager;
WebServer webServer(&configManager, &wifiManager, &tcpServer, &uartHandler, &nmeaParser, &boatState, &bleManager);

// Message queue for NMEA sentences
QueueHandle_t nmeaQueue;

// Task handles
TaskHandle_t nmeaTaskHandle;
TaskHandle_t wifiTaskHandle;

// Forward declarations
void nmeaTask(void* parameter);
void wifiTask(void* parameter);

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
    
    // Create tasks
    Serial.println("\n[Tasks] Creating FreeRTOS tasks...");
    
    BaseType_t nmeaResult = xTaskCreate(nmeaTask, "NMEA", TASK_STACK_NMEA, NULL, TASK_PRIORITY_NMEA, &nmeaTaskHandle);
    BaseType_t wifiResult = xTaskCreate(wifiTask, "WiFi", TASK_STACK_WIFI, NULL, TASK_PRIORITY_WIFI, &wifiTaskHandle);
    
    if (nmeaResult == pdPASS) {
        Serial.println("[Tasks] ✓ NMEA task created");
    } else {
        Serial.println("[Tasks] ❌ NMEA task failed");
    }
    
    if (wifiResult == pdPASS) {
        Serial.println("[Tasks] ✓ WiFi task created");
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
    Serial.println("======================================\n");
    
    // Print connection info
    Serial.println("Connection Information:");
    Serial.println("----------------------");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Web: http://%s/\n", WiFi.localIP().toString().c_str());
        Serial.printf("TCP: %s:%d\n", WiFi.localIP().toString().c_str(), TCP_PORT);
    } else if (wifiManager.getState() == WIFI_AP_MODE) {
        Serial.printf("AP SSID: %s\n", wifiManager.getSSID().c_str());
        Serial.printf("AP IP: %s\n", wifiManager.getIP().toString().c_str());
        Serial.printf("Web: http://%s/\n", wifiManager.getIP().toString().c_str());
    } else {
        Serial.println("WiFi: Not connected");
    }
    
    if (bleConfig.enabled) {
        Serial.printf("BLE Device: %s\n", bleConfig.device_name);
        Serial.printf("BLE PIN: %s\n", bleConfig.pin_code);
    } else {
        Serial.println("BLE: Disabled");
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
                Serial.println("[Autopilot] Command: ENABLE");
                break;
            case AutopilotCommand::DISABLE:
                Serial.println("[Autopilot] Command: DISABLE");
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

// NMEA Task - Process incoming NMEA data
void nmeaTask(void* parameter) {
    char lineBuffer[NMEA_MAX_LENGTH];
    NMEASentence sentence;
    
    Serial.println("[NMEA Task] Started");
    
    uint32_t lastStatsTime = millis();
    uint32_t queueFullCount = 0;
    
    while (true) {
        // Read line from UART (1 second timeout)
        if (uartHandler.readLine(lineBuffer, sizeof(lineBuffer), pdMS_TO_TICKS(1000))) {
            
            // Parse NMEA sentence
            if (nmeaParser.parseLine(lineBuffer, sentence)) {
                
                // Try to send to queue (non-blocking)
                if (nmeaQueue != NULL) {
                    if (xQueueSend(nmeaQueue, &sentence, 0) != pdTRUE) {
                        queueFullCount++;
                    }
                }
                
                // Broadcast to TCP clients
                tcpServer.broadcast(sentence.raw);
                
                // Broadcast to WebSocket clients
                webServer.broadcastNMEA(sentence.raw);
            }
        }
        
        // Print statistics every 30 seconds
        if (millis() - lastStatsTime > 30000) {
            Serial.println("\n[NMEA] === Statistics ===");
            Serial.printf("[NMEA] Sentences received: %u\n", uartHandler.getSentencesReceived());
            Serial.printf("[NMEA] Valid: %u, Invalid: %u\n", 
                         nmeaParser.getValidSentences(), 
                         nmeaParser.getInvalidSentences());
            Serial.printf("[NMEA] TCP clients: %u\n", tcpServer.getClientCount());
            if (bleManager.isEnabled()) {
                Serial.printf("[NMEA] BLE devices: %u\n", bleManager.getConnectedDevices());
            }
            if (queueFullCount > 0) {
                Serial.printf("[NMEA] Queue full events: %u\n", queueFullCount);
                queueFullCount = 0;  // Reset counter
            }
            Serial.println("[NMEA] ==================\n");
            lastStatsTime = millis();
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// WiFi Task - Monitor WiFi connection
void wifiTask(void* parameter) {
    Serial.println("[WiFi Task] Started");
    
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
