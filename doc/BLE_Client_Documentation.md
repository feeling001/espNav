# Marine Gateway — BLE Client Developer Documentation

> This document describes the complete BLE protocol exposed by the Marine Gateway (ESP32).  
> It contains all the information needed to develop a BLE client application
> (iOS, Android, Flutter, React Native, etc.) that consumes the transmitted marine data
> and sends administration commands.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Connection and Security](#2-connection-and-security)
3. [Services and Characteristics](#3-services-and-characteristics)
4. [Navigation Service](#4-navigation-service)
5. [Wind Service](#5-wind-service)
6. [Autopilot Service](#6-autopilot-service)
7. [Sail Performance Service](#7-sail-performance-service)
8. [Admin Service](#8-admin-service)
9. [Alarm Service](#9-alarm-service)
10. [AIS Service](#10-ais-service)
11. [Sending Autopilot Commands](#11-sending-autopilot-commands)
12. [Sending Admin Commands](#12-sending-admin-commands)
13. [Sending Alarm Commands](#13-sending-alarm-commands)
14. [Behavior and Timing](#14-behavior-and-timing)
15. [Integration Flow Example](#15-integration-flow-example)
16. [UUID Reference Table](#16-uuid-reference-table)

---

## 1. Overview

The Marine Gateway is an ESP32-based bridge that reads NMEA 0183 data from a serial port and exposes it over Bluetooth Low Energy (BLE). It implements a custom GATT profile organized into **7 services**:

| Service | Purpose |
|---|---|
| Navigation | GPS position, speed, heading, depth |
| Wind | Apparent wind and true wind |
| Autopilot | Autopilot state + command input |
| Sail Performance | VMG, polar efficiency, polar target speed |
| Admin | System status (uptime, datetime) + administration commands (restart, WiFi config) |
| Alarm | Depth / AIS proximity / GPS lost alarm configuration, runtime state, acknowledge and beep commands |
| **AIS** | **Nearby AIS targets (position, course, speed, CPA/TCPA), same data as the `/api/boat/ais` REST endpoint** |

Data is encoded as **UTF-8 JSON** in each characteristic and updated at **1 Hz** (every second).

---

## 2. Connection and Security

### Discovery

- **Advertised BLE name:** `MarineGateway` (configurable via the API)
- The device advertises continuously and accepts up to `BLE_MAX_CONNECTIONS = 3` simultaneous connections.
- After a client disconnects, advertising restarts automatically.
- The scan response includes both the Navigation and Sail Performance service UUIDs, allowing client apps to filter on either.

### Security (Pairing)

The Marine Gateway uses **Secure Connections with MITM** protection and **bonding**.

| Parameter | Value |
|---|---|
| Authentication mode | `ESP_LE_AUTH_REQ_SC_MITM_BOND` |
| IO Capability | `ESP_IO_CAP_OUT` — the ESP32 **displays** a PIN |
| Bonding | Enabled — keys are saved for automatic reconnection |

### Pairing Procedure

1. The client application initiates the BLE connection.
2. The device displays a **6-digit PIN code** (default: `123456`, configurable via dashboard).
3. The user enters this PIN in the client application.
4. Once paired, the bond is saved: subsequent reconnections are automatic.

> **Important:** The client application must implement numeric passkey entry. The client-side IO capability should be `KeyboardOnly` or `KeyboardDisplay`.

---

## 3. Services and Characteristics

### UUID Scheme

All UUIDs are 128-bit. The custom base is `4D475743-xxxx-4E41-5649-474154494F4E`  
(MGWC = Marine Gateway Custom).

#### Navigation Service

| Element | UUID |
|---|---|
| **Service** | `4d475743-0001-4e41-5649-474154494f4e` |
| **NavData Characteristic** | `4d475743-0101-4e41-5649-474154494f4e` |

#### Wind Service

| Element | UUID |
|---|---|
| **Service** | `4d475743-0002-4e41-5649-474154494f4e` |
| **WindData Characteristic** | `4d475743-0201-4e41-5649-474154494f4e` |

#### Autopilot Service

| Element | UUID |
|---|---|
| **Service** | `4d475743-0003-4e41-5649-474154494f4e` |
| **AutopilotData Characteristic** | `4d475743-0301-4e41-5649-474154494f4e` |
| **AutopilotCmd Characteristic** | `4d475743-0302-4e41-5649-474154494f4e` |

#### Sail Performance Service

| Element | UUID |
|---|---|
| **Service** | `4d475743-0004-4e41-5649-474154494f4e` |
| **PerformanceData Characteristic** | `4d475743-0401-4e41-5649-474154494f4e` |

#### Admin Service

| Element | UUID |
|---|---|
| **Service** | `4d475743-0005-4e41-5649-474154494f4e` |
| **AdminData Characteristic** | `4d475743-0501-4e41-5649-474154494f4e` |
| **AdminCmd Characteristic** | `4d475743-0502-4e41-5649-474154494f4e` |

#### Alarm Service

| Element | UUID |
|---|---|
| **Service** | `4d475743-0006-4e41-5649-474154494f4e` |
| **AlarmData Characteristic** | `4d475743-0601-4e41-5649-474154494f4e` |
| **AlarmCmd Characteristic** | `4d475743-0602-4e41-5649-474154494f4e` |

#### AIS Service

| Element | UUID |
|---|---|
| **Service** | `4d475743-0007-4e41-5649-474154494f4e` |
| **AisData Characteristic** | `4d475743-0701-4e41-5649-474154494f4e` |

### Characteristic Properties

| Characteristic | READ | NOTIFY | WRITE |
|---|:---:|:---:|:---:|
| NavData | ✅ | ✅ | ❌ |
| WindData | ✅ | ✅ | ❌ |
| AutopilotData | ✅ | ✅ | ❌ |
| AutopilotCmd | ❌ | ❌ | ✅ |
| PerformanceData | ✅ | ✅ | ❌ |
| AdminData | ✅ | ✅ | ❌ |
| AdminCmd | ❌ | ❌ | ✅ |
| AlarmData | ✅ | ✅ | ❌ |
| AlarmCmd | ❌ | ❌ | ✅ |
| **AisData** | **✅** | **✅** | **❌** |

> All `NOTIFY` characteristics include a **CCCD** (UUID `0x2902`).  
> The client **must enable notifications** on each desired characteristic to receive updates.

---

## 4. Navigation Service

**Service UUID:** `4d475743-0001-4e41-5649-474154494f4e`  
**Characteristic UUID:** `4d475743-0101-4e41-5649-474154494f4e`

### Data Format

```json
{
  "lat": 47.2345,
  "lon": -2.1234,
  "sog": 5.2,
  "cog": 135.0,
  "stw": 4.9,
  "hdg_mag": 138.0,
  "hdg_true": null,
  "depth": 12.5
}
```

| Field | Type | Unit | Description |
|---|---|---|---|
| `lat` | float \| null | decimal degrees | GPS latitude. Negative = South. |
| `lon` | float \| null | decimal degrees | GPS longitude. Negative = West. |
| `sog` | float \| null | kn | Speed Over Ground |
| `cog` | float \| null | degrees (0–360°) | Course Over Ground |
| `stw` | float \| null | kn | Speed Through Water |
| `hdg_mag` | float \| null | degrees (0–360°) | Magnetic heading |
| `hdg_true` | float \| null | degrees (0–360°) | True heading (reserved, currently null) |
| `depth` | float \| null | m | Depth below transducer |

A field is `null` if data is absent or stale (no NMEA update for > 10 s).

---

## 5. Wind Service

**Service UUID:** `4d475743-0002-4e41-5649-474154494f4e`  
**Characteristic UUID:** `4d475743-0201-4e41-5649-474154494f4e`

### Data Format

```json
{
  "aws": 12.3,
  "awa": 45.0,
  "tws": 10.1,
  "twa": 52.0,
  "twd": 187.0
}
```

| Field | Type | Unit | Description |
|---|---|---|---|
| `aws` | float \| null | kn | Apparent Wind Speed |
| `awa` | float \| null | degrees | Apparent Wind Angle. Positive = starboard, negative = port. Range: −180° to +180°. |
| `tws` | float \| null | kn | True Wind Speed |
| `twa` | float \| null | degrees | True Wind Angle. Same sign convention as AWA. |
| `twd` | float \| null | degrees (0–360°) | True Wind Direction, geographic North reference |

---

## 6. Autopilot Service

**Service UUID:** `4d475743-0003-4e41-5649-474154494f4e`  
**Data Characteristic UUID:** `4d475743-0301-4e41-5649-474154494f4e`

### Data Format

```json
{
  "mode": "auto",
  "status": "engaged",
  "heading_target": 135.0,
  "wind_target": null,
  "rudder": -5.2,
  "locked_heading": 135.0
}
```

| Field | Type | Description |
|---|---|---|
| `mode` | string \| null | `"standby"`, `"auto"`, `"wind"`, `"track"`, `"manual"` |
| `status` | string \| null | `"engaged"`, `"standby"`, `"alarm"` |
| `heading_target` | float \| null | Target heading in degrees (`auto` mode) |
| `wind_target` | float \| null | Target wind angle in degrees (`wind` mode) |
| `rudder` | float \| null | Rudder angle. Positive = starboard, negative = port. |
| `locked_heading` | float \| null | Locked heading in degrees |

> Autopilot support depends on SeaTalk1 integration. All fields are `null` if no data is received.

---

## 7. Sail Performance Service

**Service UUID:** `4d475743-0004-4e41-5649-474154494f4e`  
**Characteristic UUID:** `4d475743-0401-4e41-5649-474154494f4e`

This service exposes real-time sailing performance metrics derived from the boat's polar diagram. The polar file must be uploaded to the device via the web dashboard (Performance page) for `polar_pct` and `target_stw` to be valid.

### Data Format

```json
{
  "vmg": 4.2,
  "polar_pct": 85.3,
  "target_stw": 7.1,
  "polar_loaded": true
}
```

| Field | Type | Unit | Description |
|---|---|---|---|
| `vmg` | float \| null | kn | Velocity Made Good toward/away from the wind. **Positive = upwind**, **negative = downwind**. Computed as `STW × cos(TWA)`. Requires valid STW and TWA. |
| `polar_pct` | float \| null | % | Current STW expressed as a percentage of the polar target STW. `100 %` = exactly on polar. `> 100 %` = faster than polar. `null` if no polar is loaded or TWS/TWA are stale. |
| `target_stw` | float \| null | kn | The polar target boat speed for the current TWS and TWA. Interpolated bilinearly from the polar table. `null` if no polar is loaded or wind data is stale. |
| `polar_loaded` | bool | — | `true` when a polar file is loaded on the device. |

### Polar Upload

A polar file is uploaded via the web dashboard at `http://<device_ip>/` → **Performance** page.  
Accepted format: tab-delimited `.pol` / `.csv` file.

---

## 8. Admin Service

**Service UUID:** `4d475743-0005-4e41-5649-474154494f4e`

The Admin service gives BLE clients access to system diagnostics and remote administration capabilities (restart, WiFi reconfiguration). It is split into two characteristics: a read-only status stream and a write-only command sink.

### 8.1 AdminData Characteristic (READ + NOTIFY)

**UUID:** `4d475743-0501-4e41-5649-474154494f4e`

Updated at 1 Hz alongside all other service characteristics.

#### Data Format

```json
{
  "uptime_s": 3600,
  "datetime_utc": 1718445600,
  "wifi_mode": "sta",
  "wifi_ssid": "MyNetwork",
  "ip": "192.168.1.100",
  "free_heap": 182344
}
```

| Field | Type | Description |
|---|---|---|
| `uptime_s` | uint32 | Seconds elapsed since last boot. |
| `datetime_utc` | uint64 \| null | Current UTC time as a Unix timestamp (seconds since epoch), sourced from the GPS fix. `null` if no GPS fix has been received yet. |
| `wifi_mode` | `"sta"` \| `"ap"` \| null | Current WiFi operating mode. `null` if ConfigManager is unavailable. |
| `wifi_ssid` | string \| null | SSID of the connected network (STA mode) or the broadcasted access point (AP mode). `null` if not configured. |
| `ip` | string \| null | Current IP address of the device. In STA mode this is the address assigned by the router (e.g. `"192.168.1.100"`). In AP mode this is always `"192.168.4.1"`. `null` if WiFi is not yet connected/started. |
| `free_heap` | uint32 | Free heap memory in bytes. Useful for remote diagnostics. |

> `datetime_utc` is derived from GPS NMEA sentences (GGA, RMC, ZDA). It is only populated once the GPS receiver has acquired a fix and the first valid sentence with a time field has been parsed.

---

### 8.2 AdminCmd Characteristic (WRITE)

**UUID:** `4d475743-0502-4e41-5649-474154494f4e`  
**Property:** WRITE (no response expected)

All commands are sent as UTF-8 JSON objects. The `"command"` field is mandatory. Unknown commands are silently ignored (logged on the serial console).

#### Command: `restart`

Triggers a soft reboot of the ESP32 approximately 2 seconds after the command is received. The delay allows the BLE stack to complete the write transaction before the system goes down.

```json
{ "command": "restart" }
```

The device will disconnect all BLE clients and restart. Bonded clients will reconnect automatically once advertising resumes after reboot.

---

#### Command: `wifi_sta`

Configures the device to connect to an existing WiFi network (infrastructure / station mode) and saves the configuration to NVS. The device reboots approximately 3 seconds after the command to apply the new network settings.

```json
{
  "command": "wifi_sta",
  "ssid": "MyNetwork",
  "password": "mysecretpassword"
}
```

| Field | Type | Required | Constraints |
|---|---|---|---|
| `ssid` | string | **Yes** | 1–31 characters |
| `password` | string | No | Empty string for open networks |

> After reboot the device will attempt to connect to the specified network. If the connection fails within 30 seconds, it automatically falls back to Access Point mode.

---

#### Command: `wifi_ap`

Configures the device to operate as a WiFi Access Point and saves the configuration to NVS. The device reboots approximately 3 seconds after the command to apply the new settings.

```json
{
  "command": "wifi_ap",
  "ssid": "MyGateway",
  "password": "myappassword"
}
```

| Field | Type | Required | Constraints |
|---|---|---|---|
| `ssid` | string | **Yes** | 1–31 characters |
| `password` | string | No | Minimum 8 characters for WPA2. Empty string or < 8 characters will use the device default password (`marine123`). |

> In AP mode the device IP address is always `192.168.4.1`. Connect to this address in a browser to reach the web dashboard.

---

#### Error Handling

There is no write-response mechanism (the characteristic uses WRITE without response). The client should monitor the `AdminData` notification after issuing a `wifi_sta` or `wifi_ap` command:

- If the command was accepted, `wifi_mode` and `wifi_ssid` in the next `AdminData` notification will reflect the new values **before** the reboot occurs.
- The `uptime_s` field resetting to a low value (< 30) on reconnection confirms the device rebooted successfully.

---

## 10. AIS Service

**Service UUID:** `4d475743-0007-4e41-5649-474154494f4e`  
**Characteristic UUID:** `4d475743-0701-4e41-5649-474154494f4e`

This service exposes the same AIS target data as the REST `/api/boat/ais` endpoint: nearby vessels received via the AIS receiver, with position, course, speed, and collision-avoidance metrics (CPA/TCPA).

### Data Format

```json
{
  "target_count": 2,
  "targets": [
    {
      "mmsi": 227003660,
      "name": "SV EXAMPLE",
      "lat": 47.2456,
      "lon": -2.1122,
      "cog": 92.0,
      "sog": 6.5,
      "heading": 90.0,
      "distance": 1.8,
      "bearing": 112.0,
      "cpa": 0.3,
      "tcpa": 12.5,
      "age": 4
    }
  ]
}
```

| Field | Type | Unit | Description |
|---|---|---|---|
| `target_count` | int | — | Number of targets included in this notification. |
| `targets` | array | — | List of AIS targets, **closest first**. |
| `targets[].mmsi` | uint32 | — | Maritime Mobile Service Identity of the target vessel. |
| `targets[].name` | string | — | Vessel name (may be empty if not yet received). |
| `targets[].lat` | float | decimal degrees | Target latitude. |
| `targets[].lon` | float | decimal degrees | Target longitude. |
| `targets[].cog` | float | degrees (0–360°) | Course Over Ground. |
| `targets[].sog` | float | kn | Speed Over Ground. |
| `targets[].heading` | float | degrees (0–360°) | True heading. |
| `targets[].distance` | float | nm | Distance from own ship to the target. |
| `targets[].bearing` | float | degrees (0–360°) | Bearing from own ship to the target. |
| `targets[].cpa` | float | nm | Closest Point of Approach. |
| `targets[].tcpa` | float | min | Time to Closest Point of Approach. |
| `targets[].age` | uint32 | s | Time elapsed since the last AIS message received for this target. |

> **Note on target limit:** to stay within the BLE attribute/MTU size limit, only the `BLE_AIS_MAX_TARGETS` (currently **6**) closest non-stale targets are sent per notification, sorted by ascending distance. This may be fewer than the total number of targets available via the REST API. A target is excluded once its AIS data is older than the **AIS data timeout (60 s)**.

> This characteristic is **read-only** — there is no AIS command characteristic. Alarm thresholds related to AIS proximity (`ais_enabled`, `ais_distance_nm`, `own_mmsi`) are configured via the [Alarm Service](#9-alarm-service) `set_config` command.

---

## 11. Sending Autopilot Commands

**Command Characteristic UUID:** `4d475743-0302-4e41-5649-474154494f4e`  
**Property:** WRITE (no response expected)

### Command Format

```json
{ "command": "adjust+1" }
```

### Command Table

| `command` value | Action |
|---|---|
| `"auto"` | Engage autopilot (auto heading mode) |
| `"standby"` | Disengage autopilot (standby mode) |
| `"wind"` | Switch to wind mode (maintain wind angle) |
| `"track"` | Switch to track mode (follow GPS route) |
| `"adjust+10"` | Adjust target heading by +10° (starboard) |
| `"adjust-10"` | Adjust target heading by −10° (port) |
| `"adjust+1"` | Adjust target heading by +1° (starboard) |
| `"adjust-1"` | Adjust target heading by −1° (port) |
| `"tack-port"` | Execute a port tack |
| `"tack-starboard"` | Execute a starboard tack |

---

## 12. Sending Admin Commands

**Command Characteristic UUID:** `4d475743-0502-4e41-5649-474154494f4e`  
**Property:** WRITE (no response expected)

### Command Summary

| `command` value | Additional fields | Effect | Reboot? |
|---|---|---|---|
| `"restart"` | — | Soft reboot after 2 s | ✅ Yes |
| `"wifi_sta"` | `ssid`, `password` | Switch to infrastructure mode, save config | ✅ Yes (3 s) |
| `"wifi_ap"` | `ssid`, `password` | Switch to access point mode, save config | ✅ Yes (3 s) |

### Examples

```json
{ "command": "restart" }

{ "command": "wifi_sta", "ssid": "HomeNetwork", "password": "s3cr3t!" }

{ "command": "wifi_ap", "ssid": "MarineGW", "password": "marine456" }
```

---

## 13. Sending Alarm Commands

**Command Characteristic UUID:** `4d475743-0602-4e41-5649-474154494f4e`  
**Property:** WRITE (no response expected)

### Command Summary

| `command` value | Additional fields | Effect |
|---|---|---|
| `"set_config"` | `depth_enabled`, `depth_threshold_m`, `ais_enabled`, `ais_distance_nm`, `own_mmsi`, `gps_lost_enabled`, `gps_lost_timeout_s`, `alarms_enabled` | Update alarm configuration (persisted to NVS) |
| `"ack"` | — | Acknowledge all active alarms, stop beep |
| `"beep_on"` | — | Manually trigger the SeaTalk1 beep |
| `"beep_off"` | — | Manually silence the SeaTalk1 beep |

### Examples

```json
{ "command": "set_config", "depth_threshold_m": 2.5, "ais_distance_nm": 0.5 }

{ "command": "ack" }

{ "command": "beep_on" }

{ "command": "beep_off" }
```

---

## 14. Behavior and Timing

| Parameter | Value |
|---|---|
| Update frequency | **1 Hz** (every 1000 ms) — all services including Admin and AIS |
| Max simultaneous connections | **3** devices |
| NMEA data timeout | **10 seconds** (navigation, wind, performance, autopilot) |
| AIS data timeout | **60 seconds** |
| Max AIS targets per notification | **6** (closest first) |
| Reconnection | Automatic (advertising restarts after disconnection) |
| Reboot delay after `restart` command | **2 seconds** |
| Reboot delay after `wifi_sta` / `wifi_ap` command | **3 seconds** |

### Update Cycle

Every second, the device:
1. Reads the current `BoatState` and system status (FreeRTOS mutex protected).
2. Serializes all 7 characteristics to JSON.
3. Sends a BLE notification on each characteristic to all subscribed clients.

---

## 15. Integration Flow Example

```
1. SCAN
   ├── Search for peripheral named "MarineGateway"
   └── Or filter on service UUID:
       Navigation:   4d475743-0001-4e41-5649-474154494f4e
       Performance:  4d475743-0004-4e41-5649-474154494f4e

2. CONNECT
   └── Initiate BLE connection

3. PAIRING (first time only)
   ├── Device triggers passkey notification
   ├── Show numeric input field to user
   └── User enters 6-digit PIN displayed on Marine Gateway dashboard

4. SERVICE DISCOVERY
   └── Discover all services and characteristics

5. SUBSCRIBE TO NOTIFICATIONS
   ├── Write 0x0100 to NavData CCCD         (UUID 0x2902)
   ├── Write 0x0100 to WindData CCCD
   ├── Write 0x0100 to AutopilotData CCCD
   ├── Write 0x0100 to PerformanceData CCCD
   ├── Write 0x0100 to AdminData CCCD
   ├── Write 0x0100 to AlarmData CCCD
   └── Write 0x0100 to AisData CCCD

6. RECEIVE DATA (every ~1 s)
   ├── NavData         → position, speed, heading, depth
   ├── WindData        → apparent / true wind
   ├── AutopilotData   → autopilot state
   ├── PerformanceData → vmg, polar_pct, target_stw, polar_loaded
   ├── AdminData       → uptime_s, datetime_utc, wifi_mode, wifi_ssid, free_heap
   ├── AlarmData       → alarm config + depth/ais/gps_lost active/acknowledged state
   └── AisData         → target_count + list of nearby AIS targets (closest first)

7. HANDLE POLAR STATE
   ├── if polar_loaded == false → show "No polar loaded" in UI
   └── if polar_loaded == true  → display polar_pct and target_stw

8. SEND AUTOPILOT COMMANDS (optional)
   └── Write to AutopilotCmd → {"command": "adjust+1"}

9. SEND ADMIN COMMANDS (optional)
   ├── Restart device  → Write to AdminCmd → {"command": "restart"}
   ├── Switch to STA   → Write to AdminCmd → {"command": "wifi_sta", "ssid": "Net", "password": "pw"}
   └── Switch to AP    → Write to AdminCmd → {"command": "wifi_ap",  "ssid": "AP",  "password": "pw"}

10. SEND ALARM COMMANDS (optional)
    ├── Update config    → Write to AlarmCmd → {"command": "set_config", "depth_threshold_m": 2.5}
    ├── Acknowledge all  → Write to AlarmCmd → {"command": "ack"}
    ├── Manual beep on   → Write to AlarmCmd → {"command": "beep_on"}
    └── Manual beep off  → Write to AlarmCmd → {"command": "beep_off"}

11. DISCONNECT
    └── Device restarts advertising automatically
```

---

## 16. UUID Reference Table

| Element | 128-bit UUID |
|---|---|
| Navigation Service | `4d475743-0001-4e41-5649-474154494f4e` |
| Navigation Data Characteristic | `4d475743-0101-4e41-5649-474154494f4e` |
| Wind Service | `4d475743-0002-4e41-5649-474154494f4e` |
| Wind Data Characteristic | `4d475743-0201-4e41-5649-474154494f4e` |
| Autopilot Service | `4d475743-0003-4e41-5649-474154494f4e` |
| Autopilot Data Characteristic | `4d475743-0301-4e41-5649-474154494f4e` |
| Autopilot Command Characteristic | `4d475743-0302-4e41-5649-474154494f4e` |
| Sail Performance Service | `4d475743-0004-4e41-5649-474154494f4e` |
| Performance Data Characteristic | `4d475743-0401-4e41-5649-474154494f4e` |
| Admin Service | `4d475743-0005-4e41-5649-474154494f4e` |
| Admin Data Characteristic | `4d475743-0501-4e41-5649-474154494f4e` |
| Admin Command Characteristic | `4d475743-0502-4e41-5649-474154494f4e` |
| Alarm Service | `4d475743-0006-4e41-5649-474154494f4e` |
| Alarm Data Characteristic | `4d475743-0601-4e41-5649-474154494f4e` |
| Alarm Command Characteristic | `4d475743-0602-4e41-5649-474154494f4e` |
| **AIS Service** | **`4d475743-0007-4e41-5649-474154494f4e`** |
| **AIS Data Characteristic** | **`4d475743-0701-4e41-5649-474154494f4e`** |
| CCCD (enable notifications) | `0x2902` (standard Bluetooth SIG) |