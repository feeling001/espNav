/**
 * Logbook.jsx — Marine Gateway logbook configuration and status page.
 *
 * Communicates with:
 *   GET  /api/log/status   — poll stats + current config
 *   POST /api/log/config   — save settings
 *   POST /api/log/new      — start a new session
 */

import { useState, useEffect, useCallback } from 'react';

// ─────────────────────────────────────────────────────────────────────────────
// API helpers
// ─────────────────────────────────────────────────────────────────────────────

const API = '/api/log';

async function fetchLogStatus() {
  const r = await fetch(`${API}/status`);
  if (!r.ok) throw new Error('Failed to fetch log status');
  return r.json();
}

async function postLogConfig(cfg) {
  const r = await fetch(`${API}/config`, {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify(cfg),
  });
  if (!r.ok) throw new Error('Failed to save config');
  return r.json();
}

async function postNewSession() {
  const r = await fetch(`${API}/new`, { method: 'POST' });
  if (!r.ok) throw new Error('Failed to start new session');
  return r.json();
}

// ─────────────────────────────────────────────────────────────────────────────
// Utility
// ─────────────────────────────────────────────────────────────────────────────

function fmtUptime(secs) {
  if (secs < 60)   return `${secs}s`;
  if (secs < 3600) return `${Math.floor(secs / 60)}m ${secs % 60}s`;
  const h = Math.floor(secs / 3600);
  const m = Math.floor((secs % 3600) / 60);
  return `${h}h ${m}m`;
}

function fmtNum(n) {
  if (n === undefined || n === null) return '—';
  return n.toLocaleString();
}

// ─────────────────────────────────────────────────────────────────────────────
// Sub-components
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Animated toggle switch — maritime dark-teal accent.
 */
function Toggle({ checked, onChange, disabled }) {
  return (
    <button
      onClick={() => !disabled && onChange(!checked)}
      disabled={disabled}
      style={{
        position:        'relative',
        width:           52,
        height:          28,
        borderRadius:    14,
        border:          'none',
        cursor:          disabled ? 'not-allowed' : 'pointer',
        background:      checked ? '#0e7490' : '#cbd5e1',
        transition:      'background 0.25s',
        flexShrink:      0,
        padding:         0,
        opacity:         disabled ? 0.5 : 1,
      }}
    >
      <span style={{
        position:    'absolute',
        top:         3,
        left:        checked ? 27 : 3,
        width:       22,
        height:      22,
        borderRadius:'50%',
        background:  '#fff',
        boxShadow:   '0 1px 4px rgba(0,0,0,0.25)',
        transition:  'left 0.22s cubic-bezier(.4,0,.2,1)',
        display:     'block',
      }} />
    </button>
  );
}

/** Status pill badge. */
function Pill({ active, children }) {
  return (
    <span style={{
      display:      'inline-flex',
      alignItems:   'center',
      gap:          5,
      padding:      '3px 10px',
      borderRadius: 20,
      fontSize:     11,
      fontWeight:   700,
      letterSpacing:'0.04em',
      textTransform:'uppercase',
      background:   active ? '#dcfce7' : '#f1f5f9',
      color:        active ? '#15803d' : '#64748b',
      border:       `1px solid ${active ? '#bbf7d0' : '#e2e8f0'}`,
    }}>
      <span style={{ fontSize: 7 }}>●</span>
      {children}
    </span>
  );
}

/** Stat card — compact data display. */
function StatCard({ label, value, sub, accent }) {
  return (
    <div style={{
      background:   '#f8fafc',
      border:       '1px solid #e2e8f0',
      borderLeft:   `3px solid ${accent || '#0e7490'}`,
      borderRadius: 6,
      padding:      '12px 16px',
      minWidth:     130,
    }}>
      <div style={{ fontSize: 11, color: '#94a3b8', textTransform: 'uppercase',
                    letterSpacing: '0.06em', marginBottom: 4 }}>
        {label}
      </div>
      <div style={{ fontSize: 22, fontWeight: 700, color: '#1e293b',
                    fontVariantNumeric: 'tabular-nums' }}>
        {value}
      </div>
      {sub && (
        <div style={{ fontSize: 11, color: '#94a3b8', marginTop: 2 }}>{sub}</div>
      )}
    </div>
  );
}

/** A logging mode row with toggle + description. */
function LogRow({ icon, label, description, checked, onChange, disabled, children }) {
  return (
    <div style={{
      display:      'flex',
      alignItems:   'flex-start',
      gap:          14,
      padding:      '16px 20px',
      background:   checked ? '#f0f9ff' : '#fafafa',
      borderRadius: 8,
      border:       `1px solid ${checked ? '#bae6fd' : '#e2e8f0'}`,
      transition:   'all 0.2s',
    }}>
      <div style={{ fontSize: 24, flexShrink: 0, marginTop: 1 }}>{icon}</div>
      <div style={{ flex: 1 }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 4 }}>
          <span style={{ fontWeight: 600, fontSize: 14, color: '#1e293b' }}>{label}</span>
          {checked && <Pill active>Recording</Pill>}
        </div>
        <p style={{ fontSize: 12, color: '#64748b', margin: 0, lineHeight: 1.6 }}>
          {description}
        </p>
        {children && <div style={{ marginTop: 10 }}>{children}</div>}
      </div>
      <Toggle checked={checked} onChange={onChange} disabled={disabled} />
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// Logbook page
// ─────────────────────────────────────────────────────────────────────────────

export function Logbook() {
  const [status,    setStatus]    = useState(null);
  const [saving,    setSaving]    = useState(false);
  const [newSes,    setNewSes]    = useState(false);
  const [message,   setMessage]   = useState(null);
  const [autoRefresh, setAutoRefresh] = useState(true);

  // Local editable config (mirrors what was last fetched from device).
  const [cfg, setCfg] = useState({
    nmea_enabled:     false,
    seatalk_enabled:  false,
    csv_enabled:      false,
    csv_interval_min: 5,
  });

  // Dirty flag — user changed something not yet saved.
  const [dirty, setDirty] = useState(false);

  // ── Fetch status ────────────────────────────────────────────
  const load = useCallback(async () => {
    try {
      const data = await fetchLogStatus();
      setStatus(data);
      // Sync local config only when NOT dirty (i.e. user hasn't unsaved changes).
      if (!dirty) {
        setCfg({
          nmea_enabled:     data.nmea_enabled    ?? false,
          seatalk_enabled:  data.seatalk_enabled ?? false,
          csv_enabled:      data.csv_enabled     ?? false,
          csv_interval_min: data.csv_interval_min ?? 5,
        });
      }
    } catch (e) {
      // Swallow polling errors silently after initial load.
      if (!status) setMessage({ type: 'error', text: e.message });
    }
  }, [dirty, status]);

  useEffect(() => {
    load();
    if (!autoRefresh) return;
    const id = setInterval(load, 3000);
    return () => clearInterval(id);
  }, [autoRefresh, load]);

  // ── Config helpers ──────────────────────────────────────────
  function patch(key, value) {
    setCfg(prev => ({ ...prev, [key]: value }));
    setDirty(true);
    setMessage(null);
  }

  // ── Save config ─────────────────────────────────────────────
  async function handleSave() {
    setSaving(true);
    setMessage(null);
    try {
      await postLogConfig(cfg);
      setDirty(false);
      setMessage({ type: 'success', text: 'Configuration saved.' });
      await load();
    } catch (e) {
      setMessage({ type: 'error', text: e.message });
    } finally {
      setSaving(false);
    }
  }

  // ── New session ─────────────────────────────────────────────
  async function handleNewSession() {
    if (!window.confirm('Start a new log session? Current files will be closed.')) return;
    setNewSes(true);
    setMessage(null);
    try {
      await postNewSession();
      setMessage({ type: 'success', text: 'New session started.' });
      await load();
    } catch (e) {
      setMessage({ type: 'error', text: e.message });
    } finally {
      setNewSes(false);
    }
  }

  // ── File list ───────────────────────────────────────────────
  const openFiles = status?.open_files
    ? status.open_files.split(',').filter(Boolean)
    : [];

  // ── Any logging active ──────────────────────────────────────
  const anyActive = cfg.nmea_enabled || cfg.seatalk_enabled || cfg.csv_enabled;

  // ── Render ──────────────────────────────────────────────────
  return (
    <div className="page">

      {/* ── Header ── */}
      <div style={{ display:'flex', justifyContent:'space-between',
                    alignItems:'center', marginBottom: 24, flexWrap:'wrap', gap:12 }}>
        <div>
          <h2 style={{ marginBottom: 4, color:'#0f172a' }}>📓 Logbook</h2>
          <p style={{ margin:0, fontSize:13, color:'#64748b' }}>
            Record raw NMEA, SeaTalk datagrams, and structured CSV snapshots to SD card.
          </p>
        </div>
        <label style={{ fontSize:13, color:'#64748b', display:'flex',
                         alignItems:'center', gap:6 }}>
          <input type="checkbox" checked={autoRefresh}
            onChange={e => setAutoRefresh(e.target.checked)} />
          Auto-refresh
        </label>
      </div>

      {/* ── Alert banner when SD not available ── */}
      {status && !status.has_open_files && anyActive && (
        <div style={styles.notice('#fef9c3', '#ca8a04', '#fef08a')}>
          <strong>⚠ SD card required.</strong>{' '}
          Insert a card and mount it via Config → Storage before enabling logging.
        </div>
      )}

      {/* ── Message ── */}
      {message && (
        <div className={`message ${message.type}`} style={{ marginBottom: 20 }}>
          {message.type === 'success' ? '✓' : '✗'} {message.text}
        </div>
      )}

      {/* ── Session status ── */}
      {status && (
        <div style={{
          display:      'flex',
          gap:          12,
          marginBottom: 24,
          flexWrap:     'wrap',
        }}>
          <StatCard
            label="Session"
            value={status.session_name || '—'}
            accent="#0e7490"
          />
          <StatCard
            label="Duration"
            value={fmtUptime(status.session_uptime_s || 0)}
            accent="#0e7490"
          />
          <StatCard
            label="NMEA Lines"
            value={fmtNum(status.nmea_lines)}
            accent="#16a34a"
          />
          <StatCard
            label="SeaTalk Lines"
            value={fmtNum(status.seatalk_lines)}
            accent="#7c3aed"
          />
          <StatCard
            label="CSV Snapshots"
            value={fmtNum(status.csv_snapshots)}
            accent="#b45309"
          />
          {status.dropped_entries > 0 && (
            <StatCard
              label="Dropped"
              value={fmtNum(status.dropped_entries)}
              sub="queue full"
              accent="#dc2626"
            />
          )}
        </div>
      )}

      {/* ── Open files list ── */}
      {openFiles.length > 0 && (
        <div style={{
          marginBottom: 24,
          padding:      '12px 16px',
          background:   '#f0fdf4',
          border:       '1px solid #bbf7d0',
          borderRadius: 6,
        }}>
          <div style={{ fontSize:12, fontWeight:600, color:'#15803d',
                        marginBottom:6, textTransform:'uppercase', letterSpacing:'0.05em' }}>
            🟢 Active Log Files
          </div>
          {openFiles.map(f => (
            <div key={f} style={{ fontFamily:'monospace', fontSize:12,
                                  color:'#166534', marginTop:2 }}>
              {f}
            </div>
          ))}
        </div>
      )}

      {/* ── Logging modes ── */}
      <h3 style={styles.sectionTitle}>Recording Modes</h3>
      <div style={{ display:'flex', flexDirection:'column', gap:10, marginBottom:28 }}>

        <LogRow
          icon="📡"
          label="Raw NMEA"
          description="Verbatim copy of every NMEA 0183 sentence received on the serial port. One sentence per line. File extension: .nmea"
          checked={cfg.nmea_enabled}
          onChange={v => patch('nmea_enabled', v)}
        />

        <LogRow
          icon="🔗"
          label="Raw SeaTalk"
          description="SeaTalk1 datagrams captured on the ST1 bus, formatted as space-separated hex bytes (e.g. 52 01 02 FF). Prefixed with a millisecond timestamp. File extension: .st1"
          checked={cfg.seatalk_enabled}
          onChange={v => patch('seatalk_enabled', v)}
        />

        <LogRow
          icon="📊"
          label="Structured CSV"
          description="Periodic snapshot of all boat data: position, speed, heading, depth, wind, water temperature. Interval is configurable. File extension: .csv"
          checked={cfg.csv_enabled}
          onChange={v => patch('csv_enabled', v)}
        >
          {/* CSV interval input — only shown when CSV is toggled on */}
          {cfg.csv_enabled && (
            <div style={{ display:'flex', alignItems:'center', gap:10 }}>
              <label style={{ fontSize:13, color:'#475569' }}>Snapshot every</label>
              <input
                type="number"
                min={1}
                max={1440}
                value={cfg.csv_interval_min}
                onChange={e => {
                  let v = parseInt(e.target.value, 10);
                  if (isNaN(v) || v < 1)   v = 1;
                  if (v > 1440)             v = 1440;
                  patch('csv_interval_min', v);
                }}
                style={{
                  width:        70,
                  padding:      '5px 8px',
                  border:       '1px solid #cbd5e1',
                  borderRadius: 4,
                  fontSize:     13,
                  textAlign:    'center',
                  fontVariantNumeric: 'tabular-nums',
                }}
              />
              <label style={{ fontSize:13, color:'#475569' }}>minutes</label>
            </div>
          )}
        </LogRow>

      </div>

      {/* ── CSV column reference ── */}
      <details style={{ marginBottom: 28 }}>
        <summary style={{ cursor:'pointer', fontSize:13, color:'#64748b',
                          userSelect:'none', marginBottom:0 }}>
          CSV column reference
        </summary>
        <div style={{
          marginTop:8, padding:14,
          background:'#f8fafc', borderRadius:6,
          fontSize:12, lineHeight:1.8, overflowX:'auto',
        }}>
          <table style={{ borderCollapse:'collapse', width:'100%' }}>
            <thead>
              <tr style={{ background:'#334155', color:'#fff' }}>
                {['Column', 'Unit', 'NMEA / SeaTalk source', 'Notes'].map(h => (
                  <th key={h} style={{ padding:'6px 12px', textAlign:'left',
                                       fontSize:11, fontWeight:600 }}>{h}</th>
                ))}
              </tr>
            </thead>
            <tbody>
              {CSV_COLUMNS.map((row, i) => (
                <tr key={i} style={{ background: i%2===0?'#fff':'#f8fafc' }}>
                  <td style={tdStyle}><code>{row.col}</code></td>
                  <td style={tdStyle}>{row.unit}</td>
                  <td style={tdStyle}>{row.src}</td>
                  <td style={tdStyle}>{row.note}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </details>

      {/* ── Action buttons ── */}
      <div style={{
        display:    'flex',
        gap:        12,
        flexWrap:   'wrap',
        paddingTop: 20,
        borderTop:  '1px solid #e2e8f0',
      }}>
        <button
          onClick={handleSave}
          disabled={saving || !dirty}
          style={{
            ...styles.btn('#0e7490', '#0c6580'),
            opacity: (!dirty || saving) ? 0.5 : 1,
          }}
        >
          {saving ? '⏳ Saving…' : '💾 Save Configuration'}
        </button>

        <button
          onClick={handleNewSession}
          disabled={newSes}
          style={styles.btn('#475569', '#334155')}
        >
          {newSes ? '⏳ Starting…' : '🔄 New Logbook Session'}
        </button>
      </div>

      {dirty && (
        <p style={{ marginTop:10, fontSize:12, color:'#b45309' }}>
          ⚠ Unsaved changes — click "Save Configuration" to apply.
        </p>
      )}

    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// Style helpers
// ─────────────────────────────────────────────────────────────────────────────

const tdStyle = {
  padding:     '5px 12px',
  borderBottom:'1px solid #e2e8f0',
  fontSize:    12,
  color:       '#334155',
};

const styles = {
  sectionTitle: {
    fontSize:      15,
    fontWeight:    700,
    color:         '#0f172a',
    borderBottom:  '2px solid #0e7490',
    paddingBottom: 8,
    marginBottom:  16,
  },
  btn: (bg, hover) => ({
    background:   bg,
    color:        '#fff',
    border:       'none',
    borderRadius: 6,
    padding:      '10px 22px',
    fontSize:     13,
    fontWeight:   600,
    cursor:       'pointer',
  }),
  notice: (bg, text, border) => ({
    padding:      '12px 16px',
    background:   bg,
    border:       `1px solid ${border}`,
    borderLeft:   `4px solid ${text}`,
    borderRadius: 4,
    fontSize:     13,
    color:        text,
    marginBottom: 20,
  }),
};

// ─────────────────────────────────────────────────────────────────────────────
// CSV column reference data
// ─────────────────────────────────────────────────────────────────────────────

const CSV_COLUMNS = [
  { col:'timestamp_ms',  unit:'ms',  src:'millis()',      note:'ESP32 uptime in milliseconds' },
  { col:'lat',           unit:'°',   src:'GGA / RMC',     note:'Decimal degrees, negative = S' },
  { col:'lon',           unit:'°',   src:'GGA / RMC',     note:'Decimal degrees, negative = W' },
  { col:'sog_kn',        unit:'kn',  src:'RMC / VTG',     note:'Speed Over Ground' },
  { col:'cog_deg',       unit:'°',   src:'RMC / VTG',     note:'Course Over Ground 0–360' },
  { col:'stw_kn',        unit:'kn',  src:'VHW / ST1 0x10',note:'Speed Through Water' },
  { col:'hdg_mag_deg',   unit:'°',   src:'HDM / ST1 0x9C',note:'Magnetic heading' },
  { col:'hdg_true_deg',  unit:'°',   src:'HDT / VHW',     note:'True heading (empty if unavailable)' },
  { col:'depth_m',       unit:'m',   src:'DPT / DBT',     note:'Depth below transducer' },
  { col:'aws_kn',        unit:'kn',  src:'MWV (R) / ST1 0x11', note:'Apparent Wind Speed' },
  { col:'awa_deg',       unit:'°',   src:'MWV (R) / ST1 0x20', note:'Apparent Wind Angle −180…+180' },
  { col:'tws_kn',        unit:'kn',  src:'MWV (T) / MWD', note:'True Wind Speed' },
  { col:'twa_deg',       unit:'°',   src:'MWV (T)',        note:'True Wind Angle −180…+180' },
  { col:'twd_deg',       unit:'°',   src:'MWD / calculated', note:'True Wind Direction 0–360' },
  { col:'water_temp_c',  unit:'°C',  src:'MTW',            note:'Water temperature' },
];
