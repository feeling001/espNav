# Marine Gateway - Quick Start Guide

## Prerequisites

### Software
- **PlatformIO**: `pip install platformio`
- **Node.js 18+**: Download from https://nodejs.org/

### Hardware
- ESP32-S3 development board (4MB Flash)
- USB cable
- NMEA0183 device (GPS, marine instruments)

## Initial Build

### Option 1: Automated (Recommended)

```bash
# Make build script executable
chmod +x build_and_flash.sh

# Run complete build and flash
./build_and_flash.sh
```

This will:
1. Build React dashboard
2. Copy to data/www/
3. Build ESP32 firmware
4. Upload filesystem
5. Upload firmware

### Option 2: Step by Step

```bash
# 1. Install dashboard dependencies
cd web-dashboard
npm install

# 2. Build dashboard
npm run build
cd ..

# 3. Build and upload firmware
pio run
pio run -t uploadfs
pio run -t upload

# 4. Monitor serial output
pio device monitor
```

## First Configuration

1. **Connect to ESP32**
   - After flashing, ESP32 will create a WiFi AP
   - SSID: `MarineGateway-XXXXXX` (XXXXXX = last 3 MAC bytes)
   - Password: `marine123`

2. **Access Dashboard**
   - Connect to the AP
   - Open browser to `http://192.168.4.1`

3. **Configure WiFi**
   - Go to "WiFi Config" page
   - Enter your WiFi SSID and password
   - Save configuration

4. **Configure Serial Port**
   - Go to "Serial Config" page
   - Set baud rate (typically 38400 for NMEA)
   - Keep other settings at default (8N1)
   - Save configuration

5. **Restart ESP32**
   - Go to "System Status" page
   - Click "Restart ESP32"
   - Wait 30 seconds

6. **Connect NMEA Device**
   - Connect NMEA TX to ESP32 GPIO16 (UART1 RX)
   - Connect GND to GND
   - Power on NMEA device

7. **Verify Operation**
   - Connect to ESP32's new IP address (shown on System Status)
   - Go to "NMEA Monitor" to see live data
   - TCP clients can connect on port 10110

## Connecting with OpenCPN

1. In OpenCPN, go to Options → Connections
2. Add new connection:
   - Type: Network
   - Protocol: TCP
   - Address: `<ESP32_IP>`
   - Port: `10110`
   - Direction: Receive
3. Enable the connection
4. Navigation data should now appear in OpenCPN

## Troubleshooting

### ESP32 doesn't create AP
- Check power supply (USB should provide 5V/500mA minimum)
- Try erasing flash: `pio run -t erase` then reflash
- Check serial monitor for errors: `pio device monitor`

### Dashboard doesn't load
- Verify LittleFS was uploaded: `pio run -t uploadfs`
- Check serial monitor for LittleFS mount errors
- Try clearing browser cache

### No NMEA data
- Verify wiring: NMEA TX → GPIO16, GND → GND
- Check baud rate matches NMEA device (usually 38400)
- Monitor serial output to see if sentences are received
- Verify NMEA device is powered and transmitting

### WiFi won't connect
- Double-check SSID and password (case-sensitive)
- Ensure WiFi is 2.4GHz (ESP32-S3 doesn't support 5GHz)
- Check signal strength (ESP32 should be close to router)
- Try factory reset: hold BOOT button for 10s on startup

### TCP clients can't connect
- Verify ESP32 is on same network
- Check firewall settings
- Ping ESP32 IP address
- Verify port 10110 is not blocked

## Development

### Monitor Serial Output
```bash
pio device monitor
```

### Rebuild Dashboard Only
```bash
cd web-dashboard
npm run build
cd ..
pio run -t uploadfs
```

### Rebuild Firmware Only
```bash
pio run -t upload
```

### Clean Build
```bash
pio run -t clean
cd web-dashboard
rm -rf dist/ node_modules/.cache
npm run build
```

## Memory Usage

Expected memory usage:
- **Firmware**: ~800KB - 1.2MB
- **Dashboard**: ~350KB uncompressed, ~120KB gzipped
- **Free Heap**: ~200-250KB during operation
- **PSRAM**: Not required but can be used if available

## Next Steps

- Configure sentence filtering (future feature)
- Set up SD card logging (future feature)
- Enable Bluetooth (future feature)
- Add calculated wind values (future feature)

## Support

For issues or questions:
1. Check serial monitor output
2. Review documentation in `docs/` folder
3. Check GitHub issues

## License

TBD
