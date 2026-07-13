# Marine Gateway — Backend API Endpoints Documentation

> Embedded HTTP server on ESP32 (port **80**), based on ESPAsyncWebServer.  
> All responses are `application/json`.  
> Web dashboard static files are served from LittleFS (`/www/`).  
> A raw NMEA WebSocket stream is available at `/ws/nmea`.

---

## Table of Contents

1. [WiFi Configuration](#1-wifi-configuration)
2. [Serial Configuration (UART)](#2-serial-configuration-uart)
3. [BLE Configuration](#3-ble-configuration)
4. [System Status](#4-system-status)
5. [Restart](#5-restart)
6. [WiFi Scan](#6-wifi-scan)
7. [Boat Data — Navigation](#7-boat-data--navigation)
8. [Boat Data — Wind](#8-boat-data--wind)
9. [Boat Data — AIS](#9-boat-data--ais)
10. [Boat Data — Full State](#10-boat-data--full-state)
11. [NMEA WebSocket](#11-nmea-websocket)
12. [Performance Configuration](#12-performance-configuration)
13. [Alarms](#13-alarms)

---

## 1. WiFi Configuration

### `GET /api/config/wifi`

Returns the current WiFi configuration.

**Response:**
```json
{
  "ssid": "MyNetwork",
  "mode": 0,
  "has_password": true,
  "ap_ssid": "MarineGateway-AABBCC",
  "ap_has_password": false
}
```

| Field | Type | Description |
|---|---|---|
| `ssid` | string | SSID of the network to connect to (STA mode) |
| `mode` | int | `0` = STA (client), `1` = AP (access point) |
| `has_password` | bool | `true` if a STA password is configured |
| `ap_ssid` | string | SSID of the created access point (AP mode) |
| `ap_has_password` | bool | `true` if the AP password is ≥ 8 characters |

---

### `POST /api/config/wifi`

Saves the WiFi configuration to NVS. A restart is recommended to apply changes.

**Request body:**
```json
{
  "ssid": "MyNetwork",
  "password": "mypassword",
  "mode": 0,
  "ap_ssid": "MyGateway",
  "ap_password": "appassword"
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `ssid` | string | No | SSID for STA mode |
| `password` | string | No | Password for STA mode |
| `mode` | int | No | `0` = STA, `1` = AP (default: 0) |
| `ap_ssid` | string | No | Custom SSID for AP mode |
| `ap_password` | string | No | AP password (min 8 characters) |

**Success response:**
```json
{ "success": true, "message": "WiFi config saved. Restart to apply." }
```

**Error response (invalid JSON):**
```json
{ "error": "Invalid JSON" }
```

---

## 2. Serial Configuration (UART)

### `GET /api/config/serial`

Returns the current UART configuration used to read NMEA data from the serial port.

**Response:**
```json
{
  "baudRate": 38400,
  "dataBits": 8,
  "parity": 0,
  "stopBits": 1
}
```

| Field | Type | Values | Description |
|---|---|---|---|
| `baudRate` | int | e.g. 4800, 9600, 38400, 115200 | Baud rate |
| `dataBits` | int | 5–8 | Data bits |
| `parity` | int | `0`=None, `1`=Even, `2`=Odd | Parity |
| `stopBits` | int | 1–2 | Stop bits |

---

### `POST /api/config/serial`

Saves the UART configuration.

**Request body:**
```json
{
  "baudRate": 4800,
  "dataBits": 8,
  "parity": 0,
  "stopBits": 1
}
```

**Success response:**
```json
{ "success": true, "message": "Serial config saved. Restart to apply." }
```

---

## 3. BLE Configuration

### `GET /api/config/ble`

Returns the Bluetooth Low Energy configuration and current runtime state.

**Response:**
```json
{
  "enabled": true,
  "device_name": "MarineGateway",
  "pin_code": "123456",
  "advertising": true,
  "connected_devices": 1
}
```

| Field | Type | Description |
|---|---|---|
| `enabled` | bool | Whether BLE is enabled |
| `device_name` | string | BLE advertised name (max 31 characters) |
| `pin_code` | string | Pairing PIN code (exactly 6 digits) |
| `advertising` | bool | `true` if the device is currently advertising |
| `connected_devices` | int | Number of currently connected BLE devices |

---

### `POST /api/config/ble`

Saves and applies the BLE configuration.  
⚠️ A restart is required for `device_name` changes to take full effect.

**Request body:**
```json
{
  "enabled": true,
  "device_name": "MarineGateway",
  "pin_code": "654321"
}
```

| Field | Type | Constraints |
|---|---|---|
| `enabled` | bool | — |
| `device_name` | string | max 31 characters |
| `pin_code` | string | exactly 6 numeric digits |

**Success response:**
```json
{ "success": true, "message": "BLE config saved and applied" }
```

**Error responses:**
```json
{ "error": "PIN code must be exactly 6 digits" }
{ "error": "PIN code must contain only digits" }
```

---

## 4. System Status

### `GET /api/status`

Returns the overall system state: memory, WiFi, TCP, UART, NMEA buffer, BLE, SD card, and chip temperature.

**Response:**
```json
{
  "version": "1.2.0",
  "uptime": 3600,
  "datetime": 1718900000,
  "heap": {
    "free": 120000,
    "total": 327680,
    "min_free": 95000
  },
  "wifi": {
    "mode": "STA",
    "ssid": "MyNetwork",
    "rssi": -62,
    "ip": "192.168.1.100",
    "clients": 0
  },
  "tcp": {
    "clients": 2,
    "port": 10110
  },
  "uart": {
    "sentences_received": 1542,
    "errors": 3,
    "baud": 38400
  },
  "nmea_buffer": {
    "queue_size": 40,
    "overflow_total": 0,
    "full_events_recent": 0,
    "has_overflow": false,
    "queue_waiting": 2,
    "queue_load_pct": 5
  },
  "ble": {
    "enabled": true,
    "advertising": true,
    "connected_devices": 0,
    "device_name": "MarineGateway"
  },
  "sd": {
    "enabled": true,
    "mounted": true,
    "card_type": "SDHC",
    "total_mb": 15360,
    "free_mb": 14000,
    "used_pct": 9
  },
  "chip_temp": {
    "celsius": 42.3,
    "available": true
  }
}
```

| Field | Type | Description |
|---|---|---|
| `uptime` | int | Uptime in seconds since last boot |
| `datetime` | int | Unix timestamp from GPS (0 if no fix) |
| `heap.free` | int | Available heap memory in bytes |
| `heap.total` | int | Total heap memory in bytes |
| `heap.min_free` | int | Historical minimum free heap in bytes |
| `wifi.mode` | string | `"STA"`, `"AP"`, `"Connecting"`, `"Disconnected"`, `"Reconnecting"` |
| `wifi.ssid` | string | Connected or broadcasted SSID |
| `wifi.rssi` | int | Signal level in dBm (STA mode only) |
| `wifi.ip` | string | Local IP address |
| `wifi.clients` | int | Number of connected clients (AP mode only) |
| `tcp.clients` | int | Number of connected TCP NMEA clients |
| `tcp.port` | int | NMEA TCP server port |
| `uart.sentences_received` | int | Total NMEA sentences received since boot |
| `uart.errors` | int | Number of invalid NMEA sentences |
| `uart.baud` | int | Current baud rate |
| `nmea_buffer.queue_size` | int | Total NMEA queue capacity |
| `nmea_buffer.overflow_total` | int | Cumulative overflow events since boot |
| `nmea_buffer.full_events_recent` | int | Recent queue-full events |
| `nmea_buffer.has_overflow` | bool | True if any recent overflow occurred |
| `nmea_buffer.queue_waiting` | int | Messages currently waiting in queue |
| `nmea_buffer.queue_load_pct` | int | Queue fill percentage (0–100) |
| `ble.enabled` | bool | Whether BLE is enabled |
| `ble.advertising` | bool | Whether BLE is advertising |
| `ble.connected_devices` | int | Number of connected BLE devices |
| `ble.device_name` | string | BLE device name |
| `sd.enabled` | bool | Whether SD manager is present |
| `sd.mounted` | bool | Whether the SD card is currently mounted |
| `sd.card_type` | string | SD card type (e.g. `"SDHC"`) |
| `sd.total_mb` | int | Total SD capacity in MB |
| `sd.free_mb` | int | Free space on SD card in MB |
| `sd.used_pct` | int | Used space percentage |
| `chip_temp.celsius` | float\|null | ESP32-S3 internal die temperature in °C (±5 °C accuracy) |
| `chip_temp.available` | bool | False if sensor read failed |

---

## 5. Restart

### `POST /api/restart`

Triggers a software restart of the ESP32.

**Body:** none.

**Response:**
```json
{ "success": true, "message": "Restarting..." }
```

The device restarts within seconds after the response is sent.

---

## 6. WiFi Scan

### `POST /api/wifi/scan`

Starts an asynchronous scan for nearby WiFi networks.

**Body:** none.

**Responses:**
```json
{ "success": true, "message": "WiFi scan started" }
```
```json
{ "success": true, "message": "WiFi scan completed" }
```
```json
{ "success": false, "error": "Failed to start scan" }
```

---

### `GET /api/wifi/scan`

Retrieves the results of the last WiFi scan.

**Response (scan in progress) — HTTP 202:**
```json
{ "scanning": true, "networks": [] }
```

**Response (scan complete) — HTTP 200:**
```json
{
  "scanning": false,
  "networks": [
    {
      "ssid": "MyNetwork",
      "rssi": -55,
      "channel": 6,
      "encryption": 3
    }
  ]
}
```

| Field | Type | Description |
|---|---|---|
| `ssid` | string | Network name |
| `rssi` | int | Signal level in dBm |
| `channel` | int | WiFi channel (1–13) |
| `encryption` | int | `0`=Open, `1`=WEP, `2`=WPA, `3`=WPA2, `4`=WPA/WPA2, `5`=WPA2-Enterprise, `6`=WPA3 |

---

## 7. Boat Data — Navigation

### `GET /api/boat/navigation`

Returns navigation data: GPS position, speeds, heading, and depth.  
Each measurement includes its value, unit, and age in seconds (`null` if data is absent or stale > 10 s).

**Response:**
```json
{
  "position": {
    "latitude": 47.2345,
    "longitude": -2.1234
  },
  "sog": { "value": 5.2, "unit": "kn", "age": 0.8 },
  "cog": { "value": 135.0, "unit": "deg", "age": 0.8 },
  "stw": { "value": 4.9, "unit": "kn", "age": 1.2 },
  "heading": { "value": 138.0, "unit": "deg", "age": 0.5 },
  "depth": { "value": 12.5, "unit": "m", "age": 1.0 },
  "trip": { "value": 23.4, "unit": "nm" },
  "total": { "value": 1245.6, "unit": "nm" },
  "gps_quality": {
    "satellites": 8,
    "fix_quality": 1,
    "hdop": 1.2
  }
}
```

| Field | Unit | NMEA Source | Description |
|---|---|---|---|
| `position.latitude` | decimal degrees | GGA, RMC | Latitude (negative = South) |
| `position.longitude` | decimal degrees | GGA, RMC | Longitude (negative = West) |
| `sog` | kn | RMC, VTG | Speed Over Ground |
| `cog` | deg | RMC, VTG | Course Over Ground (0–360°) |
| `stw` | kn | VHW | Speed Through Water |
| `heading` | deg | HDG, HDM | Magnetic heading (0–360°) |
| `depth` | m | DPT, DBT | Depth below transducer |
| `trip` | nm | VLW | Trip distance counter |
| `total` | nm | VLW | Total cumulative distance |
| `gps_quality.satellites` | — | GGA | Number of satellites in use |
| `gps_quality.fix_quality` | — | GGA | GPS fix quality indicator |
| `gps_quality.hdop` | — | GGA | Horizontal Dilution of Precision |

---

## 8. Boat Data — Wind

### `GET /api/boat/wind`

Returns apparent and true wind data.

**Response:**
```json
{
  "aws": { "value": 12.3, "unit": "kn", "age": 0.6 },
  "awa": { "value": 45.0, "unit": "deg", "age": 0.6 },
  "tws": { "value": 10.1, "unit": "kn", "age": 0.6 },
  "twa": { "value": 52.0, "unit": "deg", "age": 0.6 },
  "twd": { "value": 187.0, "unit": "deg", "age": 0.6 }
}
```

| Field | Unit | NMEA Source | Description |
|---|---|---|---|
| `aws` | kn | MWV | Apparent Wind Speed |
| `awa` | deg | MWV | Apparent Wind Angle (−180° to +180°, positive = starboard) |
| `tws` | kn | MWV / calculated | True Wind Speed |
| `twa` | deg | MWV / calculated | True Wind Angle |
| `twd` | deg | MWD / calculated | True Wind Direction (0–360°, North reference) |

When a value is absent or stale, the `value` field is `null` and `age` is `null`.

---

## 9. Boat Data — AIS

### `GET /api/boat/ais`

Returns the list of active AIS targets (age < 60 seconds).

**Response:**
```json
{
  "target_count": 2,
  "targets": [
    {
      "mmsi": 227123456,
      "name": "VESSEL_A",
      "position": {
        "latitude": 47.250,
        "longitude": -2.110
      },
      "cog": 220.0,
      "sog": 8.5,
      "heading": 218.0,
      "proximity": {
        "distance": 1.23,
        "distance_unit": "nm",
        "bearing": 45.0,
        "bearing_unit": "deg",
        "cpa": 0.45,
        "cpa_unit": "nm",
        "tcpa": 12.3,
        "tcpa_unit": "min"
      },
      "age": 5
    }
  ]
}
```

| Field | Type | Description |
|---|---|---|
| `target_count` | int | Total number of active targets |
| `mmsi` | int | Target MMSI identifier |
| `name` | string | Vessel name (from AIS type 24) |
| `position.latitude` | float | Latitude in decimal degrees |
| `position.longitude` | float | Longitude in decimal degrees |
| `cog` | float | Course Over Ground in degrees |
| `sog` | float | Speed Over Ground in knots |
| `heading` | float | True heading in degrees |
| `proximity.distance` | float | Distance to our vessel in nm |
| `proximity.bearing` | float | Bearing to target in degrees |
| `proximity.cpa` | float | Closest Point of Approach in nm |
| `proximity.tcpa` | float | Time to CPA in minutes |
| `age` | int | Age of the last update in seconds |

---

## 10. Boat Data — Full State

### `GET /api/boat/state`

Returns the complete boat state in a single call (full serialization of `BoatState`). Includes all navigation, wind, AIS, environment, autopilot, and calculated data.

**Recommended use:** for initialization or debugging. For regular polling, prefer the specialized endpoints above.

**Response:** a full JSON object combining all structures described in the previous endpoints, plus:

- `environment`: water temperature, air temperature, barometric pressure
- `calculated`: wind VMG, waypoint VMG, current set and drift
- `autopilot`: mode, status, heading target, wind angle target, rudder angle, XTE

---

## 11. NMEA WebSocket

### `WS /ws/nmea`

Real-time stream of raw NMEA sentences received on the serial port.

- **Protocol:** text WebSocket
- **Format:** one NMEA sentence per message, e.g. `$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A`
- **Direction:** read-only (server → client)
- **Trigger:** fires on every valid NMEA sentence received

---

## Common HTTP Status Codes

| HTTP Code | Meaning |
|---|---|
| 200 | Success |
| 202 | Request accepted, result not yet available (scan in progress) |
| 400 | Invalid input (malformed JSON or constraint violation) |
| 404 | Unknown endpoint |
| 500 | Internal error (component not initialized) |

---

## 12. Performance Configuration

### `GET /api/performance/config`

Returns the current EMA damping time constant used when computing polar performance metrics (VMG and polar %).

**Response:**
```json
{
  "damping_tau": 6.0
}
```

| Field | Type | Description |
|---|---|---|
| `damping_tau` | float | EMA time constant in seconds. `0` = damping disabled. |

---

### `POST /api/performance/config`

Sets and persists the EMA damping time constant. The value is stored in NVS and survives reboots. Takes effect immediately without a restart.

**Request body:**
```json
{
  "damping_tau": 6.0
}
```

| Field | Type | Required | Constraints | Description |
|---|---|---|---|---|
| `damping_tau` | float | Yes | 0–60 s | EMA time constant in seconds. `0` disables smoothing. Recommended: 3–15 s. |

**Success response:**
```json
{
  "success": true,
  "damping_tau": 6.0
}
```

**Error response (missing or invalid field):**
```json
{ "error": "damping_tau required (number)" }
```

#### How damping works

When `damping_tau > 0`, the inputs to the polar calculation (STW, TWS, TWA) are smoothed using an **exponential moving average (EMA)** before computing the VMG and polar percentage:

```
α = 1 − exp(−Δt / τ)
y_t = y_(t-1) + α × (x_t − y_(t-1))
```

- **τ** (`damping_tau`) is the time constant in seconds.
- **Δt** is the real elapsed time between consecutive performance updates.
- Wind angles (TWA) are smoothed in `(sin, cos)` space to avoid wrap-around discontinuities near 0°/360°.
- The EMA state is reset whenever `damping_tau` is changed.

| Profile | Suggested τ |
|---|---|
| Racing | 3–5 s |
| Cruising | 6–10 s |
| Heavy sea | 10–15 s |

---

## 13. Alarms

The device implements three onboard alarms, continuously evaluated by the firmware's `AlarmManager` (checked every second):

| Alarm | Trigger condition | Config fields |
|---|---|---|
| **Depth** | `depth.below_transducer <= depth_threshold_m` | `depth_enabled`, `depth_threshold_m` |
| **AIS proximity** | Any AIS target (MMSI ≠ `own_mmsi`) within `ais_distance_nm` | `ais_enabled`, `ais_distance_nm`, `own_mmsi` |
| **GPS lost** | GPS position stale for more than `gps_lost_timeout_s` | `gps_lost_enabled`, `gps_lost_timeout_s` |

Each alarm has an independent `active` / `acknowledged` runtime state. When any alarm transitions to active and unacknowledged, the firmware automatically sends the SeaTalk1 `beep_on` datagram (same as `/api/seatalk/extra`); the beep is stopped (`beep_off`) as soon as all active alarms are acknowledged, or automatically if the condition clears. Acknowledging silences the beep but does not disable future re-triggers if the alarm clears and re-activates later.

### `GET /api/alarms/config`

Returns the current alarm configuration.

**Response:**
```json
{
  "alarms_enabled": true,
  "depth_enabled": true,
  "depth_threshold_m": 2.0,
  "ais_enabled": true,
  "ais_distance_nm": 1.0,
  "own_mmsi": 227123456,
  "gps_lost_enabled": true,
  "gps_lost_timeout_s": 10
}
```

| Field | Type | Description |
|---|---|---|
| `alarms_enabled` | bool | Master switch for all alarm evaluation |
| `depth_enabled` | bool | Enable/disable the depth alarm |
| `depth_threshold_m` | float | Depth threshold in meters (default `2.0`) |
| `ais_enabled` | bool | Enable/disable the AIS proximity alarm |
| `ais_distance_nm` | float | Proximity distance in nautical miles, 0.1 nm steps (default `1.0`) |
| `own_mmsi` | int | MMSI of your own vessel — excluded from AIS proximity detection |
| `gps_lost_enabled` | bool | Enable/disable the GPS lost alarm |
| `gps_lost_timeout_s` | int | Seconds without a valid GPS fix before triggering (default `10`) |

---

### `POST /api/alarms/config`

Saves the alarm configuration to NVS and applies it immediately (no restart required). All fields are optional — only the given fields are updated.

**Request body:**
```json
{
  "alarms_enabled": true,
  "depth_enabled": true,
  "depth_threshold_m": 2.5,
  "ais_enabled": true,
  "ais_distance_nm": 0.5,
  "own_mmsi": 227123456,
  "gps_lost_enabled": true,
  "gps_lost_timeout_s": 15
}
```

| Field | Type | Constraints |
|---|---|---|
| `depth_threshold_m` | float | Clamped to 0.1–999.0 m |
| `ais_distance_nm` | float | Snapped to nearest 0.1 nm, clamped to 0.1–99.9 nm |
| `own_mmsi` | int | Any unsigned integer, `0` = not configured |
| `gps_lost_timeout_s` | int | Clamped to 1–3600 s |

**Success response:**
```json
{ "success": true, "message": "Alarm config saved" }
```

**Error response (invalid JSON):**
```json
{ "error": "Invalid JSON" }
```

---

### `GET /api/alarms/status`

Returns the current runtime state of all three alarms.

**Response:**
```json
{
  "depth":    { "active": false, "acknowledged": false, "age": null },
  "ais":      { "active": true,  "acknowledged": false, "age": 4.2 },
  "gps_lost": { "active": false, "acknowledged": false, "age": null },
  "ais_trigger_mmsi": 227654321,
  "any_active": true,
  "any_unacked": true
}
```

| Field | Type | Description |
|---|---|---|
| `depth.active` / `ais.active` / `gps_lost.active` | bool | Whether the alarm condition currently holds |
| `*.acknowledged` | bool | Whether the operator has acknowledged the active alarm |
| `*.age` | float\|null | Seconds since the alarm last changed state; `null` if not active |
| `ais_trigger_mmsi` | int | MMSI of the closest AIS target currently triggering the proximity alarm (`0` if none) |
| `any_active` | bool | `true` if at least one alarm is active |
| `any_unacked` | bool | `true` if at least one active alarm has not yet been acknowledged (drives the beep) |

---

### `POST /api/alarms/ack`

Globally acknowledges all currently active alarms and stops the beep (sends `beep_off`).

**Body:** none.

**Response:**
```json
{ "success": true, "message": "Alarms acknowledged" }
```

---

### `POST /api/alarms/beep_on`

Manually triggers the SeaTalk1 beep, independent of alarm state (equivalent to `/api/seatalk/extra` with `command: "beep_on"`).

**Response:**
```json
{ "success": true, "message": "Beep on" }
```
```json
{ "success": false, "error": "Transmission failed" }
```

---

### `POST /api/alarms/beep_off`

Manually silences the SeaTalk1 beep, independent of alarm state (equivalent to `/api/seatalk/extra` with `command: "beep_off"`).

**Response:**
```json
{ "success": true, "message": "Beep off" }
```
```json
{ "success": false, "error": "Transmission failed" }
```
