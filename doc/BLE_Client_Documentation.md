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
7. [Sending Autopilot Commands](#7-sending-autopilot-commands)
8. [Behavior and Timing](#8-behavior-and-timing)
9. [Integration Flow Example](#9-integration-flow-example)
10. [UUID Reference Table](#10-uuid-reference-table)

---

## 1. Overview

The Marine Gateway is an ESP32-based bridge that reads NMEA 0183 data from a serial port and exposes it over Bluetooth Low Energy (BLE). It implements a custom GATT profile organized into **3 services**:

| Service | Purpose |
|---|---|
| Navigation | GPS position, speed, heading, depth |
| Wind | Apparent wind and true wind |
| Autopilot | Autopilot state + command input |

Data is encoded as **UTF-8 JSON** in each characteristic and updated at **1 Hz** (every second).

---

## 2. Connection and Security

### Discovery

- **Advertised BLE name:** `MarineGateway` (configurable via the API)
- The device advertises continuously and accepts up to `BLE_MAX_CONNECTIONS = 3` simultaneous connections.
- After a client disconnects, advertising restarts automatically.

### Security (Pairing)

The Marine Gateway uses **Secure Connections with MITM** (Man-in-the-Middle) protection and **bonding**.

| Parameter | Value |
|---|---|
| Authentication mode | `ESP_LE_AUTH_REQ_SC_MITM_BOND` |
| IO Capability | `ESP_IO_CAP_OUT` — the ESP32 **displays** a PIN (passkey only, no keyboard input) |
| Bonding | Enabled — keys are saved for automatic reconnection |

### Pairing Procedure

1. The client application initiates the BLE connection.
2. The device displays (on its interface) a **6-digit PIN code** (default: `123456`, configurable via API).
3. The user enters this PIN in the client application.
4. Once paired, the bond is saved: subsequent reconnections are automatic without re-entering the PIN.

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

### Characteristic Properties

| Characteristic | READ | NOTIFY | WRITE |
|---|:---:|:---:|:---:|
| NavData | ✅ | ✅ | ❌ |
| WindData | ✅ | ✅ | ❌ |
| AutopilotData | ✅ | ✅ | ❌ |
| AutopilotCmd | ❌ | ❌ | ✅ |

> All `NOTIFY` characteristics include a **CCCD** (Client Characteristic Configuration Descriptor — UUID `0x2902`).  
> The client application **must enable notifications** on each desired characteristic to receive updates.

---

## 4. Navigation Service

**Service UUID:** `4d475743-0001-4e41-5649-474154494f4e`  
**Characteristic UUID:** `4d475743-0101-4e41-5649-474154494f4e`

### Data Format

Encoding: **UTF-8 JSON** (text).

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

### Field Description

| Field | Type | Unit | Description |
|---|---|---|---|
| `lat` | float \| null | decimal degrees | GPS latitude. Negative = South. `null` if invalid or stale. |
| `lon` | float \| null | decimal degrees | GPS longitude. Negative = West. `null` if invalid or stale. |
| `sog` | float \| null | knots (kn) | Speed Over Ground |
| `cog` | float \| null | degrees (0–360°) | Course Over Ground |
| `stw` | float \| null | knots (kn) | Speed Through Water (log) |
| `hdg_mag` | float \| null | degrees (0–360°) | Magnetic heading |
| `hdg_true` | float \| null | degrees (0–360°) | True heading (not yet implemented — always `null`) |
| `depth` | float \| null | meters (m) | Depth below transducer |

### Null Value Handling

- A field is `null` if the data has never been received since boot, or if it is stale (no NMEA update for more than 10 seconds).
- The client application must always check for null before using a value.

### Corresponding NMEA Sources

| Field | NMEA Sentences |
|---|---|
| `lat`, `lon` | `$GPRMC`, `$GPGGA`, `$GNRMC`, `$GNGGA` |
| `sog`, `cog` | `$GPRMC`, `$GPVTG` |
| `stw` | `$VWVHW` |
| `hdg_mag` | `$HCHDG`, `$HDHDM` |
| `depth` | `$SDDPT`, `$SDDBT` |

---

## 5. Wind Service

**Service UUID:** `4d475743-0002-4e41-5649-474154494f4e`  
**Characteristic UUID:** `4d475743-0201-4e41-5649-474154494f4e`

### Data Format

Encoding: **UTF-8 JSON** (text).

```json
{
  "aws": 12.3,
  "awa": 45.0,
  "tws": 10.1,
  "twa": 52.0,
  "twd": 187.0
}
```

### Field Description

| Field | Type | Unit | Description |
|---|---|---|---|
| `aws` | float \| null | knots (kn) | Apparent Wind Speed |
| `awa` | float \| null | degrees | Apparent Wind Angle. Positive = starboard, negative = port. Range: −180° to +180°. |
| `tws` | float \| null | knots (kn) | True Wind Speed (calculated) |
| `twa` | float \| null | degrees | True Wind Angle. Same sign convention as AWA. |
| `twd` | float \| null | degrees (0–360°) | True Wind Direction, referenced to geographic North |

### Null Value Handling

Same rule as navigation: `null` if absent or stale for more than 10 seconds.

### Corresponding NMEA Sources

| Field | NMEA Sentences |
|---|---|
| `aws`, `awa` | `$WIMWV` (Relative) |
| `tws`, `twa` | `$WIMWV` (True) or calculated |
| `twd` | `$WIMWD` or calculated |

---

## 6. Autopilot Service

**Service UUID:** `4d475743-0003-4e41-5649-474154494f4e`  
**Data Characteristic UUID:** `4d475743-0301-4e41-5649-474154494f4e`

### Data Format

Encoding: **UTF-8 JSON** (text).

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

### Field Description

| Field | Type | Description |
|---|---|---|
| `mode` | string \| null | Autopilot mode: `"standby"`, `"auto"`, `"wind"`, `"track"`, `"manual"` |
| `status` | string \| null | Autopilot state: `"engaged"`, `"standby"`, `"alarm"` |
| `heading_target` | float \| null | Target heading in degrees (`auto` mode) |
| `wind_target` | float \| null | Target wind angle in degrees (`wind` mode) |
| `rudder` | float \| null | Rudder angle in degrees. Positive = starboard, negative = port. |
| `locked_heading` | float \| null | Locked heading in degrees |

> **Note:** Autopilot support depends on SeaTalk1 / NMEA integration. If no data is received, all fields will be `null`.

---

## 7. Sending Autopilot Commands

**Command Characteristic UUID:** `4d475743-0302-4e41-5649-474154494f4e`  
**Property:** WRITE (no response expected)

### Command Format

Commands are sent as **UTF-8 JSON** via a BLE characteristic write (`writeCharacteristic`).

```json
{ "cmd": 1 }
```

### Command Table

| `cmd` value | Constant | Action |
|:---:|---|---|
| `0` | `NONE` | No action |
| `1` | `ENABLE` | Enable the autopilot |
| `2` | `DISABLE` | Disable the autopilot (standby mode) |
| `3` | `ADJUST_PLUS_10` | Adjust target heading by +10° (starboard) |
| `4` | `ADJUST_MINUS_10` | Adjust target heading by −10° (port) |
| `5` | `ADJUST_PLUS_1` | Adjust target heading by +1° (starboard) |
| `6` | `ADJUST_MINUS_1` | Adjust target heading by −1° (port) |

### Send Example (pseudo-code)

```
// Adjust +1° to starboard
writeCharacteristic(
  uuid: "4d475743-0302-4e41-5649-474154494f4e",
  data: '{"cmd":5}'
)
```

---

## 8. Behavior and Timing

| Parameter | Value |
|---|---|
| Update frequency | **1 Hz** (every 1000 ms) |
| Max simultaneous connections | **3** devices |
| NMEA data timeout | **10 seconds** (navigation, wind, autopilot) |
| AIS data timeout | **60 seconds** |
| BLE MTU | Standard (23 bytes default) — JSON payloads are short (< 200 bytes) |
| Reconnection | Automatic (the device restarts advertising after disconnection) |

### Update Cycle

Every second, the device:
1. Reads the current `BoatState` (protected by a FreeRTOS mutex).
2. Serializes the 3 characteristics to JSON.
3. Sends a BLE notification on each characteristic.

---

## 9. Integration Flow Example

Typical flow for a mobile application:

```
1. SCAN
   ├── Search for a peripheral named "MarineGateway"
   └── Optionally filter on Navigation service UUID:
       4d475743-0001-4e41-5649-474154494f4e

2. CONNECT
   └── Initiate BLE connection

3. PAIRING (first time only)
   ├── Device triggers a passkey notification
   ├── Show a numeric input field to the user
   └── User enters the 6-digit PIN displayed on the Marine Gateway

4. SERVICE DISCOVERY
   └── Discover services and characteristics

5. SUBSCRIBE TO NOTIFICATIONS
   ├── Write 0x0100 to NavData CCCD
   ├── Write 0x0100 to WindData CCCD
   └── Write 0x0100 to AutopilotData CCCD

6. RECEIVE DATA (every ~1s)
   ├── NavData notification → parse JSON → display position, speed, heading, depth
   ├── WindData notification → parse JSON → display apparent/true wind
   └── AutopilotData notification → parse JSON → display autopilot state

7. SEND COMMANDS (optional)
   └── Write to AutopilotCmd → JSON {"cmd": N}

8. DISCONNECT
   └── Disconnect cleanly (the device restarts advertising automatically)
```

---

## 10. UUID Reference Table

| Element | 128-bit UUID |
|---|---|
| Navigation Service | `4d475743-0001-4e41-5649-474154494f4e` |
| Navigation Data Characteristic | `4d475743-0101-4e41-5649-474154494f4e` |
| Wind Service | `4d475743-0002-4e41-5649-474154494f4e` |
| Wind Data Characteristic | `4d475743-0201-4e41-5649-474154494f4e` |
| Autopilot Service | `4d475743-0003-4e41-5649-474154494f4e` |
| Autopilot Data Characteristic | `4d475743-0301-4e41-5649-474154494f4e` |
| Autopilot Command Characteristic | `4d475743-0302-4e41-5649-474154494f4e` |
| CCCD (enable notifications) | `0x2902` (standard Bluetooth SIG) |
