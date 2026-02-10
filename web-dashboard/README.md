# Marine Gateway Dashboard

React-based web dashboard for Marine Gateway configuration and monitoring.

## Development

```bash
npm install
npm run dev
```

Opens development server at http://localhost:5173

## Build

```bash
npm run build
```

Builds optimized bundle to `../data/www/` ready for upload to ESP32.

## Features

- **System Status**: View uptime, memory, WiFi, TCP clients, NMEA statistics
- **WiFi Configuration**: Configure STA/AP mode, SSID, password
- **Serial Configuration**: Configure UART parameters (baud rate, data bits, parity, stop bits)
- **NMEA Monitor**: Real-time NMEA sentence streaming via WebSocket

## Technology

- React 18
- React Router 6
- Vite
- WebSocket for real-time NMEA streaming
- Fetch API for REST endpoints
