#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "config.h"
#include "types.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "uart_handler.h"
#include "nmea_parser.h"
#include "tcp_server.h"
#include "web_server.h"

// Global instances
ConfigManager configManager;
WiFiManager wifiManager;
UARTHandler uartHandler;
NMEAParser nmeaParser;
TCPServer tcpServer;
WebServer webServer;

// Message queue for NMEA sentences
QueueHandle_t nmeaQueue;

// Task handles
TaskHandle_t nmeaTaskHandle;
TaskHandle_t wifiTaskHandle;

// Forward declarations
void nmeaTask(void* parameter);
void wifiTask(void* parameter);

void setup() {


    // Initialize Serial for debugging
    Serial.begin(115200);
    delay(1000);  // Give serial time to initialize
    
    Serial.println("\n\n======================================");
    Serial.println("   Marine Gateway - ESP32-S3");
    Serial.println("   Version: " VERSION);
    Serial.println("======================================\n");
    
    // Initialize configuration manager
    configManager.init();
    
    // Load WiFi configuration
    WiFiConfig wifiConfig;
    configManager.getWiFiConfig(wifiConfig);
    
    // Load Serial configuration
    SerialConfig serialConfig;
    configManager.getSerialConfig(serialConfig);
    
    // Initialize WiFi manager
    wifiManager.init(wifiConfig);
    wifiManager.start();
    
    // Initialize UART handler
    uartHandler.init(serialConfig);
    uartHandler.start();
    
    // Initialize TCP server
    tcpServer.init(TCP_PORT);
    
    // Initialize Web server
    webServer.init(&configManager, &wifiManager, &uartHandler, &tcpServer);
    
    // Create NMEA message queue
    nmeaQueue = xQueueCreate(NMEA_QUEUE_SIZE, sizeof(NMEASentence));
    
    // Create tasks
    xTaskCreate(nmeaTask, "NMEA", TASK_STACK_NMEA, NULL, TASK_PRIORITY_NMEA, &nmeaTaskHandle);
    xTaskCreate(wifiTask, "WiFi", TASK_STACK_WIFI, NULL, TASK_PRIORITY_WIFI, &wifiTaskHandle);
    
    // Wait a bit for WiFi to connect
    delay(5000);
    
    // Start TCP server
    tcpServer.start();
    
    // Start Web server
    webServer.start();
    
    Serial.println("\n[Main] Initialization complete!");
    Serial.println("[Main] Waiting for NMEA data on UART1...\n");
    
    // Print memory info
    Serial.printf("[Main] Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("[Main] Heap size: %u bytes\n", ESP.getHeapSize());
    Serial.printf("[Main] Min free heap: %u bytes\n\n", ESP.getMinFreeHeap());
}

void loop() {
    // Main loop can be mostly empty since tasks handle everything
    
    // Just monitor heap periodically
    static uint32_t lastHeapPrint = 0;
    if (millis() - lastHeapPrint > 60000) {  // Every minute
        Serial.printf("[Main] Free heap: %u bytes, Min free: %u bytes\n", 
                     ESP.getFreeHeap(), ESP.getMinFreeHeap());
        lastHeapPrint = millis();
    }
    
    delay(1000);
}

// NMEA processing task
void nmeaTask(void* parameter) {
    char line[NMEA_MAX_LENGTH];
    NMEASentence sentence;
    
    Serial.println("[NMEA] Task started");
    
    while (true) {
        // Read line from UART
        if (uartHandler.readLine(line, sizeof(line), pdMS_TO_TICKS(1000))) {
            // Parse NMEA sentence
            if (nmeaParser.parseLine(line, sentence)) {
                // Valid NMEA sentence
                
                // Broadcast to TCP clients
                tcpServer.broadcast(sentence.raw, strlen(sentence.raw));
                
                // Broadcast to WebSocket clients
                webServer.broadcastNMEA(sentence.raw);
                
                // Optional: Print to serial for debugging (comment out in production)
                // Serial.printf("[NMEA] %s (Type: %s, Valid: %s)\n", 
                //              sentence.raw, sentence.type, sentence.valid ? "Yes" : "No");
                
                // Send to queue for further processing if needed
                xQueueSend(nmeaQueue, &sentence, 0);
            } else {
                // Invalid sentence
                Serial.printf("[NMEA] Invalid sentence: %s\n", line);
            }
        }
    }
}

// WiFi monitoring task
void wifiTask(void* parameter) {
    Serial.println("[WiFi] Task started");
    
    while (true) {
        // Update WiFi manager state machine
        wifiManager.update();
        
        // Print WiFi status periodically
        static uint32_t lastStatusPrint = 0;
        if (millis() - lastStatusPrint > 30000) {  // Every 30 seconds
            WiFiState state = wifiManager.getState();
            
            const char* stateStr = "Unknown";
            switch (state) {
                case WIFI_DISCONNECTED: stateStr = "Disconnected"; break;
                case WIFI_CONNECTING: stateStr = "Connecting"; break;
                case WIFI_CONNECTED_STA: stateStr = "Connected (STA)"; break;
                case WIFI_RECONNECTING: stateStr = "Reconnecting"; break;
                case WIFI_AP_MODE: stateStr = "AP Mode"; break;
            }
            
            Serial.printf("[WiFi] Status: %s, IP: %s\n", 
                         stateStr, wifiManager.getIP().toString().c_str());
            
            if (state == WIFI_CONNECTED_STA) {
                Serial.printf("[WiFi] RSSI: %d dBm\n", wifiManager.getRSSI());
            } else if (state == WIFI_AP_MODE) {
                Serial.printf("[WiFi] AP Clients: %d\n", wifiManager.getConnectedClients());
            }
            
            Serial.printf("[TCP] Clients: %d\n", tcpServer.getClientCount());
            Serial.printf("[NMEA] Valid: %u, Invalid: %u\n\n", 
                         nmeaParser.getValidSentences(),
                         nmeaParser.getInvalidSentences());
            
            lastStatusPrint = millis();
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
