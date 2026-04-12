import { useState, useEffect, useCallback } from 'react';
import { api } from '../../services/api';

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

const fmtBytes = (b) => {
  if (b == null || b === undefined) return '--';
  if (b < 1024)       return `${b} B`;
  if (b < 1048576)    return `${(b / 1024).toFixed(1)} KB`;
  if (b < 1073741824) return `${(b / 1048576).toFixed(2)} MB`;
  return `${(b / 1073741824).toFixed(2)} GB`;
};

const fmtMB = (mb) => {
  if (mb == null) return '--';
  if (mb < 1024)  return `${mb} MB`;
  return `${(mb / 1024).toFixed(1)} GB`;
};

const fileIcon = (path, isDir = false) => {
  if (isDir) return '📁';
  const ext = path.split('.').pop().toLowerCase();
  const icons = {
    html: '🌐', js: '📜', css: '🎨', json: '📋',
    pol:  '🧭', csv: '📊', bin: '📦', txt: '📄',
    png:  '🖼', ico: '🔖', nmea: '🛰', log: '📋',
  };
  return icons[ext] || '📄';
};

const barColor = (pct) => {
  if (pct < 65) return 'linear-gradient(90deg, #27ae60, #1e8449)';
  if (pct < 85) return 'linear-gradient(90deg, #e67e22, #d35400)';
  return 'linear-gradient(90deg, #e74c3c, #c0392b)';
};

// ─────────────────────────────────────────────────────────────────────────────
// Sub-components
// ─────────────────────────────────────────────────────────────────────────────

function UsageBar({ used, total, free, usedPct, label }) {
  return (
    <div style={s.usageCard}>
      <div style={s.usageHeader}>
        <span style={s.usageTitle}>{label}</span>
        <span style={s.usageStats}>
          {fmtBytes(used)} used / {fmtBytes(total)} total —{' '}
          <strong>{fmtBytes(free)} free</strong>
        </span>
      </div>
      <div style={s.usageBarBg}>
        <div style={{ ...s.usageBarFill, width: `${Math.min(usedPct ?? 0, 100)}%`, background: barColor(usedPct ?? 0) }} />
      </div>
      <div style={s.usagePct}>{usedPct ?? 0}% used</div>
    </div>
  );
}

function UsageBarMB({ usedMB, totalMB, freeMB, usedPct, label }) {
  return (
    <div style={s.usageCard}>
      <div style={s.usageHeader}>
        <span style={s.usageTitle}>{label}</span>
        <span style={s.usageStats}>
          {fmtMB(usedMB)} used / {fmtMB(totalMB)} total —{' '}
          <strong>{fmtMB(freeMB)} free</strong>
        </span>
      </div>
      <div style={s.usageBarBg}>
        <div style={{ ...s.usageBarFill, width: `${Math.min(usedPct ?? 0, 100)}%`, background: barColor(usedPct ?? 0) }} />
      </div>
      <div style={s.usagePct}>{usedPct ?? 0}% used</div>
    </div>
  );
}

function SectionTitle({ children }) {
  return <h3 style={s.sectionTitle}>{children}</h3>;
}

function Message({ type, children }) {
  if (!children) return null;
  return (
    <div className={`message ${type}`} style={{ marginBottom: 14 }}>
      {type === 'success' ? '✓' : '✗'} {children}
    </div>
  );
}

function FileRow({ file, onDelete, onDownload, deleting }) {
  return (
    <div style={s.fileRow}>
      <span style={s.fileIconCol}>{fileIcon(file.path, file.isDir)}</span>
      <span style={s.filePath}>{file.path}</span>
      {!file.isDir && <span style={s.fileSize}>{fmtBytes(file.size)}</span>}
      {file.isDir  && <span style={s.fileSize}>—</span>}
      <div style={s.fileActions}>
        {!file.isDir && onDownload && (
          <button
            onClick={() => onDownload(file.path)}
            style={s.actionBtn('#2980b9')}
            title="Download"
          >
            ⬇
          </button>
        )}
        {!file.isDir && (
          <button
            onClick={() => onDelete(file.path)}
            disabled={deleting === file.path}
            style={s.actionBtn('#e74c3c')}
            title="Delete"
          >
            {deleting === file.path ? '…' : '🗑'}
          </button>
        )}
      </div>
    </div>
  );
}

function DangerZone({ onFormat, label = 'Format Storage', warning, formatting, confirmFormat, setConfirmFormat }) {
  return (
    <div style={s.dangerZone}>
      <div style={s.dangerTitle}>⚠️ Danger Zone</div>
      <p style={s.dangerDesc}>{warning}</p>
      {confirmFormat ? (
        <div style={s.confirmBox}>
          <span style={{ fontSize: 14, color: '#c0392b', fontWeight: 600 }}>
            Are you sure? This cannot be undone.
          </span>
          <div style={{ display: 'flex', gap: 10, marginTop: 10 }}>
            <button onClick={onFormat} disabled={formatting}
              style={{ ...s.formatBtn, background: '#e74c3c' }}>
              {formatting ? 'Processing…' : 'Yes, proceed'}
            </button>
            <button className="secondary" onClick={() => setConfirmFormat(false)}>
              Cancel
            </button>
          </div>
        </div>
      ) : (
        <button onClick={() => setConfirmFormat(true)} disabled={formatting} style={s.formatBtn}>
          🗑 {label}
        </button>
      )}
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// LittleFS panel
// ─────────────────────────────────────────────────────────────────────────────

function LittleFSPanel() {
  const [storageInfo, setStorageInfo] = useState(null);
  const [files, setFiles]             = useState([]);
  const [loading, setLoading]         = useState(true);
  const [message, setMessage]         = useState(null);
  const [deleting, setDeleting]       = useState(null);
  const [formatting, setFormatting]   = useState(false);
  const [confirmFormat, setConfirmFormat] = useState(false);

  const load = useCallback(async () => {
    try {
      const [info, list] = await Promise.all([api.getStorageInfo(), api.listStorageFiles()]);
      setStorageInfo(info);
      setFiles(list.files || []);
    } catch (e) {
      setMessage({ type: 'error', text: 'Failed to load LittleFS info.' });
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => { load(); }, [load]);

  const handleDelete = async (path) => {
    setDeleting(path);
    setMessage(null);
    try {
      await api.deleteStorageFile(path);
      setMessage({ type: 'success', text: `Deleted: ${path}` });
      await load();
    } catch {
      setMessage({ type: 'error', text: `Failed to delete ${path}.` });
    } finally {
      setDeleting(null);
    }
  };

  const handleFormat = async () => {
    setConfirmFormat(false);
    setFormatting(true);
    setMessage(null);
    try {
      await api.formatStorage();
      setMessage({ type: 'success', text: 'LittleFS formatted successfully.' });
      await load();
    } catch {
      setMessage({ type: 'error', text: 'Format failed.' });
    } finally {
      setFormatting(false);
    }
  };

  if (loading) return <div style={{ color: '#7f8c8d', padding: 12 }}>Loading…</div>;

  const sorted = [...files].sort((a, b) => a.path.localeCompare(b.path));

  return (
    <div>
      {storageInfo && (
        <UsageBar
          label="Internal Flash (LittleFS)"
          used={storageInfo.used_bytes}
          total={storageInfo.total_bytes}
          free={storageInfo.free_bytes}
          usedPct={storageInfo.used_pct}
        />
      )}

      <Message type={message?.type} >{message?.text}</Message>

      <div style={s.listHeader}>
        <span style={s.listTitle}>Files ({files.length})</span>
        <button className="secondary" onClick={load} style={{ padding: '5px 12px', fontSize: 13 }}>
          ↺ Refresh
        </button>
      </div>

      {sorted.length === 0 ? (
        <div style={s.empty}>No files found.</div>
      ) : (
        <div style={s.fileList}>
          {sorted.map(f => (
            <FileRow key={f.path} file={f} deleting={deleting}
              onDelete={handleDelete} />
          ))}
        </div>
      )}

      <DangerZone
        onFormat={handleFormat}
        formatting={formatting}
        confirmFormat={confirmFormat}
        setConfirmFormat={setConfirmFormat}
        label="Format LittleFS"
        warning="Formatting erases all files including the web dashboard, polar diagrams,
and configuration backups. The device will need to be reflashed after formatting
if the dashboard is stored on LittleFS."
      />
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// SD Card panel
// ─────────────────────────────────────────────────────────────────────────────

function SDCardPanel() {
  const [sdStatus, setSDStatus]       = useState(null);
  const [files, setFiles]             = useState([]);
  const [loading, setLoading]         = useState(true);
  const [message, setMessage]         = useState(null);
  const [deleting, setDeleting]       = useState(null);
  const [formatting, setFormatting]   = useState(false);
  const [confirmFormat, setConfirmFormat] = useState(false);
  const [mounting, setMounting]       = useState(false);

  const loadStatus = useCallback(async () => {
    try {
      const status = await api.getSDStatus();
      setSDStatus(status);
      if (status.mounted) {
        const list = await api.listSDFiles('/');
        // Only show files (not directories) at the top level for simplicity;
        // full recursive listing from the API.
        setFiles(list.files || []);
      } else {
        setFiles([]);
      }
    } catch (e) {
      setMessage({ type: 'error', text: 'Failed to load SD card info.' });
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => { loadStatus(); }, [loadStatus]);

  const handleMount = async () => {
    setMounting(true);
    setMessage(null);
    try {
      const result = await api.mountSD();
      setMessage({
        type: result.success ? 'success' : 'error',
        text: result.message || result.error
      });
      await loadStatus();
    } catch {
      setMessage({ type: 'error', text: 'Mount failed.' });
    } finally {
      setMounting(false);
    }
  };

  const handleUnmount = async () => {
    setMounting(true);
    setMessage(null);
    try {
      await api.unmountSD();
      setMessage({ type: 'success', text: 'SD card unmounted safely. You can now remove it.' });
      await loadStatus();
    } catch {
      setMessage({ type: 'error', text: 'Unmount failed.' });
    } finally {
      setMounting(false);
    }
  };

  const handleDelete = async (path) => {
    setDeleting(path);
    setMessage(null);
    try {
      await api.deleteSDFile(path);
      setMessage({ type: 'success', text: `Deleted: ${path}` });
      await loadStatus();
    } catch {
      setMessage({ type: 'error', text: `Failed to delete ${path}.` });
    } finally {
      setDeleting(null);
    }
  };

  const handleDownload = (path) => {
    const url  = api.getSDDownloadURL(path);
    const link = document.createElement('a');
    link.href  = url;
    link.download = path.split('/').pop();
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  const handleFormat = async () => {
    setConfirmFormat(false);
    setFormatting(true);
    setMessage(null);
    try {
      await api.formatSD();
      setMessage({ type: 'success', text: 'SD card formatted (all files removed).' });
      await loadStatus();
    } catch {
      setMessage({ type: 'error', text: 'Format failed.' });
    } finally {
      setFormatting(false);
    }
  };

  if (loading) return <div style={{ color: '#7f8c8d', padding: 12 }}>Loading…</div>;

  // SD not configured in firmware
  if (sdStatus && !sdStatus.enabled) {
    return (
      <div style={s.noticeBox('#e7f3ff', '#3498db')}>
        <strong>ℹ SD card support not enabled</strong>
        <p style={{ marginTop: 6, marginBottom: 0, fontSize: 13 }}>
          The SD manager is not configured in the firmware build.
          Add <code>SDManager sdManager;</code> and pass it to WebServer to enable SD support.
        </p>
      </div>
    );
  }

  const mounted = sdStatus?.mounted ?? false;

  // Compute used from total - free for MB-based stats
  const totalMB = sdStatus?.total_mb ?? 0;
  const freeMB  = sdStatus?.free_mb  ?? 0;
  const usedMB  = totalMB - freeMB;
  const usedPct = sdStatus?.used_pct ?? 0;

  const sorted = [...files].sort((a, b) => {
    if (a.isDir !== b.isDir) return a.isDir ? -1 : 1;
    return a.path.localeCompare(b.path);
  });

  return (
    <div>
      {/* ── Status badge + mount controls ── */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 12, marginBottom: 16, flexWrap: 'wrap' }}>
        <div style={{
          ...s.badge,
          background: mounted ? '#d4edda' : '#f8d7da',
          color:      mounted ? '#155724' : '#721c24',
          border:     `1px solid ${mounted ? '#c3e6cb' : '#f5c6cb'}`,
        }}>
          {mounted ? '🟢 Card mounted' : '🔴 No card / unmounted'}
          {mounted && sdStatus?.card_type && (
            <span style={{ marginLeft: 8, opacity: 0.75, fontSize: 11 }}>
              [{sdStatus.card_type}]
            </span>
          )}
        </div>

        <div style={{ display: 'flex', gap: 8 }}>
          {!mounted && (
            <button onClick={handleMount} disabled={mounting}
              style={{ padding: '6px 14px', fontSize: 13 }}>
              {mounting ? 'Mounting…' : '🔌 Mount'}
            </button>
          )}
          {mounted && (
            <button onClick={handleUnmount} disabled={mounting} className="secondary"
              style={{ padding: '6px 14px', fontSize: 13 }}>
              {mounting ? 'Unmounting…' : '⏏ Unmount'}
            </button>
          )}
          <button className="secondary" onClick={loadStatus}
            style={{ padding: '6px 12px', fontSize: 13 }}>
            ↺ Refresh
          </button>
        </div>
      </div>

      <Message type={message?.type}>{message?.text}</Message>

      {/* ── Storage usage ── */}
      {mounted && totalMB > 0 && (
        <UsageBarMB
          label="SD Card"
          usedMB={usedMB}
          totalMB={totalMB}
          freeMB={freeMB}
          usedPct={usedPct}
        />
      )}

      {/* ── File list ── */}
      {mounted && (
        <>
          <div style={{ ...s.listHeader, marginTop: 4 }}>
            <span style={s.listTitle}>
              Files ({files.filter(f => !f.isDir).length})
              {files.filter(f => f.isDir).length > 0 &&
                ` · ${files.filter(f => f.isDir).length} dir(s)`}
            </span>
          </div>

          {sorted.length === 0 ? (
            <div style={s.empty}>No files found on SD card.</div>
          ) : (
            <div style={s.fileList}>
              {sorted.map(f => (
                <FileRow
                  key={f.path}
                  file={f}
                  deleting={deleting}
                  onDelete={handleDelete}
                  onDownload={handleDownload}
                />
              ))}
            </div>
          )}
        </>
      )}

      {/* ── Format / danger zone ── */}
      {mounted && (
        <DangerZone
          onFormat={handleFormat}
          formatting={formatting}
          confirmFormat={confirmFormat}
          setConfirmFormat={setConfirmFormat}
          label="Format SD Card"
          warning={`Formatting removes all files from the SD card (FAT32 wipe).
Navigation logs, NMEA dumps, and any other data stored on the card will be
permanently lost. The card will remain mounted afterwards.`}
        />
      )}

      {!mounted && (
        <div style={s.noticeBox('#fff3cd', '#f0ad4e')}>
          <strong>No SD card detected.</strong>
          <p style={{ marginTop: 6, marginBottom: 0, fontSize: 13, lineHeight: 1.6 }}>
            Insert a FAT32-formatted card and click <em>Mount</em>, or connect the card and
            restart the device. Supported formats: FAT32 (recommended), FAT16.
          </p>
          <p style={{ marginTop: 8, marginBottom: 0, fontSize: 12, color: '#856404' }}>
            Pin mapping — MOSI: GPIO {'{SD_MOSI}'}, MISO: GPIO {'{SD_MISO}'}, SCK: GPIO {'{SD_SCK}'}, CS: GPIO {'{SD_CS}'}
          </p>
        </div>
      )}
    </div>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// Main component
// ─────────────────────────────────────────────────────────────────────────────

export function StorageManager() {
  const [activeSection, setActiveSection] = useState('littlefs');

  return (
    <div>
      {/* ── Section tabs ── */}
      <div style={s.tabRow}>
        <TabButton
          active={activeSection === 'littlefs'}
          onClick={() => setActiveSection('littlefs')}
          icon="💾"
          label="Internal Flash (LittleFS)"
        />
        <TabButton
          active={activeSection === 'sd'}
          onClick={() => setActiveSection('sd')}
          icon="🗂"
          label="SD Card"
        />
      </div>

      <div style={{ paddingTop: 4 }}>
        {activeSection === 'littlefs' && (
          <>
            <SectionTitle>Storage Management — Internal Flash</SectionTitle>
            <LittleFSPanel />
          </>
        )}
        {activeSection === 'sd' && (
          <>
            <SectionTitle>Storage Management — SD Card</SectionTitle>
            <SDCardPanel />
          </>
        )}
      </div>
    </div>
  );
}

function TabButton({ active, onClick, icon, label }) {
  return (
    <button
      onClick={onClick}
      style={{
        background:    active ? '#3498db' : 'transparent',
        color:         active ? '#fff'    : '#7f8c8d',
        border:        active ? 'none'    : '1px solid #ddd',
        borderRadius:  6,
        padding:       '7px 16px',
        fontSize:      13,
        cursor:        'pointer',
        fontWeight:    active ? 600 : 400,
        display:       'flex',
        alignItems:    'center',
        gap:           6,
        transition:    'all 0.15s',
      }}
    >
      <span>{icon}</span>
      <span>{label}</span>
    </button>
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// Styles
// ─────────────────────────────────────────────────────────────────────────────

const s = {
  sectionTitle: {
    marginBottom: 16,
    fontSize: 15,
    color: '#2c3e50',
    borderBottom: '2px solid #3498db',
    paddingBottom: 7,
    fontWeight: 600,
  },
  tabRow: {
    display: 'flex',
    gap: 8,
    marginBottom: 18,
    flexWrap: 'wrap',
  },
  usageCard: {
    background: '#f8f9fa',
    border: '1px solid #e0e0e0',
    borderRadius: 6,
    padding: '12px 16px',
    marginBottom: 16,
  },
  usageHeader: {
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'baseline',
    marginBottom: 8,
    flexWrap: 'wrap',
    gap: 6,
  },
  usageTitle: { fontWeight: 600, color: '#2c3e50', fontSize: 13 },
  usageStats:  { fontSize: 12, color: '#7f8c8d' },
  usageBarBg: {
    height: 10,
    background: '#ecf0f1',
    borderRadius: 5,
    overflow: 'hidden',
  },
  usageBarFill: {
    height: '100%',
    borderRadius: 5,
    transition: 'width 0.4s ease',
  },
  usagePct: {
    marginTop: 4,
    fontSize: 11,
    color: '#95a5a6',
    textAlign: 'right',
  },
  listHeader: {
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  listTitle: { fontWeight: 600, fontSize: 13, color: '#2c3e50' },
  fileList: {
    border: '1px solid #e0e0e0',
    borderRadius: 6,
    overflow: 'hidden',
    marginBottom: 20,
  },
  fileRow: {
    display: 'flex',
    alignItems: 'center',
    padding: '8px 12px',
    borderBottom: '1px solid #f0f0f0',
    background: '#fff',
    gap: 8,
  },
  fileIconCol: { fontSize: 15, flexShrink: 0 },
  filePath: {
    flex: 1,
    fontFamily: 'monospace',
    fontSize: 12,
    color: '#2c3e50',
    wordBreak: 'break-all',
  },
  fileSize: {
    fontSize: 11,
    color: '#95a5a6',
    flexShrink: 0,
    minWidth: 64,
    textAlign: 'right',
  },
  fileActions: {
    display: 'flex',
    gap: 5,
    flexShrink: 0,
  },
  actionBtn: (color) => ({
    background: 'transparent',
    border: `1px solid ${color}`,
    borderRadius: 4,
    color,
    cursor: 'pointer',
    padding: '2px 7px',
    fontSize: 13,
    lineHeight: 1.4,
  }),
  empty: {
    padding: 20,
    textAlign: 'center',
    color: '#95a5a6',
    border: '1px dashed #ddd',
    borderRadius: 6,
    marginBottom: 20,
    fontSize: 13,
  },
  dangerZone: {
    border: '1px solid #e74c3c',
    borderRadius: 6,
    padding: '12px 16px',
    background: '#fff5f5',
    marginTop: 4,
  },
  dangerTitle: { fontWeight: 700, color: '#c0392b', marginBottom: 6, fontSize: 13 },
  dangerDesc: { fontSize: 12, color: '#7f8c8d', lineHeight: 1.6, marginBottom: 12, whiteSpace: 'pre-line' },
  confirmBox: {
    background: '#fff3cd',
    border: '1px solid #f0ad4e',
    borderRadius: 4,
    padding: '10px 12px',
  },
  formatBtn: {
    background: '#c0392b',
    color: '#fff',
    border: 'none',
    borderRadius: 4,
    padding: '8px 18px',
    cursor: 'pointer',
    fontSize: 13,
    fontWeight: 600,
  },
  badge: {
    display: 'inline-flex',
    alignItems: 'center',
    padding: '5px 12px',
    borderRadius: 20,
    fontSize: 13,
    fontWeight: 600,
  },
  noticeBox: (bg, accent) => ({
    padding: '12px 16px',
    background: bg,
    borderLeft: `4px solid ${accent}`,
    borderRadius: 4,
    fontSize: 13,
    lineHeight: 1.6,
    marginTop: 8,
  }),
};
