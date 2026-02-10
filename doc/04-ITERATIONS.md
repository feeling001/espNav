# Iterative Development Plan

## Overview

The MVP will be developed over 6 iterations, each building upon the previous one. Each iteration delivers a functional increment that can be tested independently.

**Total Estimated Duration**: 3-4 weeks (for experienced ESP32 + React developer)

---

## Iteration 1: Foundation + UART Reception

**Duration**: 3-4 days  
**Goal**: Receive NMEA sentences from UART1 and display on serial monitor

### Tasks

1. **Project Setup** (4 hours)
   - [ ] Create PlatformIO project
   - [ ] Configure `platformio.ini` for ESP32-S3
   - [ ] Set up custom partition table (`partitions.csv`)
   - [ ] Create directory structure
   - [ ] Initialize git repository

2. **UART Handler Implementation** (8 hours)
   - [ ] Create `uart_handler.h/cpp`
   - [ ] Configure UART1 (RX pin, configurable baud rate)
   - [ ] Implement FreeRTOS StreamBuffer for RX
   - [ ] Create UART RX task
   - [ ] Add line buffering (detect `\r\n`)
   - [ ] Handle buffer overflow gracefully

3. **NMEA Parser Implementation** (8 hours)
   - [ ] Create `nmea_parser.h/cpp`
   - [ ] Implement sentence detection (`$...*HH\r\n`)
   - [ ] Implement checksum calculation (XOR)
   - [ ] Implement checksum validation
   - [ ] Parse sentence type (e.g., GPGGA, GPRMC)
   - [ ] Add timestamp to each sentence

4. **Debug and Testing** (4 hours)
   - [ ] Display received sentences on USB Serial
   - [ ] Format output for readability
   - [ ] Test with NMEA simulator or GPS module
   - [ ] Verify checksum validation works correctly
   - [ ] Test with different baud rates (4800, 9600, 38400)

### Deliverables

- ✅ Functional UART reception on UART1
- ✅ NMEA sentence parsing with checksum validation
- ✅ Debug output on USB Serial showing:
  - Raw sentences
  - Sentence type
  - Checksum status (valid/invalid)
  - Timestamp

### Test Plan

```
Test 1: NMEA Reception
- Connect GPS module to UART1
- Verify sentences are received
- Verify checksum validation
- Expected: Valid NMEA sentences displayed

Test 2: Invalid Data Handling
- Send invalid checksums
- Send incomplete sentences
- Expected: Errors logged, system continues

Test 3: High Data Rate
- Configure for 38400 baud
- Verify no data loss
- Expected: All sentences received
```

### Success Criteria

- [ ] Receives NMEA sentences at 38400 baud without data loss
- [ ] Correctly validates checksums (>99% accuracy)
- [ ] Handles invalid data gracefully
- [ ] Displays formatted output on serial monitor

---

## Iteration 2: Configuration Storage + WiFi Management

**Duration**: 3-4 days  
**Goal**: Persistent configuration with WiFi connectivity and automatic AP fallback

### Tasks

1. **Config Manager Implementation** (6 hours)
   - [ ] Create `config_manager.h/cpp`
   - [ ] Define `WiFiConfig` and `SerialConfig` structures
   - [ ] Implement NVS initialization
   - [ ] Implement read/write functions for WiFi config
   - [ ] Implement read/write functions for Serial config
   - [ ] Add default configuration values
   - [ ] Implement factory reset function
   - [ ] Add configuration validation

2. **WiFi Manager Implementation** (10 hours)
   - [ ] Create `wifi_manager.h/cpp`
   - [ ] Implement STA mode connection
   - [ ] Implement AP mode (fallback)
   - [ ] Create WiFi state machine
   - [ ] Add connection timeout (30 seconds)
   - [ ] Implement automatic fallback to AP
   - [ ] Add reconnection logic (3 attempts)
   - [ ] Implement RSSI monitoring
   - [ ] Add connection event callbacks

3. **Integration** (4 hours)
   - [ ] Integrate Config Manager into main.cpp
   - [ ] Load WiFi config from NVS on boot
   - [ ] Load Serial config from NVS on boot
   - [ ] Apply Serial config to UART Handler
   - [ ] Start WiFi Manager with loaded config
   - [ ] Add LED status indicator (optional)

4. **Testing** (4 hours)
   - [ ] Test STA mode connection
   - [ ] Test AP fallback on connection failure
   - [ ] Test configuration persistence across reboots
   - [ ] Test WiFi reconnection on signal loss
   - [ ] Test factory reset

### Deliverables

- ✅ Config Manager with NVS storage
- ✅ WiFi Manager with STA/AP modes
- ✅ Automatic AP fallback (30s timeout)
- ✅ Configuration persistence across reboots
- ✅ Serial output showing WiFi status

### Test Plan

```
Test 1: STA Mode Success
- Configure valid WiFi credentials
- Boot ESP32
- Expected: Connects to WiFi within 30s, gets IP

Test 2: STA Mode Failure → AP Fallback
- Configure invalid WiFi credentials
- Boot ESP32
- Expected: AP mode activated after 30s timeout
- AP SSID: "MarineGateway-XXXXXX"

Test 3: Configuration Persistence
- Configure WiFi and Serial settings
- Save to NVS
- Reboot ESP32
- Expected: Same settings loaded on boot

Test 4: Reconnection
- Connect to WiFi
- Disable WiFi router
- Wait 60s
- Re-enable router
- Expected: ESP32 reconnects automatically

Test 5: Factory Reset
- Configure settings
- Trigger factory reset
- Reboot
- Expected: Default settings loaded
```

### Success Criteria

- [ ] WiFi connects reliably in STA mode
- [ ] AP fallback activates within 35 seconds on failure
- [ ] Configuration persists across reboots
- [ ] Automatic reconnection works (3 attempts)
- [ ] RSSI correctly reported

---

## Iteration 3: TCP Server Implementation

**Duration**: 3-4 days  
**Goal**: Stream NMEA sentences to TCP clients on port 10110

### Tasks

1. **TCP Server Implementation** (10 hours)
   - [ ] Create `tcp_server.h/cpp`
   - [ ] Set up AsyncTCP server on port 10110
   - [ ] Implement client connection handler
   - [ ] Implement client disconnection handler
   - [ ] Create client list with mutex protection
   - [ ] Implement broadcast function
   - [ ] Add client buffer management
   - [ ] Implement slow client detection and handling
   - [ ] Limit maximum clients (5)
   - [ ] Add client statistics (bytes sent, uptime)

2. **Message Queue Implementation** (6 hours)
   - [ ] Create FreeRTOS message queue (50 messages)
   - [ ] Integrate queue between NMEA Parser and TCP Server
   - [ ] Create NMEA processing task
   - [ ] Implement queue full handling
   - [ ] Add queue statistics monitoring

3. **Integration** (4 hours)
   - [ ] Connect NMEA Parser output to message queue
   - [ ] Connect message queue to TCP Server broadcast
   - [ ] Start TCP Server after WiFi connection
   - [ ] Add TCP status to serial debug output
   - [ ] Implement graceful shutdown

4. **Testing** (4 hours)
   - [ ] Test single client connection
   - [ ] Test multiple simultaneous clients (5)
   - [ ] Test client connect/disconnect
   - [ ] Measure latency (UART RX → TCP TX)
   - [ ] Test with OpenCPN or similar software

### Deliverables

- ✅ TCP Server on port 10110
- ✅ Multi-client support (up to 5 clients)
- ✅ NMEA broadcast to all connected clients
- ✅ Message queue for buffering
- ✅ <100ms latency from UART to TCP

### Test Plan

```
Test 1: Single Client
- Connect with netcat: nc <ESP_IP> 10110
- Expected: Receive NMEA stream

Test 2: Multiple Clients
- Connect 5 clients simultaneously
- Expected: All clients receive same data

Test 3: Client Disconnect
- Connect client
- Disconnect abruptly
- Expected: Server handles gracefully, no crash

Test 4: Slow Client
- Connect client with artificially slow read
- Expected: Client dropped if buffer full

Test 5: Latency Test
- Timestamp UART RX and TCP TX
- Expected: <100ms latency

Test 6: OpenCPN Integration
- Configure OpenCPN with TCP connection
- Expected: Navigation data displayed correctly
```

### Success Criteria

- [ ] TCP server accepts connections on port 10110
- [ ] Successfully broadcasts to 5 simultaneous clients
- [ ] Latency <100ms (UART RX → TCP TX)
- [ ] No data loss under normal load
- [ ] Graceful handling of client disconnects
- [ ] Compatible with OpenCPN and other NMEA software

---

## Iteration 4: Web Dashboard Backend

**Duration**: 3-4 days  
**Goal**: REST API and WebSocket for dashboard communication

### Tasks

1. **Web Server Setup** (6 hours)
   - [ ] Create `web_server.h/cpp`
   - [ ] Initialize ESPAsyncWebServer on port 80
   - [ ] Set up LittleFS for file serving
   - [ ] Enable GZIP compression for static files
   - [ ] Add 404 handler
   - [ ] Configure CORS (if needed)

2. **REST API Implementation** (10 hours)
   - [ ] `GET /api/config/wifi` - Get WiFi configuration
   - [ ] `POST /api/config/wifi` - Set WiFi configuration
   - [ ] `GET /api/config/serial` - Get UART configuration
   - [ ] `POST /api/config/serial` - Set UART configuration
   - [ ] `GET /api/status` - Get system status
   - [ ] `POST /api/restart` - Restart ESP32
   - [ ] Add request validation
   - [ ] Add JSON error responses
   - [ ] Integrate with Config Manager

3. **WebSocket Implementation** (6 hours)
   - [ ] Create `/ws/nmea` WebSocket endpoint
   - [ ] Implement WebSocket connection handler
   - [ ] Stream NMEA sentences to WebSocket clients
   - [ ] Add connection/disconnection events
   - [ ] Implement ping/pong for keep-alive
   - [ ] Limit WebSocket clients (2-3)

4. **Testing** (2 hours)
   - [ ] Test all REST endpoints with curl/Postman
   - [ ] Test WebSocket with browser console
   - [ ] Verify JSON response formats
   - [ ] Test concurrent API requests

### Deliverables

- ✅ HTTP server serving API on port 80
- ✅ 6 REST API endpoints
- ✅ WebSocket endpoint for NMEA streaming
- ✅ JSON request/response handling
- ✅ Integration with Config Manager

### API Documentation

#### GET /api/config/wifi
**Response**:
```json
{
  "ssid": "MyNetwork",
  "mode": 0,
  "has_password": true
}
```

#### POST /api/config/wifi
**Request**:
```json
{
  "ssid": "NewNetwork",
  "password": "secret123",
  "mode": 0
}
```
**Response**:
```json
{
  "success": true,
  "message": "WiFi configuration saved"
}
```

#### GET /api/config/serial
**Response**:
```json
{
  "baudRate": 38400,
  "dataBits": 8,
  "parity": 0,
  "stopBits": 1
}
```

#### POST /api/config/serial
**Request**:
```json
{
  "baudRate": 9600,
  "dataBits": 8,
  "parity": 0,
  "stopBits": 1
}
```
**Response**:
```json
{
  "success": true,
  "message": "Serial configuration saved"
}
```

#### GET /api/status
**Response**:
```json
{
  "uptime": 86400,
  "heap": {
    "free": 245760,
    "total": 327680
  },
  "wifi": {
    "mode": "STA",
    "ssid": "MyNetwork",
    "rssi": -67,
    "ip": "192.168.1.100"
  },
  "tcp": {
    "clients": 2,
    "port": 10110
  },
  "uart": {
    "baud": 38400,
    "sentences_received": 123456
  }
}
```

#### POST /api/restart
**Response**:
```json
{
  "success": true,
  "message": "Restarting in 2 seconds"
}
```

### WebSocket Protocol

**Client → Server**: (None in MVP, just receive)

**Server → Client**: NMEA sentences as text
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
```

### Test Plan

```
Test 1: WiFi Config API
curl http://<ESP_IP>/api/config/wifi
curl -X POST http://<ESP_IP>/api/config/wifi \
  -H "Content-Type: application/json" \
  -d '{"ssid":"Test","password":"pass","mode":0}'

Test 2: Serial Config API
curl http://<ESP_IP>/api/config/serial
curl -X POST http://<ESP_IP>/api/config/serial \
  -H "Content-Type: application/json" \
  -d '{"baudRate":9600,"dataBits":8,"parity":0,"stopBits":1}'

Test 3: Status API
curl http://<ESP_IP>/api/status

Test 4: WebSocket
Open browser console:
ws = new WebSocket('ws://<ESP_IP>/ws/nmea');
ws.onmessage = (e) => console.log(e.data);

Test 5: Restart API
curl -X POST http://<ESP_IP>/api/restart
```

### Success Criteria

- [ ] All REST endpoints return correct JSON
- [ ] Configuration changes persist via NVS
- [ ] WebSocket streams NMEA in real-time
- [ ] API responses <50ms
- [ ] Concurrent requests handled correctly

---

## Iteration 5: Web Dashboard Frontend

**Duration**: 4-5 days  
**Goal**: Responsive React dashboard for configuration and monitoring

### Tasks

1. **React Project Setup** (4 hours)
   - [ ] Initialize React project with Vite
   - [ ] Configure Vite for optimized builds
   - [ ] Set up React Router
   - [ ] Configure build output to `../data/www`
   - [ ] Create build script
   - [ ] Set up basic CSS/styling

2. **Layout Components** (6 hours)
   - [ ] Create App.jsx with router
   - [ ] Create collapsible Sidebar component
   - [ ] Create Header component
   - [ ] Create main Layout component
   - [ ] Implement mobile-responsive design
   - [ ] Add navigation menu items

3. **WiFi Configuration Page** (8 hours)
   - [ ] Create WiFiConfig component
   - [ ] Add SSID/password input form
   - [ ] Add mode selector (STA/AP)
   - [ ] Implement network scanner (future enhancement)
   - [ ] Display connection status
   - [ ] Show RSSI and IP address
   - [ ] Add save button with confirmation
   - [ ] Implement form validation

4. **Serial Configuration Page** (6 hours)
   - [ ] Create SerialConfig component
   - [ ] Add baud rate dropdown (4800-115200)
   - [ ] Add data bits selector (5-8)
   - [ ] Add parity selector (None/Even/Odd)
   - [ ] Add stop bits selector (1-2)
   - [ ] Add save button with confirmation
   - [ ] Show current UART status

5. **NMEA Monitor Page** (8 hours)
   - [ ] Create NMEAMonitor component
   - [ ] Implement WebSocket connection
   - [ ] Display live NMEA stream
   - [ ] Add auto-scroll functionality
   - [ ] Add pause/resume button
   - [ ] Limit display to last 100 lines
   - [ ] Add clear button
   - [ ] Show connection status

6. **System Status Page** (6 hours)
   - [ ] Create SystemStatus component
   - [ ] Display uptime
   - [ ] Display heap memory (free/total)
   - [ ] Display WiFi info (mode, SSID, RSSI, IP)
   - [ ] Display TCP clients count
   - [ ] Display UART statistics
   - [ ] Add restart button with confirmation
   - [ ] Auto-refresh every 5 seconds

7. **API Service Layer** (4 hours)
   - [ ] Create api.js for REST calls
   - [ ] Create websocket.js for WS connection
   - [ ] Add error handling
   - [ ] Add loading states
   - [ ] Create custom hooks (useWebSocket, useConfig)

8. **Build Optimization** (4 hours)
   - [ ] Configure Terser for minification
   - [ ] Enable tree-shaking
   - [ ] Inline small assets
   - [ ] Test build size (<500KB total)
   - [ ] Create automated build script
   - [ ] Test gzip compression

9. **Testing** (4 hours)
   - [ ] Test on desktop browsers
   - [ ] Test on mobile browsers
   - [ ] Test on tablet
   - [ ] Verify all forms work correctly
   - [ ] Verify WebSocket connection
   - [ ] Test navigation
   - [ ] Verify responsive design

### Deliverables

- ✅ Fully functional React dashboard
- ✅ WiFi configuration page
- ✅ Serial configuration page
- ✅ Live NMEA monitor
- ✅ System status page
- ✅ Responsive design (mobile/tablet/desktop)
- ✅ Optimized bundle (<500KB)

### Component Structure

```
src/
├── App.jsx
├── components/
│   ├── Layout/
│   │   ├── Sidebar.jsx
│   │   └── Header.jsx
│   ├── WiFi/
│   │   └── WiFiConfig.jsx
│   ├── Serial/
│   │   └── SerialConfig.jsx
│   ├── NMEA/
│   │   └── NMEAMonitor.jsx
│   └── System/
│       └── SystemStatus.jsx
├── services/
│   ├── api.js
│   └── websocket.js
└── hooks/
    ├── useWebSocket.js
    └── useConfig.js
```

### Test Plan

```
Test 1: WiFi Configuration
- Open dashboard in browser
- Navigate to WiFi config
- Enter SSID and password
- Save configuration
- Expected: Success message, settings saved

Test 2: Serial Configuration
- Navigate to Serial config
- Change baud rate to 9600
- Save configuration
- Expected: Settings saved, UART reconfigured

Test 3: NMEA Monitor
- Navigate to NMEA monitor
- Expected: Live NMEA sentences displayed
- Click pause
- Expected: Stream pauses
- Click resume
- Expected: Stream resumes

Test 4: System Status
- Navigate to System Status
- Expected: All metrics displayed
- Wait 5 seconds
- Expected: Metrics auto-refresh

Test 5: Responsive Design
- Open on mobile device
- Expected: Sidebar collapses, content responsive
- Test on tablet
- Expected: Layout adapts correctly

Test 6: Bundle Size
- Run: npm run build
- Check dist/ size
- Expected: <500KB total, <150KB gzipped
```

### Success Criteria

- [ ] Dashboard loads in <2 seconds
- [ ] All pages functional and responsive
- [ ] Forms validate input correctly
- [ ] WebSocket connects and streams data
- [ ] Total bundle size <500KB
- [ ] Works on Chrome, Firefox, Safari, Edge
- [ ] Mobile-friendly interface

---

## Iteration 6: Final Integration and Testing

**Duration**: 2-3 days  
**Goal**: Complete MVP with polishing and comprehensive testing

### Tasks

1. **Filesystem Upload** (2 hours)
   - [ ] Upload LittleFS image to ESP32
   - [ ] Verify files are accessible via web server
   - [ ] Test GZIP compression
   - [ ] Verify dashboard loads correctly

2. **End-to-End Integration** (6 hours)
   - [ ] Test complete workflow: UART → TCP → Dashboard
   - [ ] Verify all components work together
   - [ ] Test configuration changes propagate correctly
   - [ ] Test WiFi failover scenarios
   - [ ] Test system under load

3. **Error Handling** (4 hours)
   - [ ] Implement UART disconnection handling
   - [ ] Implement WiFi reconnection edge cases
   - [ ] Add memory overflow protection
   - [ ] Add watchdog timer
   - [ ] Test all error scenarios

4. **Memory Optimization** (4 hours)
   - [ ] Profile heap usage over 24 hours
   - [ ] Check for memory leaks
   - [ ] Optimize buffer sizes if needed
   - [ ] Adjust task stack sizes
   - [ ] Verify PSRAM usage (if available)

5. **Documentation** (4 hours)
   - [ ] Write user manual
   - [ ] Document flash procedure
   - [ ] Document initial configuration
   - [ ] Create troubleshooting guide
   - [ ] Document API endpoints

6. **Final Testing** (4 hours)
   - [ ] 24-hour uptime test
   - [ ] Stress test with multiple clients
   - [ ] Test all error recovery paths
   - [ ] Verify memory stability
   - [ ] Test with real marine instruments

### Deliverables

- ✅ Complete MVP firmware flashable to ESP32-S3
- ✅ User documentation
- ✅ All tests passed
- ✅ 24+ hour stable operation verified

### Test Plan

```
Test 1: Complete Workflow
- Boot ESP32
- Connect to WiFi
- Open dashboard
- Configure serial port
- Connect NMEA device
- Open TCP client
- Expected: NMEA data flows through entire chain

Test 2: WiFi Failover
- Connect to WiFi
- Disconnect router
- Expected: AP mode activates within 35s
- Reconnect router
- Expected: Returns to STA mode

Test 3: Configuration Persistence
- Configure all settings
- Power cycle ESP32
- Expected: All settings retained

Test 4: 24-Hour Uptime
- Start system
- Monitor for 24 hours
- Check heap usage every hour
- Expected: Stable operation, no crashes

Test 5: Multi-Client Stress
- Connect 5 TCP clients
- Connect 2 WebSocket clients (dashboard)
- Run for 1 hour
- Expected: All clients receive data, no drops

Test 6: Error Recovery
- Disconnect NMEA source → reconnect
- Disconnect WiFi → reconnect
- Kill TCP client abruptly
- Expected: System recovers gracefully

Test 7: Real-World Integration
- Connect to actual marine instruments
- Test with OpenCPN
- Verify navigation data accuracy
- Expected: Correct data display in OpenCPN
```

### Success Criteria

- [ ] ✅ All MVP features functional
- [ ] ✅ 24+ hour continuous operation without crashes
- [ ] ✅ Heap usage stable (<200KB, no leaks)
- [ ] ✅ All error scenarios handled gracefully
- [ ] ✅ User documentation complete
- [ ] ✅ Ready for real-world deployment

---

## Build and Deploy Procedure

### Full Build Process

1. **Build React Dashboard**
   ```bash
   cd web-dashboard
   npm install
   npm run build
   ```

2. **Build Firmware**
   ```bash
   cd ..
   pio run
   ```

3. **Upload Filesystem**
   ```bash
   pio run -t uploadfs
   ```

4. **Upload Firmware**
   ```bash
   pio run -t upload
   ```

5. **Monitor**
   ```bash
   pio device monitor
   ```

### Automated Script

```bash
./build_and_flash.sh
```

---

## Post-MVP Roadmap

### Priority A: TCP and NMEA Enhancements
- Configurable sentence filtering
- NMEA sentence statistics
- Better error reporting

### Priority B: Bluetooth Support
- Product 203: Bluetooth Classic SPP
- Product 905: Bluetooth configuration

### Priority C: Calculated Values
- Product 302: TWS/TWA calculation
- Product 906: Calculated values configuration

### Priority D: SD Card Storage
- Product 204: SD card logging
- Product 904: SD configuration
- Product 303: Replay mode

### Priority E: SignalK
- Product 202: SignalK server integration

### Priority F: SeaTalk1
- Product 102: SeaTalk1 interface

### Priority G: Advanced Features
- Products 907, 908, 909
- OTA updates
- Advanced dashboard widgets
