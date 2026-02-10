# Marine Gateway - Project Documentation

Complete documentation for the Marine Gateway ESP32-S3 firmware development.

## Overview

Firmware for ESP32-S3 to interface with marine navigation systems (wind, speed, depth data, etc.) and stream this information via WiFi and Bluetooth (TCP and/or SignalK).

## Documents

- [01-REQUIREMENTS.md](./01-REQUIREMENTS.md) - Initial requirements and product breakdown
- [02-ARCHITECTURE.md](./02-ARCHITECTURE.md) - Detailed technical architecture
- [03-PROJECT_STRUCTURE.md](./03-PROJECT_STRUCTURE.md) - File and folder structure
- [04-ITERATIONS.md](./04-ITERATIONS.md) - Iterative development plan
- [05-CONFIGURATION.md](./05-CONFIGURATION.md) - PlatformIO configuration and partitioning
- [06-BUILD_DEPLOY.md](./06-BUILD_DEPLOY.md) - Build and deployment procedures
- [07-IMPLEMENTATION_NOTES.md](./07-IMPLEMENTATION_NOTES.md) - Technical implementation notes
- [08-TESTING.md](./08-TESTING.md) - Testing strategy and plan

## Quick Start

### Prerequisites
- PlatformIO (via pip or VS Code extension)
- Node.js 18+ (for React dashboard)
- ESP32-S3 with 4MB Flash

### Build MVP
```bash
./build_and_flash.sh
```

See [06-BUILD_DEPLOY.md](./06-BUILD_DEPLOY.md) for details.

## MVP Roadmap

**Phase 1**: UART + NMEA parsing  
**Phase 2**: Persistent WiFi configuration  
**Phase 3**: TCP server on port 10110  
**Phase 4**: Web dashboard backend (REST API + WebSocket)  
**Phase 5**: Web dashboard frontend (React)  
**Phase 6**: Final integration and testing

## Target Hardware

- **Microcontroller**: ESP32-S3 Zero (4MB Flash)
- **UART**: UART1 for NMEA0183 input
- **WiFi**: 2.4GHz 802.11 b/g/n
- **Storage**: LittleFS for web dashboard

## MVP Features

- [x] Product 101: NMEA0183 via UART1 interface
- [x] Product 201: TCP stream (port 10110)
- [x] Product 301: NMEA stream buffering
- [x] Product 901: WiFi configuration dashboard
- [x] Product 902: Serial port configuration dashboard

## Future Features (Post-MVP)

**Priority A**: TCP and NMEA  
**Priority B**: Bluetooth support  
**Priority C**: Calculated values (TWS/TWA)  
**Priority D**: SD card storage  
**Priority E**: SignalK integration  
**Priority F**: SeaTalk1 support  
**Priority G**: Additional features  

## License

TBD

## Contributing

TBD
