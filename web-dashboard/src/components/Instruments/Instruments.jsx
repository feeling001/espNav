import { useState, useEffect } from 'react';
import { api } from '../../services/api';

export function Instruments() {
  const [navData, setNavData]         = useState(null);
  const [windData, setWindData]       = useState(null);
  const [aisData, setAisData]         = useState(null);
  const [perfData, setPerfData]       = useState(null);
  const [polarStatus, setPolarStatus] = useState(null);
  const [loading, setLoading]         = useState(true);
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [error, setError]             = useState(null);

  useEffect(() => {
    loadData();

    const interval = setInterval(() => {
      if (autoRefresh) loadData();
    }, 2000);

    return () => clearInterval(interval);
  }, [autoRefresh]);

  const loadData = async () => {
    try {
      const [nav, wind, ais, perf, polar] = await Promise.all([
        api.getBoatNavigation(),
        api.getBoatWind(),
        api.getBoatAIS(),
        api.getPerformance(),
        api.getPolarStatus(),
      ]);

      setNavData(nav);
      setWindData(wind);
      setAisData(ais);
      setPerfData(perf);
      setPolarStatus(polar);
      setLoading(false);
      setError(null);
    } catch (err) {
      console.error('Failed to load instrument data:', err);
      setError('Failed to load data');
      setLoading(false);
    }
  };

  // ── Formatting helpers ──────────────────────────────────────

  const formatValue = (dataPoint) => {
    if (!dataPoint || !dataPoint.valid || dataPoint.value === null || dataPoint.value === undefined) {
      return { value: '--', stale: true };
    }
    const isStale = dataPoint.timestamp && (Date.now() - dataPoint.timestamp > 10000);
    return {
      value: typeof dataPoint.value === 'number' ? dataPoint.value.toFixed(1) : dataPoint.value,
      unit: dataPoint.unit || '',
      stale: isStale,
    };
  };

  const formatLatLon = (lat, lon) => {
    if (!lat || !lat.valid || !lon || !lon.valid) return '--';
    const latDeg = Math.abs(lat.value);
    const latDir = lat.value >= 0 ? 'N' : 'S';
    const lonDeg = Math.abs(lon.value);
    const lonDir = lon.value >= 0 ? 'E' : 'W';
    return `${latDeg.toFixed(5)}°${latDir}, ${lonDeg.toFixed(5)}°${lonDir}`;
  };

  const formatAISDistance = (nm) => {
    if (nm === null || nm === undefined) return '--';
    if (nm < 1) return `${(nm * 1852).toFixed(0)} m`;
    return `${nm.toFixed(2)} nm`;
  };

  const formatAISTime = (minutes) => {
    if (minutes === null || minutes === undefined || minutes < 0) return '--';
    if (minutes < 1) return `${(minutes * 60).toFixed(0)}s`;
    if (minutes < 60) return `${minutes.toFixed(0)}min`;
    return `${(minutes / 60).toFixed(1)}h`;
  };

  /**
   * Performance value from the /api/boat/performance response.
   * Returns { value, unit, stale } or { value: '--', stale: true }.
   */
  const formatPerf = (field) => {
    if (!perfData || !perfData[field] || perfData[field].value === null) {
      return { value: '--', unit: '', stale: true };
    }
    const d = perfData[field];
    return {
      value: typeof d.value === 'number' ? d.value.toFixed(1) : d.value,
      unit:  d.unit || '',
      stale: false,
    };
  };

  /**
   * Colour for the polarPct card background gradient.
   * < 80 % red, 80–95 % orange, ≥ 95 % green (matches the instrument style).
   */
  const polarPctGradient = () => {
    if (!perfData?.polar_pct?.value) return 'linear-gradient(135deg, #95a5a6 0%, #7f8c8d 100%)';
    const v = perfData.polar_pct.value;
    if (v >= 95)  return 'linear-gradient(135deg, #27ae60 0%, #1e8449 100%)';
    if (v >= 80)  return 'linear-gradient(135deg, #e67e22 0%, #ca6f1e 100%)';
    return              'linear-gradient(135deg, #e74c3c 0%, #c0392b 100%)';
  };

  /**
   * VMG direction indicator.
   * Positive VMG = upwind (▲), negative = downwind (▼).
   */
  const vmgLabel = () => {
    if (!perfData?.vmg?.value) return '';
    return perfData.vmg.value >= 0 ? '▲ upwind' : '▼ downwind';
  };

  if (loading) return <div className="page">Loading instruments…</div>;

  return (
    <div className="page">

      {/* ── Header ── */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 20 }}>
        <h2>⚓ Marine Instruments</h2>
        <label>
          <input
            type="checkbox"
            checked={autoRefresh}
            onChange={(e) => setAutoRefresh(e.target.checked)}
          />
          {' '}Auto-refresh (2 s)
        </label>
      </div>

      {error && <div className="message error" style={{ marginBottom: 20 }}>{error}</div>}

      {/* ══════════════════════════════════════════════════════
          NAVIGATION
      ══════════════════════════════════════════════════════ */}
      <div className="instrument-section">
        <h3>🧭 Navigation</h3>
        <div className="instrument-grid">

          <div className="instrument-card">
            <div className="instrument-label">Position</div>
            <div className="instrument-value" style={{ fontSize: 14 }}>
              {navData?.gps?.position
                ? formatLatLon(navData.gps.position.lat, navData.gps.position.lon)
                : '--'}
            </div>
          </div>

          <div className={`instrument-card ${formatValue(navData?.gps?.sog).stale ? 'stale' : ''}`}>
            <div className="instrument-label">Speed Over Ground</div>
            <div className="instrument-value">
              {formatValue(navData?.gps?.sog).value}
              <span className="instrument-unit">{formatValue(navData?.gps?.sog).unit}</span>
            </div>
          </div>

          <div className={`instrument-card ${formatValue(navData?.gps?.cog).stale ? 'stale' : ''}`}>
            <div className="instrument-label">Course Over Ground</div>
            <div className="instrument-value">
              {formatValue(navData?.gps?.cog).value}
              <span className="instrument-unit">°</span>
            </div>
          </div>

          <div className={`instrument-card ${formatValue(navData?.heading?.magnetic).stale ? 'stale' : ''}`}>
            <div className="instrument-label">Heading (Magnetic)</div>
            <div className="instrument-value">
              {formatValue(navData?.heading?.magnetic).value}
              <span className="instrument-unit">°</span>
            </div>
          </div>

          <div className={`instrument-card ${formatValue(navData?.heading?.true_heading).stale ? 'stale' : ''}`}>
            <div className="instrument-label">Heading (True)</div>
            <div className="instrument-value">
              {formatValue(navData?.heading?.true_heading).value}
              <span className="instrument-unit">°</span>
            </div>
          </div>

          <div className={`instrument-card ${formatValue(navData?.speed?.stw).stale ? 'stale' : ''}`}>
            <div className="instrument-label">Speed Through Water</div>
            <div className="instrument-value">
              {formatValue(navData?.speed?.stw).value}
              <span className="instrument-unit">{formatValue(navData?.speed?.stw).unit}</span>
            </div>
          </div>

          <div className={`instrument-card ${formatValue(navData?.depth?.below_transducer).stale ? 'stale' : ''}`}>
            <div className="instrument-label">Depth</div>
            <div className="instrument-value">
              {formatValue(navData?.depth?.below_transducer).value}
              <span className="instrument-unit">{formatValue(navData?.depth?.below_transducer).unit}</span>
            </div>
          </div>

          <div className={`instrument-card ${formatValue(navData?.gps?.satellites).stale ? 'stale' : ''}`}>
            <div className="instrument-label">Satellites</div>
            <div className="instrument-value">
              {formatValue(navData?.gps?.satellites).value}
            </div>
          </div>

        </div>
      </div>

      {/* ══════════════════════════════════════════════════════
          PERFORMANCE  (between Navigation and Wind)
      ══════════════════════════════════════════════════════ */}
      <div className="instrument-section">
        <h3>
          ⚡ Performance
          {polarStatus && !polarStatus.loaded && (
            <span style={{
              marginLeft: 12,
              fontSize: 12,
              fontWeight: 400,
              color: '#e67e22',
              background: '#fef9e7',
              border: '1px solid #f0ca4d',
              borderRadius: 4,
              padding: '2px 8px',
            }}>
              No polar — upload via Performance page
            </span>
          )}
        </h3>

        <div className="instrument-grid">

          {/* VMG card */}
          <div
            className="instrument-card"
            style={{
              background: formatPerf('vmg').stale
                ? 'linear-gradient(135deg, #95a5a6 0%, #7f8c8d 100%)'
                : 'linear-gradient(135deg, #2980b9 0%, #1a5276 100%)',
              opacity: formatPerf('vmg').stale ? 0.7 : 1,
            }}
          >
            <div className="instrument-label">VMG</div>
            <div className="instrument-value">
              {formatPerf('vmg').value}
              {!formatPerf('vmg').stale && (
                <span className="instrument-unit">{formatPerf('vmg').unit}</span>
              )}
            </div>
            {!formatPerf('vmg').stale && (
              <div style={{ fontSize: 11, marginTop: 4, opacity: 0.85 }}>
                {vmgLabel()}
              </div>
            )}
          </div>

          {/* Polar % card */}
          <div
            className="instrument-card"
            style={{
              background: formatPerf('polar_pct').stale
                ? 'linear-gradient(135deg, #95a5a6 0%, #7f8c8d 100%)'
                : polarPctGradient(),
              opacity: formatPerf('polar_pct').stale ? 0.7 : 1,
            }}
          >
            <div className="instrument-label">Polar Efficiency</div>
            <div className="instrument-value">
              {formatPerf('polar_pct').value}
              {!formatPerf('polar_pct').stale && (
                <span className="instrument-unit">%</span>
              )}
            </div>
            {!formatPerf('polar_pct').stale && polarStatus?.loaded && (
              <div style={{ fontSize: 11, marginTop: 4, opacity: 0.85 }}>
                {perfData.polar_pct.value >= 95  ? 'On target 🎯'  :
                 perfData.polar_pct.value >= 80  ? 'Room to improve' :
                                                   'Below polar ⚠️'}
              </div>
            )}
          </div>

        </div>
      </div>

      {/* ══════════════════════════════════════════════════════
          WIND
      ══════════════════════════════════════════════════════ */}
      <div className="instrument-section">
        <h3>💨 Wind</h3>
        <div className="instrument-grid">

          <div className={`instrument-card ${formatValue(windData?.aws).stale ? 'stale' : ''}`}>
            <div className="instrument-label">Apparent Wind Speed</div>
            <div className="instrument-value">
              {formatValue(windData?.aws).value}
              <span className="instrument-unit">{formatValue(windData?.aws).unit}</span>
            </div>
          </div>

          <div className={`instrument-card ${formatValue(windData?.awa).stale ? 'stale' : ''}`}>
            <div className="instrument-label">Apparent Wind Angle</div>
            <div className="instrument-value">
              {formatValue(windData?.awa).value}
              <span className="instrument-unit">°</span>
            </div>
          </div>

          <div className={`instrument-card ${formatValue(windData?.tws).stale ? 'stale' : ''}`}>
            <div className="instrument-label">True Wind Speed</div>
            <div className="instrument-value">
              {formatValue(windData?.tws).value}
              <span className="instrument-unit">{formatValue(windData?.tws).unit}</span>
            </div>
          </div>

          <div className={`instrument-card ${formatValue(windData?.twa).stale ? 'stale' : ''}`}>
            <div className="instrument-label">True Wind Angle</div>
            <div className="instrument-value">
              {formatValue(windData?.twa).value}
              <span className="instrument-unit">°</span>
            </div>
          </div>

          <div className={`instrument-card ${formatValue(windData?.twd).stale ? 'stale' : ''}`}>
            <div className="instrument-label">True Wind Direction</div>
            <div className="instrument-value">
              {formatValue(windData?.twd).value}
              <span className="instrument-unit">°</span>
            </div>
          </div>

          <div className={`instrument-card ${formatValue(windData?.environment?.water_temp).stale ? 'stale' : ''}`}>
            <div className="instrument-label">Water Temperature</div>
            <div className="instrument-value">
              {formatValue(windData?.environment?.water_temp).value}
              <span className="instrument-unit">{formatValue(windData?.environment?.water_temp).unit}</span>
            </div>
          </div>

        </div>
      </div>

      {/* ══════════════════════════════════════════════════════
          AIS
      ══════════════════════════════════════════════════════ */}
      <div className="instrument-section">
        <h3>🚢 AIS Targets</h3>
        {!aisData?.targets || aisData.targets.length === 0 ? (
          <div style={{ padding: 20, textAlign: 'center', color: '#7f8c8d' }}>
            No AIS targets detected
          </div>
        ) : (
          <div className="ais-table-container">
            <table className="ais-table">
              <thead>
                <tr>
                  <th>MMSI</th>
                  <th>Name</th>
                  <th>Distance</th>
                  <th>Bearing</th>
                  <th>SOG</th>
                  <th>COG</th>
                  <th>CPA</th>
                  <th>TCPA</th>
                </tr>
              </thead>
              <tbody>
                {aisData.targets.map((target, idx) => (
                  <tr key={target.mmsi || idx} className={target.cpa < 0.5 ? 'ais-warning' : ''}>
                    <td>{target.mmsi || '--'}</td>
                    <td>{target.name || '--'}</td>
                    <td>{formatAISDistance(target.distance)}</td>
                    <td>{target.bearing != null ? `${target.bearing.toFixed(0)}°` : '--'}</td>
                    <td>{target.sog    != null ? `${target.sog.toFixed(1)} kn`    : '--'}</td>
                    <td>{target.cog    != null ? `${target.cog.toFixed(0)}°`      : '--'}</td>
                    <td className={target.cpa < 0.5 ? 'ais-cpa-warning' : ''}>
                      {formatAISDistance(target.cpa)}
                    </td>
                    <td>{formatAISTime(target.tcpa)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* ══════════════════════════════════════════════════════
          LOG
      ══════════════════════════════════════════════════════ */}
      {navData?.speed && (navData.speed.trip?.valid || navData.speed.total?.valid) && (
        <div className="instrument-section">
          <h3>📊 Log</h3>
          <div className="instrument-grid">

            <div className={`instrument-card ${formatValue(navData?.speed?.trip).stale ? 'stale' : ''}`}>
              <div className="instrument-label">Trip Distance</div>
              <div className="instrument-value">
                {formatValue(navData?.speed?.trip).value}
                <span className="instrument-unit">{formatValue(navData?.speed?.trip).unit}</span>
              </div>
            </div>

            <div className={`instrument-card ${formatValue(navData?.speed?.total).stale ? 'stale' : ''}`}>
              <div className="instrument-label">Total Distance</div>
              <div className="instrument-value">
                {formatValue(navData?.speed?.total).value}
                <span className="instrument-unit">{formatValue(navData?.speed?.total).unit}</span>
              </div>
            </div>

          </div>
        </div>
      )}

    </div>
  );
}
