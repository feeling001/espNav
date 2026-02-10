# Requirements Specification

## Project Objective

Development of firmware for ESP32 to interface with marine navigation systems (wind, speed, depth data, etc.) and stream this information via WiFi, Bluetooth (TCP and/or SignalK).

## Product Breakdown

### 1xx - Navigation System Interfaces

#### Product 101: NMEA0183 Interface
- **Description**: Interface with NMEA0183 navigation system via UART1
- **Technical Details**:
  - Hardware: UART1 on ESP32
  - Configuration: Configurable baud rate (typically 38400, but must support 4800-115200)
  - Direction: Receive only (RX)
  - Data format: Standard NMEA0183 sentences
- **MVP**: ✅ Included

#### Product 102: SeaTalk1 Interface
- **Description**: Interface with SeaTalk1 equipment via software serial
- **Technical Details**:
  - Protocol: 9N1 (9 bits, no parity, 1 stop bit)
  - Implementation: Software serial
- **MVP**: ❌ Post-MVP (Priority F)

#### Product 103: NMEA2000 Interface
- **Description**: Interface with NMEA2000 equipment
- **Technical Details**:
  - Protocol: CAN bus based
  - Hardware: Requires CAN transceiver
- **MVP**: ❌ Post-MVP

### 2xx - Output Interfaces

#### Product 201: TCP Stream
- **Description**: Stream NMEA data over TCP
- **Technical Details**:
  - Protocol: TCP server
  - Port: 10110 (NMEA-0183 standard)
  - Clients: Support multiple simultaneous connections
  - Format: Raw NMEA sentences with `\r\n` line endings
- **MVP**: ✅ Included

#### Product 202: SignalK via WiFi
- **Description**: Publish data using SignalK protocol
- **Technical Details**:
  - Protocol: SignalK over HTTP/WebSocket
  - Format: JSON
- **MVP**: ❌ Post-MVP (Priority E)

#### Product 203: Bluetooth Classic SPP
- **Description**: Stream data via Bluetooth Serial Port Profile
- **Technical Details**:
  - Protocol: Bluetooth Classic SPP
  - Pairing: PIN-based
- **MVP**: ❌ Post-MVP (Priority B)

#### Product 204: SD Card Storage
- **Description**: Log NMEA data to SD card
- **Technical Details**:
  - Interface: SPI
  - Format: Plain text NMEA sentences
  - File rotation: Configurable max file size
- **MVP**: ❌ Post-MVP (Priority D)

### 3xx - Data Processing Features

#### Product 301: Stream Buffering
- **Description**: Buffer NMEA stream (~50 lines)
- **Technical Details**:
  - Purpose: Handle congestion, rate limiting, calculations
  - Implementation: FreeRTOS message queue
- **MVP**: ✅ Included (basic implementation)
- **Note**: Advanced features (rate limiting, congestion control) post-MVP

#### Product 302: True Wind Calculation
- **Description**: Calculate TWS (True Wind Speed) and TWA (True Wind Angle)
- **Technical Details**:
  - Inputs: AWS (Apparent Wind Speed), AWA (Apparent Wind Angle), GPS data
  - Output: Calculated NMEA sentences for TWS/TWA
  - Configuration: Enable/disable via dashboard
- **MVP**: ❌ Post-MVP (Priority C)

#### Product 303: Replay Mode
- **Description**: Switch between live and replay mode
- **Technical Details**:
  - Live mode: Real-time NMEA/SeaTalk data
  - Replay mode: Playback from SD card file
  - Dependencies: Product 204 (SD storage)
- **MVP**: ❌ Post-MVP (depends on Product 204)

### 9xx - Web Dashboard

#### Product 901: WiFi Configuration
- **Description**: Configure WiFi settings
- **Features**:
  - Mode selection: Infrastructure (STA) or Access Point (AP)
  - Network scan and selection
  - Connection status display (connected clients, signal strength)
  - Automatic AP fallback on connection failure
  - Persistent configuration
- **MVP**: ✅ Included

#### Product 902: Serial Port Configuration
- **Description**: Configure NMEA0183 UART parameters
- **Features**:
  - Baud rate selection (4800-115200)
  - Data bits, parity, stop bits
  - Configuration persistence
- **MVP**: ✅ Included

#### Product 903: Live NMEA Monitor
- **Description**: Display live NMEA stream in dashboard
- **Features**:
  - Real-time display via WebSocket
  - Auto-scroll
  - Pause/resume
  - Line limit (e.g., 100 last lines)
- **MVP**: ✅ Included (basic implementation)

#### Product 904: SD Card Configuration
- **Description**: Configure SD storage parameters
- **Features**:
  - Max file size
  - File rotation settings
  - Dependencies: Product 204
- **MVP**: ❌ Post-MVP

#### Product 905: Bluetooth Configuration
- **Description**: Configure Bluetooth parameters
- **Features**:
  - Device name
  - PIN code
  - Visibility settings
  - Dependencies: Product 203
- **MVP**: ❌ Post-MVP

#### Product 906: Calculated Values Configuration
- **Description**: Configure multiplexed and calculated values
- **Features**:
  - Enable/disable TWA/TWS calculation
  - Depth conversion (DPT/DBT)
  - Other calculations
  - Dependencies: Product 302
- **MVP**: ❌ Post-MVP

#### Product 907: Sentence Filtering
- **Description**: Filter redundant NMEA sentences
- **Features**:
  - Select which sentence types to forward
  - Reduce bandwidth on redundant data
- **MVP**: ❌ Post-MVP

#### Product 908: Unit Configuration
- **Description**: Display units and conversion settings
- **Features**:
  - Unit preferences (metric/imperial)
  - Automatic conversion for display
- **MVP**: ❌ Post-MVP

#### Product 909: Navigation Instruments Dashboard
- **Description**: Configurable web dashboard for navigation instruments
- **Features**:
  - Real-time data visualization
  - Configurable widgets (wind rose, depth gauge, etc.)
  - Suitable for tablet display
  - Data via WebSocket stream
- **MVP**: ❌ Post-MVP

## Dashboard Requirements

### General Requirements
- **Responsive**: Must work on mobile, tablet, and desktop
- **Modern Interface**: React-based
- **Persistent Menu**: Collapsible left sidebar
- **Communication**: REST API for configuration, WebSocket for real-time data
- **System Interface**: Display space/memory/GPIO status/temperature
- **OTA Updates**: Firmware update capability
- **Logs**: Diagnostics and logging
- **Persistent Configuration**: Store settings in NVS
- **Access Control**: Open access except for configuration (basic user/password)

### Technical Stack
- **Frontend**: React + Vite
- **Backend**: ESPAsyncWebServer
- **API**: REST + WebSocket
- **Storage**: LittleFS (dashboard files), NVS (configuration)

## MVP Scope

The initial MVP will focus on core NMEA streaming functionality:

### Included Products
1. **Product 101**: NMEA0183 via UART1
2. **Product 201**: TCP stream (port 10110)
3. **Product 301**: Basic stream buffering
4. **Product 901**: WiFi configuration dashboard
5. **Product 902**: Serial configuration dashboard
6. **Product 903**: Live NMEA monitor (basic)

### Success Criteria
- Receive NMEA0183 data from UART1 at configurable baud rate
- Stream received data to multiple TCP clients on port 10110
- Configure WiFi via web dashboard with persistence
- Configure serial port parameters via web dashboard
- Automatic AP fallback if WiFi connection fails
- View live NMEA stream in web dashboard

## Post-MVP Priorities

**A**: TCP and NMEA enhancements  
**B**: Bluetooth support (Product 203, 905)  
**C**: Calculated values (Product 302, 906)  
**D**: SD card storage (Product 204, 904)  
**E**: SignalK integration (Product 202)  
**F**: SeaTalk1 support (Product 102)  
**G**: All other features

## Hardware Constraints

- **Target Device**: ESP32-S3 Zero
- **Flash Memory**: 4MB total
- **RAM**: 512KB SRAM + optional PSRAM
- **WiFi**: 2.4GHz only
- **Filesystem**: LittleFS preferred over SPIFFS

## Non-Functional Requirements

- **Reliability**: 24/7 operation capability
- **Performance**: <100ms latency for NMEA forwarding
- **Memory**: Stable heap usage over extended operation
- **Maintainability**: Modular architecture for easy feature additions
- **Compatibility**: Standard NMEA-0183 TCP port 10110
