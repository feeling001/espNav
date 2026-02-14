const API_BASE = '/api';

export const api = {
  // WiFi configuration
  async getWiFiConfig() {
    const response = await fetch(`${API_BASE}/config/wifi`);
    if (!response.ok) throw new Error('Failed to get WiFi config');
    return response.json();
  },

  async setWiFiConfig(config) {
    const response = await fetch(`${API_BASE}/config/wifi`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(config)
    });
    if (!response.ok) throw new Error('Failed to set WiFi config');
    return response.json();
  },

  // WiFi scanning
  async startWiFiScan() {
    const response = await fetch(`${API_BASE}/wifi/scan`, {
      method: 'POST'
    });
    if (!response.ok) throw new Error('Failed to start WiFi scan');
    return response.json();
  },

  async getWiFiScanResults() {
    const response = await fetch(`${API_BASE}/wifi/scan`);
    if (!response.ok) throw new Error('Failed to get WiFi scan results');
    return response.json();
  },

  // Serial configuration
  async getSerialConfig() {
    const response = await fetch(`${API_BASE}/config/serial`);
    if (!response.ok) throw new Error('Failed to get serial config');
    return response.json();
  },

  async setSerialConfig(config) {
    const response = await fetch(`${API_BASE}/config/serial`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(config)
    });
    if (!response.ok) throw new Error('Failed to set serial config');
    return response.json();
  },

  // System status
  async getStatus() {
    const response = await fetch(`${API_BASE}/status`);
    if (!response.ok) throw new Error('Failed to get status');
    return response.json();
  },

  // Restart
  async restart() {
    const response = await fetch(`${API_BASE}/restart`, { method: 'POST' });
    if (!response.ok) throw new Error('Failed to restart');
    return response.json();
  },

  // BLE configuration
  async getBLEConfig() {
    const response = await fetch(`${API_BASE}/config/ble`);
    if (!response.ok) throw new Error('Failed to get BLE config');
    return response.json();
  },

  async setBLEConfig(config) {
    const response = await fetch(`${API_BASE}/config/ble`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(config)
    });
    if (!response.ok) throw new Error('Failed to set BLE config');
    return response.json();
  },

  // Boat data endpoints
  async getBoatNavigation() {
    const response = await fetch(`${API_BASE}/boat/navigation`);
    if (!response.ok) throw new Error('Failed to get navigation data');
    return response.json();
  },

  async getBoatWind() {
    const response = await fetch(`${API_BASE}/boat/wind`);
    if (!response.ok) throw new Error('Failed to get wind data');
    return response.json();
  },

  async getBoatAIS() {
    const response = await fetch(`${API_BASE}/boat/ais`);
    if (!response.ok) throw new Error('Failed to get AIS data');
    return response.json();
  }
};