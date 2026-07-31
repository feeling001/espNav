import { useState, useEffect } from 'react';
import { api } from '../../services/api';

// ── Static field catalogue ────────────────────────────────────────────────
// `id` and each option `value` must match DS_FIELD_IDS / DS_SUB_IDS in
// data_source_manager.cpp / types.h.
const FIELD_CATALOGUE = [
  {
    id: 'gps_position',
    label: 'Position GPS (Lat/Lon)',
    options: [
      { value: 'nmea_gga', label: 'NMEA – GGA' },
      { value: 'nmea_rmc', label: 'NMEA – RMC' },
      { value: 'nmea_gll', label: 'NMEA – GLL' },
      { value: 'seatalk',  label: 'SeaTalk – 0x50/0x51' },
    ],
  },
  {
    id: 'gps_sog',
    label: 'Vitesse fond (SOG)',
    options: [
      { value: 'nmea_rmc', label: 'NMEA – RMC' },
      { value: 'nmea_vtg', label: 'NMEA – VTG' },
      { value: 'seatalk',  label: 'SeaTalk – 0x52' },
    ],
  },
  {
    id: 'gps_cog',
    label: 'Route fond (COG)',
    options: [
      { value: 'nmea_rmc', label: 'NMEA – RMC' },
      { value: 'nmea_vtg', label: 'NMEA – VTG' },
      { value: 'seatalk',  label: 'SeaTalk – 0x53' },
    ],
  },
  {
    id: 'heading_mag',
    label: 'Cap magnétique',
    options: [
      { value: 'nmea_hdm', label: 'NMEA – HDM' },
      { value: 'nmea_vhw', label: 'NMEA – VHW (VWVHW)' },
      { value: 'seatalk',  label: 'SeaTalk – 0x9C' },
    ],
  },
  {
    id: 'heading_true',
    label: 'Cap vrai',
    options: [
      { value: 'nmea_hdt', label: 'NMEA – HDT' },
      { value: 'nmea_vhw', label: 'NMEA – VHW (VWVHW)' },
      { value: 'compute',  label: 'Calculé (cap magnétique + variation)' },
    ],
  },
  {
    id: 'stw',
    label: 'Vitesse surface (STW)',
    options: [
      { value: 'nmea_vhw', label: 'NMEA – VHW (VWVHW)' },
      { value: 'seatalk',  label: 'SeaTalk – 0x20' },
    ],
  },
  {
    id: 'depth',
    label: 'Profondeur',
    options: [
      { value: 'nmea_dpt', label: 'NMEA – DPT' },
      { value: 'nmea_dbt', label: 'NMEA – DBT' },
      { value: 'seatalk',  label: 'SeaTalk – 0x00' },
    ],
  },
  {
    id: 'wind_apparent',
    label: 'Vent apparent (AWS/AWA)',
    options: [
      { value: 'nmea_mwv', label: 'NMEA – MWV' },
      { value: 'seatalk',  label: 'SeaTalk – 0x10/0x11' },
    ],
  },
  {
    id: 'wind_true',
    label: 'Vent réel (TWS/TWA/TWD)',
    options: [
      { value: 'nmea_mwv', label: 'NMEA – MWV(T)' },
      { value: 'nmea_mwd', label: 'NMEA – MWD' },
      { value: 'compute',  label: 'Calculé (AWS/AWA + STW + cap)' },
    ],
  },
  {
    id: 'water_temp',
    label: 'Température de l\u2019eau',
    options: [
      { value: 'nmea_mtw', label: 'NMEA – MTW' },
      { value: 'seatalk',  label: 'SeaTalk – 0x27' },
    ],
  },
  {
    id: 'trip',
    label: 'Distance trip (loch)',
    options: [
      { value: 'nmea_vlw', label: 'NMEA – VLW' },
      { value: 'seatalk',  label: 'SeaTalk – 0x21' },
    ],
  },
  {
    id: 'total',
    label: 'Distance totale (loch)',
    options: [
      { value: 'nmea_vlw', label: 'NMEA – VLW' },
      { value: 'seatalk',  label: 'SeaTalk – 0x22' },
    ],
  },
];

export function DataSourceConfig() {
  const [sources, setSources]                 = useState({});
  const [magneticVariation, setMagVar]         = useState(0);
  const [loading, setLoading]                  = useState(true);
  const [saving, setSaving]                    = useState(false);
  const [message, setMessage]                  = useState(null);

  useEffect(() => {
    load();
  }, []);

  const load = async () => {
    setLoading(true);
    try {
      const data = await api.getDataSourceConfig();
      const map = {};
      (data.fields || []).forEach(f => { map[f.id] = f.source; });
      setSources(map);
      setMagVar(data.magneticVariation || 0);
    } catch {
      setMessage({ type: 'error', text: 'Failed to load data source config' });
    } finally {
      setLoading(false);
    }
  };

  const setSource = (id, value) => {
    setSources(prev => ({ ...prev, [id]: value }));
  };

  const handleSave = async () => {
    setSaving(true);
    setMessage(null);
    try {
      const payload = {
        fields: FIELD_CATALOGUE.map(f => ({ id: f.id, source: sources[f.id] })),
        magneticVariation: parseFloat(magneticVariation) || 0,
      };
      await api.setDataSourceConfig(payload);
      setMessage({ type: 'success', text: 'Data source config saved' });
    } catch {
      setMessage({ type: 'error', text: 'Failed to save data source config' });
    } finally {
      setSaving(false);
    }
  };

  if (loading) return <div className="page">Loading...</div>;

  return (
    <div>
      <div style={{ marginBottom: '16px' }}>
        <p style={{ margin: '0 0 8px', color: '#94a3b8', fontSize: '14px' }}>
          Choisissez la source qui alimente chaque donnée du bateau : une phrase
          NMEA précise, un datagramme SeaTalk précis, ou une valeur calculée
          localement (quand disponible).
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
              <th style={{ ...thStyle, textAlign: 'left' }}>Donnée</th>
              <th style={{ ...thStyle, textAlign: 'left' }}>Source</th>
            </tr>
          </thead>
          <tbody>
            {FIELD_CATALOGUE.map(field => (
              <tr key={field.id} style={{ borderBottom: '1px solid #1e293b' }}>
                <td style={tdStyle}>{field.label}</td>
                <td style={tdStyle}>
                  <select
                    value={sources[field.id] || field.options[0].value}
                    onChange={(e) => setSource(field.id, e.target.value)}
                  >
                    {field.options.map(opt => (
                      <option key={opt.value} value={opt.value}>{opt.label}</option>
                    ))}
                  </select>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <div className="form-group" style={{ marginTop: '20px', maxWidth: '320px' }}>
        <label>Variation magnétique (deg, + Est / - Ouest)</label>
        <input
          type="number"
          step="0.1"
          value={magneticVariation}
          onChange={(e) => setMagVar(e.target.value)}
        />
        <p style={{ margin: '4px 0 0', color: '#64748b', fontSize: '12px' }}>
          Utilisée pour calculer le cap vrai = cap magnétique + variation.
        </p>
      </div>

      <button
        onClick={handleSave}
        disabled={saving}
        style={{ marginTop: '20px' }}
      >
        {saving ? 'Saving...' : 'Save Configuration'}
      </button>
    </div>
  );
}

const thStyle = {
  padding: '8px',
  textAlign: 'center',
  color: '#94a3b8',
  fontWeight: 600,
};

const tdStyle = {
  padding: '8px',
  verticalAlign: 'middle',
};
