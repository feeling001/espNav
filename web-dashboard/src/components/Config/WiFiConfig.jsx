import { useState, useEffect } from 'react';
import { api } from '../../services/api';

export function WiFiConfig() {
  const [config, setConfig] = useState({
    mode: 0,
    ssid: '',
    password: '',
    ap_ssid: '',
    ap_password: ''
  });
  
  const [scanning, setScanning] = useState(false);
  const [networks, setNetworks] = useState([]);
  const [selectedNetwork, setSelectedNetwork] = useState(null);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState(null);

  useEffect(() => {
    loadConfig();
  }, []);

  const loadConfig = async () => {
    try {
      const data = await api.getWiFiConfig();
      setConfig({
        mode: data.mode || 0,
        ssid: data.ssid || '',
        password: '',
        ap_ssid: data.ap_ssid || '',
        ap_password: ''
      });
    } catch (error) {
      console.error('Failed to load WiFi config:', error);
      setMessage({ type: 'error', text: 'Failed to load configuration' });
    }
  };

  const startScan = async () => {
    try {
      setScanning(true);
      setNetworks([]);
      await api.startWiFiScan();
      
      // Poll for results
      const pollInterval = setInterval(async () => {
        try {
          const results = await api.getWiFiScanResults();
          
          if (!results.scanning) {
            clearInterval(pollInterval);
            setScanning(false);
            setNetworks(results.networks || []);
          }
        } catch (error) {
          clearInterval(pollInterval);
          setScanning(false);
          console.error('Failed to get scan results:', error);
        }
      }, 1000);
      
      // Timeout after 15 seconds
      setTimeout(() => {
        clearInterval(pollInterval);
        setScanning(false);
      }, 15000);
      
    } catch (error) {
      setScanning(false);
      console.error('Failed to start scan:', error);
      setMessage({ type: 'error', text: 'Failed to start WiFi scan' });
    }
  };

  const selectNetwork = (network) => {
    setSelectedNetwork(network);
    setConfig({
      ...config,
      ssid: network.ssid,
      password: ''
    });
  };

  const handleSave = async () => {
    try {
      setSaving(true);
      setMessage(null);
      
      // Validation
      if (config.mode === 0 && !config.ssid) {
        setMessage({ type: 'error', text: 'Please select a network or enter SSID' });
        setSaving(false);
        return;
      }
      
      if (config.mode === 1) {
        if (config.ap_password && config.ap_password.length < 8) {
          setMessage({ type: 'error', text: 'AP password must be at least 8 characters' });
          setSaving(false);
          return;
        }
      }
      
      await api.setWiFiConfig(config);
      setMessage({ type: 'success', text: 'Configuration saved. Please restart the device to apply changes.' });
      
    } catch (error) {
      console.error('Failed to save config:', error);
      setMessage({ type: 'error', text: 'Failed to save configuration' });
    } finally {
      setSaving(false);
    }
  };

  const getSignalIcon = (quality) => {
    if (quality >= 75) return '📶';
    if (quality >= 50) return '📶';
    if (quality >= 25) return '📶';
    return '📶';
  };

  const getSignalClass = (quality) => {
    if (quality >= 75) return 'signal-excellent';
    if (quality >= 50) return 'signal-good';
    if (quality >= 25) return 'signal-fair';
    return 'signal-poor';
  };

  return (
    <div className="page">
      <h2>WiFi Configuration</h2>
      
      {message && (
        <div className={`message ${message.type}`}>
          {message.text}
        </div>
      )}
      
      <div className="form-group">
        <label>WiFi Mode</label>
        <select 
          value={config.mode} 
          onChange={(e) => setConfig({...config, mode: parseInt(e.target.value)})}
        >
          <option value={0}>Infrastructure (STA) - Connect to existing network</option>
          <option value={1}>Access Point (AP) - Create network</option>
        </select>
      </div>
      
      {config.mode === 0 && (
        <div>
          <h3>Connect to WiFi Network</h3>
          
          <div className="form-group">
            <button 
              onClick={startScan} 
              disabled={scanning}
              className="secondary"
            >
              {scanning ? 'Scanning...' : '🔍 Scan Networks'}
            </button>
          </div>
          
          {scanning && (
            <div style={{ textAlign: 'center', padding: '20px' }}>
              <p>Scanning for networks...</p>
            </div>
          )}
          
          {networks.length > 0 && (
            <div style={{ margin: '20px 0' }}>
              <h4>Available Networks ({networks.length})</h4>
              <div style={{ border: '1px solid #ddd', borderRadius: '4px', maxHeight: '400px', overflowY: 'auto' }}>
                {networks.map((network, index) => (
                  <div 
                    key={index}
                    onClick={() => selectNetwork(network)}
                    style={{
                      padding: '12px 16px',
                      borderBottom: index < networks.length - 1 ? '1px solid #eee' : 'none',
                      cursor: 'pointer',
                      backgroundColor: selectedNetwork?.ssid === network.ssid ? '#e7f3ff' : 'transparent',
                      borderLeft: selectedNetwork?.ssid === network.ssid ? '3px solid #3498db' : 'none'
                    }}
                  >
                    <div>
                      <strong>{network.ssid}</strong>
                      <div style={{ fontSize: '12px', color: '#666', marginTop: '4px' }}>
                        <span className={getSignalClass(network.quality)}>
                          {getSignalIcon(network.quality)} {network.quality}%
                        </span>
                        <span style={{ marginLeft: '10px' }}>
                          {network.encryption_type === 'Open' ? '🔓' : '🔒'} {network.encryption_type}
                        </span>
                        <span style={{ marginLeft: '10px' }}>Ch {network.channel}</span>
                        <span style={{ marginLeft: '10px', color: '#999' }}>{network.rssi} dBm</span>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}
          
          <div className="form-group">
            <label>Network SSID</label>
            <input
              type="text"
              value={config.ssid}
              onChange={(e) => setConfig({...config, ssid: e.target.value})}
              placeholder="Enter network name"
            />
          </div>
          
          <div className="form-group">
            <label>Password</label>
            <input
              type="password"
              value={config.password}
              onChange={(e) => setConfig({...config, password: e.target.value})}
              placeholder="Enter password"
            />
          </div>
        </div>
      )}
      
      {config.mode === 1 && (
        <div>
          <h3>Access Point Settings</h3>
          
          <div className="form-group">
            <label>AP SSID (Network Name)</label>
            <input
              type="text"
              value={config.ap_ssid}
              onChange={(e) => setConfig({...config, ap_ssid: e.target.value})}
              placeholder="Leave empty for auto-generated (MarineGateway-XXXXXX)"
            />
            <small style={{ display: 'block', marginTop: '5px', color: '#666', fontSize: '12px' }}>
              Leave empty to use default: MarineGateway-XXXXXX
            </small>
          </div>
          
          <div className="form-group">
            <label>AP Password</label>
            <input
              type="password"
              value={config.ap_password}
              onChange={(e) => setConfig({...config, ap_password: e.target.value})}
              placeholder="Minimum 8 characters"
              minLength={8}
            />
            <small style={{ display: 'block', marginTop: '5px', color: '#666', fontSize: '12px' }}>
              Minimum 8 characters. Leave empty to use default password.
            </small>
          </div>
        </div>
      )}
      
      <div style={{ marginTop: '30px', paddingTop: '20px', borderTop: '1px solid #eee' }}>
        <button 
          onClick={handleSave} 
          disabled={saving}
        >
          {saving ? 'Saving...' : 'Save Configuration'}
        </button>
      </div>
    </div>
  );
}
