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
    const data = await response.json();
    
    // Transform flat structure to nested structure expected by frontend
    return {
      gps: {
        position: {
          lat: {
            value: data.position?.latitude,
            valid: data.position?.latitude !== null
          },
          lon: {
            value: data.position?.longitude,
            valid: data.position?.longitude !== null
          }
        },
        sog: {
          value: data.sog?.value,
          unit: data.sog?.unit || 'kn',
          valid: data.sog?.value !== null,
          timestamp: data.sog?.age ? Date.now() - (data.sog.age * 1000) : Date.now()
        },
        cog: {
          value: data.cog?.value,
          unit: data.cog?.unit || 'deg',
          valid: data.cog?.value !== null,
          timestamp: data.cog?.age ? Date.now() - (data.cog.age * 1000) : Date.now()
        },
        satellites: {
          value: data.gps_quality?.satellites,
          valid: data.gps_quality?.satellites !== null,
          timestamp: Date.now()
        }
      },
      heading: {
        magnetic: {
          value: data.heading?.value,
          unit: data.heading?.unit || 'deg',
          valid: data.heading?.value !== null,
          timestamp: data.heading?.age ? Date.now() - (data.heading.age * 1000) : Date.now()
        },
        true_heading: {
          value: null, // Not provided in your backend response
          valid: false,
          timestamp: Date.now()
        }
      },
      speed: {
        stw: {
          value: data.stw?.value,
          unit: data.stw?.unit || 'kn',
          valid: data.stw?.value !== null,
          timestamp: data.stw?.age ? Date.now() - (data.stw.age * 1000) : Date.now()
        },
        trip: {
          value: data.trip?.value,
          unit: data.trip?.unit || 'nm',
          valid: data.trip?.value !== null,
          timestamp: Date.now()
        },
        total: {
          value: data.total?.value,
          unit: data.total?.unit || 'nm',
          valid: data.total?.value !== null,
          timestamp: Date.now()
        }
      },
      depth: {
        below_transducer: {
          value: data.depth?.value,
          unit: data.depth?.unit || 'm',
          valid: data.depth?.value !== null,
          timestamp: data.depth?.age ? Date.now() - (data.depth.age * 1000) : Date.now()
        }
      }
    };
  },

  async getBoatWind() {
    const response = await fetch(`${API_BASE}/boat/wind`);
    if (!response.ok) throw new Error('Failed to get wind data');
    const data = await response.json();
    
    // Transform wind data if needed
    return {
      aws: {
        value: data.aws?.value,
        unit: data.aws?.unit || 'kn',
        valid: data.aws?.value !== null,
        timestamp: data.aws?.age ? Date.now() - (data.aws.age * 1000) : Date.now()
      },
      awa: {
        value: data.awa?.value,
        unit: data.awa?.unit || 'deg',
        valid: data.awa?.value !== null,
        timestamp: data.awa?.age ? Date.now() - (data.awa.age * 1000) : Date.now()
      },
      tws: {
        value: data.tws?.value,
        unit: data.tws?.unit || 'kn',
        valid: data.tws?.value !== null,
        timestamp: data.tws?.age ? Date.now() - (data.tws.age * 1000) : Date.now()
      },
      twa: {
        value: data.twa?.value,
        unit: data.twa?.unit || 'deg',
        valid: data.twa?.value !== null,
        timestamp: data.twa?.age ? Date.now() - (data.twa.age * 1000) : Date.now()
      },
      twd: {
        value: data.twd?.value,
        unit: data.twd?.unit || 'deg',
        valid: data.twd?.value !== null,
        timestamp: data.twd?.age ? Date.now() - (data.twd.age * 1000) : Date.now()
      },
      environment: {
        water_temp: {
          value: data.water_temp?.value,
          unit: data.water_temp?.unit || '°C',
          valid: data.water_temp?.value !== null,
          timestamp: data.water_temp?.age ? Date.now() - (data.water_temp.age * 1000) : Date.now()
        }
      }
    };
  },

  async getBoatAIS() {
    const response = await fetch(`${API_BASE}/boat/ais`);
    if (!response.ok) throw new Error('Failed to get AIS data');
    return response.json();
  }
};