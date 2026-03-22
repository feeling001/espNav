# Marine Gateway — BLE Client Developer Documentation

> This document describes the complete BLE protocol exposed by the Marine Gateway (ESP32).  
> It contains all the information needed to develop a BLE client application
> (iOS, Android, Flutter, React Native, etc.) that consumes the transmitted marine data.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Connection and Security](#2-connection-and-security)
3. [Services and Characteristics](#3-services-and-characteristics)
4. [Navigation Service](#4-navigation-service)
5. [Wind Service](#5-wind-service)
6. [Autopilot Service](#6-autopilot-service)
7. [Sail Performance Service](#7-sail-performance-service)
8. [Sending Autopilot Commands](#8-sending-autopilot-commands)
9. [Behavior and Timing](#9-behavior-and-timing)
10. [Integration Flow Example](#10-integration-flow-example)
11. [UUID Reference Table](#11-uuid-reference-table)

---

## 1. Overview

The Marine Gateway is an ESP32-based bridge that reads NMEA 0183 data from a serial port and exposes it over Bluetooth Low Energy (BLE). It implements a custom GATT profile organized into **4 services**:

| Service | Purpose |
|---|---|
| Navigation | GPS position, speed, heading, depth |
| Wind | Apparent wind and true wind |
| Autopilot | Autopilot state + command input |
| Sail Performance | VMG, polar efficiency, polar target speed |

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

### Characteristic Properties

| Characteristic | READ | NOTIFY | WRITE |
|---|:---:|:---:|:---:|
| NavData | ✅ | ✅ | ❌ |
| WindData | ✅ | ✅ | ❌ |
| AutopilotData | ✅ | ✅ | ❌ |
| AutopilotCmd | ❌ | ❌ | ✅ |
| PerformanceData | ✅ | ✅ | ❌ |

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

### Null Value Rules

| Field | Null when |
|---|---|
| `vmg` | STW or TWA stale / invalid |
| `polar_pct` | No polar loaded, or STW / TWS / TWA stale / invalid |
| `target_stw` | No polar loaded, or TWS / TWA stale / invalid, or target ≤ 0.1 kn (e.g. dead upwind in a sparse polar) |

### Typical Display Usage

```
VMG        +4.2 kn  ▲ upwind
Polar         85 %
Target STW   7.1 kn
```

### Polar Upload

A polar file is uploaded via the web dashboard at `http://<device_ip>/` → **Performance** page.  
Accepted format: tab-delimited `.pol` / `.csv` file.  
- Row 0, Col 0: label (e.g. `TWA\TWS`) — ignored  
- Row 0, Col 1…N: TWS breakpoints in knots  
- Row 1…M, Col 0: TWA breakpoints in degrees (0–180)  
- Cells: target STW in knots  

The file is persisted on the device (LittleFS) and reloaded automatically on reboot.

---

## 8. Sending Autopilot Commands

**Command Characteristic UUID:** `4d475743-0302-4e41-5649-474154494f4e`  
**Property:** WRITE (no response expected)

### Command Format

```json
{ "command": "adjust+1" }
```

### Command Table

| `command` value | Action |
|---|---|
| `"enable"` | Enable the autopilot |
| `"disable"` | Disable the autopilot (standby mode) |
| `"adjust+10"` | Adjust target heading by +10° (starboard) |
| `"adjust-10"` | Adjust target heading by −10° (port) |
| `"adjust+1"` | Adjust target heading by +1° (starboard) |
| `"adjust-1"` | Adjust target heading by −1° (port) |

---

## 9. Behavior and Timing

| Parameter | Value |
|---|---|
| Update frequency | **1 Hz** (every 1000 ms) — all services |
| Max simultaneous connections | **3** devices |
| NMEA data timeout | **10 seconds** (navigation, wind, performance, autopilot) |
| AIS data timeout | **60 seconds** |
| Reconnection | Automatic (advertising restarts after disconnection) |

### Update Cycle

Every second, the device:
1. Reads the current `BoatState` (FreeRTOS mutex protected).
2. Serializes all 4 characteristics to JSON.
3. Sends a BLE notification on each characteristic to all subscribed clients.

---

## 10. Integration Flow Example

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
   └── Write 0x0100 to PerformanceData CCCD

6. RECEIVE DATA (every ~1 s)
   ├── NavData         → position, speed, heading, depth
   ├── WindData        → apparent / true wind
   ├── AutopilotData   → autopilot state
   └── PerformanceData → vmg, polar_pct, target_stw, polar_loaded

7. HANDLE POLAR STATE
   ├── if polar_loaded == false → show "No polar loaded" in UI
   └── if polar_loaded == true  → display polar_pct and target_stw

8. SEND COMMANDS (optional)
   └── Write to AutopilotCmd → {"command": "adjust+1"}

9. DISCONNECT
   └── Device restarts advertising automatically
```

---

## 11. UUID Reference Table

| Element | 128-bit UUID |
|---|---|
| Navigation Service | `4d475743-0001-4e41-5649-474154494f4e` |
| Navigation Data Characteristic | `4d475743-0101-4e41-5649-474154494f4e` |
| Wind Service | `4d475743-0002-4e41-5649-474154494f4e` |
| Wind Data Characteristic | `4d475743-0201-4e41-5649-474154494f4e` |
| Autopilot Service | `4d475743-0003-4e41-5649-474154494f4e` |
| Autopilot Data Characteristic | `4d475743-0301-4e41-5649-474154494f4e` |
| Autopilot Command Characteristic | `4d475743-0302-4e41-5649-474154494f4e` |
| **Sail Performance Service** | **`4d475743-0004-4e41-5649-474154494f4e`** |
| **Performance Data Characteristic** | **`4d475743-0401-4e41-5649-474154494f4e`** |
| CCCD (enable notifications) | `0x2902` (standard Bluetooth SIG) |
