# Marine Gateway - API REST Documentation

## 📡 Endpoints de Données Nautiques (BoatState)

### Base URL
```
http://<IP_ESP32>/api/boat/
```

---

## 🧭 Navigation Data

### `GET /api/boat/navigation`

Retourne les données critiques de navigation : position GPS, vitesses, cap, profondeur.

#### Response Example
```json
{
  "position": {
    "latitude": 51.234567,
    "longitude": 4.123456,
    "age": 1.2
  },
  "sog": {
    "value": 5.2,
    "unit": "kn",
    "age": 1.2
  },
  "cog": {
    "value": 245.3,
    "unit": "deg",
    "age": 1.2
  },
  "stw": {
    "value": 5.5,
    "unit": "kn",
    "age": 0.8
  },
  "heading": {
    "value": 242.0,
    "unit": "deg",
    "age": 0.5
  },
  "depth": {
    "value": 12.3,
    "unit": "m",
    "age": 2.1
  },
  "gps_quality": {
    "satellites": 12,
    "fix_quality": 1,
    "hdop": 0.9
  },
  "trip": {
    "value": 23.4,
    "unit": "nm"
  },
  "total": {
    "value": 1234.5,
    "unit": "nm"
  }
}
```

#### Fields Description

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `position.latitude` | float | degrees | Latitude GPS (+ = Nord, - = Sud) |
| `position.longitude` | float | degrees | Longitude GPS (+ = Est, - = Ouest) |
| `position.age` | float | seconds | Âge de la donnée |
| `sog.value` | float | kn | Speed Over Ground |
| `cog.value` | float | deg | Course Over Ground (0-360°) |
| `stw.value` | float | kn | Speed Through Water |
| `heading.value` | float | deg | Cap vrai (0-360°) |
| `depth.value` | float | m | Profondeur sous transducteur |
| `gps_quality.satellites` | int | count | Nombre de satellites visibles |
| `gps_quality.fix_quality` | int | - | 0=Invalid, 1=GPS, 2=DGPS |
| `gps_quality.hdop` | float | - | Horizontal Dilution of Precision |
| `trip.value` | float | nm | Distance parcourue (trip) |
| `total.value` | float | nm | Distance totale parcourue |

#### HTTP Status Codes
- `200 OK` - Success
- `500 Internal Server Error` - BoatState not available

#### Notes
- Les valeurs `null` indiquent des données non disponibles ou périmées
- `age` indique le temps écoulé depuis la dernière mise à jour (en secondes)
- Données considérées périmées après 10 secondes sans mise à jour

---

## 🌊 Wind Data

### `GET /api/boat/wind`

Retourne les données de vent apparent et vrai (calculé).

#### Response Example
```json
{
  "apparent": {
    "speed": {
      "value": 15.2,
      "unit": "kn",
      "age": 0.5
    },
    "angle": {
      "value": 45.0,
      "unit": "deg",
      "age": 0.5
    }
  },
  "true": {
    "speed": {
      "value": 12.8,
      "unit": "kn",
      "age": 1.2
    },
    "angle": {
      "value": 38.0,
      "unit": "deg",
      "age": 1.2
    },
    "direction": {
      "value": 280.0,
      "unit": "deg",
      "age": 1.2
    }
  }
}
```

#### Fields Description

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `apparent.speed.value` | float | kn | Apparent Wind Speed (vitesse apparente) |
| `apparent.angle.value` | float | deg | Apparent Wind Angle (-180 à +180°, tribord positif) |
| `true.speed.value` | float | kn | True Wind Speed (calculé) |
| `true.angle.value` | float | deg | True Wind Angle (calculé) |
| `true.direction.value` | float | deg | True Wind Direction absolue (0-360°) |

#### Calculation Notes
- **True Wind** est calculé automatiquement à partir de :
  - Apparent Wind (AWS, AWA)
  - Speed Through Water (STW)
  - True Heading
- La formule vectorielle utilisée :
  ```
  True Wind = Apparent Wind - Boat Velocity
  ```

#### HTTP Status Codes
- `200 OK` - Success
- `500 Internal Server Error` - BoatState not available

---

## 🚢 AIS Data

### `GET /api/boat/ais`

Retourne la liste des cibles AIS actives avec calculs de proximité.

#### Response Example
```json
{
  "target_count": 2,
  "targets": [
    {
      "mmsi": 123456789,
      "name": "VESSEL NAME",
      "position": {
        "latitude": 51.245678,
        "longitude": 4.134567
      },
      "cog": 180.0,
      "sog": 8.5,
      "heading": 182.0,
      "proximity": {
        "distance": 2.3,
        "distance_unit": "nm",
        "bearing": 045.0,
        "bearing_unit": "deg",
        "cpa": 0.5,
        "cpa_unit": "nm",
        "tcpa": 12.5,
        "tcpa_unit": "min"
      },
      "age": 15
    },
    {
      "mmsi": 987654321,
      "name": "ANOTHER SHIP",
      "position": {
        "latitude": 51.267890,
        "longitude": 4.156789
      },
      "cog": 270.0,
      "sog": 12.0,
      "heading": 268.0,
      "proximity": {
        "distance": 5.8,
        "distance_unit": "nm",
        "bearing": 315.0,
        "bearing_unit": "deg",
        "cpa": 2.1,
        "cpa_unit": "nm",
        "tcpa": 25.0,
        "tcpa_unit": "min"
      },
      "age": 8
    }
  ]
}
```

#### Fields Description

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `target_count` | int | count | Nombre de cibles AIS actives |
| `mmsi` | int | - | Maritime Mobile Service Identity |
| `name` | string | - | Nom du navire |
| `position.latitude` | float | deg | Latitude de la cible |
| `position.longitude` | float | deg | Longitude de la cible |
| `cog` | float | deg | Course Over Ground de la cible |
| `sog` | float | kn | Speed Over Ground de la cible |
| `heading` | float | deg | Cap de la cible |
| `proximity.distance` | float | nm | Distance actuelle à la cible |
| `proximity.bearing` | float | deg | Relèvement de la cible |
| `proximity.cpa` | float | nm | Closest Point of Approach |
| `proximity.tcpa` | float | min | Time to CPA |
| `age` | int | sec | Âge de la dernière mise à jour |

#### Important Notes
- Seules les cibles avec `age < 60 secondes` sont retournées
- Les cibles plus anciennes sont automatiquement supprimées
- Maximum 20 cibles en mémoire (configurable via `MAX_AIS_TARGETS`)
- CPA et TCPA sont calculés automatiquement si les données sont disponibles

#### HTTP Status Codes
- `200 OK` - Success (peut retourner un tableau vide si aucune cible)
- `500 Internal Server Error` - BoatState not available

---

## 📊 Complete Boat State

### `GET /api/boat/state`

Retourne l'état complet du bateau (toutes les données combinées).

#### Response Example
```json
{
  "gps": {
    "position": {
      "lat": { "value": 51.234567, "unit": "deg", "age": 1.2 },
      "lon": { "value": 4.123456, "unit": "deg", "age": 1.2 }
    },
    "sog": { "value": 5.2, "unit": "kn", "age": 1.2 },
    "cog": { "value": 245.3, "unit": "deg", "age": 1.2 },
    "satellites": { "value": 12, "unit": "count", "age": 1.2 },
    "fix_quality": { "value": 1, "unit": "", "age": 1.2 },
    "hdop": { "value": 0.9, "unit": "", "age": 1.2 }
  },
  "speed": {
    "stw": { "value": 5.5, "unit": "kn", "age": 0.8 },
    "trip": { "value": 23.4, "unit": "nm", "age": 0.8 },
    "total": { "value": 1234.5, "unit": "nm", "age": 0.8 }
  },
  "heading": {
    "magnetic": { "value": 240.0, "unit": "deg", "age": 0.5 },
    "true": { "value": 242.0, "unit": "deg", "age": 0.5 }
  },
  "depth": {
    "below_transducer": { "value": 12.3, "unit": "m", "age": 2.1 },
    "offset": { "value": 0.5, "unit": "m", "age": null }
  },
  "wind": {
    "aws": { "value": 15.2, "unit": "kn", "age": 0.5 },
    "awa": { "value": 45.0, "unit": "deg", "age": 0.5 },
    "tws": { "value": 12.8, "unit": "kn", "age": 1.2 },
    "twa": { "value": 38.0, "unit": "deg", "age": 1.2 },
    "twd": { "value": 280.0, "unit": "deg", "age": 1.2 }
  },
  "environment": {
    "water_temp": { "value": 18.5, "unit": "C", "age": 5.0 },
    "air_temp": { "value": null, "unit": "C", "age": null },
    "pressure": { "value": null, "unit": "hPa", "age": null }
  },
  "calculated": {
    "vmg_wind": { "value": 4.2, "unit": "kn", "age": 1.0 },
    "vmg_waypoint": { "value": null, "unit": "kn", "age": null },
    "set": { "value": 95.0, "unit": "deg", "age": 2.0 },
    "drift": { "value": 0.8, "unit": "kn", "age": 2.0 }
  },
  "autopilot": {
    "mode": null,
    "status": null,
    "age": null
  },
  "ais": {
    "targets": [
      // ... voir GET /api/boat/ais
    ]
  }
}
```

#### HTTP Status Codes
- `200 OK` - Success
- `500 Internal Server Error` - BoatState not available

#### Notes
- Cet endpoint retourne **toutes** les données disponibles
- Peut être volumineux si beaucoup de cibles AIS
- Préférer les endpoints spécialisés (`/navigation`, `/wind`, `/ais`) pour des requêtes plus légères

---

## 🔄 Data Freshness & Timeouts

| Data Type | Timeout | Notes |
|-----------|---------|-------|
| GPS | 10s | Position, SOG, COG |
| Speed | 10s | STW, Trip, Total |
| Heading | 10s | Magnetic, True |
| Depth | 10s | Below transducer |
| Wind | 10s | Apparent & True |
| Environment | 10s | Temp, Pressure |
| AIS | 60s | AIS targets |

Après le timeout, les données sont considérées comme **stale** et retournées avec `value: null`.

---

## 🔌 WebSocket Streaming

### `WS /ws/nmea`

Stream temps réel de toutes les sentences NMEA reçues.

#### Connection
```javascript
const ws = new WebSocket('ws://192.168.1.100/ws/nmea');

ws.onmessage = (event) => {
  console.log('NMEA:', event.data);
  // Exemple: "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"
};
```

#### Message Format
- Messages envoyés en **texte brut**
- Une sentence NMEA par message
- Format standard NMEA 0183 : `$XXYYY,field1,field2,...*checksum`

#### Use Cases
- Affichage temps réel des données brutes
- Logging de toutes les sentences
- Debug du flux NMEA
- Applications nécessitant un accès direct aux sentences

---

## 📡 TCP Streaming

### Port: `10110`

Stream NMEA 0183 brut sur TCP (compatible OpenCPN, etc.)

#### Connection Example (Python)
```python
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.1.100', 10110))

while True:
    data = sock.recv(1024)
    print(data.decode('utf-8'))
```

#### Format
- Sentences NMEA terminées par `\r\n`
- Compatible avec tous les logiciels de navigation supportant NMEA 0183 over TCP
- Jusqu'à 5 clients simultanés

---

## 🛠️ Configuration Endpoints

### `GET /api/config/wifi`
Récupère la configuration WiFi

### `POST /api/config/wifi`
Configure le WiFi (STA ou AP mode)

### `GET /api/config/serial`
Récupère la configuration série (baud rate, etc.)

### `POST /api/config/serial`
Configure le port série NMEA

### `GET /api/status`
Status système (uptime, heap, WiFi, TCP, UART)

### `POST /api/restart`
Redémarre l'ESP32

### `POST /api/wifi/scan`
Lance un scan WiFi

### `GET /api/wifi/scan`
Récupère les résultats du scan

Voir la documentation complète dans le code source pour les détails.

---

## 💡 Usage Examples

### JavaScript (Fetch API)

```javascript
// Get navigation data
async function getNavigation() {
  const response = await fetch('http://192.168.1.100/api/boat/navigation');
  const data = await response.json();
  
  if (data.position.latitude !== null) {
    console.log(`Position: ${data.position.latitude}, ${data.position.longitude}`);
    console.log(`SOG: ${data.sog.value} ${data.sog.unit}`);
    console.log(`COG: ${data.cog.value}°`);
  }
}

// Get wind data
async function getWind() {
  const response = await fetch('http://192.168.1.100/api/boat/wind');
  const data = await response.json();
  
  if (data.apparent.speed.value !== null) {
    console.log(`AWS: ${data.apparent.speed.value} kn @ ${data.apparent.angle.value}°`);
  }
  
  if (data.true.speed.value !== null) {
    console.log(`TWS: ${data.true.speed.value} kn from ${data.true.direction.value}°`);
  }
}

// Get AIS targets
async function getAIS() {
  const response = await fetch('http://192.168.1.100/api/boat/ais');
  const data = await response.json();
  
  console.log(`${data.target_count} AIS targets:`);
  data.targets.forEach(target => {
    console.log(`${target.name} (${target.mmsi}): ${target.proximity.distance} nm @ ${target.proximity.bearing}°`);
  });
}
```

### Python

```python
import requests

# Get navigation data
def get_navigation():
    r = requests.get('http://192.168.1.100/api/boat/navigation')
    data = r.json()
    
    if data['position']['latitude'] is not None:
        print(f"Position: {data['position']['latitude']}, {data['position']['longitude']}")
        print(f"SOG: {data['sog']['value']} {data['sog']['unit']}")
        print(f"COG: {data['cog']['value']}°")

# Get wind data
def get_wind():
    r = requests.get('http://192.168.1.100/api/boat/wind')
    data = r.json()
    
    if data['apparent']['speed']['value'] is not None:
        print(f"AWS: {data['apparent']['speed']['value']} kn @ {data['apparent']['angle']['value']}°")
```

### curl

```bash
# Navigation
curl http://192.168.1.100/api/boat/navigation | jq

# Wind
curl http://192.168.1.100/api/boat/wind | jq

# AIS
curl http://192.168.1.100/api/boat/ais | jq

# Complete state
curl http://192.168.1.100/api/boat/state | jq
```

---

## 🎯 Best Practices

1. **Polling Interval**: Ne pas interroger plus d'une fois par seconde pour éviter de surcharger l'ESP32
2. **Check for null**: Toujours vérifier si les valeurs sont `null` avant utilisation
3. **Use specialized endpoints**: Préférer `/navigation`, `/wind`, `/ais` plutôt que `/state` pour réduire la bande passante
4. **WebSocket for real-time**: Utiliser le WebSocket pour du monitoring temps réel
5. **Check age**: Vérifier l'âge des données (`age` field) pour s'assurer qu'elles sont fraîches

---

## 🔒 Security Notes

⚠️ **Production Warning**: 
- Aucune authentification par défaut
- À utiliser uniquement sur réseau privé/sécurisé
- Pas de HTTPS (ESP32 limité)
- Considérer VPN ou firewall pour usage en production

---

## 📝 Changelog

### Version 1.0.0
- ✅ Endpoints Navigation, Wind, AIS
- ✅ WebSocket NMEA streaming
- ✅ TCP NMEA streaming
- ✅ Calcul automatique du vent vrai
- ✅ Thread-safe data access
- ✅ Automatic stale data cleanup
