# Technical Architecture

## 1. Technology Stack

### 1.1 Core Platform
- **Microcontroller**: ESP32-S3
- **Framework**: Arduino (with ESP-IDF components where needed)
- **Build System**: PlatformIO
- **RTOS**: FreeRTOS (built into ESP32)

### 1.2 Storage
- **Filesystem**: LittleFS (more robust than SPIFFS, better wear leveling)
- **Configuration**: NVS (Non-Volatile Storage)
- **Partition**: Custom partition table for OTA support

### 1.3 Web Stack
- **Frontend Framework**: React 18
- **Build Tool**: Vite (lightweight, fast builds, excellent tree-shaking)
- **Web Server**: ESPAsyncWebServer
- **Protocol**: HTTP/1.1 + WebSocket
- **API Format**: JSON (via ArduinoJson)

### 1.4 Networking
- **WiFi**: ESP32 native WiFi (2.4GHz)
- **TCP**: AsyncTCP library
- **WebSocket**: ESPAsyncWebServer WebSocket support

## 2. Memory Partitioning (4MB Flash)

```
┌──────────────────────────────────────┐ 0x0000
│ Bootloader                           │ 64KB
├──────────────────────────────────────┤ 0x8000
│ Partition Table                      │ 4KB
├──────────────────────────────────────┤ 0x9000
│ NVS (Configuration Storage)          │ 32KB
├──────────────────────────────────────┤ 0x11000
│ PHY Init Data                        │ 4KB
├──────────────────────────────────────┤ 0x20000
│ Factory App (Main Firmware)          │ 1.5MB
├──────────────────────────────────────┤ 0x1A0000
│ OTA_0 (Update Partition)             │ 1.5MB
├──────────────────────────────────────┤ 0x320000
│ LittleFS (Web Dashboard + Data)      │ 896KB
└──────────────────────────────────────┘ 0x400000
```

**Rationale**:
- **NVS**: 32KB sufficient for WiFi credentials, UART config, system settings
- **Factory + OTA_0**: 1.5MB each allows for feature-rich firmware with growth room
- **LittleFS**: ~900KB for compressed React bundle + future data files

## 3. Software Architecture

### 3.1 High-Level Block Diagram

```
┌─────────────────────────────────────────────────────────┐
│                     ESP32-S3                            │
│                                                         │
│  ┌──────────────┐         ┌─────────────────┐         │
│  │ UART Handler │────────▶│  NMEA Parser    │         │
│  │  (UART1 RX)  │         │ (Checksum, Line │         │
│  │              │         │  Extraction)    │         │
│  └──────────────┘         └────────┬────────┘         │
│                                    │                   │
│                                    ▼                   │
│                         ┌──────────────────┐          │
│                         │  Message Queue   │          │
│                         │  (FreeRTOS)      │          │
│                         │  ~50 messages    │          │
│                         └────────┬─────────┘          │
│                                  │                     │
│                 ┌────────────────┼──────────────┐     │
│                 │                │              │     │
│                 ▼                ▼              ▼     │
│         ┌──────────┐    ┌──────────┐    ┌──────────┐│
│         │   TCP    │    │WebSocket │    │   REST   ││
│         │  Server  │    │  Server  │    │   API    ││
│         │ (10110)  │    │ (/ws/*)  │    │(/api/*)  ││
│         └──────────┘    └──────────┘    └──────────┘│
│              │                │              │        │
│              │                └──────────────┘        │
│              │                       │                │
│              ▼                       ▼                │
│      ┌─────────────┐       ┌─────────────────┐      │
│      │TCP Clients  │       │  Web Dashboard  │      │
│      │(OpenCPN,etc)│       │   (LittleFS)    │      │
│      └─────────────┘       └─────────────────┘      │
│                                                       │
│  ┌──────────────────────────────────────────┐       │
│  │         Configuration Manager            │       │
│  │              (NVS)                        │       │
│  │  - WiFi credentials                      │       │
│  │  - UART parameters                       │       │
│  │  - System settings                       │       │
│  └──────────────────────────────────────────┘       │
│                                                       │
│  ┌──────────────────────────────────────────┐       │
│  │          WiFi Manager                     │       │
│  │  - STA mode (connect to network)         │       │
│  │  - AP mode (create access point)         │       │
│  │  - Auto fallback STA→AP                  │       │
│  └──────────────────────────────────────────┘       │
└─────────────────────────────────────────────────────────┘
```

### 3.2 Data Flow

#### NMEA Reception and Distribution
```
UART RX ISR
    │
    ▼
Ring Buffer (2KB)
    │
    ▼
NMEA Parser Task
    │
    ├─ Validate checksum
    ├─ Extract complete sentence ($...*HH\r\n)
    │
    ▼
Message Queue (50 messages)
    │
    ├──────────────┬──────────────┬──────────────┐
    │              │              │              │
    ▼              ▼              ▼              ▼
TCP Client 1  TCP Client 2  WebSocket     (Future: SD, etc)
```

#### Configuration Flow
```
User (Browser)
    │
    ▼
REST API Request (POST /api/config/wifi)
    │
    ▼
Config Manager
    │
    ├─ Validate input
    ├─ Save to NVS
    │
    ▼
Apply configuration
    │
    └─ WiFi reconnect / UART reinit / etc.
```

## 4. Component Design

### 4.1 UART Handler
**Responsibility**: Receive data from UART1, buffer it, and extract complete lines

**Interface**:
```cpp
class UARTHandler {
public:
    void init(uart_config_t config);
    void start();
    void stop();
    bool readLine(char* buffer, size_t maxLen, TickType_t timeout);
    
private:
    void rxTask();
    StreamBufferHandle_t rxStreamBuffer;
};
```

**Implementation Notes**:
- Use FreeRTOS `StreamBuffer` for efficient UART→Task communication
- ISR writes to stream buffer, task reads and parses lines
- Configurable baud rate, data bits, parity, stop bits

### 4.2 NMEA Parser
**Responsibility**: Validate and parse NMEA sentences

**Interface**:
```cpp
class NMEAParser {
public:
    bool parseLine(const char* line, NMEASentence& out);
    bool validateChecksum(const char* line);
    
private:
    uint8_t calculateChecksum(const char* data, size_t len);
};

struct NMEASentence {
    char raw[128];      // Original sentence
    char type[8];       // Sentence type (e.g., "GPGGA")
    uint8_t checksum;   // Calculated checksum
    bool valid;         // Checksum validation result
    uint32_t timestamp; // Receive timestamp (millis)
};
```

**NMEA Format**:
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n
^                                                            ^^
│                                                            └┘─ Checksum
└─ Start delimiter
```

**Checksum Calculation**:
- XOR all characters between `$` and `*`
- Convert to 2-digit hex string

### 4.3 TCP Server
**Responsibility**: Accept client connections and broadcast NMEA sentences

**Interface**:
```cpp
class TCPServer {
public:
    void init(uint16_t port);
    void start();
    void stop();
    void broadcast(const char* data, size_t len);
    size_t getClientCount();
    
private:
    void onConnect(AsyncClient* client);
    void onDisconnect(AsyncClient* client);
    void onData(AsyncClient* client, void* data, size_t len);
    
    std::vector<AsyncClient*> clients;
    SemaphoreHandle_t clientsMutex;
};
```

**Implementation Notes**:
- Use `AsyncTCP` library for non-blocking I/O
- Maintain client list with mutex protection
- Handle slow clients: drop if send buffer full
- Max clients: 5 (configurable, memory-dependent)

### 4.4 Web Server
**Responsibility**: Serve dashboard files and handle API requests

**Interface**:
```cpp
class WebServer {
public:
    void init();
    void start();
    void registerRESTHandlers();
    void registerWebSocketHandlers();
    
private:
    AsyncWebServer server;
    AsyncWebSocket wsNMEA;
    
    // REST handlers
    void handleGetWiFiConfig(AsyncWebServerRequest* request);
    void handlePostWiFiConfig(AsyncWebServerRequest* request);
    void handleGetSerialConfig(AsyncWebServerRequest* request);
    void handlePostSerialConfig(AsyncWebServerRequest* request);
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleRestart(AsyncWebServerRequest* request);
    
    // WebSocket handlers
    void onWSEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                   AwsEventType type, void* arg, uint8_t* data, size_t len);
};
```

**REST API Endpoints**:

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/config/wifi` | Get WiFi configuration |
| POST | `/api/config/wifi` | Set WiFi configuration |
| GET | `/api/config/serial` | Get UART configuration |
| POST | `/api/config/serial` | Set UART configuration |
| GET | `/api/status` | Get system status |
| POST | `/api/restart` | Restart ESP32 |

**WebSocket Endpoints**:
- `/ws/nmea` - Real-time NMEA stream

### 4.5 Config Manager
**Responsibility**: Persistent storage of configuration

**Interface**:
```cpp
class ConfigManager {
public:
    void init();
    
    // WiFi config
    bool getWiFiConfig(WiFiConfig& config);
    bool setWiFiConfig(const WiFiConfig& config);
    
    // UART config
    bool getSerialConfig(SerialConfig& config);
    bool setSerialConfig(const SerialConfig& config);
    
    // Factory reset
    void factoryReset();
    
private:
    Preferences nvs;
};

struct WiFiConfig {
    char ssid[32];
    char password[64];
    uint8_t mode;  // 0=STA, 1=AP
};

struct SerialConfig {
    uint32_t baudRate;
    uint8_t dataBits;  // 5-8
    uint8_t parity;    // 0=None, 1=Even, 2=Odd
    uint8_t stopBits;  // 1-2
};
```

**NVS Namespace**: `marine_gw`

### 4.6 WiFi Manager
**Responsibility**: Manage WiFi connectivity with automatic fallback

**State Machine**:
```
┌─────────┐
│  INIT   │
└────┬────┘
     │
     ▼
┌─────────────┐      Success      ┌──────────────┐
│ Try STA     │─────────────────▶ │ CONNECTED    │
│ (30s timer) │                    │ (STA mode)   │
└─────┬───────┘                    └──────────────┘
      │ Timeout                            │
      │                                    │ Connection lost
      ▼                                    ▼
┌─────────────┐                    ┌──────────────┐
│ FALLBACK_AP │◀───────────────────│ RECONNECTING │
│ (AP mode)   │    3 failures      │ (retry STA)  │
└─────────────┘                    └──────────────┘
```

**Interface**:
```cpp
class WiFiManager {
public:
    void init(const WiFiConfig& config);
    void start();
    WiFiState getState();
    int8_t getRSSI();
    IPAddress getIP();
    size_t getConnectedClients();  // For AP mode
    
private:
    void connectSTA();
    void startAP();
    void monitorConnection();
    
    WiFiConfig config;
    WiFiState state;
    uint8_t reconnectAttempts;
};

enum WiFiState {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED_STA,
    WIFI_AP_MODE
};
```

### 4.7 SeaTalk Manager
**Responsibility**: Decode SeaTalk1 datagrams and send autopilot commands

**Interface**:
```cpp
class SeatalkManager {
public:
    SeatalkManager(SeatalkRMT* rmt, BoatState* boatState = nullptr);
    
    // Autopilot commands
    bool sendAutopilotCommand(const char* command);  // "auto", "standby", "track", etc.
    
    // Utility commands
    bool sendExtraCommand(const char* command);      // "lamp:0..3", "beep_on", "beep_off"
    
    // Bus conversions (new)
    void setConversionConfig(const ConversionConfig& cfg);
    ConversionConfig getConversionConfig() const;
    void runConversions(BoatState* bs);
    
    // Processing
    void update();  // Call regularly from SeaTalk task
    
private:
    void parseFrame(const uint8_t* frame, uint8_t len);
    bool sendRaw(uint8_t* buf, uint8_t len);
    
    // Conversion helpers
    bool convSendCOG(float cog_deg);
    bool convSendSOG(float sog_knots);
    bool convSendPosition(float lat, float lon);
    // ... (7 more conversion methods)
    
    SeatalkRMT* rmt;
    BoatState* boatState;
    ConversionConfig _convCfg;
    uint32_t _convLastSent[CONV_COUNT];
};
```

**Supported Autopilot Commands**:
| Command | Effect | SeaTalk Datagram |
|---------|--------|------------------|
| `auto` | Compass lock | 0x86 key=0x01 |
| `standby` | Disengage | 0x86 key=0x02 |
| `track` | GPS track mode | 0x86 key=0x03 |
| `wind` | Wind vane mode | 0x86 key=0x23 |
| `adjust+1` / `adjust+10` | Course +1° / +10° | 0x86 key=0x07/0x08 |
| `adjust-1` / `adjust-10` | Course -1° / -10° | 0x86 key=0x05/0x06 |
| `tack-port` / `tack-starboard` | Tack modes | 0x86 key=0x21/0x22 |

### 4.8 Bus Conversion System
**Responsibility**: Bridge data between NMEA0183 and SeaTalk1 buses

**Purpose**: Allow non-native instruments to share data. For example:
- GPS receiver on NMEA → convert to SeaTalk for autopilot
- Wind sensor on SeaTalk → already native (no conversion needed)
- Calculated True Wind → broadcast as NMEA MWV sentence to network clients

**Configuration**:
```cpp
#define CONV_GPS_COG_TO_ST     0   // NMEA GPS COG → SeaTalk 0x53
#define CONV_GPS_SOG_TO_ST     1   // NMEA GPS SOG → SeaTalk 0x52
#define CONV_GPS_POS_TO_ST     2   // NMEA GPS LAT/LON → SeaTalk 0x50/0x51
#define CONV_DEPTH_TO_ST       3   // NMEA Depth → SeaTalk 0x00
#define CONV_STW_TO_ST         4   // NMEA STW → SeaTalk 0x20
#define CONV_AWA_TO_ST         5   // NMEA AWA → SeaTalk 0x10 (⚠ native on SeaTalk)
#define CONV_AWS_TO_ST         6   // NMEA AWS → SeaTalk 0x11 (⚠ native on SeaTalk)
#define CONV_WATER_TEMP_TO_ST  7   // NMEA Water Temp → SeaTalk 0x27
#define CONV_HDG_TO_ST         8   // NMEA Heading → SeaTalk 0x9C
#define CONV_TW_TO_NMEA        9   // Calculated True Wind → NMEA MWV(T)
#define CONV_COUNT             10

struct ConversionRule {
    bool enabled;           // Is this rule active?
    uint8_t interval_s;     // Transmission interval (1-60 seconds)
};

struct ConversionConfig {
    ConversionRule rules[CONV_COUNT];
};
```

**Encoding Reference** (per Thomas Knauf SeaTalk spec):

| Datagram | Format | Source Field | Notes |
|----------|--------|--------------|-------|
| 0x53 | COG | GPS COG | Angle encoding: (U&3)*90 + (VW&0x3F)*2 + (U&0xC)/8 |
| 0x52 | SOG | GPS SOG | XXXX/10 knots |
| 0x50+0x51 | LAT/LON | GPS position | Degrees + minutes/100, S/E flags |
| 0x00 | Depth | NMEA DPT/DBT | XXXX/10 feet (converted from meters) |
| 0x20 | STW | NMEA VHW | XXXX/10 knots |
| 0x10 | AWA | NMEA MWV | XXYY/2 degrees (big-endian) |
| 0x11 | AWS | NMEA MWV | Integer + 1/10ths knots |
| 0x27 | WaterTemp | NMEA MTW | (XXXX-100)/10 °C |
| 0x9C | Heading | NMEA HDM/HDG | Same angle encoding as 0x53 |

**True Wind Output** (broadcast to network):
```
$IIMWV,{twa:.1f},T,{tws:.1f},N,A*{checksum}
```
- Calculated from AWA + AWS + STW (via BoatState::calculateDerivedData)
- Broadcast on TCP (port 10110) and WebSocket (/ws/nmea)
- Rate-limited per configured interval

**Data Flow**:
```
BoatState (receives NMEA data)
    │
    ├─ AWA/AWS set ──▶ calculateDerivedData() ──▶ TWA/TWS updated
    │
    └─ runConversions() [called from SeaTalk task]
         │
         ├─ Check enabled rules
         ├─ Verify data freshness (not stale)
         ├─ Rate-limit per interval
         │
         └─ Encode & transmit to SeaTalk bus
```

**Configuration Persistence**:
- Stored in NVS namespace `marine_gw`
- Keys: `conv_en_N` (enabled flag), `conv_int_N` (interval seconds)
- REST API: `GET/POST /api/config/conversions`

## 5. FreeRTOS Task Design

### Task Priority Levels
```cpp
#define TASK_PRIORITY_UART      5  // High - time-critical
#define TASK_PRIORITY_NMEA      4  // High - data processing
#define TASK_PRIORITY_TCP       3  // Medium - network I/O
#define TASK_PRIORITY_WEB       3  // Medium - HTTP/WS
#define TASK_PRIORITY_WIFI      2  // Low - connection management
#define TASK_PRIORITY_MONITOR   1  // Lowest - system monitoring
```

### Task Stack Sizes
```cpp
#define TASK_STACK_UART     4096   // Moderate buffer handling
#define TASK_STACK_NMEA     4096   // String processing
#define TASK_STACK_TCP      8192   // AsyncTCP callbacks
#define TASK_STACK_WEB      8192   // HTTP parsing, JSON
#define TASK_STACK_WIFI     4096   // WiFi events
#define TASK_STACK_MONITOR  2048   // Simple monitoring
```

### Task Communication
- **UART → NMEA**: StreamBuffer (2KB)
- **NMEA → TCP/WS**: Message Queue (50 x NMEASentence)
- **Config changes**: Event flags + mutexes

## 6. Web Dashboard Architecture

### 6.1 React Component Structure
```
App
├── Router
│   ├── Layout
│   │   ├── Sidebar (collapsible menu)
│   │   └── Main Content
│   │       ├── WiFiConfigPage
│   │       ├── SerialConfigPage
│   │       ├── NMEAMonitorPage
│   │       └── SystemStatusPage
│   └── Toast Notifications
├── API Service (REST client)
└── WebSocket Service (NMEA stream)
```

### 6.2 State Management
- **Local State**: React useState for component-specific data
- **Context**: WiFi connection status, system info
- **No Redux**: Keep it simple for embedded constraints

### 6.3 Build Optimization
**Target**: <500KB total bundle (gzipped)

**Vite Configuration**:
```javascript
export default {
  build: {
    target: 'es2015',           // Wider browser support
    minify: 'terser',
    terserOptions: {
      compress: {
        drop_console: true,     // Remove console.log
        dead_code: true
      }
    },
    rollupOptions: {
      output: {
        manualChunks: undefined // Single bundle for small app
      }
    },
    cssMinify: true,
    assetsInlineLimit: 4096     // Inline small assets
  }
}
```

**Bundle Content**:
- HTML: ~2KB
- JS: ~300KB (minified)
- CSS: ~50KB (minified)
- **Total**: ~350KB uncompressed, ~120KB gzipped

## 7. Security Considerations

### 7.1 MVP (Minimal Security)
- **WiFi**: WPA2 password protection (user's network)
- **Web Dashboard**: No authentication (local network only)
- **API**: No authentication
- **CORS**: Disabled (same-origin only)

### 7.2 Future Enhancements
- HTTP Basic Auth for configuration endpoints
- Session cookies
- HTTPS (self-signed certificate)
- API rate limiting

## 8. Error Handling Strategy

### 8.1 Critical Errors (System Halt)
- NVS initialization failure → Factory reset, restart
- UART initialization failure → Log error, continue in config-only mode
- LittleFS mount failure → Serve minimal API without dashboard

### 8.2 Recoverable Errors
- WiFi connection lost → Automatic reconnection (3 attempts) → Fallback to AP
- TCP client disconnect → Remove from client list, continue
- Invalid NMEA checksum → Log warning, discard sentence
- WebSocket disconnect → Client reconnects automatically

### 8.3 Error Logging
- Serial console (USB CDC)
- Future: Log to SD card
- Future: Log viewer in dashboard

## 9. Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| NMEA latency | <100ms | UART RX to TCP TX |
| Heap usage | <200KB | Stable over 24h |
| Task switching | <1ms | FreeRTOS overhead |
| Web page load | <2s | From local LittleFS |
| API response | <50ms | Simple GET requests |
| WebSocket latency | <200ms | NMEA display update |
| TCP clients | 5 simultaneous | Without performance degradation |
| Uptime | >7 days | Continuous operation |

## 10. Compatibility

### 10.1 NMEA-0183 TCP Standard
- **Port**: 10110 (de facto standard)
- **Protocol**: TCP/IP
- **Format**: Raw NMEA sentences, `\r\n` terminated
- **Compatible with**:
  - OpenCPN
  - SignalK (input plugin)
  - QtVlm
  - Expedition
  - Most navigation software

### 10.2 Browser Support
- Chrome 90+
- Firefox 88+
- Safari 14+
- Edge 90+
- Mobile browsers (iOS Safari, Chrome Mobile)

## 11. Monitoring and Diagnostics

### System Status Information
```json
{
  "uptime": 86400,
  "heap": {
    "free": 245760,
    "total": 327680,
    "min_free": 240000
  },
  "wifi": {
    "mode": "STA",
    "ssid": "MyNetwork",
    "rssi": -67,
    "ip": "192.168.1.100",
    "clients": 0
  },
  "tcp": {
    "clients": 2,
    "port": 10110
  },
  "uart": {
    "baud": 38400,
    "rx_errors": 0,
    "sentences_received": 123456
  },
  "temperature": 56.5
}
```

### Health Monitoring
- Watchdog timer (120s)
- Heap fragmentation monitoring
- UART error counter
- WiFi reconnection counter
