#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
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
#include "polar.h"

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
volatile uint32_t g_nmeaQueueOverflows  = 0;
volatile uint32_t g_nmeaQueueFullEvents = 0;
volatile uint32_t g_messagesRead        = 0;
volatile uint32_t g_messagesProcessed   = 0;

// Shared serial mutex — prevents interleaved output from concurrent tasks
SemaphoreHandle_t g_serialMutex = nullptr;

// Thread-safe Serial printf helper
static void serialPrintf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (g_serialMutex && xSemaphoreTake(g_serialMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        DEBUG_SERIAL.print(buf);
        xSemaphoreGive(g_serialMutex);
    }
}

void listLittleFSFiles(const char* dirname, uint8_t levels) {
    Serial.printf("[LittleFS] Listing directory: %s\n", dirname);

    File root = LittleFS.open(dirname);
    if (!root || !root.isDirectory()) {
        Serial.println("[LittleFS] Failed to open directory");
        return;
    }

    File file = root.openNextFile();
    int fileCount = 0;
    size_t totalSize = 0;

    while (file) {
        if (file.isDirectory()) {
            Serial.printf("  [DIR]  %s\n", file.name());
            if (levels) listLittleFSFiles(file.path(), levels - 1);
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
    Serial.begin(115200);          // USB CDC
    DEBUG_SERIAL.begin(115200);    // UART0 — debug fiable
    delay(1000);
    
    // Create the serial mutex early so tasks can use serialPrintf()
    g_serialMutex = xSemaphoreCreateMutex();

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

    Serial.println("\n======================================\n");

    // Initialize configuration manager
    Serial.println("[Config] Initializing...");
    configManager.init();

    // Initialize boat state
    boatState.init();

    // Load polar diagram from LittleFS (non-fatal if absent)
    Serial.println("\n[Polar] Loading polar diagram...");
    if (LittleFS.exists(POLAR_FILE_PATH)) {
        if (boatState.polar.loadFromFile()) {
            Serial.printf("[Polar] ✓ Polar loaded: %u TWA × %u TWS  (file: %s)\n",
                          boatState.polar.twaCount(),
                          boatState.polar.twsCount(),
                          POLAR_FILE_PATH);
            Serial.printf("[Polar]   Wind speeds: %s kn\n",
                          boatState.polar.twsString().c_str());
        } else {
            Serial.println("[Polar] ✗ Failed to parse polar file — upload a new one via dashboard");
        }
    } else {
        Serial.printf("[Polar] No polar file at %s — upload via Performance page\n", POLAR_FILE_PATH);
    }

    // Load WiFi config
    WiFiConfig wifiConfig;
    configManager.getWiFiConfig(wifiConfig);
    Serial.printf("\n[Config] WiFi: %s (%s mode)\n",
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
    strncpy(bleManagerConfig.pin_code,    bleConfig.pin_code,    sizeof(bleManagerConfig.pin_code) - 1);
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

    // Create FreeRTOS tasks
    Serial.println("\n[Tasks] Creating dual-core FreeRTOS tasks...");

    BaseType_t readerResult = xTaskCreatePinnedToCore(
        uartReaderTask, "UART_Reader", 4096, NULL, 5, &uartReaderTaskHandle, 0);
    BaseType_t processorResult = xTaskCreatePinnedToCore(
        processorTask, "Processor", 8192, NULL, 3, &processorTaskHandle, 1);
    BaseType_t wifiResult = xTaskCreatePinnedToCore(
        wifiTask, "WiFi", 4096, NULL, 2, &wifiTaskHandle, 1);

    if (readerResult    == pdPASS) Serial.println("[Tasks] ✓ UART Reader task created (Core 0)");
    else                            Serial.println("[Tasks] ❌ UART Reader task failed");
    if (processorResult == pdPASS) Serial.println("[Tasks] ✓ Processor task created (Core 1)");
    else                            Serial.println("[Tasks] ❌ Processor task failed");
    if (wifiResult      == pdPASS) Serial.println("[Tasks] ✓ WiFi task created (Core 1)");
    else                            Serial.println("[Tasks] ❌ WiFi task failed");

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

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Web:  http://%s/\n",       WiFi.localIP().toString().c_str());
        Serial.printf("TCP:  %s:%d\n",            WiFi.localIP().toString().c_str(), TCP_PORT);
    } else {
        Serial.println("WiFi not connected — check configuration");
    }
}

void loop() {
    // Check for autopilot commands from BLE
    if (bleManager.hasAutopilotCommand()) {
        AutopilotCommand cmd = bleManager.getAutopilotCommand();
        serialPrintf("[BLE] Autopilot command received: %d\n", cmd.type);
        switch (cmd.type) {
            case AutopilotCommand::ENABLE:          serialPrintf("[Autopilot] Enable\n");  break;
            case AutopilotCommand::DISABLE:         serialPrintf("[Autopilot] Disable\n"); break;
            case AutopilotCommand::ADJUST_PLUS_10:  serialPrintf("[Autopilot] +10°\n");    break;
            case AutopilotCommand::ADJUST_MINUS_10: serialPrintf("[Autopilot] -10°\n");    break;
            case AutopilotCommand::ADJUST_PLUS_1:   serialPrintf("[Autopilot] +1°\n");     break;
            case AutopilotCommand::ADJUST_MINUS_1:  serialPrintf("[Autopilot] -1°\n");     break;
            default: break;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
}

// ═══════════════════════════════════════════════════════════════
// CORE 0: UART Reader Task
// ═══════════════════════════════════════════════════════════════
void uartReaderTask(void* parameter) {
    char lineBuffer[NMEA_MAX_LENGTH];
    NMEASentence sentence;

    serialPrintf("[UART Reader] Started on Core 0\n");

    uint32_t lastStatsTime  = millis();
    uint32_t sentencesRead  = 0;
    uint32_t parseErrors    = 0;
    uint32_t queueFullCount = 0;

    while (true) {
        if (uartHandler.readLine(lineBuffer, sizeof(lineBuffer), pdMS_TO_TICKS(100))) {
            if (nmeaParser.parseLine(lineBuffer, sentence)) {
                sentencesRead++;
                g_messagesRead++;

                if (nmeaQueue != NULL) {
                    if (xQueueSend(nmeaQueue, &sentence, pdMS_TO_TICKS(5)) != pdTRUE) {
                        queueFullCount++;
                        g_nmeaQueueOverflows++;
                        // Update the shared counter unconditionally (no DEBUG_UART guard)
                        g_nmeaQueueFullEvents = queueFullCount;
                    }
                }
            } else {
                parseErrors++;
            }
        }
        // No taskYIELD() needed here: readLine already yields inside its loop
        // via xStreamBufferReceive (blocked wait) and vTaskDelay(1ms).
        // Adding taskYIELD() at priority 5 is a no-op anyway.

#ifdef DEBUG_CPU
        if (millis() - lastStatsTime > 30000) {
            serialPrintf("[UART Reader] read=%u errors=%u queueFull=%u\n",
                         sentencesRead, parseErrors, queueFullCount);
            lastStatsTime  = millis();
            sentencesRead  = 0;
            parseErrors    = 0;
            queueFullCount = 0;
        }
#endif
    }
}

// ═══════════════════════════════════════════════════════════════
// CORE 1: Processor Task
// ═══════════════════════════════════════════════════════════════
void processorTask(void* parameter) {
    NMEASentence sentence;
    serialPrintf("[Processor] Started on Core 1\n");

    uint32_t lastStatsTime     = millis();
    uint32_t messagesProcessed = 0;

    while (true) {
        if (xQueueReceive(nmeaQueue, &sentence, pdMS_TO_TICKS(100)) == pdTRUE) {
            messagesProcessed++;
            g_messagesProcessed++;

            if (tcpServer.getClientCount() > 0) {
                tcpServer.broadcast(sentence.raw);
            }
            webServer.broadcastNMEA(sentence.raw);
        }
        // No extra vTaskDelay needed: xQueueReceive already blocks for up to 100 ms
        // when the queue is empty, giving other Core 1 tasks (WiFi, Web) time to run.

#ifdef DEBUG_CPU
        if (millis() - lastStatsTime > 30000) {
            serialPrintf("[Processor] ═══ 30 s stats ═══\n");
            serialPrintf("[Processor]   processed     : %u\n", messagesProcessed);
            serialPrintf("[Processor]   valid total   : %u\n", nmeaParser.getValidSentences());
            serialPrintf("[Processor]   invalid total : %u\n", nmeaParser.getInvalidSentences());
            serialPrintf("[Processor]   queue overflow: %u\n", (uint32_t)g_nmeaQueueOverflows);
            serialPrintf("[Processor]   free heap     : %u B\n", ESP.getFreeHeap());
            serialPrintf("[Processor] ════════════════════\n");
            lastStatsTime     = millis();
            messagesProcessed = 0;
        }
#endif
    }
}

// ═══════════════════════════════════════════════════════════════
// CORE 1: WiFi Task
// ═══════════════════════════════════════════════════════════════
void wifiTask(void* parameter) {
    serialPrintf("[WiFi Task] Started on Core 1\n");
    WiFiState lastState = WIFI_DISCONNECTED;

    while (true) {
        wifiManager.update();
        WiFiState currentState = wifiManager.getState();

        if (currentState != lastState) {
            switch (currentState) {
                case WIFI_CONNECTED_STA:
                    serialPrintf("[WiFi] ✓ Connected to %s  IP: %s  RSSI: %d dBm\n",
                                 wifiManager.getSSID().c_str(),
                                 wifiManager.getIP().toString().c_str(),
                                 wifiManager.getRSSI());
                    break;
                case WIFI_AP_MODE:
                    serialPrintf("[WiFi] ✓ AP Mode: %s  IP: %s\n",
                                 wifiManager.getSSID().c_str(),
                                 wifiManager.getIP().toString().c_str());
                    break;
                case WIFI_DISCONNECTED:  serialPrintf("[WiFi] Disconnected\n");   break;
                case WIFI_CONNECTING:    serialPrintf("[WiFi] Connecting...\n");   break;
                case WIFI_RECONNECTING:  serialPrintf("[WiFi] Reconnecting...\n"); break;
            }
            lastState = currentState;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}