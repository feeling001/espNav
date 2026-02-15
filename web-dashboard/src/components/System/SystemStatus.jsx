import { useState, useEffect } from 'react';
import { api } from '../../services/api';

export function SystemStatus() {
  const [status, setStatus] = useState(null);
  const [loading, setLoading] = useState(true);
  const [autoRefresh, setAutoRefresh] = useState(true);

  useEffect(() => {
    loadStatus();
    
    const interval = setInterval(() => {
      if (autoRefresh) {
        loadStatus();
      }
    }, 5000);

    return () => clearInterval(interval);
  }, [autoRefresh]);

  const loadStatus = async () => {
    try {
      const data = await api.getStatus();
      setStatus(data);
      setLoading(false);
    } catch (error) {
      console.error('Failed to load status:', error);
    }
  };

  const handleRestart = async () => {
    if (!confirm('Are you sure you want to restart the ESP32?')) return;
    
    try {
      await api.restart();
      alert('ESP32 is restarting. Please wait 30 seconds and refresh this page.');
    } catch (error) {
      alert('Failed to restart ESP32');
    }
  };

  const formatUptime = (seconds) => {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const mins = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    
    if (days > 0) return `${days}d ${hours}h ${mins}m`;
    if (hours > 0) return `${hours}h ${mins}m ${secs}s`;
    if (mins > 0) return `${mins}m ${secs}s`;
    return `${secs}s`;
  };

  const formatBytes = (bytes) => {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(1) + ' MB';
  };

  // Fonction pour déterminer la couleur du CPU selon la charge
  const getCpuColor = (usage) => {
    if (usage < 50) return '#27ae60'; // Vert
    if (usage < 75) return '#f39c12'; // Orange
    return '#e74c3c'; // Rouge
  };

  // Fonction pour déterminer la couleur du buffer NMEA
  const getBufferColor = (hasOverflow) => {
    return hasOverflow ? '#e74c3c' : '#27ae60';
  };

  if (loading || !status) {
    return <div className="page">Loading...</div>;
  }

  return (
    <div className="page">
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '20px' }}>
        <h2>System Status</h2>
        <div>
          <label style={{ marginRight: '10px' }}>
            <input 
              type="checkbox" 
              checked={autoRefresh}
              onChange={(e) => setAutoRefresh(e.target.checked)}
            />
            {' '}Auto-refresh (5s)
          </label>
          <button onClick={handleRestart} className="danger">
            Restart ESP32
          </button>
        </div>
      </div>

      <div className="status-grid">
        <div className="status-card">
          <h3>Uptime</h3>
          <div className="value">{formatUptime(status.uptime)}</div>
        </div>

        <div className="status-card">
          <h3>Free Heap</h3>
          <div className="value">{formatBytes(status.heap.free)}</div>
          <div className="label">of {formatBytes(status.heap.total)}</div>
        </div>

        <div className="status-card">
          <h3>Min Free Heap</h3>
          <div className="value">{formatBytes(status.heap.min_free)}</div>
        </div>

        {/* NOUVEAU: CPU Load */}
        <div className="status-card" style={{ borderLeftColor: getCpuColor(status.cpu?.usage_percent || 0) }}>
          <h3>CPU Load</h3>
          <div className="value" style={{ color: getCpuColor(status.cpu?.usage_percent || 0) }}>
            {status.cpu?.usage_percent >= 0 ? `${status.cpu.usage_percent}%` : 'N/A'}
          </div>
          <div className="label">
            {status.cpu?.usage_percent < 50 && '✓ Normal'}
            {status.cpu?.usage_percent >= 50 && status.cpu?.usage_percent < 75 && '⚠️ Moderate'}
            {status.cpu?.usage_percent >= 75 && '⚠️ High'}
          </div>
        </div>

        {/* NOUVEAU: NMEA Buffer Status */}
        <div className="status-card" style={{ borderLeftColor: getBufferColor(status.nmea_buffer?.has_overflow) }}>
          <h3>NMEA Buffer</h3>
          <div className="value" style={{ color: getBufferColor(status.nmea_buffer?.has_overflow) }}>
            {status.nmea_buffer?.has_overflow ? '⚠️ Overflow' : '✓ OK'}
          </div>
          <div className="label">
            Recent: {status.nmea_buffer?.full_events_recent || 0} events
          </div>
          <div className="label" style={{ fontSize: '10px', marginTop: '3px' }}>
            Total overflows: {status.nmea_buffer?.overflow_total || 0}
          </div>
        </div>

        <div className="status-card">
          <h3>WiFi Mode</h3>
          <div className="value">{status.wifi.mode}</div>
          <div className="label">SSID: {status.wifi.ssid || 'N/A'}</div>
        </div>

        <div className="status-card">
          <h3>IP Address</h3>
          <div className="value" style={{ fontSize: '18px' }}>{status.wifi.ip}</div>
          {status.wifi.rssi !== 0 && (
            <div className="label">Signal: {status.wifi.rssi} dBm</div>
          )}
        </div>

        <div className="status-card">
          <h3>TCP Clients</h3>
          <div className="value">{status.tcp.clients}</div>
          <div className="label">Port: {status.tcp.port}</div>
        </div>

        <div className="status-card">
          <h3>NMEA Sentences</h3>
          <div className="value">{status.uart.sentences_received.toLocaleString()}</div>
          <div className="label">Errors: {status.uart.errors}</div>
        </div>

        <div className="status-card">
          <h3>UART Config</h3>
          <div className="value">{status.uart.baud}</div>
          <div className="label">baud</div>
        </div>

        <div className="status-card">
          <h3>Bluetooth BLE</h3>
          <div className="value">{status.ble.enabled ? '🟢 Enabled' : '⚫ Disabled'}</div>
          <div className="label">Advertising: {status.ble.advertising ? 'Yes' : 'No'}</div>
          <div className="label">Clients: {status.ble.connected_devices || 0}</div>
          <div className="label">Device: {status.ble.device_name || 'N/A'}</div>
        </div>
      </div>
    </div>
  );
}