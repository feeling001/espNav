import { useState, useEffect } from 'react';
import { api } from '../../services/api';

const BAUD_RATES = [4800, 9600, 19200, 38400, 57600, 115200];

export function SerialConfig() {
  const [config, setConfig] = useState({
    baudRate: 38400,
    dataBits: 8,
    parity: 0,
    stopBits: 1
  });
  const [message, setMessage] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadConfig();
  }, []);

  const loadConfig = async () => {
    try {
      const data = await api.getSerialConfig();
      setConfig(data);
      setLoading(false);
    } catch (error) {
      setMessage({ type: 'error', text: 'Failed to load serial config' });
      setLoading(false);
    }
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    setMessage(null);
    
    try {
      const result = await api.setSerialConfig(config);
      setMessage({ type: 'success', text: result.message });
    } catch (error) {
      setMessage({ type: 'error', text: 'Failed to save serial config' });
    }
  };

  if (loading) return <div className="page">Loading...</div>;

  return (
    <div className="page">
      <h2>Serial Port Configuration</h2>
      
      {message && (
        <div className={`message ${message.type}`}>
          {message.text}
        </div>
      )}

      <form onSubmit={handleSubmit}>
        <div className="form-group">
          <label>Baud Rate</label>
          <select 
            value={config.baudRate}
            onChange={(e) => setConfig({ ...config, baudRate: parseInt(e.target.value) })}
          >
            {BAUD_RATES.map(rate => (
              <option key={rate} value={rate}>{rate}</option>
            ))}
          </select>
        </div>

        <div className="form-group">
          <label>Data Bits</label>
          <select 
            value={config.dataBits}
            onChange={(e) => setConfig({ ...config, dataBits: parseInt(e.target.value) })}
          >
            <option value={5}>5</option>
            <option value={6}>6</option>
            <option value={7}>7</option>
            <option value={8}>8</option>
          </select>
        </div>

        <div className="form-group">
          <label>Parity</label>
          <select 
            value={config.parity}
            onChange={(e) => setConfig({ ...config, parity: parseInt(e.target.value) })}
          >
            <option value={0}>None</option>
            <option value={1}>Even</option>
            <option value={2}>Odd</option>
          </select>
        </div>

        <div className="form-group">
          <label>Stop Bits</label>
          <select 
            value={config.stopBits}
            onChange={(e) => setConfig({ ...config, stopBits: parseInt(e.target.value) })}
          >
            <option value={1}>1</option>
            <option value={2}>2</option>
          </select>
        </div>

        <button type="submit">Save Configuration</button>
      </form>

      <div style={{ marginTop: '20px', padding: '15px', background: '#fff3cd', borderRadius: '4px' }}>
        <strong>Note:</strong> You will need to restart the ESP32 for changes to take effect.
      </div>
    </div>
  );
}
