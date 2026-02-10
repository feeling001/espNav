# PlatformIO Configuration

## platformio.ini

Complete PlatformIO configuration file for the Marine Gateway project.

```ini
; Marine Gateway - ESP32-S3 Firmware
; NMEA-0183 to TCP/WiFi Gateway

[platformio]
default_envs = esp32s3

[env:esp32s3]
; Platform and board
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

; Partition scheme
board_build.partitions = partitions.csv
board_build.filesystem = littlefs

; CPU and Flash frequencies
board_build.f_cpu = 240000000L
board_build.f_flash = 80000000L

; Flash size
board_upload.flash_size = 4MB

; Serial monitor
monitor_speed = 115200
monitor_filters = 
    esp32_exception_decoder
    colorize

; Monitor port (auto-detect, or specify)
; monitor_port = /dev/ttyUSB0

; Upload settings
upload_protocol = esptool
upload_speed = 921600

; Build flags
build_flags = 
    ; Debug level (0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose)
    -DCORE_DEBUG_LEVEL=3
    
    ; Enable PSRAM if available
    -DBOARD_HAS_PSRAM
    
    ; USB CDC for serial output
    -DARDUINO_USB_CDC_ON_BOOT=1
    
    ; Arduino IDE compatibility
    -DARDUINO_ESP32_DEV
    
    ; Optimization flags
    -O2
    
    ; Project version
    -DVERSION=\"1.0.0\"
    -DBUILD_DATE=\"$UNIX_TIME\"

; Library dependencies
lib_deps = 
    ; Async web server
    me-no-dev/ESPAsyncWebServer@^1.2.3
    
    ; Async TCP for non-blocking sockets
    me-no-dev/AsyncTCP@^1.1.1
    
    ; JSON parsing and generation
    bblanchon/ArduinoJson@^7.0.4
    
    ; Preferences (NVS wrapper)
    ; (Built into ESP32 Arduino Core)

; Extra scripts for automation
; extra_scripts = 
;     pre:scripts/generate_version.py
;     post:scripts/compress_filesystem.py

; OTA settings (for future use)
; upload_protocol = espota
; upload_port = 192.168.1.100
; upload_flags =
;     --port=3232
;     --auth=marine123

[env:esp32s3_debug]
extends = env:esp32s3
build_type = debug
build_flags = 
    ${env:esp32s3.build_flags}
    -DCORE_DEBUG_LEVEL=5
    -DDEBUG_ESP_PORT=Serial
    -DDEBUG_ESP_HTTP_SERVER
    -DDEBUG_ESP_WIFI
    -DDEBUG_ESP_TCP
```

## Configuration Sections Explained

### Platform and Board

```ini
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
```

- **platform**: Espressif 32 (ESP32, ESP32-S3, ESP32-C3, etc.)
- **board**: ESP32-S3 DevKit C (adjust for your specific board)
- **framework**: Arduino framework for ease of development

### Partitioning

```ini
board_build.partitions = partitions.csv
board_build.filesystem = littlefs
```

- Uses custom partition table (see below)
- LittleFS for web dashboard storage

### Frequencies

```ini
board_build.f_cpu = 240000000L
board_build.f_flash = 80000000L
```

- CPU: 240MHz (maximum performance)
- Flash: 80MHz (fast filesystem access)

### Debug Settings

```ini
monitor_speed = 115200
monitor_filters = 
    esp32_exception_decoder
    colorize
```

- Serial monitor at 115200 baud
- Exception decoder for crash analysis
- Colored output for readability

### Build Flags

Key flags explained:

- `CORE_DEBUG_LEVEL=3`: Info-level logging
- `BOARD_HAS_PSRAM`: Enable PSRAM support
- `ARDUINO_USB_CDC_ON_BOOT=1`: USB Serial available immediately
- `-O2`: Optimization level 2 (balance speed/size)

### Library Dependencies

All dependencies are automatically downloaded by PlatformIO:

1. **ESPAsyncWebServer**: Non-blocking HTTP server
2. **AsyncTCP**: Non-blocking TCP library
3. **ArduinoJson**: Efficient JSON library

## partitions.csv

Custom partition table for 4MB flash.

```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x8000,   
phy_init, data, phy,     0x11000, 0x1000,   
factory,  app,  factory, 0x20000, 0x180000, 
ota_0,    app,  ota_0,   0x1A0000,0x180000, 
spiffs,   data, spiffs,  0x320000,0xE0000,  
```

### Partition Layout

| Partition | Type | Offset | Size | Purpose |
|-----------|------|--------|------|---------|
| nvs | data/nvs | 0x9000 | 32KB | Configuration storage |
| phy_init | data/phy | 0x11000 | 4KB | WiFi PHY calibration data |
| factory | app/factory | 0x20000 | 1.5MB | Main firmware |
| ota_0 | app/ota_0 | 0x1A0000 | 1.5MB | OTA update partition |
| spiffs | data/spiffs | 0x320000 | 896KB | LittleFS filesystem |

**Note**: Although named "spiffs" in partition table (for compatibility), we configure LittleFS via `board_build.filesystem = littlefs`.

### Memory Map Visualization

```
0x000000 ┌─────────────────────┐
         │ Bootloader (64KB)   │
0x008000 ├─────────────────────┤
         │ Partition Table     │
0x009000 ├─────────────────────┤
         │ NVS (32KB)          │
0x011000 ├─────────────────────┤
         │ PHY Init (4KB)      │
0x020000 ├─────────────────────┤
         │                     │
         │ Factory App         │
         │ (1.5MB)             │
         │                     │
0x1A0000 ├─────────────────────┤
         │                     │
         │ OTA_0 Partition     │
         │ (1.5MB)             │
         │                     │
0x320000 ├─────────────────────┤
         │                     │
         │ LittleFS            │
         │ (896KB)             │
         │                     │
0x400000 └─────────────────────┘
```

## Build Variants

### Development Build

Use default environment:
```bash
pio run -e esp32s3
```

Features:
- Debug level: Info (3)
- Optimized for balance (O2)
- Normal size constraints

### Debug Build

Use debug environment:
```bash
pio run -e esp32s3_debug
```

Features:
- Debug level: Verbose (5)
- All debug output enabled
- Easier troubleshooting

### Production Build

Modify build flags for production:
```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=1  ; Errors only
    -Os                    ; Optimize for size
    -DNDEBUG               ; Disable assertions
```

## Upload Methods

### USB Serial (Default)

```bash
pio run -t upload
```

Auto-detects serial port and uploads via USB.

### OTA (Over-The-Air)

For future implementation:

```ini
upload_protocol = espota
upload_port = 192.168.1.100
upload_flags =
    --port=3232
    --auth=marine123
```

```bash
pio run -t upload --upload-port 192.168.1.100
```

## Filesystem Upload

Upload web dashboard to LittleFS:

```bash
pio run -t uploadfs
```

**Important**: Build React dashboard first!

```bash
cd web-dashboard
npm run build
cd ..
pio run -t uploadfs
```

## Monitoring

### Serial Monitor

```bash
pio device monitor
```

Options in platformio.ini:
- Speed: 115200 baud
- Filters: exception decoder, colorize

### Custom Monitor Filters

Available filters:
- `esp32_exception_decoder`: Decode crash dumps
- `colorize`: Color-coded output
- `time`: Add timestamps
- `log2file`: Log to file

Example:
```ini
monitor_filters = 
    esp32_exception_decoder
    colorize
    time
```

## Build Commands Reference

| Command | Description |
|---------|-------------|
| `pio run` | Build firmware |
| `pio run -t upload` | Upload firmware via USB |
| `pio run -t uploadfs` | Upload filesystem (LittleFS) |
| `pio run -t clean` | Clean build files |
| `pio device monitor` | Open serial monitor |
| `pio run -t menuconfig` | ESP-IDF configuration (if needed) |
| `pio update` | Update platforms and libraries |
| `pio lib install` | Install library dependencies |

## Custom Build Scripts (Optional)

### Pre-build: Generate Version

`scripts/generate_version.py`:
```python
Import("env")
import datetime

build_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
env.Append(CPPDEFINES=[
    ("BUILD_TIMESTAMP", f'\\"{build_time}\\"')
])
```

### Post-build: Compress Filesystem

`scripts/compress_filesystem.py`:
```python
Import("env")
import gzip
import os

def compress_files(source, target, env):
    data_dir = "data/www"
    for root, dirs, files in os.walk(data_dir):
        for file in files:
            if file.endswith(('.html', '.css', '.js')):
                filepath = os.path.join(root, file)
                with open(filepath, 'rb') as f_in:
                    with gzip.open(filepath + '.gz', 'wb') as f_out:
                        f_out.writelines(f_in)
                print(f"Compressed: {filepath}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", compress_files)
```

## Library Configuration

### ArduinoJson

Configuration in code:
```cpp
#define ARDUINOJSON_USE_LONG_LONG 1
#define ARDUINOJSON_ENABLE_PROGMEM 1
```

Recommended document size:
```cpp
StaticJsonDocument<1024> doc;  // For config
DynamicJsonDocument doc(2048); // For large responses
```

### ESPAsyncWebServer

Default configuration usually sufficient. For custom:
```cpp
AsyncWebServer server(80);
server.begin();
```

## Troubleshooting Build Issues

### Issue: Library Not Found

```bash
pio lib install
```

Or specify version in platformio.ini:
```ini
lib_deps = 
    bblanchon/ArduinoJson@^7.0.4
```

### Issue: Partition Table Too Large

Reduce partition sizes in `partitions.csv` or use different flash size.

### Issue: Filesystem Upload Fails

1. Ensure `data/www/` contains files
2. Check partition table has `spiffs` entry
3. Try: `pio run -t erase` then `pio run -t uploadfs`

### Issue: Serial Port Not Found

Linux:
```bash
sudo usermod -a -G dialout $USER
# Logout and login
```

Specify port manually:
```ini
upload_port = /dev/ttyUSB0
monitor_port = /dev/ttyUSB0
```

### Issue: Upload Speed Too Slow

Increase upload speed:
```ini
upload_speed = 921600
```

Or use faster protocol:
```ini
upload_protocol = esp-builtin  ; For ESP32-S3 native USB
```

## Advanced Configuration

### Enable PSRAM

If board has PSRAM:
```ini
build_flags = 
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
```

Usage in code:
```cpp
void* buffer = ps_malloc(100000);  // Allocate from PSRAM
```

### Custom Partition Scheme

For different flash sizes:

**8MB Flash**:
```csv
nvs,      data, nvs,     0x9000,  0x10000,  
phy_init, data, phy,     0x19000, 0x1000,   
factory,  app,  factory, 0x20000, 0x300000, 
ota_0,    app,  ota_0,   0x320000,0x300000, 
spiffs,   data, spiffs,  0x620000,0x1E0000,
```

### JTAG Debugging

For hardware debugging:
```ini
debug_tool = esp-builtin
debug_init_break = tbreak setup
debug_speed = 5000
```

## Environment Variables

Set in platformio.ini or shell:

```ini
[env:esp32s3]
extra_scripts = pre:set_env.py

; set_env.py
Import("env")
env.Append(CPPDEFINES=[
    ("WIFI_SSID", '\\"' + env.get("WIFI_SSID", "MarineGateway") + '\\"'),
    ("WIFI_PASS", '\\"' + env.get("WIFI_PASS", "marine123") + '\\"')
])
```

Shell:
```bash
export WIFI_SSID="MyNetwork"
export WIFI_PASS="secret"
pio run
```

## Size Optimization Tips

### Optimize for Size

```ini
build_flags = 
    -Os                    ; Optimize for size
    -ffunction-sections    ; Enable dead code elimination
    -fdata-sections
    -Wl,--gc-sections
```

### Reduce Binary Size

- Remove unused libraries
- Minimize String literals
- Use PROGMEM for constants
- Disable unused features

### Check Binary Size

```bash
pio run --target size
```

Output:
```
RAM:   [==        ]  20.5% (used 67120 bytes from 327680 bytes)
Flash: [=====     ]  45.3% (used 711234 bytes from 1572864 bytes)
```

## Recommended .gitignore

```gitignore
.pio/
.vscode/
*.pyc
__pycache__/
```
