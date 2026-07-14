import { useState, useEffect } from 'react';
import { api } from '../../services/api';

// ── Static rule catalogue ─────────────────────────────────────────────────────
// Order and IDs must match CONV_RULE_IDS in web_server.cpp / types.h
const RULE_CATALOGUE = [
  {
    id: 'gps_cog_to_st',
    label: 'GPS COG → SeaTalk',
    source: 'NMEA0183',
    target: 'SeaTalk1',
    datagram: '0x53',
    description: 'Course Over Ground (from RMC/VTG) → SeaTalk datagram 53',
  },
  {
    id: 'gps_sog_to_st',
    label: 'GPS SOG → SeaTalk',
    source: 'NMEA0183',
    target: 'SeaTalk1',
    datagram: '0x52',
    description: 'Speed Over Ground (from RMC/VTG) → SeaTalk datagram 52',
  },
  {
    id: 'gps_pos_to_st',
    label: 'GPS Position → SeaTalk',
    source: 'NMEA0183',
    target: 'SeaTalk1',
    datagram: '0x50 + 0x51',
    description: 'Latitude/Longitude (from RMC/GGA/GLL) → SeaTalk datagrams 50 & 51',
  },
  {
    id: 'depth_to_st',
    label: 'Depth → SeaTalk',
    source: 'NMEA0183',
    target: 'SeaTalk1',
    datagram: '0x00',
    description: 'Depth below transducer (from DPT/DBT) → SeaTalk datagram 00',
  },
  {
    id: 'stw_to_st',
    label: 'Speed Through Water → SeaTalk',
    source: 'NMEA0183',
    target: 'SeaTalk1',
    datagram: '0x20',
    description: 'Speed through water (from VHW) → SeaTalk datagram 20',
  },
  {
    id: 'awa_to_st',
    label: 'AWA → SeaTalk',
    source: 'NMEA0183',
    target: 'SeaTalk1',
    datagram: '0x10',
    description: 'Apparent Wind Angle (from MWV) → SeaTalk datagram 10',
    nativeOnSeatalk: true,
  },
  {
    id: 'aws_to_st',
    label: 'AWS → SeaTalk',
    source: 'NMEA0183',
    target: 'SeaTalk1',
    datagram: '0x11',
    description: 'Apparent Wind Speed (from MWV) → SeaTalk datagram 11',
    nativeOnSeatalk: true,
  },
  {
    id: 'water_temp_to_st',
    label: 'Water Temperature → SeaTalk',
    source: 'NMEA0183',
    target: 'SeaTalk1',
    datagram: '0x27',
    description: 'Water temperature (from MTW) → SeaTalk datagram 27',
  },
  {
    id: 'hdg_to_st',
    label: 'Heading → SeaTalk',
    source: 'NMEA0183',
    target: 'SeaTalk1',
    datagram: '0x9C',
    description: 'Magnetic compass heading (from HDM/HDG) → SeaTalk datagram 9C',
  },
  {
    id: 'tw_to_nmea',
    label: 'True Wind → NMEA0183',
    source: 'Calculated',
    target: 'NMEA0183',
    datagram: 'MWV(T)',
    description: 'TWA + TWS calculated from AWA/AWS/STW → $IIMWV sentence (True reference) broadcast on TCP',
  },
];

const INTERVALS = [1, 2, 5, 10, 30, 60];

export function ConversionsConfig() {
  const [rules, setRules]     = useState([]);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving]   = useState(false);
  const [message, setMessage] = useState(null);

  // ── Load ──────────────────────────────────────────────────────────────────
  useEffect(() => {
    load();
  }, []);

  const load = async () => {
    setLoading(true);
    try {
      const data = await api.getConversionConfig();
      // Merge server state with the catalogue (server may omit unknown rules)
      const merged = RULE_CATALOGUE.map(cat => {
        const server = (data.rules || []).find(r => r.id === cat.id);
        return {
          ...cat,
          enabled:    server ? server.enabled    : false,
          interval_s: server ? server.interval_s : 1,
        };
      });
      setRules(merged);
    } catch {
      setMessage({ type: 'error', text: 'Failed to load conversion config' });
    } finally {
      setLoading(false);
    }
  };

  // ── Toggle enabled ────────────────────────────────────────────────────────
  const toggleEnabled = (id) => {
    setRules(prev => prev.map(r =>
      r.id === id ? { ...r, enabled: !r.enabled } : r
    ));
  };

  // ── Set interval ──────────────────────────────────────────────────────────
  const setInterval = (id, interval_s) => {
    setRules(prev => prev.map(r =>
      r.id === id ? { ...r, interval_s } : r
    ));
  };

  // ── Save ──────────────────────────────────────────────────────────────────
  const handleSave = async () => {
    setSaving(true);
    setMessage(null);
    try {
      const payload = {
        rules: rules.map(r => ({
          id:         r.id,
          enabled:    r.enabled,
          interval_s: r.interval_s,
        })),
      };
      await api.setConversionConfig(payload);
      setMessage({ type: 'success', text: 'Conversion config saved' });
    } catch {
      setMessage({ type: 'error', text: 'Failed to save conversion config' });
    } finally {
      setSaving(false);
    }
  };

  if (loading) return <div className="page">Loading...</div>;

  const enabledCount = rules.filter(r => r.enabled).length;

  return (
    <div>
      <div style={{ marginBottom: '16px' }}>
        <p style={{ margin: '0 0 8px', color: '#94a3b8', fontSize: '14px' }}>
          Bridge data between buses. Each rule reads data from the source and re-transmits
          it on the target at the chosen interval.
          SeaTalk encoding follows the{' '}
          <a
            href="http://www.thomasknauf.de/rap/seatalk2.htm"
            target="_blank"
            rel="noreferrer"
            style={{ color: '#38bdf8' }}
          >
            Thomas Knauf SeaTalk reference
          </a>.
          Rules marked <span style={{ color: '#fb923c', fontSize: '12px' }}>⚠ native on SeaTalk</span> are
          only needed when the sensor is NMEA-only (e.g. no SeaTalk wind instrument).
        </p>
      </div>

      {message && (
        <div className={`message ${message.type}`} style={{ marginBottom: '16px' }}>
          {message.text}
        </div>
      )}

      <div style={{ overflowX: 'auto' }}>
        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '14px' }}>
          <thead>
            <tr style={{ borderBottom: '1px solid #334155' }}>
              <th style={thStyle}>Active</th>
              <th style={{ ...thStyle, textAlign: 'left' }}>Conversion</th>
              <th style={thStyle}>Datagram</th>
              <th style={thStyle}>Interval</th>
              <th style={{ ...thStyle, textAlign: 'left' }}>Description</th>
            </tr>
          </thead>
          <tbody>
            {rules.map(rule => (
              <tr
                key={rule.id}
                style={{
                  borderBottom: '1px solid #1e293b',
                  background: rule.enabled ? 'rgba(56,189,248,0.05)' : 'transparent',
                  transition: 'background 0.15s',
                }}
              >
                {/* Toggle */}
                <td style={tdStyle}>
                  <label style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', cursor: 'pointer' }}>
                    <input
                      type="checkbox"
                      checked={rule.enabled}
                      onChange={() => toggleEnabled(rule.id)}
                      style={{ width: '16px', height: '16px', cursor: 'pointer' }}
                    />
                  </label>
                </td>

                {/* Label */}
                <td style={{ ...tdStyle, textAlign: 'left', fontWeight: rule.enabled ? '600' : '400', whiteSpace: 'nowrap' }}>
                  <span style={{ display: 'inline-flex', alignItems: 'center', gap: '6px', flexWrap: 'wrap' }}>
                    <BusTag bus={rule.source} />
                    <span style={{ color: '#64748b' }}>→</span>
                    <BusTag bus={rule.target} />
                    <span style={{ color: rule.enabled ? '#e2e8f0' : '#64748b' }}>
                      {rule.label.split('→')[0].trim()}
                    </span>
                    {rule.nativeOnSeatalk && (
                      <span title="AWA/AWS are normally already broadcast natively by the SeaTalk wind instrument — only enable this if your wind sensor is NMEA-only" style={{
                        padding: '1px 6px',
                        borderRadius: '8px',
                        fontSize: '10px',
                        background: '#422006',
                        color: '#fb923c',
                        border: '1px solid #7c2d12',
                        whiteSpace: 'nowrap',
                        cursor: 'help',
                      }}>
                        ⚠ native on SeaTalk
                      </span>
                    )}
                  </span>
                </td>

                {/* Datagram */}
                <td style={tdStyle}>
                  <span style={{
                    display: 'inline-block',
                    padding: '2px 8px',
                    borderRadius: '12px',
                    fontSize: '11px',
                    background: '#0f172a',
                    color: '#7dd3fc',
                    border: '1px solid #1e3a5f',
                    fontFamily: 'monospace',
                  }}>
                    {rule.datagram}
                  </span>
                </td>

                {/* Interval */}
                <td style={{ ...tdStyle, textAlign: 'center' }}>
                  <select
                    value={rule.interval_s}
                    onChange={e => setInterval(rule.id, parseInt(e.target.value))}
                    disabled={!rule.enabled}
                    style={{
                      background: '#1e293b',
                      color: rule.enabled ? '#e2e8f0' : '#475569',
                      border: '1px solid #334155',
                      borderRadius: '4px',
                      padding: '3px 6px',
                      fontSize: '13px',
                      cursor: rule.enabled ? 'pointer' : 'not-allowed',
                    }}
                  >
                    {INTERVALS.map(s => (
                      <option key={s} value={s}>{s}s</option>
                    ))}
                  </select>
                </td>

                {/* Description */}
                <td style={{ ...tdStyle, textAlign: 'left', color: '#94a3b8', fontSize: '12px' }}>
                  {rule.description}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <div style={{ marginTop: '20px', display: 'flex', gap: '12px', alignItems: 'center' }}>
        <button
          onClick={handleSave}
          disabled={saving}
          style={{
            padding: '8px 20px',
            background: '#0ea5e9',
            color: '#fff',
            border: 'none',
            borderRadius: '6px',
            fontSize: '14px',
            cursor: saving ? 'wait' : 'pointer',
            opacity: saving ? 0.7 : 1,
          }}
        >
          {saving ? 'Saving…' : 'Save'}
        </button>
        <span style={{ fontSize: '13px', color: '#64748b' }}>
          {enabledCount} rule{enabledCount !== 1 ? 's' : ''} active
        </span>
      </div>

      <div style={{
        marginTop: '20px',
        padding: '12px 16px',
        background: '#0f172a',
        borderRadius: '6px',
        borderLeft: '3px solid #0ea5e9',
        fontSize: '13px',
        color: '#94a3b8',
      }}>
        <strong style={{ color: '#e2e8f0' }}>Note: </strong>
        A rule only transmits when fresh (non-stale) data is available from
        the source bus. If the source instrument is offline the datagram is
        silently skipped until data resumes.
      </div>
    </div>
  );
}

// ── Sub-components ────────────────────────────────────────────────────────────

function BusTag({ bus }) {
  const colors = {
    'NMEA0183':  { bg: '#14532d', color: '#4ade80', border: '#166534' },
    'SeaTalk1':  { bg: '#1e1b4b', color: '#818cf8', border: '#3730a3' },
    'Calculated':{ bg: '#1c1917', color: '#a8a29e', border: '#44403c' },
  };
  const style = colors[bus] || { bg: '#1e293b', color: '#94a3b8', border: '#334155' };
  return (
    <span style={{
      padding: '1px 7px',
      borderRadius: '10px',
      fontSize: '11px',
      background: style.bg,
      color: style.color,
      border: `1px solid ${style.border}`,
      whiteSpace: 'nowrap',
    }}>
      {bus}
    </span>
  );
}

// ── Styles ────────────────────────────────────────────────────────────────────

const thStyle = {
  padding: '8px 12px',
  textAlign: 'center',
  color: '#64748b',
  fontWeight: '600',
  fontSize: '12px',
  textTransform: 'uppercase',
  letterSpacing: '0.05em',
};

const tdStyle = {
  padding: '10px 12px',
  textAlign: 'center',
  verticalAlign: 'middle',
};
