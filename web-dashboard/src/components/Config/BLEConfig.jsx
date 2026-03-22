import { useState, useEffect } from 'react';
import { api } from '../../services/api';

export function BLEConfig() {
  const [config, setConfig] = useState({
    enabled: false,
    device_name: 'MarineGateway',
    pin_code: '123456'
  });
  const [message, setMessage] = useState(null);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);

  useEffect(() => {
    loadConfig();
  }, []);

  const loadConfig = async () => {
    try {
      const data = await api.getBLEConfig();
      setConfig({
        enabled: data.enabled || false,
        device_name: data.device_name || 'MarineGateway',
        pin_code: data.pin_code || '123456'
      });
      setLoading(false);
    } catch (error) {
      console.error('Failed to load BLE config:', error);
      setMessage({ type: 'error', text: 'Failed to load Bluetooth configuration' });
      setLoading(false);
    }
  };

  const validatePinCode = (pin) => {
    return /^\d{6}$/.test(pin);
  };

  const validateDeviceName = (name) => {
    return name.length > 0 && name.length <= 31;
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    setMessage(null);

    if (!validateDeviceName(config.device_name)) {
      setMessage({ type: 'error', text: 'Device name must be between 1 and 31 characters' });
      return;
    }

    if (!validatePinCode(config.pin_code)) {
      setMessage({ type: 'error', text: 'PIN code must be exactly 6 digits' });
      return;
    }

    try {
      setSaving(true);
      const result = await api.setBLEConfig(config);
      setMessage({ 
        type: 'success', 
        text: result.message || 'Configuration saved. Please restart the device to apply changes.' 
      });
    } catch (error) {
      console.error('Failed to save BLE config:', error);
      setMessage({ type: 'error', text: 'Failed to save Bluetooth configuration' });
    } finally {
      setSaving(false);
    }
  };

  if (loading) {
    return <div className="page">Loading...</div>;
  }

  return (
    <div className="page">
      <h2>Bluetooth Configuration</h2>

      <div style={{ marginBottom: '20px', padding: '15px', background: '#e7f3ff', borderRadius: '4px', borderLeft: '4px solid #3498db' }}>
        <strong>ℹ️ About Bluetooth BLE</strong>
        <p style={{ marginTop: '8px', marginBottom: '0', fontSize: '14px', lineHeight: '1.5' }}>
          Bluetooth Low Energy allows wireless connection to marine apps and chart plotters. 
          When enabled, your device will broadcast navigation data (GPS, wind, depth, etc.) 
          that can be received by compatible applications on smartphones and tablets.
        </p>
      </div>

      {message && (
        <div className={`message ${message.type}`}>
          {message.text}
        </div>
      )}

      <form onSubmit={handleSubmit}>
        <div className="form-group">
          <label style={{ display: 'flex', alignItems: 'center', cursor: 'pointer' }}>
            <input
              type="checkbox"
              checked={config.enabled}
              onChange={(e) => setConfig({ ...config, enabled: e.target.checked })}
              style={{ marginRight: '10px', width: 'auto' }}
            />
            <span style={{ fontWeight: 500 }}>Enable Bluetooth BLE</span>
          </label>
          <small style={{ display: 'block', marginTop: '5px', color: '#666', fontSize: '12px' }}>
            Turn on to allow Bluetooth connections from marine apps
          </small>
        </div>

        {config.enabled && (
          <>
            <div className="form-group">
              <label>Device Name</label>
              <input
                type="text"
                value={config.device_name}
                onChange={(e) => setConfig({ ...config, device_name: e.target.value })}
                placeholder="MarineGateway"
                maxLength={31}
                required
              />
              <small style={{ display: 'block', marginTop: '5px', color: '#666', fontSize: '12px' }}>
                This name will appear when scanning for Bluetooth devices (max 31 characters)
              </small>
            </div>

            <div className="form-group">
              <label>PIN Code (6 digits)</label>
              <input
                type="text"
                value={config.pin_code}
                onChange={(e) => {
                  const value = e.target.value.replace(/\D/g, '').slice(0, 6);
                  setConfig({ ...config, pin_code: value });
                }}
                placeholder="123456"
                pattern="\d{6}"
                maxLength={6}
                required
              />
              <small style={{ display: 'block', marginTop: '5px', color: '#666', fontSize: '12px' }}>
                Enter a 6-digit PIN code for secure pairing. This PIN will be displayed on the device when pairing.
              </small>
            </div>

            <div style={{ marginTop: '20px', padding: '15px', background: '#fff3cd', borderRadius: '4px' }}>
              <strong>🔐 Security Note</strong>
              <p style={{ marginTop: '8px', marginBottom: '0', fontSize: '13px', lineHeight: '1.5' }}>
                The PIN code provides basic security for Bluetooth pairing. When a device attempts to connect, 
                you'll need to enter this PIN on both devices to establish the connection.
              </p>
            </div>
          </>
        )}

        <div style={{ marginTop: '30px', paddingTop: '20px', borderTop: '1px solid #eee' }}>
          <button type="submit" disabled={saving}>
            {saving ? 'Saving...' : 'Save Configuration'}
          </button>
        </div>
      </form>

      <div style={{ marginTop: '30px', padding: '15px', background: '#f8f9fa', borderRadius: '4px' }}>
        <h3 style={{ marginBottom: '10px', fontSize: '16px' }}>Bluetooth Services</h3>
        <p style={{ fontSize: '13px', lineHeight: '1.5', marginBottom: '10px' }}>
          When enabled, the following BLE services are available:
        </p>
        <ul style={{ fontSize: '13px', lineHeight: '1.8', marginLeft: '20px' }}>
          <li><strong>Navigation Service:</strong> GPS position, speed, and course data</li>
          <li><strong>Wind Service:</strong> Apparent and true wind speed/angle</li>
          <li><strong>Autopilot Service:</strong> Autopilot status and control (if available)</li>
        </ul>
      </div>

      <div style={{ marginTop: '20px', padding: '15px', background: '#fff3cd', borderRadius: '4px' }}>
        <strong>⚠️ Important:</strong> You will need to restart the ESP32 for Bluetooth changes to take effect.
      </div>
    </div>
  );
}