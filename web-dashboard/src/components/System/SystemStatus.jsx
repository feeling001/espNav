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

  // Colour for queue load bar
  const getQueueColor = (pct) => {
    if (pct < 50) return '#27ae60'; // green
    if (pct < 80) return '#f39c12'; // orange
    return '#e74c3c';               // red
  };

  // Colour for NMEA buffer overflow indicator
  const getBufferColor = (hasOverflow) => {
    return hasOverflow ? '#e74c3c' : '#27ae60';
  };

  if (loading || !status) {
    return <div className="page">Loading...</div>;
  }

  const queueLoadPct  = status.nmea_buffer?.queue_load_pct ?? 0;
  const queueWaiting  = status.nmea_buffer?.queue_waiting  ?? 0;
  const queueSize     = status.nmea_buffer?.queue_size     ?? 0;

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

        {/* Firmware version */}
        <div className="status-card">
          <h3>Firmware</h3>
          <div className="value" style={{ fontSize: '18px', fontFamily: 'monospace' }}>
            {status.version ?? '—'}
          </div>
          <div className="label">version</div>
        </div>

        {/* Uptime */}
        <div className="status-card">
          <h3>Uptime</h3>
          <div className="value">{formatUptime(status.uptime)}</div>
        </div>

        {/* Free Heap */}
        <div className="status-card">
          <h3>Free Heap</h3>
          <div className="value">{formatBytes(status.heap.free)}</div>
          <div className="label">of {formatBytes(status.heap.total)}</div>
        </div>

        {/* Min Free Heap */}
        <div className="status-card">
          <h3>Min Free Heap</h3>
          <div className="value">{formatBytes(status.heap.min_free)}</div>
        </div>

        {/* Queue Load — replaces the unreliable CPU Load card */}
        <div className="status-card" style={{ borderLeftColor: getQueueColor(queueLoadPct) }}>
          <h3>Queue Load</h3>
          <div className="value" style={{ color: getQueueColor(queueLoadPct) }}>
            {queueLoadPct}%
          </div>
          <div className="label">
            {queueWaiting} / {queueSize} messages pending
          </div>
          <div className="label" style={{ marginTop: '6px' }}>
            {/* Mini progress bar */}
            <div style={{
              background: '#e0e0e0',
              borderRadius: '3px',
              height: '6px',
              width: '100%',
              overflow: 'hidden',
            }}>
              <div style={{
                background: getQueueColor(queueLoadPct),
                height: '100%',
                width: `${queueLoadPct}%`,
                borderRadius: '3px',
                transition: 'width 0.4s ease',
              }} />
            </div>
          </div>
        </div>

        {/* NMEA Buffer overflow status */}
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

        {/* WiFi Mode */}
        <div className="status-card">
          <h3>WiFi Mode</h3>
          <div className="value">{status.wifi.mode}</div>
          <div className="label">SSID: {status.wifi.ssid || 'N/A'}</div>
        </div>

        {/* IP Address */}
        <div className="status-card">
          <h3>IP Address</h3>
          <div className="value" style={{ fontSize: '18px' }}>{status.wifi.ip}</div>
          {status.wifi.rssi !== 0 && (
            <div className="label">Signal: {status.wifi.rssi} dBm</div>
          )}
        </div>

        {/* TCP Clients */}
        <div className="status-card">
          <h3>TCP Clients</h3>
          <div className="value">{status.tcp.clients}</div>
          <div className="label">Port: {status.tcp.port}</div>
        </div>

        {/* NMEA Sentences */}
        <div className="status-card">
          <h3>NMEA Sentences</h3>
          <div className="value">{status.uart.sentences_received.toLocaleString()}</div>
          <div className="label">Errors: {status.uart.errors}</div>
        </div>

        {/* UART Config */}
        <div className="status-card">
          <h3>UART Config</h3>
          <div className="value">{status.uart.baud}</div>
          <div className="label">baud</div>
        </div>

        {/* Bluetooth BLE */}
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
