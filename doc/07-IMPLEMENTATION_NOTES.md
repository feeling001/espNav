# Implementation Notes

Technical implementation details, best practices, and gotchas for the Marine Gateway firmware.

## NMEA-0183 Implementation

### Sentence Format

NMEA-0183 sentences follow this structure:
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n
^     ^                                                      ^^   ^^
│     │                                                      │└───┘└─ Line ending
│     │                                                      └─ Checksum
│     └─ Comma-separated data fields
└─ Talker ID + Sentence Type
```

**Key Points**:
- Start delimiter: `$`
- Checksum delimiter: `*`
- Checksum: 2-digit hexadecimal
- Line ending: `\r\n` (carriage return + line feed)
- Maximum length: 82 characters (including `\r\n`)

### Checksum Calculation

XOR of all characters between `$` and `*`:

```cpp
uint8_t calculateChecksum(const char* sentence) {
    uint8_t checksum = 0;
    
    // Skip the '$'
    const char* ptr = sentence + 1;
    
    // XOR until we hit '*' or end of string
    while (*ptr && *ptr != '*') {
        checksum ^= *ptr;
        ptr++;
    }
    
    return checksum;
}
```

### Checksum Validation

```cpp
bool validateNMEA(const char* sentence) {
    // Find the checksum position
    const char* asterisk = strchr(sentence, '*');
    if (!asterisk || asterisk - sentence > 80) {
        return false;  // No checksum or sentence too long
    }
    
    // Calculate checksum
    uint8_t calculated = calculateChecksum(sentence);
    
    // Parse provided checksum
    uint8_t provided = 0;
    sscanf(asterisk + 1, "%02hhx", &provided);
    
    return calculated == provided;
}
```

### Common NMEA Sentences

| Type | Description | Example |
|------|-------------|---------|
| GGA | GPS Fix Data | `$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47` |
| RMC | Recommended Minimum | `$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A` |
| VTG | Track & Ground Speed | `$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48` |
| MWV | Wind Speed & Angle | `$IIMWV,215.5,R,10.4,N,A*2C` |
| DPT | Depth | `$IIDPT,5.2,0.0,100.0*4F` |
| HDG | Heading | `$IIHDG,090.0,,,5.0,W*3A` |

## UART Configuration

### ESP32 UART Peripherals

ESP32-S3 has 3 UART controllers:
- **UART0**: Usually used for USB CDC (Serial)
- **UART1**: Available for NMEA (recommended)
- **UART2**: Available

### UART Pin Configuration

Example for UART1:
```cpp
#define UART_NUM        UART_NUM_1
#define UART_RX_PIN     GPIO_NUM_16
#define UART_TX_PIN     GPIO_NUM_17  // Not used for NMEA RX only
#define UART_BUFFER_SIZE 2048

uart_config_t uart_config = {
    .baud_rate = 38400,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
    .source_clk = UART_SCLK_DEFAULT,
};

uart_param_config(UART_NUM, &uart_config);
uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
uart_driver_install(UART_NUM, UART_BUFFER_SIZE, 0, 0, NULL, 0);
```

### Baud Rate Selection

Common NMEA baud rates:
- **4800**: Traditional NMEA-0183 standard
- **9600**: Common alternative
- **38400**: High-speed NMEA (most modern instruments)

**Important**: Must match the NMEA device's configuration.

### Ring Buffer for UART

Use FreeRTOS StreamBuffer for efficient UART handling:

```cpp
StreamBufferHandle_t uartStreamBuffer;

void uartInit() {
    // Create stream buffer (2KB)
    uartStreamBuffer = xStreamBufferCreate(2048, 1);
    
    // Create UART task
    xTaskCreate(uartTask, "UART", 4096, NULL, 5, NULL);
}

void uartTask(void* parameter) {
    uint8_t data[128];
    
    while (1) {
        // Read from UART
        int len = uart_read_bytes(UART_NUM, data, sizeof(data), 
                                  pdMS_TO_TICKS(100));
        
        if (len > 0) {
            // Write to stream buffer
            xStreamBufferSend(uartStreamBuffer, data, len, 0);
        }
    }
}
```

### Line Extraction

Extract complete NMEA sentences from stream:

```cpp
bool readLine(char* buffer, size_t maxLen, TickType_t timeout) {
    static char lineBuffer[128];
    static size_t linePos = 0;
    
    while (1) {
        // Read one byte from stream buffer
        uint8_t byte;
        size_t received = xStreamBufferReceive(uartStreamBuffer, &byte, 1, timeout);
        
        if (received == 0) {
            return false;  // Timeout
        }
        
        // Add to line buffer
        if (linePos < sizeof(lineBuffer) - 1) {
            lineBuffer[linePos++] = byte;
        }
        
        // Check for line ending
        if (byte == '\n' && linePos > 1 && lineBuffer[linePos - 2] == '\r') {
            // Complete line received
            lineBuffer[linePos - 2] = '\0';  // Remove \r\n
            strncpy(buffer, lineBuffer, maxLen);
            linePos = 0;
            return true;
        }
        
        // Prevent buffer overflow
        if (linePos >= sizeof(lineBuffer)) {
            linePos = 0;  // Reset on overflow
        }
    }
}
```

## TCP Server Implementation

### Port 10110 Standard

**Port 10110** is the de facto standard for NMEA-0183 over TCP/IP:
- Defined by convention in marine electronics
- Widely supported by navigation software (OpenCPN, SignalK, etc.)
- Simple protocol: raw NMEA sentences with `\r\n` line endings

### AsyncTCP Server

```cpp
AsyncServer tcpServer(10110);

void tcpServerInit() {
    tcpServer.onClient([](void* arg, AsyncClient* client) {
        // New client connected
        Serial.printf("TCP client connected: %s\n", 
                     client->remoteIP().toString().c_str());
        
        // Add to client list
        addClient(client);
        
        // Set up disconnect handler
        client->onDisconnect([](void* arg, AsyncClient* c) {
            removeClient(c);
        });
    }, NULL);
    
    tcpServer.begin();
}
```

### Client Management

```cpp
std::vector<AsyncClient*> tcpClients;
SemaphoreHandle_t clientsMutex;

void addClient(AsyncClient* client) {
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    
    // Limit max clients
    if (tcpClients.size() < TCP_MAX_CLIENTS) {
        tcpClients.push_back(client);
    } else {
        client->close();
    }
    
    xSemaphoreGive(clientsMutex);
}

void removeClient(AsyncClient* client) {
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    
    auto it = std::find(tcpClients.begin(), tcpClients.end(), client);
    if (it != tcpClients.end()) {
        tcpClients.erase(it);
    }
    
    xSemaphoreGive(clientsMutex);
}
```

### Broadcasting NMEA

```cpp
void broadcastNMEA(const char* sentence) {
    xSemaphoreTake(clientsMutex, portMAX_DELAY);
    
    // Prepare data with line ending
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s\r\n", sentence);
    size_t len = strlen(buffer);
    
    // Send to all clients
    for (auto client : tcpClients) {
        if (client->connected() && client->canSend()) {
            client->write(buffer, len);
        }
    }
    
    xSemaphoreGive(clientsMutex);
}
```

### Handling Slow Clients

```cpp
void broadcastNMEA(const char* sentence) {
    // ... preparation ...
    
    for (auto it = tcpClients.begin(); it != tcpClients.end(); ) {
        AsyncClient* client = *it;
        
        if (!client->connected()) {
            // Client disconnected
            it = tcpClients.erase(it);
        } else if (!client->canSend()) {
            // Client buffer full (slow client)
            Serial.println("Dropping slow client");
            client->close();
            it = tcpClients.erase(it);
        } else {
            // Send data
            client->write(buffer, len);
            ++it;
        }
    }
}
```

## WiFi Management

### State Machine

```cpp
enum WiFiState {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING_STA,
    WIFI_CONNECTED_STA,
    WIFI_RECONNECTING,
    WIFI_FALLBACK_AP
};

WiFiState currentState = WIFI_DISCONNECTED;
uint32_t connectStartTime = 0;
uint8_t reconnectAttempts = 0;

#define WIFI_CONNECT_TIMEOUT_MS  30000
#define WIFI_MAX_RECONNECT       3
```

### Connection Logic

```cpp
void wifiTask(void* parameter) {
    while (1) {
        switch (currentState) {
            case WIFI_DISCONNECTED:
                attemptSTAConnection();
                break;
                
            case WIFI_CONNECTING_STA:
                checkSTAConnection();
                break;
                
            case WIFI_CONNECTED_STA:
                monitorSTAConnection();
                break;
                
            case WIFI_RECONNECTING:
                handleReconnection();
                break;
                
            case WIFI_FALLBACK_AP:
                // Stay in AP mode
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### STA Mode Connection

```cpp
void attemptSTAConnection() {
    Serial.println("Attempting STA connection...");
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiConfig.ssid, wifiConfig.password);
    
    currentState = WIFI_CONNECTING_STA;
    connectStartTime = millis();
}

void checkSTAConnection() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        currentState = WIFI_CONNECTED_STA;
        reconnectAttempts = 0;
    } else if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
        Serial.println("STA connection timeout");
        fallbackToAP();
    }
}
```

### AP Fallback

```cpp
void fallbackToAP() {
    Serial.println("Falling back to AP mode...");
    
    WiFi.mode(WIFI_AP);
    
    // Generate SSID with MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char apSSID[32];
    snprintf(apSSID, sizeof(apSSID), "MarineGateway-%02X%02X%02X", 
             mac[3], mac[4], mac[5]);
    
    WiFi.softAP(apSSID, WIFI_AP_PASSWORD);
    
    Serial.printf("AP Mode: %s\n", apSSID);
    Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    currentState = WIFI_FALLBACK_AP;
}
```

### Connection Monitoring

```cpp
void monitorSTAConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost");
        currentState = WIFI_RECONNECTING;
        reconnectAttempts = 0;
    }
}

void handleReconnection() {
    if (reconnectAttempts < WIFI_MAX_RECONNECT) {
        reconnectAttempts++;
        Serial.printf("Reconnection attempt %d/%d\n", 
                     reconnectAttempts, WIFI_MAX_RECONNECT);
        attemptSTAConnection();
    } else {
        Serial.println("Max reconnect attempts reached");
        fallbackToAP();
    }
}
```

## NVS Configuration Storage

### Initialization

```cpp
#include <Preferences.h>

Preferences preferences;

void configInit() {
    // Open NVS namespace
    preferences.begin("marine_gw", false);  // false = read/write
}
```

### Save/Load WiFi Config

```cpp
void saveWiFiConfig(const WiFiConfig& config) {
    preferences.putString("wifi_ssid", config.ssid);
    preferences.putString("wifi_pass", config.password);
    preferences.putUChar("wifi_mode", config.mode);
}

void loadWiFiConfig(WiFiConfig& config) {
    String ssid = preferences.getString("wifi_ssid", "");
    String pass = preferences.getString("wifi_pass", "");
    uint8_t mode = preferences.getUChar("wifi_mode", 0);
    
    strncpy(config.ssid, ssid.c_str(), sizeof(config.ssid));
    strncpy(config.password, pass.c_str(), sizeof(config.password));
    config.mode = mode;
}
```

### Save/Load Serial Config

```cpp
void saveSerialConfig(const SerialConfig& config) {
    preferences.putUInt("serial_baud", config.baudRate);
    preferences.putUChar("serial_data", config.dataBits);
    preferences.putUChar("serial_parity", config.parity);
    preferences.putUChar("serial_stop", config.stopBits);
}

void loadSerialConfig(SerialConfig& config) {
    config.baudRate = preferences.getUInt("serial_baud", 38400);
    config.dataBits = preferences.getUChar("serial_data", 8);
    config.parity = preferences.getUChar("serial_parity", 0);
    config.stopBits = preferences.getUChar("serial_stop", 1);
}
```

### Factory Reset

```cpp
void factoryReset() {
    preferences.clear();
    
    // Set default values
    WiFiConfig defaultWiFi = {"", "", 0};
    SerialConfig defaultSerial = {38400, 8, 0, 1};
    
    saveWiFiConfig(defaultWiFi);
    saveSerialConfig(defaultSerial);
    
    Serial.println("Factory reset complete");
}
```

## Web Server Implementation

### Serve Static Files from LittleFS

```cpp
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

AsyncWebServer server(80);

void webServerInit() {
    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return;
    }
    
    // Serve static files
    server.serveStatic("/", LittleFS, "/www/")
          .setDefaultFile("index.html")
          .setCacheControl("max-age=86400");
    
    // Register API routes
    registerAPIRoutes();
    
    // Start server
    server.begin();
}
```

### REST API Handlers

```cpp
void registerAPIRoutes() {
    // GET /api/config/wifi
    server.on("/api/config/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        WiFiConfig config;
        loadWiFiConfig(config);
        
        StaticJsonDocument<256> doc;
        doc["ssid"] = config.ssid;
        doc["mode"] = config.mode;
        doc["has_password"] = (strlen(config.password) > 0);
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // POST /api/config/wifi
    server.on("/api/config/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {},
              NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, 
                      size_t index, size_t total) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, (char*)data);
        
        if (error) {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        WiFiConfig config;
        strncpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
        strncpy(config.password, doc["password"] | "", sizeof(config.password));
        config.mode = doc["mode"] | 0;
        
        saveWiFiConfig(config);
        
        request->send(200, "application/json", 
                     "{\"success\":true,\"message\":\"WiFi config saved\"}");
        
        // Trigger WiFi reconnection
        wifiReconnect();
    });
}
```

### WebSocket Handler

```cpp
AsyncWebSocket wsNMEA("/ws/nmea");

void webSocketInit() {
    wsNMEA.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                Serial.printf("WebSocket client connected: %u\n", client->id());
                break;
                
            case WS_EVT_DISCONNECT:
                Serial.printf("WebSocket client disconnected: %u\n", client->id());
                break;
                
            case WS_EVT_DATA:
                // Handle incoming data (if needed)
                break;
                
            case WS_EVT_ERROR:
                Serial.printf("WebSocket error: %u\n", client->id());
                break;
        }
    });
    
    server.addHandler(&wsNMEA);
}

void broadcastToWebSocket(const char* message) {
    wsNMEA.textAll(message);
}
```

## Memory Management

### Heap Monitoring

```cpp
void printHeapInfo() {
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Heap size: %u bytes\n", ESP.getHeapSize());
    Serial.printf("Min free heap: %u bytes\n", ESP.getMinFreeHeap());
}
```

### Prevent Fragmentation

```cpp
// Use stack variables where possible
void processNMEA() {
    char sentence[128];  // Stack allocation
    // ... process ...
}

// Pre-allocate buffers
char nmeaBuffer[NMEA_QUEUE_SIZE][128];  // Static allocation
```

### PSRAM Usage (if available)

```cpp
#ifdef BOARD_HAS_PSRAM
void* largeBuffer = ps_malloc(100000);  // Allocate from PSRAM
if (largeBuffer) {
    // Use buffer
    free(largeBuffer);
}
#endif
```

## FreeRTOS Best Practices

### Task Stack Sizing

Monitor stack usage:
```cpp
UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
Serial.printf("Stack remaining: %u bytes\n", stackHighWaterMark * 4);
```

### Mutex Protection

```cpp
SemaphoreHandle_t dataMutex;

void init() {
    dataMutex = xSemaphoreCreateMutex();
}

void accessSharedData() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Access shared data
        
        xSemaphoreGive(dataMutex);
    } else {
        Serial.println("Failed to acquire mutex");
    }
}
```

### Task Delays

Use `vTaskDelay` instead of `delay()`:
```cpp
void myTask(void* parameter) {
    while (1) {
        // Do work
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // Yield to other tasks
    }
}
```

## Error Handling Patterns

### UART Errors

```cpp
void handleUARTError() {
    // Check for errors
    uint32_t errors = uart_get_hw_errors(UART_NUM);
    
    if (errors & UART_FRAMING_ERR) {
        Serial.println("UART framing error");
    }
    if (errors & UART_PARITY_ERR) {
        Serial.println("UART parity error");
    }
    
    // Clear errors
    uart_clear_hw_errors(UART_NUM);
}
```

### Network Errors

```cpp
void handleWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case SYSTEM_EVENT_STA_GOT_IP:
            Serial.println("WiFi connected");
            break;
            
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.println("WiFi disconnected");
            // Trigger reconnection
            break;
            
        case SYSTEM_EVENT_AP_START:
            Serial.println("AP mode started");
            break;
    }
}
```

## Performance Optimization

### Minimize String Operations

```cpp
// Avoid String class in performance-critical code
// Bad:
String nmea = "$GPGGA,123519,...*47";
nmea += "\r\n";

// Good:
char nmea[128];
snprintf(nmea, sizeof(nmea), "$GPGGA,123519,...*47\r\n");
```

### Reduce Serial.print Calls

```cpp
// Bad:
Serial.print("Data: ");
Serial.print(value);
Serial.println(" units");

// Good:
Serial.printf("Data: %d units\n", value);
```

### Optimize JSON

```cpp
// Use StaticJsonDocument for known sizes
StaticJsonDocument<256> doc;

// Use DynamicJsonDocument only when necessary
DynamicJsonDocument doc(2048);
```

## Debugging Tips

### Enable Debug Output

```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=5
    -DDEBUG_ESP_HTTP_SERVER
    -DDEBUG_ESP_WIFI
```

### Add Timestamps to Logs

```cpp
#define LOG(msg, ...) Serial.printf("[%lu] " msg "\n", millis(), ##__VA_ARGS__)

LOG("NMEA received: %s", sentence);
```

### Watchdog Timer

```cpp
#include "esp_task_wdt.h"

void setup() {
    // Enable watchdog (120 seconds)
    esp_task_wdt_init(120, true);
    esp_task_wdt_add(NULL);
}

void loop() {
    // Reset watchdog
    esp_task_wdt_reset();
    
    // Do work
}
```

## Common Pitfalls

### 1. String Overflow
Always use `strncpy` and check buffer sizes:
```cpp
// Bad:
strcpy(buffer, input);

// Good:
strncpy(buffer, input, sizeof(buffer) - 1);
buffer[sizeof(buffer) - 1] = '\0';
```

### 2. Blocking Operations
Don't block in callbacks:
```cpp
// Bad:
client->onData([](void* arg, AsyncClient* c, void* data, size_t len) {
    delay(1000);  // NEVER block!
});

// Good:
client->onData([](void* arg, AsyncClient* c, void* data, size_t len) {
    // Queue data for processing in task
    xQueueSend(dataQueue, data, 0);
});
```

### 3. Task Priority Inversion
Use appropriate priorities:
```cpp
// Higher number = higher priority
xTaskCreate(criticalTask, "Critical", 4096, NULL, 5, NULL);  // High
xTaskCreate(normalTask, "Normal", 4096, NULL, 3, NULL);      // Medium
xTaskCreate(backgroundTask, "BG", 2048, NULL, 1, NULL);      // Low
```

### 4. Memory Leaks
Always free allocated memory:
```cpp
AsyncClient* client = new AsyncClient();
// ... use client ...
delete client;  // Don't forget!
```

### 5. Race Conditions
Protect shared data with mutexes:
```cpp
// Bad:
clientCount++;

// Good:
xSemaphoreTake(mutex, portMAX_DELAY);
clientCount++;
xSemaphoreGive(mutex);
```
