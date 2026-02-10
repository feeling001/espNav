# Marine Gateway - ESP32-S3 MVP

Firmware pour ESP32-S3 permettant d'interfacer avec des systèmes de navigation marine (données de vent, vitesse, profondeur, etc.) et de diffuser ces informations via WiFi.

## Fonctionnalités MVP

- ✅ Interface NMEA0183 via UART1
- ✅ Serveur TCP sur port 10110
- ✅ Diffusion NMEA vers plusieurs clients TCP
- ✅ Configuration WiFi via dashboard web
- ✅ Configuration port série via dashboard web
- ✅ Moniteur NMEA en temps réel

## Matériel requis

- ESP32-S3 (4MB Flash minimum)
- Câble USB
- Appareil NMEA0183 (GPS, instruments marins)

## Installation

### Prérequis

- PlatformIO (`pip install platformio`)
- Node.js 18+ (pour le dashboard React)

### Build complet

```bash
chmod +x build_and_flash.sh
./build_and_flash.sh
```

Ou étape par étape:

```bash
# 1. Build dashboard React
cd web-dashboard
npm install
npm run build
cd ..

# 2. Build firmware
pio run

# 3. Upload filesystem
pio run -t uploadfs

# 4. Upload firmware
pio run -t upload

# 5. Moniteur série
pio device monitor
```

## Configuration initiale

1. L'ESP32 démarre en mode AP: `MarineGateway-XXXXXX`
2. Connectez-vous au WiFi AP
3. Ouvrez `http://192.168.4.1`
4. Configurez vos paramètres WiFi et série
5. Redémarrez

## Utilisation

### Connexion TCP

```bash
nc <ESP_IP> 10110
```

Ou utilisez OpenCPN, SignalK, etc.

### Dashboard web

Ouvrez `http://<ESP_IP>` dans votre navigateur.

## Structure du projet

```
marine-gateway/
├── platformio.ini       # Configuration PlatformIO
├── partitions.csv       # Table de partitions
├── include/            # Headers
├── src/                # Code source firmware
├── web-dashboard/      # Dashboard React
└── data/www/          # Dashboard compilé (LittleFS)
```

## Licence

TBD
