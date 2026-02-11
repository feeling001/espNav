import React, { useState, useEffect } from 'react';
import { api } from '../services/api';

const WiFiConfig = () => {
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
    <div className="wifi-config">
      <h2>WiFi Configuration</h2>
      
      {message && (
        <div className={`message message-${message.type}`}>
          {message.text}
        </div>
      )}
      
      {/* Mode Selection */}
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
      
      {/* STA Mode Configuration */}
      {config.mode === 0 && (
        <div className="sta-config">
          <h3>Connect to WiFi Network</h3>
          
          <div className="form-group">
            <button 
              onClick={startScan} 
              disabled={scanning}
              className="btn btn-secondary"
            >
              {scanning ? 'Scanning...' : '🔍 Scan Networks'}
            </button>
          </div>
          
          {scanning && (
            <div className="scanning-indicator">
              <div className="spinner"></div>
              <p>Scanning for networks...</p>
            </div>
          )}
          
          {networks.length > 0 && (
            <div className="network-list">
              <h4>Available Networks ({networks.length})</h4>
              <div className="networks">
                {networks.map((network, index) => (
                  <div 
                    key={index}
                    className={`network-item ${selectedNetwork?.ssid === network.ssid ? 'selected' : ''}`}
                    onClick={() => selectNetwork(network)}
                  >
                    <div className="network-info">
                      <span className="network-ssid">{network.ssid}</span>
                      <div className="network-details">
                        <span className={`signal ${getSignalClass(network.quality)}`}>
                          {getSignalIcon(network.quality)} {network.quality}%
                        </span>
                        <span className="encryption">
                          {network.encryption_type === 'Open' ? '🔓' : '🔒'} {network.encryption_type}
                        </span>
                        <span className="channel">Ch {network.channel}</span>
                      </div>
                    </div>
                    <div className="rssi">{network.rssi} dBm</div>
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
      
      {/* AP Mode Configuration */}
      {config.mode === 1 && (
        <div className="ap-config">
          <h3>Access Point Settings</h3>
          
          <div className="form-group">
            <label>AP SSID (Network Name)</label>
            <input
              type="text"
              value={config.ap_ssid}
              onChange={(e) => setConfig({...config, ap_ssid: e.target.value})}
              placeholder="Leave empty for auto-generated (MarineGateway-XXXXXX)"
            />
            <small>Leave empty to use default: MarineGateway-XXXXXX</small>
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
            <small>Minimum 8 characters. Leave empty to use default password.</small>
          </div>
        </div>
      )}
      
      <div className="form-actions">
        <button 
          onClick={handleSave} 
          disabled={saving}
          className="btn btn-primary"
        >
          {saving ? 'Saving...' : 'Save Configuration'}
        </button>
      </div>
      
      <style jsx>{`
        .wifi-config {
          max-width: 800px;
          margin: 0 auto;
          padding: 20px;
        }
        
        h2 {
          margin-bottom: 20px;
          color: #333;
        }
        
        h3 {
          margin: 20px 0 15px;
          color: #555;
          font-size: 1.2em;
        }
        
        h4 {
          margin: 15px 0 10px;
          color: #666;
          font-size: 1em;
        }
        
        .message {
          padding: 12px 16px;
          border-radius: 4px;
          margin-bottom: 20px;
        }
        
        .message-success {
          background-color: #d4edda;
          color: #155724;
          border: 1px solid #c3e6cb;
        }
        
        .message-error {
          background-color: #f8d7da;
          color: #721c24;
          border: 1px solid #f5c6cb;
        }
        
        .form-group {
          margin-bottom: 20px;
        }
        
        label {
          display: block;
          margin-bottom: 5px;
          font-weight: 500;
          color: #555;
        }
        
        input, select {
          width: 100%;
          padding: 10px;
          border: 1px solid #ddd;
          border-radius: 4px;
          font-size: 14px;
        }
        
        input:focus, select:focus {
          outline: none;
          border-color: #007bff;
        }
        
        small {
          display: block;
          margin-top: 5px;
          color: #666;
          font-size: 12px;
        }
        
        .btn {
          padding: 10px 20px;
          border: none;
          border-radius: 4px;
          font-size: 14px;
          cursor: pointer;
          transition: background-color 0.2s;
        }
        
        .btn-primary {
          background-color: #007bff;
          color: white;
        }
        
        .btn-primary:hover:not(:disabled) {
          background-color: #0056b3;
        }
        
        .btn-secondary {
          background-color: #6c757d;
          color: white;
        }
        
        .btn-secondary:hover:not(:disabled) {
          background-color: #545b62;
        }
        
        .btn:disabled {
          opacity: 0.6;
          cursor: not-allowed;
        }
        
        .scanning-indicator {
          text-align: center;
          padding: 20px;
        }
        
        .spinner {
          border: 3px solid #f3f3f3;
          border-top: 3px solid #007bff;
          border-radius: 50%;
          width: 40px;
          height: 40px;
          animation: spin 1s linear infinite;
          margin: 0 auto 10px;
        }
        
        @keyframes spin {
          0% { transform: rotate(0deg); }
          100% { transform: rotate(360deg); }
        }
        
        .network-list {
          margin: 20px 0;
        }
        
        .networks {
          border: 1px solid #ddd;
          border-radius: 4px;
          max-height: 400px;
          overflow-y: auto;
        }
        
        .network-item {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 12px 16px;
          border-bottom: 1px solid #eee;
          cursor: pointer;
          transition: background-color 0.2s;
        }
        
        .network-item:last-child {
          border-bottom: none;
        }
        
        .network-item:hover {
          background-color: #f8f9fa;
        }
        
        .network-item.selected {
          background-color: #e7f3ff;
          border-left: 3px solid #007bff;
        }
        
        .network-info {
          flex: 1;
        }
        
        .network-ssid {
          display: block;
          font-weight: 500;
          margin-bottom: 5px;
          color: #333;
        }
        
        .network-details {
          display: flex;
          gap: 15px;
          font-size: 12px;
          color: #666;
        }
        
        .signal {
          font-weight: 500;
        }
        
        .signal-excellent {
          color: #28a745;
        }
        
        .signal-good {
          color: #5cb85c;
        }
        
        .signal-fair {
          color: #ffc107;
        }
        
        .signal-poor {
          color: #dc3545;
        }
        
        .rssi {
          color: #999;
          font-size: 12px;
        }
        
        .form-actions {
          margin-top: 30px;
          padding-top: 20px;
          border-top: 1px solid #eee;
        }
      `}</style>
    </div>
  );
};

export default WiFiConfig;
