import { useState, useEffect, useCallback } from 'react';
import { api } from '../../services/api';

/**
 * StorageManager — LittleFS file browser and management.
 *
 * Features:
 *  - Storage usage bar (used / total / free)
 *  - File listing with sizes
 *  - Per-file delete with confirmation
 *  - Full format with confirmation dialog
 */
export function StorageManager() {
  const [storageInfo, setStorageInfo]   = useState(null);
  const [files, setFiles]               = useState([]);
  const [loading, setLoading]           = useState(true);
  const [message, setMessage]           = useState(null);
  const [confirmFormat, setConfirmFormat] = useState(false);
  const [deleting, setDeleting]         = useState(null);   // path being deleted
  const [formatting, setFormatting]     = useState(false);

  const loadData = useCallback(async () => {
    try {
      const [info, fileList] = await Promise.all([
        api.getStorageInfo(),
        api.listStorageFiles(),
      ]);
      setStorageInfo(info);
      setFiles(fileList.files || []);
    } catch (err) {
      setMessage({ type: 'error', text: 'Failed to load storage info.' });
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => { loadData(); }, [loadData]);

  // ── Delete file ────────────────────────────────────────────
  const handleDelete = async (path) => {
    setDeleting(path);
    setMessage(null);
    try {
      await api.deleteStorageFile(path);
      setMessage({ type: 'success', text: `Deleted: ${path}` });
      await loadData();
    } catch (err) {
      setMessage({ type: 'error', text: `Failed to delete ${path}.` });
    } finally {
      setDeleting(null);
    }
  };

  // ── Format storage ─────────────────────────────────────────
  const handleFormat = async () => {
    setConfirmFormat(false);
    setFormatting(true);
    setMessage(null);
    try {
      await api.formatStorage();
      setMessage({ type: 'success', text: 'Storage formatted successfully.' });
      await loadData();
    } catch (err) {
      setMessage({ type: 'error', text: 'Format failed.' });
    } finally {
      setFormatting(false);
    }
  };

  // ── Helpers ────────────────────────────────────────────────
  const fmtBytes = (b) => {
    if (b == null) return '--';
    if (b < 1024)       return `${b} B`;
    if (b < 1048576)    return `${(b / 1024).toFixed(1)} KB`;
    return `${(b / 1048576).toFixed(2)} MB`;
  };

  // Determine icon by file extension
  const fileIcon = (path) => {
    const ext = path.split('.').pop().toLowerCase();
    const icons = {
      html: '🌐', js: '📜', css: '🎨', json: '📋',
      pol:  '🧭', csv: '📊', bin: '📦', txt: '📄',
      png:  '🖼',  ico: '🔖',
    };
    return icons[ext] || '📄';
  };

  // Sort: directories first then by path
  const sortedFiles = [...files].sort((a, b) => a.path.localeCompare(b.path));

  // ── Render ─────────────────────────────────────────────────
  if (loading) {
    return <div style={{ padding: 20, color: '#7f8c8d' }}>Loading storage info…</div>;
  }

  return (
    <div>
      <h3 style={styles.sectionTitle}>Storage Management (LittleFS)</h3>

      {/* ── Usage bar ── */}
      {storageInfo && (
        <div style={styles.usageCard}>
          <div style={styles.usageHeader}>
            <span style={styles.usageTitle}>Flash Storage</span>
            <span style={styles.usageStats}>
              {fmtBytes(storageInfo.used_bytes)} used
              {' / '}
              {fmtBytes(storageInfo.total_bytes)} total
              {' — '}
              <strong>{fmtBytes(storageInfo.free_bytes)} free</strong>
            </span>
          </div>
          <div style={styles.usageBarBg}>
            <div
              style={{
                ...styles.usageBarFill,
                width: `${Math.min(storageInfo.used_pct, 100)}%`,
                background: storageInfo.used_pct > 85
                  ? 'linear-gradient(90deg, #e74c3c, #c0392b)'
                  : storageInfo.used_pct > 65
                    ? 'linear-gradient(90deg, #e67e22, #d35400)'
                    : 'linear-gradient(90deg, #27ae60, #1e8449)',
              }}
            />
          </div>
          <div style={styles.usagePct}>{storageInfo.used_pct}% used</div>
        </div>
      )}

      {/* ── Message ── */}
      {message && (
        <div className={`message ${message.type}`} style={{ marginBottom: 16 }}>
          {message.type === 'success' ? '✓' : '✗'} {message.text}
        </div>
      )}

      {/* ── File list ── */}
      <div style={styles.fileListHeader}>
        <span style={styles.fileListTitle}>
          Files ({files.length})
        </span>
        <button
          className="secondary"
          onClick={loadData}
          style={{ padding: '6px 14px', fontSize: 13 }}
        >
          ↺ Refresh
        </button>
      </div>

      {files.length === 0 ? (
        <div style={styles.emptyState}>
          No files found on storage.
        </div>
      ) : (
        <div style={styles.fileList}>
          {sortedFiles.map((f) => (
            <div key={f.path} style={styles.fileRow}>
              <span style={styles.fileIcon}>{fileIcon(f.path)}</span>
              <span style={styles.filePath}>{f.path}</span>
              <span style={styles.fileSize}>{fmtBytes(f.size)}</span>
              <button
                onClick={() => handleDelete(f.path)}
                disabled={deleting === f.path}
                style={styles.deleteBtn}
                title={`Delete ${f.path}`}
              >
                {deleting === f.path ? '…' : '🗑'}
              </button>
            </div>
          ))}
        </div>
      )}

      {/* ── Danger zone ── */}
      <div style={styles.dangerZone}>
        <div style={styles.dangerTitle}>⚠️ Danger Zone</div>
        <p style={styles.dangerDesc}>
          Formatting erases all files including the web dashboard, polar diagrams, and
          configuration backups. The device will need to be reflashed after formatting
          if the dashboard is stored on LittleFS.
        </p>

        {confirmFormat ? (
          <div style={styles.confirmBox}>
            <span style={{ fontSize: 14, color: '#c0392b', fontWeight: 600 }}>
              Are you sure? This cannot be undone.
            </span>
            <div style={{ display: 'flex', gap: 10, marginTop: 10 }}>
              <button
                onClick={handleFormat}
                disabled={formatting}
                style={{ ...styles.formatBtn, background: '#e74c3c' }}
              >
                {formatting ? 'Formatting…' : 'Yes, format now'}
              </button>
              <button
                className="secondary"
                onClick={() => setConfirmFormat(false)}
              >
                Cancel
              </button>
            </div>
          </div>
        ) : (
          <button
            onClick={() => setConfirmFormat(true)}
            disabled={formatting}
            style={styles.formatBtn}
          >
            🗑 Format Storage
          </button>
        )}
      </div>
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
  usageCard: {
    background: '#f8f9fa',
    border: '1px solid #e0e0e0',
    borderRadius: 6,
    padding: '14px 18px',
    marginBottom: 20,
  },
  usageHeader: {
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'baseline',
    marginBottom: 10,
    flexWrap: 'wrap',
    gap: 8,
  },
  usageTitle: {
    fontWeight: 600,
    color: '#2c3e50',
    fontSize: 14,
  },
  usageStats: {
    fontSize: 13,
    color: '#7f8c8d',
  },
  usageBarBg: {
    height: 12,
    background: '#ecf0f1',
    borderRadius: 6,
    overflow: 'hidden',
  },
  usageBarFill: {
    height: '100%',
    borderRadius: 6,
    transition: 'width 0.4s ease',
  },
  usagePct: {
    marginTop: 6,
    fontSize: 12,
    color: '#95a5a6',
    textAlign: 'right',
  },
  fileListHeader: {
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 10,
  },
  fileListTitle: {
    fontWeight: 600,
    fontSize: 14,
    color: '#2c3e50',
  },
  fileList: {
    border: '1px solid #e0e0e0',
    borderRadius: 6,
    overflow: 'hidden',
    marginBottom: 24,
  },
  fileRow: {
    display: 'flex',
    alignItems: 'center',
    padding: '9px 14px',
    borderBottom: '1px solid #f0f0f0',
    background: '#fff',
    gap: 10,
    transition: 'background 0.15s',
  },
  fileIcon: {
    fontSize: 16,
    flexShrink: 0,
  },
  filePath: {
    flex: 1,
    fontFamily: 'monospace',
    fontSize: 13,
    color: '#2c3e50',
    wordBreak: 'break-all',
  },
  fileSize: {
    fontSize: 12,
    color: '#95a5a6',
    flexShrink: 0,
    minWidth: 64,
    textAlign: 'right',
  },
  deleteBtn: {
    background: 'transparent',
    border: '1px solid #e74c3c',
    borderRadius: 4,
    color: '#e74c3c',
    cursor: 'pointer',
    padding: '3px 8px',
    fontSize: 14,
    flexShrink: 0,
    transition: 'background 0.15s',
  },
  emptyState: {
    padding: 24,
    textAlign: 'center',
    color: '#95a5a6',
    border: '1px dashed #ddd',
    borderRadius: 6,
    marginBottom: 24,
    fontSize: 14,
  },
  dangerZone: {
    border: '1px solid #e74c3c',
    borderRadius: 6,
    padding: '14px 18px',
    background: '#fff5f5',
  },
  dangerTitle: {
    fontWeight: 700,
    color: '#c0392b',
    marginBottom: 8,
    fontSize: 14,
  },
  dangerDesc: {
    fontSize: 13,
    color: '#7f8c8d',
    lineHeight: 1.6,
    marginBottom: 14,
  },
  confirmBox: {
    background: '#fff3cd',
    border: '1px solid #f0ad4e',
    borderRadius: 4,
    padding: '12px 14px',
  },
  formatBtn: {
    background: '#c0392b',
    color: '#fff',
    border: 'none',
    borderRadius: 4,
    padding: '10px 20px',
    cursor: 'pointer',
    fontSize: 14,
    fontWeight: 600,
  },
};
