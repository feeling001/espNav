# Build and Deployment Procedures

## Prerequisites

### Software Requirements

1. **PlatformIO**
   - Install via pip: `pip install platformio`
   - Or install PlatformIO IDE extension for VS Code

2. **Node.js** (version 18 or higher)
   - Download from: https://nodejs.org/
   - Verify: `node --version`

3. **Git** (optional but recommended)
   - For version control

### Hardware Requirements

- ESP32-S3 development board (4MB flash minimum)
- USB cable (data capable)
- UART-compatible NMEA device (for testing)

### Driver Installation

**Windows**: Install CP210x or CH340 USB-to-Serial driver

**Linux**: Add user to dialout group
```bash
sudo usermod -a -G dialout $USER
# Logout and login for changes to take effect
```

**macOS**: Usually works out of the box

## Project Setup

### 1. Clone or Download Project

```bash
git clone <repository-url> marine-gateway
cd marine-gateway
```

### 2. Install Dependencies

#### Firmware Dependencies
PlatformIO will automatically download all required libraries on first build.

#### Dashboard Dependencies
```bash
cd web-dashboard
npm install
cd ..
```

## Build Process

### Option 1: Automated Build (Recommended)

Use the provided build script for a complete build and flash:

```bash
chmod +x build_and_flash.sh
./build_and_flash.sh
```

This script will:
1. Build the React dashboard
2. Copy dashboard files to `data/www/`
3. Build the ESP32 firmware
4. Upload the filesystem to ESP32
5. Upload the firmware to ESP32

### Option 2: Manual Build Steps

#### Step 1: Build React Dashboard

```bash
cd web-dashboard
npm run build
```

Output will be in `web-dashboard/dist/`

#### Step 2: Copy Dashboard to Data Folder

```bash
# From web-dashboard directory
cp -r dist/* ../data/www/

# Or from project root
rm -rf data/www/*
cp -r web-dashboard/dist/* data/www/
```

#### Step 3: Build Firmware

```bash
# From project root
pio run
```

Build output: `.pio/build/esp32s3/firmware.bin`

#### Step 4: Upload Filesystem

```bash
pio run -t uploadfs
```

This uploads the contents of `data/` to the ESP32's LittleFS partition.

#### Step 5: Upload Firmware

```bash
pio run -t upload
```

## Build Script: build_and_flash.sh

Complete automated build script:

```bash
#!/bin/bash
set -e

echo "========================================"
echo "  Marine Gateway - Build & Flash"
echo "========================================"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if we're in the right directory
if [ ! -f "platformio.ini" ]; then
    echo "Error: platformio.ini not found. Run this script from the project root."
    exit 1
fi

# Step 1: Build React Dashboard
echo -e "${BLUE}[1/5] Building React dashboard...${NC}"
cd web-dashboard
npm run build
cd ..
echo -e "${GREEN}✓ Dashboard built${NC}"

# Step 2: Copy to data folder
echo -e "${BLUE}[2/5] Copying dashboard to data/www...${NC}"
rm -rf data/www/*
mkdir -p data/www
cp -r web-dashboard/dist/* data/www/
echo -e "${GREEN}✓ Dashboard copied${NC}"

# Step 3: Build firmware
echo -e "${BLUE}[3/5] Building firmware...${NC}"
pio run
echo -e "${GREEN}✓ Firmware built${NC}"

# Step 4: Upload filesystem
echo -e "${BLUE}[4/5] Uploading filesystem to ESP32...${NC}"
pio run -t uploadfs
echo -e "${GREEN}✓ Filesystem uploaded${NC}"

# Step 5: Upload firmware
echo -e "${BLUE}[5/5] Uploading firmware to ESP32...${NC}"
pio run -t upload
echo -e "${GREEN}✓ Firmware uploaded${NC}"

echo ""
echo "========================================"
echo -e "${GREEN}  Build & Flash Complete!${NC}"
echo "========================================"
echo ""
echo "Next steps:"
echo "1. Open serial monitor: pio device monitor"
echo "2. The ESP32 will create an AP: 'MarineGateway-XXXXXX'"
echo "3. Connect and configure WiFi at: http://192.168.4.1"
echo ""
```

Make it executable:
```bash
chmod +x build_and_flash.sh
```

## Incremental Builds

### Only Dashboard Changed

```bash
cd web-dashboard
npm run build
cd ..
cp -r web-dashboard/dist/* data/www/
pio run -t uploadfs
```

### Only Firmware Changed

```bash
pio run -t upload
```

No need to rebuild or upload filesystem.

### Only Configuration Changed

If you only modified configuration in the dashboard (saved in NVS), no rebuild is necessary.

## Build Variants

### Development Build

Default configuration with debug output:
```bash
pio run -e esp32s3
```

### Production Build

Modify `platformio.ini` to add a production environment:

```ini
[env:esp32s3_production]
extends = env:esp32s3
build_flags = 
    ${env:esp32s3.build_flags}
    -DCORE_DEBUG_LEVEL=1      ; Errors only
    -Os                        ; Optimize for size
    -DNDEBUG                   ; Disable assertions
```

Build:
```bash
pio run -e esp32s3_production
```

## Clean Build

Remove all build artifacts and rebuild from scratch:

```bash
# Clean PlatformIO build
pio run -t clean

# Clean React build
cd web-dashboard
rm -rf dist/ node_modules/.cache
cd ..

# Full rebuild
./build_and_flash.sh
```

## Deployment Procedures

### Initial Deployment

For first-time setup on a new ESP32:

1. **Erase Flash** (optional but recommended)
   ```bash
   pio run -t erase
   ```

2. **Flash Everything**
   ```bash
   ./build_and_flash.sh
   ```

3. **Verify Upload**
   ```bash
   pio device monitor
   ```
   
   Look for:
   - Boot messages
   - WiFi initialization
   - AP mode activation

4. **Initial Configuration**
   - Connect to WiFi AP: `MarineGateway-XXXXXX`
   - Open browser: `http://192.168.4.1`
   - Configure WiFi credentials
   - Configure serial port settings
   - Reboot ESP32

### Firmware Update (OTA)

For future implementation:

```bash
# Set OTA parameters in platformio.ini
pio run -t upload --upload-port 192.168.1.100
```

Or via web interface (when implemented).

### Field Deployment

1. **Pre-configure** WiFi settings before deployment
2. **Test** in controlled environment
3. **Backup** configuration (document settings)
4. **Deploy** and connect to marine instruments
5. **Verify** data flow with OpenCPN or similar

## Monitoring and Debugging

### Serial Monitor

Open serial monitor after upload:
```bash
pio device monitor
```

Useful shortcuts:
- `Ctrl+C` - Exit monitor
- `Ctrl+T` then `Ctrl+H` - Show help

### Monitor with Filters

```bash
pio device monitor --filter esp32_exception_decoder
```

Or configure in `platformio.ini`:
```ini
monitor_filters = 
    esp32_exception_decoder
    colorize
    time
```

### Debug Output Levels

Set in `platformio.ini`:
```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=5  ; 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose
```

### Remote Monitoring

After WiFi connection, you can monitor via TCP:
```bash
nc 192.168.1.100 10110
```

Or via WebSocket in browser console:
```javascript
ws = new WebSocket('ws://192.168.1.100/ws/nmea');
ws.onmessage = (e) => console.log(e.data);
```

## Troubleshooting

### Build Errors

**Error: Library not found**
```bash
pio lib install
pio lib update
```

**Error: Python not found (Windows)**
- Install Python 3.x from python.org
- Add to PATH during installation

**Error: Node/npm not found**
- Install Node.js from nodejs.org
- Verify: `node --version` and `npm --version`

### Upload Errors

**Error: Serial port not found**

Linux:
```bash
ls /dev/tty*  # Find your device (usually /dev/ttyUSB0 or /dev/ttyACM0)
sudo usermod -a -G dialout $USER
```

Windows:
- Check Device Manager for COM port
- Install CH340 or CP210x drivers

**Error: Permission denied**

Linux/macOS:
```bash
sudo chmod 666 /dev/ttyUSB0
# Or add user to dialout group (permanent fix)
```

**Error: Upload timeout**
- Hold BOOT button while connecting
- Try lower upload speed in `platformio.ini`:
  ```ini
  upload_speed = 115200
  ```

### Filesystem Upload Errors

**Error: Filesystem image too large**

Check partition size:
```bash
pio run -t size
```

Reduce dashboard size:
- Remove unused components
- Optimize images
- Check build output in `data/www/`

**Error: LittleFS mount failed**

Re-format filesystem:
```bash
pio run -t erase
pio run -t uploadfs
```

### Runtime Errors

**WiFi not connecting**
- Check SSID and password
- Verify WiFi signal strength
- Check serial monitor for error messages
- Try factory reset (hold BOOT for 10s on startup)

**Dashboard not loading**
- Verify filesystem upload completed
- Check LittleFS partition in serial output
- Try accessing via IP directly: `http://<ESP_IP>`
- Clear browser cache

**NMEA data not flowing**
- Verify UART connections (RX, TX, GND)
- Check baud rate matches NMEA device
- Monitor serial output for UART errors
- Test with NMEA simulator

## Version Management

### Semantic Versioning

Use semantic versioning: MAJOR.MINOR.PATCH

Update version in:
1. `platformio.ini`: `-DVERSION=\"1.0.0\"`
2. `web-dashboard/package.json`: `"version": "1.0.0"`
3. Git tag: `git tag v1.0.0`

### Build Metadata

Include build information:

```ini
build_flags = 
    -DVERSION=\"1.0.0\"
    -DBUILD_DATE=\"2024-02-10\"
    -DGIT_COMMIT=\"abc1234\"
```

Generate automatically with script:
```python
# scripts/version.py
import subprocess
import datetime

git_hash = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD']).decode().strip()
build_date = datetime.datetime.now().strftime("%Y-%m-%d")

print(f'-DGIT_COMMIT=\\"{git_hash}\\"')
print(f'-DBUILD_DATE=\\"{build_date}\\"')
```

## Binary Distribution

### Generate Release Binary

```bash
# Build production version
pio run -e esp32s3_production

# Binary location
.pio/build/esp32s3_production/firmware.bin
```

### Flash Pre-built Binary

Using esptool.py:
```bash
pip install esptool

esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash -z 0x0 bootloader.bin 0x8000 partitions.bin \
  0x20000 firmware.bin 0x320000 littlefs.bin
```

### Create Complete Flash Image

Merge all partitions into single image:
```bash
esptool.py --chip esp32s3 merge_bin -o complete_flash.bin \
  --flash_mode dio --flash_size 4MB \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x20000 firmware.bin \
  0x320000 littlefs.bin
```

Flash complete image:
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 \
  write_flash 0x0 complete_flash.bin
```

## Continuous Integration (CI/CD)

### GitHub Actions Example

`.github/workflows/build.yml`:
```yaml
name: Build Firmware

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Set up Python
        uses: actions/setup-python@v2
        with:
          python-version: '3.x'
      
      - name: Set up Node.js
        uses: actions/setup-node@v2
        with:
          node-version: '18'
      
      - name: Install PlatformIO
        run: pip install platformio
      
      - name: Build Dashboard
        run: |
          cd web-dashboard
          npm install
          npm run build
          cp -r dist/* ../data/www/
      
      - name: Build Firmware
        run: pio run
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v2
        with:
          name: firmware
          path: .pio/build/esp32s3/firmware.bin
```

## Best Practices

### Pre-Deployment Checklist

- [ ] All tests passing
- [ ] Version number updated
- [ ] Configuration documented
- [ ] Serial output clean (no errors)
- [ ] Dashboard loads correctly
- [ ] WiFi connection stable
- [ ] TCP server accepting connections
- [ ] NMEA data flowing correctly
- [ ] 24-hour uptime test passed
- [ ] Backup configuration documented

### Configuration Backup

Before deployment, document:
```
WiFi SSID: ______________
WiFi Password: ______________
Serial Baud: ______________
Serial Config: 8N1
TCP Port: 10110
```

### Update Procedure

1. Test new firmware in development environment
2. Backup current configuration
3. Upload new firmware
4. Verify all functions work
5. Restore configuration if needed
6. Monitor for 1 hour
7. Document changes

## Quick Reference

| Task | Command |
|------|---------|
| Full build & flash | `./build_and_flash.sh` |
| Build firmware only | `pio run` |
| Upload firmware only | `pio run -t upload` |
| Upload filesystem only | `pio run -t uploadfs` |
| Clean build | `pio run -t clean` |
| Serial monitor | `pio device monitor` |
| Erase flash | `pio run -t erase` |
| Build dashboard | `cd web-dashboard && npm run build` |
| Update libraries | `pio lib update` |

## Support and Resources

- PlatformIO Documentation: https://docs.platformio.org/
- ESP32 Arduino Core: https://github.com/espressif/arduino-esp32
- ESPAsyncWebServer: https://github.com/me-no-dev/ESPAsyncWebServer
- ArduinoJson: https://arduinojson.org/
