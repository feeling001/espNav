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

// Function to list files in LittleFS
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
    
    // Try to mount LittleFS with format on failure
//    if (!LittleFS.begin(true)) {
     if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
        Serial.println("[LittleFS] ❌ MOUNT FAILED!");
        Serial.println("[LittleFS] Possible reasons:");
        Serial.println("  1. Filesystem not formatted");
        Serial.println("  2. Partition table incorrect");
        Serial.println("  3. Flash memory issue");
        Serial.println("  4. Filesystem not uploaded (run: pio run -t uploadfs)");
        Serial.println("[LittleFS] Attempting to format...");
        
        if (!LittleFS.format()) {
            Serial.println("[LittleFS] ❌ FORMAT FAILED!");
            Serial.println("[LittleFS] Web server will not serve files!");
        } else {
            Serial.println("[LittleFS] ✓ Format successful, retrying mount...");
            if (LittleFS.begin(true)) {
                Serial.println("[LittleFS] ✓ Mount successful after format");
            } else {
                Serial.println("[LittleFS] ❌ Mount still failed after format");
            }
        }
    } else {
        Serial.println("[LittleFS] ✓ Mounted successfully");
    }
    
    // Get filesystem info
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    Serial.printf("[LittleFS] Total space: %zu bytes (%.2f KB)\n", 
                  totalBytes, totalBytes / 1024.0);
    Serial.printf("[LittleFS] Used space: %zu bytes (%.2f KB)\n", 
                  usedBytes, usedBytes / 1024.0);
    Serial.printf("[LittleFS] Free space: %zu bytes (%.2f KB)\n", 
                  totalBytes - usedBytes, (totalBytes - usedBytes) / 1024.0);
    Serial.printf("[LittleFS] Usage: %.1f%%\n", 
                  (usedBytes * 100.0) / totalBytes);
    
    // List all files in the filesystem
    Serial.println("[LittleFS] Contents:");
    listLittleFSFiles("/", 2);
    
    // Check specifically for web files
    Serial.println("\n[LittleFS] Checking for web dashboard files...");
    const char* webFiles[] = {"/www/index.html", "/www/assets", "/index.html"};
    bool foundWebFiles = false;
    
    for (int i = 0; i < 3; i++) {
        if (LittleFS.exists(webFiles[i])) {
            Serial.printf("[LittleFS] ✓ Found: %s\n", webFiles[i]);
            foundWebFiles = true;
            
            if (String(webFiles[i]).endsWith(".html")) {
                File f = LittleFS.open(webFiles[i], "r");
                if (f) {
                    Serial.printf("[LittleFS]   Size: %zu bytes\n", f.size());
                    f.close();
                }
            }
        } else {
            Serial.printf("[LittleFS] ✗ Not found: %s\n", webFiles[i]);
        }
    }
    
    if (!foundWebFiles) {
        Serial.println("[LittleFS] ⚠ WARNING: No web dashboard files found!");
        Serial.println("[LittleFS] Web interface will not work.");
        Serial.println("[LittleFS] To fix: ");
        Serial.println("[LittleFS]   1. cd web-dashboard && npm run build");
        Serial.println("[LittleFS]   2. pio run -t uploadfs");
    } else {
        Serial.println("[LittleFS] ✓ Web dashboard files present");
    }
    
    Serial.println("\n======================================\n");
    
    // ============================================================
    // Now initialize other components
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
    webServer.init(&configManager, &wifiManager, &uartHandler, &tcpServer);
    
    // Create NMEA message queue
    Serial.println("\n[NMEA] Creating message queue...");
    nmeaQueue = xQueueCreate(NMEA_QUEUE_SIZE, sizeof(NMEASentence));
    if (nmeaQueue == NULL) {
        Serial.println("[NMEA] ❌ Failed to create queue!");
    } else {
        Serial.println("[NMEA] ✓ Queue created successfully");
    }
    
    // Create tasks
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
    
    // Wait a bit for WiFi to connect
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
    }
    Serial.println("----------------------\n");
}

void loop() {
    // Main loop can be empty (tasks handle everything)
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// Task implementations would follow here...
void nmeaTask(void* parameter) {
    // Implementation from original file
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void wifiTask(void* parameter) {
    // Implementation from original file
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
