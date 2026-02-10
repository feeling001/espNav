import { useState, useEffect } from 'react';
import { api } from '../../services/api';

export function WiFiConfig() {
  const [config, setConfig] = useState({ ssid: '', password: '', mode: 0 });
  const [message, setMessage] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadConfig();
  }, []);

  const loadConfig = async () => {
    try {
      const data = await api.getWiFiConfig();
      setConfig({ ...data, password: '' }); // Don't show password
      setLoading(false);
    } catch (error) {
      setMessage({ type: 'error', text: 'Failed to load WiFi config' });
      setLoading(false);
    }
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    setMessage(null);
    
    try {
      const result = await api.setWiFiConfig(config);
      setMessage({ type: 'success', text: result.message });
    } catch (error) {
      setMessage({ type: 'error', text: 'Failed to save WiFi config' });
    }
  };

  if (loading) return <div className="page">Loading...</div>;

  return (
    <div className="page">
      <h2>WiFi Configuration</h2>
      
      {message && (
        <div className={`message ${message.type}`}>
          {message.text}
        </div>
      )}

      <form onSubmit={handleSubmit}>
        <div className="form-group">
          <label>Mode</label>
          <select 
            value={config.mode} 
            onChange={(e) => setConfig({ ...config, mode: parseInt(e.target.value) })}
          >
            <option value={0}>Station (Connect to WiFi)</option>
            <option value={1}>Access Point</option>
          </select>
        </div>

        {config.mode === 0 && (
          <>
            <div className="form-group">
              <label>SSID</label>
              <input 
                type="text" 
                value={config.ssid}
                onChange={(e) => setConfig({ ...config, ssid: e.target.value })}
                placeholder="Network name"
                required
              />
            </div>

            <div className="form-group">
              <label>Password</label>
              <input 
                type="password" 
                value={config.password}
                onChange={(e) => setConfig({ ...config, password: e.target.value })}
                placeholder="Enter new password or leave blank"
              />
            </div>
          </>
        )}

        <button type="submit">Save Configuration</button>
      </form>

      <div style={{ marginTop: '20px', padding: '15px', background: '#fff3cd', borderRadius: '4px' }}>
        <strong>Note:</strong> You will need to restart the ESP32 for changes to take effect.
      </div>
    </div>
  );
}
