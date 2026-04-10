import { useState, useEffect, useRef } from 'react';
import { api } from '../../services/api';

/**
 * OTAUpdate — firmware update via HTTP multipart upload.
 *
 * Features:
 *  - Displays current firmware info (version, partition, compile date)
 *  - Drag-and-drop or click-to-browse for firmware.bin
 *  - XHR upload with real-time progress bar
 *  - Success / failure feedback
 *  - Auto-reboot countdown on success
 */
export function OTAUpdate() {
  const [otaStatus, setOtaStatus]     = useState(null);
  const [file, setFile]               = useState(null);
  const [dragging, setDragging]       = useState(false);
  const [uploading, setUploading]     = useState(false);
  const [progress, setProgress]       = useState(0);
  const [message, setMessage]         = useState(null);
  const [countdown, setCountdown]     = useState(null);
  const inputRef                      = useRef(null);
  const countdownRef                  = useRef(null);

  useEffect(() => {
    loadStatus();
    return () => {
      if (countdownRef.current) clearInterval(countdownRef.current);
    };
  }, []);

  const loadStatus = async () => {
    try {
      const data = await api.getOTAStatus();
      setOtaStatus(data);
    } catch {
      setOtaStatus(null);
    }
  };

  // ── Drag-and-drop ─────────────────────────────────────────
  const onDragOver  = (e) => { e.preventDefault(); setDragging(true); };
  const onDragLeave = ()  => setDragging(false);
  const onDrop      = (e) => {
    e.preventDefault();
    setDragging(false);
    const dropped = e.dataTransfer.files[0];
    if (dropped) pickFile(dropped);
  };
  const onFileChange = (e) => { if (e.target.files[0]) pickFile(e.target.files[0]); };

  const pickFile = (f) => {
    // Basic check: must be a .bin file
    if (!f.name.endsWith('.bin')) {
      setMessage({ type: 'error', text: 'Please select a firmware .bin file.' });
      return;
    }
    setFile(f);
    setMessage(null);
  };

  // ── Upload ─────────────────────────────────────────────────
  const handleUpload = () => {
    if (!file || uploading) return;

    setUploading(true);
    setProgress(0);
    setMessage(null);

    const xhr  = new XMLHttpRequest();
    const form = new FormData();
    form.append('firmware', file, file.name);

    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable) {
        setProgress(Math.round((e.loaded / e.total) * 100));
      }
    };

    xhr.onload = () => {
      setUploading(false);
      try {
        const result = JSON.parse(xhr.responseText);
        if (result.success) {
          setMessage({ type: 'success', text: result.message || 'Firmware flashed! Rebooting…' });
          setFile(null);
          startCountdown(5);
        } else {
          setMessage({ type: 'error', text: result.error || 'OTA failed.' });
        }
      } catch {
        setMessage({ type: 'error', text: 'Unexpected server response.' });
      }
    };

    xhr.onerror = () => {
      setUploading(false);
      setMessage({ type: 'error', text: 'Network error during upload.' });
    };

    xhr.open('POST', '/api/ota/upload');
    xhr.send(form);
  };

  // ── Reboot countdown ───────────────────────────────────────
  const startCountdown = (seconds) => {
    setCountdown(seconds);
    countdownRef.current = setInterval(() => {
      setCountdown((prev) => {
        if (prev <= 1) {
          clearInterval(countdownRef.current);
          // Reload page after reboot
          setTimeout(() => window.location.reload(), 4000);
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
  };

  // ── Helpers ────────────────────────────────────────────────
  const fmtBytes = (b) => {
    if (!b) return '--';
    if (b < 1024)       return `${b} B`;
    if (b < 1048576)    return `${(b / 1024).toFixed(1)} KB`;
    return `${(b / 1048576).toFixed(2)} MB`;
  };

  // ── Render ─────────────────────────────────────────────────
  return (
    <div>
      <h3 style={styles.sectionTitle}>Firmware Update (OTA)</h3>

      {/* ── Current firmware info ── */}
      {otaStatus && (
        <div style={styles.infoBox}>
          <div style={styles.infoGrid}>
            <InfoRow label="Running partition" value={otaStatus.running_partition ?? '--'} />
            <InfoRow label="Next OTA partition" value={otaStatus.next_partition    ?? '--'} />
            <InfoRow label="Version"            value={otaStatus.version           ?? '--'} />
            <InfoRow label="Compiled"
              value={otaStatus.compile_date
                ? `${otaStatus.compile_date}  ${otaStatus.compile_time ?? ''}`
                : '--'} />
            <InfoRow label="IDF version"        value={otaStatus.idf_version       ?? '--'} />
            <InfoRow label="Partition size"     value={fmtBytes(otaStatus.partition_size)} />
          </div>
        </div>
      )}

      {/* ── Important notice ── */}
      <div style={styles.notice}>
        <strong>⚠️ Important:</strong> Only upload firmware built for this exact board variant.
        An incompatible binary may brick the device. The device will reboot automatically after
        a successful flash.
      </div>

      {/* ── Message ── */}
      {message && (
        <div className={`message ${message.type}`} style={{ marginBottom: 16 }}>
          {message.type === 'success' ? '✓' : '✗'} {message.text}
          {countdown !== null && countdown > 0 && (
            <span style={{ marginLeft: 10, fontWeight: 600 }}>
              Page refreshes in {countdown} s…
            </span>
          )}
          {countdown === 0 && (
            <span style={{ marginLeft: 10, fontWeight: 600 }}>
              Waiting for device to come back online…
            </span>
          )}
        </div>
      )}

      {/* ── Drop zone ── */}
      <div
        className={`drop-zone ${dragging ? 'drag-over' : ''} ${file ? 'has-file' : ''}`}
        onDragOver={onDragOver}
        onDragLeave={onDragLeave}
        onDrop={onDrop}
        onClick={() => !uploading && inputRef.current?.click()}
        style={{ cursor: uploading ? 'not-allowed' : 'pointer', marginBottom: 16 }}
      >
        <input
          ref={inputRef}
          type="file"
          accept=".bin"
          style={{ display: 'none' }}
          onChange={onFileChange}
          disabled={uploading}
        />
        {file ? (
          <div className="file-info">
            <span className="file-icon">📦</span>
            <div>
              <div className="file-name">{file.name}</div>
              <div className="file-size">{fmtBytes(file.size)} — ready to flash</div>
            </div>
          </div>
        ) : (
          <div className="drop-hint">
            <span className="drop-icon">🔧</span>
            <p>Drop firmware.bin here</p>
            <p className="drop-sub">or click to browse (.bin only)</p>
          </div>
        )}
      </div>

      {/* ── Progress bar ── */}
      {uploading && (
        <div className="progress-container" style={{ marginBottom: 14 }}>
          <div className="progress-bar">
            <div className="progress-fill" style={{ width: `${progress}%` }} />
          </div>
          <span className="progress-label">{progress}%</span>
        </div>
      )}

      {/* ── Actions ── */}
      <div className="ota-actions">
        <button
          onClick={handleUpload}
          disabled={!file || uploading}
        >
          {uploading ? `Flashing… ${progress}%` : '⬆ Flash Firmware'}
        </button>
        {file && !uploading && (
          <button className="secondary" onClick={() => { setFile(null); setMessage(null); }}>
            Cancel
          </button>
        )}
      </div>
    </div>
  );
}

// ── Sub-components ────────────────────────────────────────────

function InfoRow({ label, value }) {
  return (
    <div style={styles.infoRow}>
      <span style={styles.infoLabel}>{label}</span>
      <span style={styles.infoValue}>{value}</span>
    </div>
  );
}

// ── Styles ────────────────────────────────────────────────────

const styles = {
  sectionTitle: {
    marginBottom: 16,
    fontSize: 16,
    color: '#2c3e50',
    borderBottom: '2px solid #3498db',
    paddingBottom: 8,
  },
  infoBox: {
    background: '#f8f9fa',
    borderRadius: 6,
    padding: '14px 18px',
    marginBottom: 16,
    border: '1px solid #e0e0e0',
  },
  infoGrid: {
    display: 'flex',
    flexDirection: 'column',
    gap: 6,
  },
  infoRow: {
    display: 'flex',
    gap: 12,
    fontSize: 13,
    lineHeight: 1.5,
  },
  infoLabel: {
    minWidth: 160,
    color: '#7f8c8d',
    fontWeight: 500,
  },
  infoValue: {
    color: '#2c3e50',
    fontFamily: 'monospace',
    fontSize: 13,
  },
  notice: {
    padding: '10px 14px',
    marginBottom: 16,
    background: '#fff3cd',
    borderLeft: '4px solid #f0ad4e',
    borderRadius: 4,
    fontSize: 13,
    lineHeight: 1.6,
  },
};
