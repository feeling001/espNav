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

  // ── OTA Update ────────────────────────────────────────────────
  async getOTAStatus() {
    const response = await fetch(`${API_BASE}/ota/status`);
    if (!response.ok) throw new Error('Failed to get OTA status');
    return response.json();
  },

  async uploadFirmware(file, onProgress) {
    return new Promise((resolve, reject) => {
      const xhr  = new XMLHttpRequest();
      const form = new FormData();
      form.append('firmware', file, file.name);

      xhr.upload.onprogress = (e) => {
        if (e.lengthComputable && onProgress) {
          onProgress(Math.round((e.loaded / e.total) * 100));
        }
      };

      xhr.onload = () => {
        try { resolve(JSON.parse(xhr.responseText)); }
        catch { resolve({ success: xhr.status < 300 }); }
      };

      xhr.onerror = () => reject(new Error('Upload failed'));
      xhr.open('POST', `${API_BASE}/ota/upload`);
      xhr.send(form);
    });
  },

  // ── LittleFS Storage ──────────────────────────────────────────
  async getStorageInfo() {
    const response = await fetch(`${API_BASE}/storage/info`);
    if (!response.ok) throw new Error('Failed to get storage info');
    return response.json();
  },

  async listStorageFiles() {
    const response = await fetch(`${API_BASE}/storage/files`);
    if (!response.ok) throw new Error('Failed to list storage files');
    return response.json();
  },

  async deleteStorageFile(path) {
    const url = `${API_BASE}/storage/delete?path=${encodeURIComponent(path)}`;
    const response = await fetch(url, { method: 'DELETE' });
    if (!response.ok) throw new Error(`Failed to delete ${path}`);
    return response.json();
  },

  async formatStorage() {
    const response = await fetch(`${API_BASE}/storage/format`, { method: 'POST' });
    if (!response.ok) throw new Error('Failed to format storage');
    return response.json();
  },

  // ── SD Card ───────────────────────────────────────────────────

  /**
   * GET /api/sd/status
   * Returns SD card mount status and storage statistics.
   * @returns {Promise<{enabled, mounted, card_type, total_mb, free_mb, used_pct}>}
   */
  async getSDStatus() {
    const response = await fetch(`${API_BASE}/sd/status`);
    if (!response.ok) throw new Error('Failed to get SD status');
    return response.json();
  },

  /**
   * GET /api/sd/files?dir=<path>
   * Returns a recursive listing of files on the SD card.
   * @param {string} dir  Root directory to list (default "/")
   * @returns {Promise<{dir, count, files: Array<{path, size, isDir}>}>}
   */
  async listSDFiles(dir = '/') {
    const url = `${API_BASE}/sd/files?dir=${encodeURIComponent(dir)}`;
    const response = await fetch(url);
    if (!response.ok) throw new Error('Failed to list SD files');
    return response.json();
  },

  /**
   * GET /api/sd/download?path=<file>
   * Returns the download URL for a file.  The caller may set window.location
   * or create a temporary <a> element to trigger the browser download.
   * @param {string} path  Absolute file path on the SD card.
   * @returns {string}  URL that, when fetched, streams the file.
   */
  getSDDownloadURL(path) {
    return `${API_BASE}/sd/download?path=${encodeURIComponent(path)}`;
  },

  /**
   * DELETE /api/sd/delete?path=<file>
   * @param {string} path  Absolute file path.
   * @returns {Promise<{success, message?, error?}>}
   */
  async deleteSDFile(path) {
    const url = `${API_BASE}/sd/delete?path=${encodeURIComponent(path)}`;
    const response = await fetch(url, { method: 'DELETE' });
    if (!response.ok) throw new Error(`Failed to delete SD file: ${path}`);
    return response.json();
  },

  /**
   * POST /api/sd/mkdir  body: {path}
   * @param {string} path  Directory to create, e.g. "/logs"
   */
  async mkdirSD(path) {
    const response = await fetch(`${API_BASE}/sd/mkdir`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path })
    });
    if (!response.ok) throw new Error('Failed to create directory');
    return response.json();
  },

  /**
   * POST /api/sd/format — erase all files on the SD card.
   * WARNING: destructive.
   */
  async formatSD() {
    const response = await fetch(`${API_BASE}/sd/format`, { method: 'POST' });
    if (!response.ok) throw new Error('Failed to format SD card');
    return response.json();
  },

  /**
   * POST /api/sd/mount — (re-)mount the SD card.
   */
  async mountSD() {
    const response = await fetch(`${API_BASE}/sd/mount`, { method: 'POST' });
    if (!response.ok) throw new Error('Failed to mount SD card');
    return response.json();
  },

  /**
   * POST /api/sd/unmount — safely unmount the SD card before removal.
   */
  async unmountSD() {
    const response = await fetch(`${API_BASE}/sd/unmount`, { method: 'POST' });
    if (!response.ok) throw new Error('Failed to unmount SD card');
    return response.json();
  },

  // ── Autopilot ─────────────────────────────────────────────────
  async sendAutopilotCommand(command) {
    const response = await fetch(`${API_BASE}/autopilot/command`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command })
    });
    if (!response.ok) throw new Error('Failed to send autopilot command');
    return response.json();
  },

  async sendExtraCommand(command) {
  const response = await fetch(`${API_BASE}/seatalk/extra`, {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ command }),
  });
  if (!response.ok) throw new Error('Failed to send extra command');
  return response.json();
},

  // ── Polar / Performance ───────────────────────────────────────
  async getPolarStatus() {
    const response = await fetch(`${API_BASE}/polar/status`);
    if (!response.ok) throw new Error('Failed to get polar status');
    return response.json();
  },

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
        try { resolve(JSON.parse(xhr.responseText)); }
        catch { resolve({ success: xhr.status < 300 }); }
      };

      xhr.onerror = () => reject(new Error('Upload failed'));
      xhr.open('POST', `${API_BASE}/polar/upload`);
      xhr.send(form);
    });
  },

  async getPerformance() {
    const response = await fetch(`${API_BASE}/boat/performance`);
    if (!response.ok) throw new Error('Failed to get performance data');
    return response.json();
  },

  // ── Boat data ─────────────────────────────────────────────────
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
