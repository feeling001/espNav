import { useState, useEffect, useCallback } from 'react';
import { api } from '../../services/api';

/**
 * Alarms page — configuration and acknowledge for the three onboard alarms:
 *  - Depth alarm     (below_transducer <= threshold)
 *  - AIS proximity   (target distance <= threshold, excluding own MMSI)
 *  - GPS lost        (position stale beyond timeout)
 *
 * Communicates with:
 *   GET  /api/alarms/config   — current thresholds
 *   POST /api/alarms/config   — save thresholds
 *   GET  /api/alarms/status   — polled runtime state (active/acknowledged)
 *   POST /api/alarms/ack      — global acknowledge (silences beep)
 *   POST /api/alarms/beep_on  — manual beep
 *   POST /api/alarms/beep_off — manual silence
 */

const sectionTitle = {
  fontSize: 16,
  fontWeight: 600,
  color: '#2c3e50',
  marginBottom: 14,
  paddingBottom: 8,
  borderBottom: '2px solid #3498db',
};

const helpText = { fontSize: 13, color: '#7f8c8d', marginBottom: 14, lineHeight: 1.6 };

function StatusCard({ label, value, accent }) {
  return (
    <div className="status-card" style={{ borderLeftColor: accent }}>
      <h3>{label}</h3>
      <div className="value">{value}</div>
    </div>
  );
}

function Toggle({ checked, onChange, disabled }) {
  return (
    <button
      onClick={() => !disabled && onChange(!checked)}
      disabled={disabled}
      style={{
        position: 'relative',
        width: 52,
        height: 28,
        borderRadius: 14,
        border: 'none',
        cursor: disabled ? 'not-allowed' : 'pointer',
        background: checked ? '#27ae60' : '#cbd5e1',
        transition: 'background 0.25s',
        flexShrink: 0,
        padding: 0,
        opacity: disabled ? 0.5 : 1,
      }}
    >
      <span style={{
        position: 'absolute',
        top: 3,
        left: checked ? 27 : 3,
        width: 22,
        height: 22,
        borderRadius: '50%',
        background: '#fff',
        boxShadow: '0 1px 4px rgba(0,0,0,0.25)',
        transition: 'left 0.22s cubic-bezier(.4,0,.2,1)',
        display: 'block',
      }} />
    </button>
  );
}

function AlarmPill({ active, acknowledged }) {
  let bg = '#f1f5f9', color = '#64748b', label = 'Inactive';
  if (active && !acknowledged) { bg = '#f8d7da'; color = '#721c24'; label = 'ACTIVE'; }
  else if (active && acknowledged) { bg = '#fff3cd'; color = '#856404'; label = 'Acked'; }

  return (
    <span style={{
      display: 'inline-flex', alignItems: 'center', gap: 5,
      padding: '3px 10px', borderRadius: 20, fontSize: 11, fontWeight: 700,
      letterSpacing: '0.04em', textTransform: 'uppercase',
      background: bg, color,
    }}>
      <span style={{ fontSize: 7 }}>●</span>
      {label}
    </span>
  );
}

export function Alarms() {
  const [config, setConfig]           = useState(null);
  const [status, setStatus]           = useState(null);
  const [saving, setSaving]           = useState(false);
  const [saveResult, setSaveResult]   = useState(null);
  const [ackSending, setAckSending]   = useState(false);
  const [ackResult, setAckResult]     = useState(null);
  const [beepSending, setBeepSending] = useState(false);
  const [beepResult, setBeepResult]   = useState(null);
  const [autoRefresh, setAutoRefresh] = useState(true);

  // ── Load config once ─────────────────────────────────────────
  useEffect(() => {
    api.getAlarmsConfig().then(setConfig).catch(() => {});
  }, []);

  // ── Poll status ───────────────────────────────────────────────
  const loadStatus = useCallback(async () => {
    try {
      const s = await api.getAlarmsStatus();
      setStatus(s);
    } catch {
      // ignore polling errors silently
    }
  }, []);

  useEffect(() => {
    loadStatus();
    if (!autoRefresh) return;
    const id = setInterval(loadStatus, 2000);
    return () => clearInterval(id);
  }, [autoRefresh, loadStatus]);

  // ── Save config ───────────────────────────────────────────────
  const saveConfig = useCallback(async () => {
    if (!config || saving) return;
    setSaving(true);
    setSaveResult(null);
    try {
      const result = await api.setAlarmsConfig(config);
      setSaveResult({ ok: result.success, text: result.message || result.error || 'Saved' });
      const fresh = await api.getAlarmsConfig();
      setConfig(fresh);
    } catch (err) {
      setSaveResult({ ok: false, text: err.message });
    } finally {
      setSaving(false);
    }
  }, [config, saving]);

  // ── Acknowledge ───────────────────────────────────────────────
  const acknowledge = useCallback(async () => {
    if (ackSending) return;
    setAckSending(true);
    setAckResult(null);
    try {
      const result = await api.acknowledgeAlarms();
      setAckResult({ ok: result.success, text: result.message || result.error || 'Acknowledged' });
      loadStatus();
    } catch (err) {
      setAckResult({ ok: false, text: err.message });
    } finally {
      setAckSending(false);
    }
  }, [ackSending, loadStatus]);

  // ── Manual beep ───────────────────────────────────────────────
  const sendBeep = useCallback(async (on) => {
    if (beepSending) return;
    setBeepSending(true);
    setBeepResult(null);
    try {
      const result = on ? await api.alarmsBeepOn() : await api.alarmsBeepOff();
      setBeepResult({ ok: result.success, text: result.message || result.error || (on ? 'Beep on' : 'Beep off') });
    } catch (err) {
      setBeepResult({ ok: false, text: err.message });
    } finally {
      setBeepSending(false);
    }
  }, [beepSending]);

  const updateField = (field, value) => {
    setConfig(prev => ({ ...prev, [field]: value }));
  };

  if (!config) {
    return <div className="page"><h2>⚠️ Alarms</h2><p>Loading configuration…</p></div>;
  }

  const anyActive  = status?.any_active;
  const anyUnacked = status?.any_unacked;

  return (
    <div className="page">
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 20 }}>
        <h2>⚠️ Alarms</h2>
        <label style={{ fontSize: 13 }}>
          <input
            type="checkbox"
            checked={autoRefresh}
            onChange={e => setAutoRefresh(e.target.checked)}
            style={{ marginRight: 6 }}
          />
          Auto-refresh (2 s)
        </label>
      </div>

      {/* ── Global master switch ── */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: 14,
        padding: '14px 18px', marginBottom: 24,
        background: config.alarms_enabled ? '#e7f8ee' : '#f8f9fa',
        borderLeft: `4px solid ${config.alarms_enabled ? '#27ae60' : '#95a5a6'}`,
        borderRadius: 4,
      }}>
        <Toggle checked={config.alarms_enabled} onChange={v => updateField('alarms_enabled', v)} />
        <div>
          <strong>Alarms {config.alarms_enabled ? 'Enabled' : 'Disabled'}</strong>
          <div style={{ fontSize: 12, color: '#7f8c8d' }}>
            Master switch for depth, AIS proximity and GPS lost detection.
          </div>
        </div>
      </div>

      {/* ── Status strip ── */}
      <div style={{ display: 'flex', gap: 16, flexWrap: 'wrap', marginBottom: 12 }}>
        <StatusCard label="Depth"    value={<AlarmPill active={status?.depth?.active}    acknowledged={status?.depth?.acknowledged} />}    accent="#3498db" />
        <StatusCard label="AIS"      value={<AlarmPill active={status?.ais?.active}       acknowledged={status?.ais?.acknowledged} />}       accent="#e67e22" />
        <StatusCard label="GPS Lost" value={<AlarmPill active={status?.gps_lost?.active}  acknowledged={status?.gps_lost?.acknowledged} />} accent="#8e44ad" />
      </div>

      {status?.ais?.active && status?.ais_trigger_mmsi > 0 && (
        <p style={{ fontSize: 12, color: '#7f8c8d', marginBottom: 20 }}>
          Closest AIS threat MMSI: <strong>{status.ais_trigger_mmsi}</strong>
        </p>
      )}

      {/* ── Acknowledge banner ── */}
      {anyUnacked && (
        <div style={{
          display: 'flex', justifyContent: 'space-between', alignItems: 'center',
          padding: '14px 18px', marginBottom: 24,
          background: '#f8d7da', borderLeft: '4px solid #e74c3c', borderRadius: 4,
        }}>
          <span style={{ color: '#721c24', fontWeight: 600 }}>
            🔔 {anyActive ? 'Active unacknowledged alarm(s)!' : ''}
          </span>
          <button
            disabled={ackSending}
            onClick={acknowledge}
            style={{ background: '#e74c3c' }}
          >
            {ackSending ? 'Acknowledging…' : '✓ Acknowledge All'}
          </button>
        </div>
      )}

      {ackResult && (
        <div className={`message ${ackResult.ok ? 'success' : 'error'}`}>
          {ackResult.ok ? '✓' : '✗'} {ackResult.text}
        </div>
      )}

      {/* ── Depth configuration ── */}
      <section style={{ marginBottom: 28 }}>
        <h3 style={sectionTitle}>🌊 Depth Alarm</h3>
        <p style={helpText}>Triggers when depth below transducer drops to or below the threshold.</p>
        <div style={{ display: 'flex', alignItems: 'center', gap: 16, flexWrap: 'wrap' }}>
          <Toggle checked={config.depth_enabled} onChange={v => updateField('depth_enabled', v)} />
          <div className="form-group" style={{ marginBottom: 0, maxWidth: 200 }}>
            <label>Threshold (m)</label>
            <input
              type="number" step="0.1" min="0.1"
              value={config.depth_threshold_m}
              onChange={e => updateField('depth_threshold_m', parseFloat(e.target.value) || 0)}
              disabled={!config.depth_enabled}
            />
          </div>
        </div>
      </section>

      {/* ── AIS configuration ── */}
      <section style={{ marginBottom: 28 }}>
        <h3 style={sectionTitle}>🚢 AIS Proximity Alarm</h3>
        <p style={helpText}>
          Triggers when any AIS target (other than your own vessel) comes within the configured distance.
        </p>
        <div style={{ display: 'flex', alignItems: 'center', gap: 16, flexWrap: 'wrap', marginBottom: 14 }}>
          <Toggle checked={config.ais_enabled} onChange={v => updateField('ais_enabled', v)} />
          <div className="form-group" style={{ marginBottom: 0, maxWidth: 200 }}>
            <label>Distance (nm)</label>
            <input
              type="number" step="0.1" min="0.1"
              value={config.ais_distance_nm}
              onChange={e => updateField('ais_distance_nm', parseFloat(e.target.value) || 0)}
              disabled={!config.ais_enabled}
            />
          </div>
        </div>
        <div className="form-group" style={{ maxWidth: 260 }}>
          <label>Own vessel MMSI (excluded from alarm)</label>
          <input
            type="number" min="0"
            value={config.own_mmsi}
            onChange={e => updateField('own_mmsi', parseInt(e.target.value, 10) || 0)}
          />
        </div>
      </section>

      {/* ── GPS lost configuration ── */}
      <section style={{ marginBottom: 28 }}>
        <h3 style={sectionTitle}>📍 GPS Lost Alarm</h3>
        <p style={helpText}>Triggers when the GPS position has not been updated for longer than the timeout.</p>
        <div style={{ display: 'flex', alignItems: 'center', gap: 16, flexWrap: 'wrap' }}>
          <Toggle checked={config.gps_lost_enabled} onChange={v => updateField('gps_lost_enabled', v)} />
          <div className="form-group" style={{ marginBottom: 0, maxWidth: 200 }}>
            <label>Timeout (s)</label>
            <input
              type="number" step="1" min="1"
              value={config.gps_lost_timeout_s}
              onChange={e => updateField('gps_lost_timeout_s', parseInt(e.target.value, 10) || 1)}
              disabled={!config.gps_lost_enabled}
            />
          </div>
        </div>
      </section>

      {/* ── Save ── */}
      <div style={{ marginBottom: 12 }}>
        <button disabled={saving} onClick={saveConfig}>
          {saving ? 'Saving…' : 'Save Configuration'}
        </button>
      </div>
      {saveResult && (
        <div className={`message ${saveResult.ok ? 'success' : 'error'}`}>
          {saveResult.ok ? '✓' : '✗'} {saveResult.text}
        </div>
      )}

      {/* ── Manual beep controls ── */}
      <section style={{ marginTop: 36, paddingTop: 24, borderTop: '2px dashed #e0e0e0' }}>
        <h3 style={{ ...sectionTitle, borderBottomColor: '#95a5a6' }}>🔔 Manual Beep Control</h3>
        <p style={helpText}>
          Sends beep_on / beep_off directly via SeaTalk1 (same commands used by the alarm engine).
        </p>
        {beepResult && (
          <div className={`message ${beepResult.ok ? 'success' : 'error'}`}>
            {beepResult.ok ? '✓' : '✗'} {beepResult.text}
          </div>
        )}
        <div style={{ display: 'flex', gap: 14 }}>
          <button disabled={beepSending} onClick={() => sendBeep(true)} style={{ background: '#d35400' }}>
            🔔 Send Alarm (beep_on)
          </button>
          <button disabled={beepSending} onClick={() => sendBeep(false)} style={{ background: '#8e44ad' }}>
            🔕 Stop Alarm (beep_off)
          </button>
        </div>
      </section>
    </div>
  );
}
