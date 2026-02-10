import { useWebSocket } from '../../hooks/useWebSocket';

export function NMEAMonitor() {
  const { messages, isConnected, isPaused, clearMessages, togglePause } = useWebSocket();

  return (
    <div className="page">
      <h2>NMEA Monitor</h2>
      
      <div style={{ 
        padding: '10px', 
        marginBottom: '15px', 
        background: isConnected ? '#d4edda' : '#f8d7da',
        borderRadius: '4px'
      }}>
        Status: {isConnected ? '🟢 Connected' : '🔴 Disconnected'}
      </div>

      <div className="nmea-controls">
        <button onClick={togglePause} className="secondary">
          {isPaused ? '▶️ Resume' : '⏸️ Pause'}
        </button>
        <button onClick={clearMessages} className="secondary">
          🗑️ Clear
        </button>
      </div>

      <div className={`nmea-monitor ${!isConnected ? 'disconnected' : ''}`}>
        {messages.length === 0 ? (
          <div style={{ color: '#95a5a6', textAlign: 'center', padding: '50px' }}>
            {isConnected ? 'Waiting for NMEA data...' : 'WebSocket disconnected'}
          </div>
        ) : (
          messages.map((msg, idx) => (
            <div key={idx}>{msg}</div>
          ))
        )}
      </div>

      <div style={{ marginTop: '10px', fontSize: '12px', color: '#7f8c8d' }}>
        Messages: {messages.length} (last 100 shown)
      </div>
    </div>
  );
}
