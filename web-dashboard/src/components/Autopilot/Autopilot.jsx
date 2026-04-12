import { useState, useEffect, useCallback } from 'react';
import { api } from '../../services/api';

/**
 * Autopilot page — ST4000+ SeaTalk control via the /api/autopilot/command endpoint,
 * plus utility SeaTalk commands via /api/seatalk/extra.
 *
 * Displays:
 *  - Live autopilot status (mode, heading target, rudder angle) polled from /api/status
 *  - Mode buttons: Standby / Auto / Wind / Track
 *  - Course adjustment buttons: -10 / -1 / +1 / +10
 *  - Port / Starboard tack buttons
 *  - Extra Commands section: lamp intensity, alarm acknowledge, beep
 */

// ── Mode labels ───────────────────────────────────────────────
const MODES = [
  { id: 'standby', label: 'Standby', icon: '⏹', description: 'Disengage autopilot' },
  { id: 'auto',    label: 'Auto',    icon: '🧭', description: 'Hold compass heading' },
  { id: 'wind',    label: 'Wind',    icon: '💨', description: 'Hold wind angle (vane)' },
  { id: 'track',   label: 'Track',   icon: '📍', description: 'Follow GPS track' },
];

// ── Adjust steps ──────────────────────────────────────────────
const ADJUST_STEPS = [
  { label: '−10°', command: 'adjust-10', side: 'port'      },
  { label: '−1°',  command: 'adjust-1',  side: 'port'      },
  { label: '+1°',  command: 'adjust+1',  side: 'starboard' },
  { label: '+10°', command: 'adjust+10', side: 'starboard' },
];

// ── Lamp levels ───────────────────────────────────────────────
// Datagram 30 00 0X: X=0 off, X=4 L1, X=8 L2, X=C L3
const LAMP_LEVELS = [
  { id: 'lamp:0', label: 'Off',  icon: '⬛' },
  { id: 'lamp:1', label: 'L1',   icon: '🔅' },
  { id: 'lamp:2', label: 'L2',   icon: '💡' },
  { id: 'lamp:3', label: 'L3',   icon: '🔆' },
];

export function Autopilot() {
  const [apStatus, setApStatus]             = useState(null);
  const [sending, setSending]               = useState(false);
  const [lastResult, setLastResult]         = useState(null);
  const [autoRefresh, setAutoRefresh]       = useState(true);
  const [activeLamp, setActiveLamp]         = useState(null);   // currently lit lamp level id
  const [extraSending, setExtraSending]     = useState(false);
  const [extraResult, setExtraResult]       = useState(null);

  // ── Poll autopilot status ─────────────────────────────────
  const loadStatus = useCallback(async () => {
    try {
      const data = await api.getStatus();
      setApStatus(prev => ({ ...prev, system: data }));
    } catch {
      // ignore polling errors silently
    }
    try {
      const nav = await api.getBoatNavigation();
      setApStatus(prev => ({ ...prev, nav }));
    } catch {
      // ignore
    }
  }, []);

  useEffect(() => {
    loadStatus();
    if (!autoRefresh) return;
    const id = setInterval(loadStatus, 2000);
    return () => clearInterval(id);
  }, [autoRefresh, loadStatus]);

  // ── Send autopilot command ────────────────────────────────
  const sendCommand = useCallback(async (command) => {
    if (sending) return;
    setSending(true);
    setLastResult(null);
    try {
      const result = await api.sendAutopilotCommand(command);
      setLastResult({ ok: result.success, text: result.message || result.error || command });
    } catch (err) {
      setLastResult({ ok: false, text: err.message });
    } finally {
      setSending(false);
    }
  }, [sending]);

  // ── Send extra SeaTalk command ────────────────────────────
  const sendExtra = useCallback(async (command) => {
    if (extraSending) return;
    setExtraSending(true);
    setExtraResult(null);
    try {
      const result = await api.sendExtraCommand(command);
      setExtraResult({ ok: result.success, text: result.message || result.error || command });
      // Track active lamp level optimistically
      if (result.success && command.startsWith('lamp:')) {
        setActiveLamp(command);
      }
    } catch (err) {
      setExtraResult({ ok: false, text: err.message });
    } finally {
      setExtraSending(false);
    }
  }, [extraSending]);

  // ── Derived state ─────────────────────────────────────────
  const currentMode    = apStatus?.nav?.autopilot?.mode ?? null;
  const headingTarget  = apStatus?.nav?.heading?.magnetic?.value;
  const headingStr     = headingTarget != null ? `${headingTarget.toFixed(0)}°` : '--';

  return (
    <div className="page">
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 20 }}>
        <h2>🚢 Autopilot Control</h2>
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

      {/* ── Info banner ── */}
      <div style={{
        padding: '12px 16px',
        marginBottom: 24,
        background: '#e7f3ff',
        borderLeft: '4px solid #3498db',
        borderRadius: 4,
        fontSize: 13,
        lineHeight: 1.6,
      }}>
        Commands are transmitted over the <strong>SeaTalk1 bus</strong> using
        the ST4000+ datagram protocol.
        The autopilot must be powered and connected to GPIO&nbsp;
        <strong>{'{ST1_TX_PIN}'}</strong>.
      </div>

      {/* ── Status strip ── */}
      <div style={{ display: 'flex', gap: 16, flexWrap: 'wrap', marginBottom: 24 }}>
        <StatusCard label="Current Mode"  value={currentMode ?? '--'} accent="#2c3e50" />
        <StatusCard label="Heading (Mag)" value={headingStr}          accent="#3498db" />
      </div>

      {/* ── Autopilot command result ── */}
      {lastResult && (
        <div className={`message ${lastResult.ok ? 'success' : 'error'}`} style={{ marginBottom: 20 }}>
          {lastResult.ok ? '✓' : '✗'} {lastResult.text}
        </div>
      )}

      {/* ── Mode buttons ── */}
      <section style={{ marginBottom: 28 }}>
        <h3 style={sectionTitle}>Mode Selection</h3>
        <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap' }}>
          {MODES.map(mode => (
            <ModeButton
              key={mode.id}
              mode={mode}
              active={currentMode === mode.id}
              disabled={sending}
              onClick={() => sendCommand(mode.id)}
            />
          ))}
        </div>
      </section>

      {/* ── Course adjustment ── */}
      <section style={{ marginBottom: 28 }}>
        <h3 style={sectionTitle}>Course Adjustment</h3>
        <p style={{ fontSize: 13, color: '#7f8c8d', marginBottom: 14 }}>
          Only active when autopilot is in <strong>Auto</strong> or <strong>Track</strong> mode.
        </p>
        <div style={{ display: 'flex', gap: 10, flexWrap: 'wrap', alignItems: 'center' }}>
          {ADJUST_STEPS.map(step => (
            <AdjustButton
              key={step.command}
              step={step}
              disabled={sending}
              onClick={() => sendCommand(step.command)}
            />
          ))}
        </div>
      </section>

      {/* ── Tack buttons ── */}
      <section style={{ marginBottom: 28 }}>
        <h3 style={sectionTitle}>Tack (Wind Mode)</h3>
        <p style={{ fontSize: 13, color: '#7f8c8d', marginBottom: 14 }}>
          Auto-tack manoeuvres — only valid in <strong>Wind</strong> vane mode.
        </p>
        <div style={{ display: 'flex', gap: 14 }}>
          <button
            disabled={sending}
            onClick={() => sendCommand('tack-port')}
            style={{ ...tackStyle, background: 'linear-gradient(135deg, #c0392b, #e74c3c)' }}
          >
            ◀ Port Tack
          </button>
          <button
            disabled={sending}
            onClick={() => sendCommand('tack-starboard')}
            style={{ ...tackStyle, background: 'linear-gradient(135deg, #27ae60, #2ecc71)' }}
          >
            Starboard Tack ▶
          </button>
        </div>
      </section>

      {/* ════════════════════════════════════════════════════════
          EXTRA COMMANDS
      ════════════════════════════════════════════════════════ */}
      <section style={{
        marginTop: 36,
        paddingTop: 24,
        borderTop: '2px dashed #e0e0e0',
      }}>
        <h3 style={{ ...sectionTitle, borderBottomColor: '#95a5a6' }}>
          🛠 Extra Commands
        </h3>

        <p style={{ fontSize: 13, color: '#7f8c8d', marginBottom: 20, lineHeight: 1.6 }}>
          Utility SeaTalk datagrams sent directly on the bus, independent of autopilot state.
          These commands are broadcast to all instruments on the SeaTalk network.
        </p>

        {/* Extra command result */}
        {extraResult && (
          <div className={`message ${extraResult.ok ? 'success' : 'error'}`}
               style={{ marginBottom: 20 }}>
            {extraResult.ok ? '✓' : '✗'} {extraResult.text}
          </div>
        )}

        {/* ── Lamp intensity ─────────────────────────────────── */}
        <div style={{ marginBottom: 24 }}>
          <h4 style={subSectionTitle}>
            💡 Lamp Intensity
            <span style={datagrams}>datagram 30 00 0X</span>
          </h4>
          <p style={helpText}>
            Sets display brightness on all connected SeaTalk instruments simultaneously.
          </p>
          <div style={{ display: 'flex', gap: 10, flexWrap: 'wrap' }}>
            {LAMP_LEVELS.map(level => (
              <LampButton
                key={level.id}
                level={level}
                active={activeLamp === level.id}
                disabled={extraSending}
                onClick={() => sendExtra(level.id)}
              />
            ))}
          </div>
        </div>

        {/* ── Alarm acknowledge ──────────────────────────────── */}
        <div style={{ marginBottom: 24 }}>
          <h4 style={subSectionTitle}>
            🔕 Alarm Acknowledge
            <span style={datagrams}>datagram 68 41 15 00</span>
          </h4>
          <p style={helpText}>
            Sends a generic alarm acknowledgement keystroke (ST40 Wind Instrument format).
            Silences active depth, wind, and anchor alarms on the instrument network.
          </p>
          <button
            disabled={extraSending}
            onClick={() => sendExtra('beep_off')}
            style={extraBtnStyle('#8e44ad', '#9b59b6')}
          >
            🔕 Acknowledge Alarm
          </button>
        </div>

        {/* ── Beep ──────────────────────────────────────────── */}
        <div style={{ marginBottom: 24 }}>
          <h4 style={subSectionTitle}>
            🔔 Alarm Send
            <span style={datagrams}>datagram 86 21 04 FB</span>
          </h4>
          <p style={helpText}>
            Triggers a single audible beep on the autopilot display by simulating a
            Disp key press. Does not change autopilot state.
          </p>
          <button
            disabled={extraSending}
            onClick={() => sendExtra('beep_on')}
            style={extraBtnStyle('#d35400', '#e67e22')}
          >
            🔔 Send Alarm
          </button>
        </div>

        {/* ── Extra commands reference ───────────────────────── */}
        <details style={{ marginTop: 8 }}>
          <summary style={{ cursor: 'pointer', fontSize: 13, color: '#7f8c8d', userSelect: 'none' }}>
            Extra commands datagram reference
          </summary>
          <div style={{
            marginTop: 10,
            padding: 14,
            background: '#f8f9fa',
            borderRadius: 6,
            fontSize: 12,
            lineHeight: 1.8,
            overflowX: 'auto',
          }}>
            <table style={{ borderCollapse: 'collapse', width: '100%' }}>
              <thead>
                <tr style={{ background: '#34495e', color: 'white' }}>
                  <th style={th}>Command</th>
                  <th style={th}>Datagram (hex)</th>
                  <th style={th}>Description</th>
                </tr>
              </thead>
              <tbody>
                {EXTRA_REF.map((row, i) => (
                  <tr key={i} style={{ background: i % 2 === 0 ? '#fff' : '#f4f6f8' }}>
                    <td style={{ ...td, fontFamily: 'monospace', color: '#2c3e50' }}>{row.cmd}</td>
                    <td style={{ ...td, fontFamily: 'monospace' }}>{row.hex}</td>
                    <td style={td}>{row.desc}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </details>
      </section>

      {/* ── Autopilot datagram reference ── */}
      <details style={{ marginTop: 24 }}>
        <summary style={{ cursor: 'pointer', fontSize: 13, color: '#7f8c8d', userSelect: 'none' }}>
          Autopilot datagram reference (ST4000+)
        </summary>
        <div style={{
          marginTop: 10,
          padding: 14,
          background: '#f8f9fa',
          borderRadius: 6,
          fontFamily: 'monospace',
          fontSize: 12,
          lineHeight: 1.8,
          overflowX: 'auto',
        }}>
          <table style={{ borderCollapse: 'collapse', width: '100%' }}>
            <thead>
              <tr style={{ background: '#34495e', color: 'white' }}>
                <th style={th}>Action</th>
                <th style={th}>Datagram (hex)</th>
                <th style={th}>Source</th>
              </tr>
            </thead>
            <tbody>
              {DATAGRAM_REF.map((row, i) => (
                <tr key={i} style={{ background: i % 2 === 0 ? '#fff' : '#f4f6f8' }}>
                  <td style={td}>{row.action}</td>
                  <td style={{ ...td, fontFamily: 'monospace', color: '#2c3e50' }}>{row.hex}</td>
                  <td style={td}>{row.source}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </details>
    </div>
  );
}

// ── Sub-components ────────────────────────────────────────────

function StatusCard({ label, value, accent }) {
  return (
    <div style={{
      background: '#f8f9fa',
      borderLeft: `4px solid ${accent}`,
      borderRadius: 4,
      padding: '12px 20px',
      minWidth: 140,
    }}>
      <div style={{ fontSize: 11, textTransform: 'uppercase', color: '#7f8c8d', marginBottom: 4 }}>{label}</div>
      <div style={{ fontSize: 22, fontWeight: 'bold', color: '#2c3e50' }}>{value}</div>
    </div>
  );
}

function ModeButton({ mode, active, disabled, onClick }) {
  return (
    <button
      disabled={disabled}
      onClick={onClick}
      title={mode.description}
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: 4,
        padding: '14px 22px',
        border: `2px solid ${active ? '#3498db' : '#ddd'}`,
        borderRadius: 8,
        background: active
          ? 'linear-gradient(135deg, #3498db, #2980b9)'
          : '#fff',
        color: active ? '#fff' : '#2c3e50',
        cursor: disabled ? 'not-allowed' : 'pointer',
        opacity: disabled ? 0.6 : 1,
        transition: 'all 0.15s',
        minWidth: 90,
        boxShadow: active ? '0 4px 12px rgba(52,152,219,0.4)' : '0 1px 3px rgba(0,0,0,0.1)',
      }}
    >
      <span style={{ fontSize: 24 }}>{mode.icon}</span>
      <span style={{ fontWeight: 600, fontSize: 14 }}>{mode.label}</span>
    </button>
  );
}

function AdjustButton({ step, disabled, onClick }) {
  const isPort = step.side === 'port';
  return (
    <button
      disabled={disabled}
      onClick={onClick}
      style={{
        padding: '12px 20px',
        borderRadius: 6,
        border: 'none',
        background: isPort
          ? 'linear-gradient(135deg, #c0392b, #e74c3c)'
          : 'linear-gradient(135deg, #27ae60, #2ecc71)',
        color: '#fff',
        fontWeight: 700,
        fontSize: 16,
        cursor: disabled ? 'not-allowed' : 'pointer',
        opacity: disabled ? 0.6 : 1,
        minWidth: 70,
        boxShadow: '0 2px 6px rgba(0,0,0,0.15)',
        transition: 'opacity 0.15s',
      }}
    >
      {step.label}
    </button>
  );
}

/**
 * Lamp intensity button — highlights the currently active level.
 */
function LampButton({ level, active, disabled, onClick }) {
  return (
    <button
      disabled={disabled}
      onClick={onClick}
      title={`Set lamp intensity: ${level.label}`}
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: 4,
        padding: '12px 18px',
        border: `2px solid ${active ? '#f39c12' : '#ddd'}`,
        borderRadius: 8,
        background: active
          ? 'linear-gradient(135deg, #f39c12, #e67e22)'
          : '#fff',
        color: active ? '#fff' : '#2c3e50',
        cursor: disabled ? 'not-allowed' : 'pointer',
        opacity: disabled ? 0.6 : 1,
        transition: 'all 0.15s',
        minWidth: 70,
        boxShadow: active ? '0 4px 12px rgba(243,156,18,0.4)' : '0 1px 3px rgba(0,0,0,0.1)',
        fontWeight: 600,
        fontSize: 13,
      }}
    >
      <span style={{ fontSize: 22 }}>{level.icon}</span>
      {level.label}
    </button>
  );
}

// ── Style helpers ─────────────────────────────────────────────

const sectionTitle = {
  marginBottom: 14,
  fontSize: 16,
  color: '#2c3e50',
  borderBottom: '2px solid #3498db',
  paddingBottom: 6,
};

const subSectionTitle = {
  fontSize: 14,
  color: '#2c3e50',
  marginBottom: 6,
  fontWeight: 600,
  display: 'flex',
  alignItems: 'center',
  gap: 10,
};

const datagrams = {
  fontSize: 11,
  fontFamily: 'monospace',
  background: '#ecf0f1',
  color: '#7f8c8d',
  padding: '2px 7px',
  borderRadius: 3,
  fontWeight: 400,
};

const helpText = {
  fontSize: 13,
  color: '#7f8c8d',
  lineHeight: 1.6,
  marginBottom: 12,
};

const extraBtnStyle = (dark, light) => ({
  padding: '11px 24px',
  border: 'none',
  borderRadius: 6,
  background: `linear-gradient(135deg, ${dark}, ${light})`,
  color: '#fff',
  fontWeight: 600,
  fontSize: 14,
  cursor: 'pointer',
  boxShadow: '0 2px 6px rgba(0,0,0,0.15)',
  transition: 'opacity 0.15s',
});

const tackStyle = {
  padding: '12px 28px',
  border: 'none',
  borderRadius: 8,
  color: '#fff',
  fontWeight: 700,
  fontSize: 15,
  cursor: 'pointer',
  boxShadow: '0 3px 8px rgba(0,0,0,0.2)',
  transition: 'opacity 0.15s',
};

const th = {
  padding: '8px 12px',
  textAlign: 'left',
  fontWeight: 600,
  fontSize: 11,
  textTransform: 'uppercase',
  letterSpacing: '0.5px',
};

const td = {
  padding: '6px 12px',
  borderBottom: '1px solid #ecf0f1',
  fontSize: 12,
};

// ── Reference tables ──────────────────────────────────────────

const EXTRA_REF = [
  { cmd: 'lamp:0',    hex: '30 00 00',      desc: 'Lamp off — all connected SeaTalk displays' },
  { cmd: 'lamp:1',    hex: '30 00 04',      desc: 'Lamp intensity level 1 (dim)' },
  { cmd: 'lamp:2',    hex: '30 00 08',      desc: 'Lamp intensity level 2 (medium)' },
  { cmd: 'lamp:3',    hex: '30 00 0C',      desc: 'Lamp intensity level 3 (bright)' },
  { cmd: 'alarm-ack', hex: '68 41 15 00',   desc: 'Generic alarm acknowledge (ST40 Wind Instrument keystroke)' },
  { cmd: 'beep',      hex: '86 21 04 FB',   desc: 'Audible beep — Disp keystroke (ST4000+, X=2)' },
];

const DATAGRAM_REF = [
  { action: 'Auto',             hex: '86 21 01 FE', source: 'ST4000+ (X=2)' },
  { action: 'Standby',          hex: '86 21 02 FD', source: 'ST4000+ (X=2)' },
  { action: 'Track',            hex: '86 21 03 FC', source: 'ST4000+ (X=2)' },
  { action: 'Wind (vane mode)', hex: '86 21 23 DC', source: 'ST4000+ (X=2)' },
  { action: '−1°',              hex: '86 21 05 FA', source: 'ST4000+ (X=2)' },
  { action: '−10°',             hex: '86 21 06 F9', source: 'ST4000+ (X=2)' },
  { action: '+1°',              hex: '86 21 07 F8', source: 'ST4000+ (X=2)' },
  { action: '+10°',             hex: '86 21 08 F7', source: 'ST4000+ (X=2)' },
  { action: 'Port tack',        hex: '86 21 21 DE', source: 'ST4000+ (X=2)' },
  { action: 'Starboard tack',   hex: '86 21 22 DD', source: 'ST4000+ (X=2)' },
];
