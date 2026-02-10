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
  }
};
