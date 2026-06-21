import { useState, useEffect, useRef } from 'react';
import { api } from '../../services/api';

/**
 * Performance page — upload a polar diagram and view its status.
 * The polar file is stored on the ESP32 (LittleFS) and survives reboots.
 */
export function Performance() {
  const [status, setStatus]           = useState(null);
  const [dragging, setDragging]       = useState(false);
  const [file, setFile]               = useState(null);
  const [uploading, setUploading]     = useState(false);
  const [progress, setProgress]       = useState(0);
  const [message, setMessage]         = useState(null);
  const [dampingTau, setDampingTau]   = useState(0);
  const [dampingSaving, setDampingSaving] = useState(false);
  const [dampingMsg, setDampingMsg]   = useState(null);
  const inputRef = useRef(null);

  useEffect(() => {
    loadStatus();
    loadDampingConfig();
  }, []);

  const loadStatus = async () => {
    try {
      const data = await api.getPolarStatus();
      setStatus(data);
    } catch {
      setStatus(null);
    }
  };

  const loadDampingConfig = async () => {
    try {
      const data = await api.getPerformanceConfig();
      setDampingTau(data.damping_tau ?? 0);
    } catch {
      // keep default 0
    }
  };

  const saveDamping = async () => {
    setDampingSaving(true);
    setDampingMsg(null);
    try {
      await api.setPerformanceConfig({ damping_tau: dampingTau });
      setDampingMsg({ type: 'success', text: `Damping saved (τ = ${dampingTau} s).` });
    } catch (err) {
      setDampingMsg({ type: 'error', text: err.message });
    } finally {
      setDampingSaving(false);
    }
  };

  // ── Drag & drop handlers ──────────────────────────────────────

  const onDragOver = (e) => { e.preventDefault(); setDragging(true); };
  const onDragLeave = ()  => setDragging(false);

  const onDrop = (e) => {
    e.preventDefault();
    setDragging(false);
    const dropped = e.dataTransfer.files[0];
    if (dropped) pickFile(dropped);
  };

  const onFileChange = (e) => {
    if (e.target.files[0]) pickFile(e.target.files[0]);
  };

  const pickFile = (f) => {
    setFile(f);
    setMessage(null);
  };

  // ── Upload ────────────────────────────────────────────────────

  const handleUpload = async () => {
    if (!file) return;
    setUploading(true);
    setProgress(0);
    setMessage(null);

    try {
      const result = await api.uploadPolar(file, setProgress);
      if (result.success) {
        setMessage({ type: 'success', text: result.message || 'Polar loaded successfully.' });
        setFile(null);
        await loadStatus();
      } else {
        setMessage({ type: 'error', text: result.error || 'Upload failed.' });
      }
    } catch (err) {
      setMessage({ type: 'error', text: err.message });
    } finally {
      setUploading(false);
      setProgress(0);
    }
  };

  // ── Render ────────────────────────────────────────────────────

  return (
    <div className="page">
      <h2>⚡ Performance &amp; Polar</h2>

      {/* ── Current polar status ── */}
      <div className="polar-status-box">
        <h3>Current Polar</h3>
        {status === null ? (
          <p className="polar-status-text muted">Loading status…</p>
        ) : status.loaded ? (
          <div className="polar-status-loaded">
            <span className="polar-badge polar-badge--ok">✓ Loaded</span>
            <div className="polar-meta">
              <span>{status.twa_count} TWA angles</span>
              <span>·</span>
              <span>{status.tws_count} wind speeds</span>
              <span>·</span>
              <span>{(status.file_size / 1024).toFixed(1)} KB</span>
            </div>
            <div className="polar-tws">
              <span className="polar-tws-label">Wind speeds (kn):</span>
              <span className="polar-tws-values">{status.tws_list}</span>
            </div>
          </div>
        ) : (
          <div>
            <span className="polar-badge polar-badge--none">✗ No polar loaded</span>
            <p className="polar-status-text muted" style={{ marginTop: 8 }}>
              Upload a polar file below to enable performance calculations.
            </p>
          </div>
        )}
      </div>

      {/* ── Upload area ── */}
      <div className="polar-upload-section">
        <h3>Upload Polar File</h3>
        <p className="muted" style={{ marginBottom: 16, fontSize: 13 }}>
          Accepts tab-delimited polar files (.pol, .csv).
          First row: wind speed columns (kn). First column: TWA (degrees).
        </p>

        {message && (
          <div className={`message ${message.type}`} style={{ marginBottom: 16 }}>
            {message.text}
          </div>
        )}

        {/* Drop zone */}
        <div
          className={`drop-zone ${dragging ? 'drag-over' : ''} ${file ? 'has-file' : ''}`}
          onDragOver={onDragOver}
          onDragLeave={onDragLeave}
          onDrop={onDrop}
          onClick={() => inputRef.current?.click()}
        >
          <input
            ref={inputRef}
            type="file"
            accept=".pol,.csv,.txt"
            style={{ display: 'none' }}
            onChange={onFileChange}
          />
          {file ? (
            <div className="file-info">
              <span className="file-icon">📄</span>
              <div>
                <div className="file-name">{file.name}</div>
                <div className="file-size">{(file.size / 1024).toFixed(1)} KB — ready to upload</div>
              </div>
            </div>
          ) : (
            <div className="drop-hint">
              <span className="drop-icon">🧭</span>
              <p>Drop your polar file here</p>
              <p className="drop-sub">or click to browse</p>
            </div>
          )}
        </div>

        {/* Progress bar — visible only while uploading */}
        {uploading && (
          <div className="progress-container" style={{ marginBottom: 12 }}>
            <div className="progress-bar">
              <div className="progress-fill" style={{ width: `${progress}%` }} />
            </div>
            <span className="progress-label">{progress}%</span>
          </div>
        )}

        <div className="ota-actions">
          <button
            onClick={handleUpload}
            disabled={!file || uploading}
          >
            {uploading ? 'Uploading…' : '⬆ Upload Polar'}
          </button>
          {file && !uploading && (
            <button className="secondary" onClick={() => setFile(null)}>
              Cancel
            </button>
          )}
        </div>
      </div>

      {/* ── Format reference ── */}
      <div style={{ marginTop: 28, padding: 16, background: '#f8f9fa', borderRadius: 6, fontSize: 13 }}>
        <strong>Expected file format</strong>
        <pre style={{ marginTop: 8, fontSize: 12, overflowX: 'auto', color: '#555' }}>{`TWA\\TWS\t6\t8\t10\t12\t14\t16\t20
52\t6.3\t7.1\t7.6\t7.8\t7.9\t8.0\t8.1
60\t6.7\t7.5\t7.9\t8.1\t8.3\t8.4\t8.5
75\t7.0\t7.9\t8.3\t8.6\t8.8\t8.9\t9.1
90\t7.1\t8.0\t8.4\t8.8\t9.2\t9.4\t9.7`}</pre>
      </div>

      {/* ── Damping configuration ── */}
      <div className="config-section" style={{ marginTop: 28 }}>
        <h3>Performance Damping</h3>
        <p className="muted" style={{ marginBottom: 16, fontSize: 13 }}>
          Smooths the wind and speed inputs before computing the polar performance
          percentage, reducing noisy spikes caused by mast movement or sensor jitter.
          Uses an exponential moving average (EMA) with time constant τ.
          Set to <strong>0</strong> to disable damping.
        </p>

        {dampingMsg && (
          <div className={`message ${dampingMsg.type}`} style={{ marginBottom: 12 }}>
            {dampingMsg.text}
          </div>
        )}

        <div style={{ display: 'flex', alignItems: 'center', gap: 16, flexWrap: 'wrap' }}>
          <label style={{ fontWeight: 500, minWidth: 140 }}>
            τ = <strong>{dampingTau === 0 ? 'Off' : `${dampingTau} s`}</strong>
          </label>
          <input
            type="range"
            min={0}
            max={15}
            step={1}
            value={dampingTau}
            onChange={e => { setDampingTau(Number(e.target.value)); setDampingMsg(null); }}
            style={{ flex: 1, minWidth: 160, maxWidth: 320 }}
          />
          <span className="muted" style={{ fontSize: 12 }}>0 s (off) — 15 s</span>
        </div>

        <div style={{ marginTop: 8, fontSize: 12, color: '#888', display: 'flex', gap: 16 }}>
          <span>Racing: 3–5 s</span>
          <span>Cruising: 6–10 s</span>
          <span>Heavy sea: 10–15 s</span>
        </div>

        <div className="ota-actions" style={{ marginTop: 14 }}>
          <button onClick={saveDamping} disabled={dampingSaving}>
            {dampingSaving ? 'Saving…' : '💾 Save Damping'}
          </button>
        </div>
      </div>
    </div>
  );
}
