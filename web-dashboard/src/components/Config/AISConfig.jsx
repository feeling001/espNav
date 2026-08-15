import { useState, useEffect } from 'react';
import { api } from '../../services/api';

// Preset retention times (seconds). AIS transmitters do not report
// continuously — reporting intervals vary from ~2 s (fast-moving Class A)
// up to several minutes (slow/anchored vessels, Class B, ATON, SAR aircraft).
// See: https://comarsystems.com/support-hub/what-are-ais-reporting-intervals/
const PRESETS = [
  { value: 60,   label: '1 minute' },
  { value: 120,  label: '2 minutes' },
  { value: 180,  label: '3 minutes' },
  { value: 300,  label: '5 minutes (default)' },
  { value: 600,  label: '10 minutes' },
  { value: 900,  label: '15 minutes' },
  { value: 1800, label: '30 minutes' },
];

const MIN_TIMEOUT = 10;
const MAX_TIMEOUT = 3600;

export function AISConfig() {
  const [timeoutS, setTimeoutS] = useState(300);
  const [isCustom, setIsCustom] = useState(false);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving]   = useState(false);
  const [message, setMessage] = useState(null);

  useEffect(() => {
    load();
  }, []);

  const load = async () => {
    setLoading(true);
    try {
      const data = await api.getAISConfig();
      const value = data.target_timeout_s ?? 300;
      setTimeoutS(value);
      setIsCustom(!PRESETS.some(p => p.value === value));
    } catch {
      setMessage({ type: 'error', text: 'Failed to load AIS config' });
    } finally {
      setLoading(false);
    }
  };

  const handlePresetChange = (e) => {
    const value = e.target.value;
    if (value === 'custom') {
      setIsCustom(true);
      return;
    }
    setIsCustom(false);
    setTimeoutS(parseInt(value, 10));
  };

  const handleSave = async () => {
    setSaving(true);
    setMessage(null);
    try {
      const clamped = Math.min(MAX_TIMEOUT, Math.max(MIN_TIMEOUT, parseInt(timeoutS, 10) || 300));
      const result = await api.setAISConfig({ target_timeout_s: clamped });
      setTimeoutS(result.target_timeout_s ?? clamped);
      setMessage({ type: 'success', text: 'AIS config saved' });
    } catch {
      setMessage({ type: 'error', text: 'Failed to save AIS config' });
    } finally {
      setSaving(false);
    }
  };

  if (loading) return <div className="page">Loading...</div>;

  return (
    <div className="page">
      <h2>AIS Target Retention</h2>

      <p style={{ margin: '0 0 16px', color: '#94a3b8', fontSize: '14px', maxWidth: '640px' }}>
        AIS transponders do not transmit continuously: reporting intervals
        range from about 2 seconds for a fast-moving Class&nbsp;A vessel up to
        several minutes for a slow or anchored vessel, a Class&nbsp;B unit, or
        an aid to navigation. Choose how long a received AIS target ("echo")
        is kept on the map and in the target list after its last report
        before it is dropped as stale.
      </p>

      {message && (
        <div className={`message ${message.type}`}>
          {message.text}
        </div>
      )}

      <div className="form-group" style={{ maxWidth: '320px' }}>
        <label>Target retention time</label>
        <select
          value={isCustom ? 'custom' : timeoutS}
          onChange={handlePresetChange}
        >
          {PRESETS.map(p => (
            <option key={p.value} value={p.value}>{p.label}</option>
          ))}
          <option value="custom">Custom…</option>
        </select>
      </div>

      {isCustom && (
        <div className="form-group" style={{ maxWidth: '320px' }}>
          <label>Custom retention time (seconds)</label>
          <input
            type="number"
            min={MIN_TIMEOUT}
            max={MAX_TIMEOUT}
            value={timeoutS}
            onChange={(e) => setTimeoutS(e.target.value)}
          />
          <p style={{ margin: '4px 0 0', color: '#64748b', fontSize: '12px' }}>
            Allowed range: {MIN_TIMEOUT}–{MAX_TIMEOUT} seconds.
          </p>
        </div>
      )}

      <button onClick={handleSave} disabled={saving} style={{ marginTop: '20px' }}>
        {saving ? 'Saving...' : 'Save Configuration'}
      </button>
    </div>
  );
}
