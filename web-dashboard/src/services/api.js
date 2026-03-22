const API_BASE = '/api';

export const api = {
  // ── WiFi configuration ───────────────────────────────────────
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

  // ── WiFi scanning ─────────────────────────────────────────────
  async startWiFiScan() {
    const response = await fetch(`${API_BASE}/wifi/scan`, { method: 'POST' });
    if (!response.ok) throw new Error('Failed to start WiFi scan');
    return response.json();
  },

  async getWiFiScanResults() {
    const response = await fetch(`${API_BASE}/wifi/scan`);
    if (!response.ok) throw new Error('Failed to get WiFi scan results');
    return response.json();
  },

  // ── Serial configuration ──────────────────────────────────────
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

  // ── System status ─────────────────────────────────────────────
  async getStatus() {
    const response = await fetch(`${API_BASE}/status`);
    if (!response.ok) throw new Error('Failed to get status');
    return response.json();
  },

  // ── Restart ───────────────────────────────────────────────────
  async restart() {
    const response = await fetch(`${API_BASE}/restart`, { method: 'POST' });
    if (!response.ok) throw new Error('Failed to restart');
    return response.json();
  },

  // ── BLE configuration ─────────────────────────────────────────
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

  // ── Polar / Performance ───────────────────────────────────────

  /**
   * Returns polar status: { loaded, file_size, tws_count, twa_count, tws_list, file_exists }
   */
  async getPolarStatus() {
    const response = await fetch(`${API_BASE}/polar/status`);
    if (!response.ok) throw new Error('Failed to get polar status');
    return response.json();
  },

  /**
   * Upload a polar file (.pol / .csv) to the ESP32.
   * @param {File}     file
   * @param {function} onProgress  called with (percent: number) during upload
   * @returns {Promise<{success, message?, error?}>}
   */
  async uploadPolar(file, onProgress) {
    return new Promise((resolve, reject) => {
      const xhr  = new XMLHttpRequest();
      const form = new FormData();
      form.append('polar', file, file.name);

      xhr.upload.onprogress = (e) => {
        if (e.lengthComputable && onProgress) {
          onProgress(Math.round((e.loaded / e.total) * 100));
        }
      };

      xhr.onload = () => {
        try {
          resolve(JSON.parse(xhr.responseText));
        } catch {
          resolve({ success: xhr.status < 300 });
        }
      };

      xhr.onerror = () => reject(new Error('Upload failed'));

      xhr.open('POST', `${API_BASE}/polar/upload`);
      xhr.send(form);
    });
  },

  /**
   * Returns performance data: { vmg: {value, unit, age}, polar_pct: {...}, polar_loaded }
   */
  async getPerformance() {
    const response = await fetch(`${API_BASE}/boat/performance`);
    if (!response.ok) throw new Error('Failed to get performance data');
    return response.json();
  },

  // ── Boat data endpoints ───────────────────────────────────────
  async getBoatNavigation() {
    const response = await fetch(`${API_BASE}/boat/navigation`);
    if (!response.ok) throw new Error('Failed to get navigation data');
    const data = await response.json();

    return {
      gps: {
        position: {
          lat: { value: data.position?.latitude,  valid: data.position?.latitude  !== null },
          lon: { value: data.position?.longitude, valid: data.position?.longitude !== null }
        },
        sog: {
          value: data.sog?.value, unit: data.sog?.unit || 'kn',
          valid: data.sog?.value !== null,
          timestamp: data.sog?.age ? Date.now() - (data.sog.age * 1000) : Date.now()
        },
        cog: {
          value: data.cog?.value, unit: data.cog?.unit || 'deg',
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
          value: data.heading?.value, unit: data.heading?.unit || 'deg',
          valid: data.heading?.value !== null,
          timestamp: data.heading?.age ? Date.now() - (data.heading.age * 1000) : Date.now()
        },
        true_heading: { value: null, valid: false, timestamp: Date.now() }
      },
      speed: {
        stw: {
          value: data.stw?.value, unit: data.stw?.unit || 'kn',
          valid: data.stw?.value !== null,
          timestamp: data.stw?.age ? Date.now() - (data.stw.age * 1000) : Date.now()
        },
        trip:  { value: data.trip?.value,  unit: data.trip?.unit  || 'nm', valid: data.trip?.value  !== null, timestamp: Date.now() },
        total: { value: data.total?.value, unit: data.total?.unit || 'nm', valid: data.total?.value !== null, timestamp: Date.now() }
      },
      depth: {
        below_transducer: {
          value: data.depth?.value, unit: data.depth?.unit || 'm',
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

    const mapDP = (d, unit) => ({
      value: d?.value ?? null,
      unit:  d?.unit  || unit,
      valid: d?.value !== null && d?.value !== undefined,
      timestamp: d?.age ? Date.now() - (d.age * 1000) : Date.now()
    });

    return {
      aws: mapDP(data.aws, 'kn'),
      awa: mapDP(data.awa, 'deg'),
      tws: mapDP(data.tws, 'kn'),
      twa: mapDP(data.twa, 'deg'),
      twd: mapDP(data.twd, 'deg'),
      environment: {
        water_temp: { value: null, valid: false, unit: '°C', timestamp: Date.now() }
      }
    };
  },

  async getBoatAIS() {
    const response = await fetch(`${API_BASE}/boat/ais`);
    if (!response.ok) throw new Error('Failed to get AIS data');
    const data = await response.json();

    // Flatten proximity sub-object so Instruments.jsx can use target.distance etc.
    const targets = (data.targets || []).map(t => ({
      mmsi:     t.mmsi,
      name:     t.name,
      lat:      t.position?.latitude,
      lon:      t.position?.longitude,
      cog:      t.cog,
      sog:      t.sog,
      heading:  t.heading,
      distance: t.proximity?.distance,
      bearing:  t.proximity?.bearing,
      cpa:      t.proximity?.cpa,
      tcpa:     t.proximity?.tcpa,
      age:      t.age,
    }));

    return { ...data, targets };
  }
};
