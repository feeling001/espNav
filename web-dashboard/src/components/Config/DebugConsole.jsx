import { useEffect, useRef, useState } from 'react';

const MAX_LINES = 500;

export function DebugConsole() {
  const [lines, setLines] = useState([]);
  const [connected, setConnected] = useState(false);
  const [paused, setPaused] = useState(false);
  const wsRef = useRef(null);
  const bufferRef = useRef([]);
  const bottomRef = useRef(null);
  const autoScrollRef = useRef(true);

  useEffect(() => {
    let reconnectTimeout;
    let closedByUs = false;

    const connect = () => {
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const ws = new WebSocket(`${protocol}//${window.location.host}/ws/debug`);
      wsRef.current = ws;

      ws.onopen = () => setConnected(true);
      ws.onclose = () => {
        setConnected(false);
        if (!closedByUs) reconnectTimeout = setTimeout(connect, 3000);
      };
      ws.onerror = () => ws.close();
      ws.onmessage = (event) => {
        const newLines = event.data.split('\n').filter(l => l.length > 0);
        if (paused) {
          bufferRef.current.push(...newLines);
          return;
        }
        setLines(prev => {
          const combined = [...prev, ...newLines];
          return combined.length > MAX_LINES ? combined.slice(combined.length - MAX_LINES) : combined;
        });
      };
    };

    connect();

    return () => {
      closedByUs = true;
      clearTimeout(reconnectTimeout);
      wsRef.current?.close();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (!paused && bufferRef.current.length > 0) {
      const buffered = bufferRef.current;
      bufferRef.current = [];
      setLines(prev => {
        const combined = [...prev, ...buffered];
        return combined.length > MAX_LINES ? combined.slice(combined.length - MAX_LINES) : combined;
      });
    }
  }, [paused]);

  useEffect(() => {
    if (autoScrollRef.current && !paused) {
      bottomRef.current?.scrollIntoView({ block: 'end' });
    }
  }, [lines, paused]);

  const handleScroll = (e) => {
    const el = e.target;
    autoScrollRef.current = el.scrollHeight - el.scrollTop - el.clientHeight < 40;
  };

  const clear = () => setLines([]);

  return (
    <div className="debug-console">
      <div className="debug-console-toolbar">
        <span className={`ws-status ${connected ? 'connected' : 'disconnected'}`}>
          {connected ? '🟢 Connecté' : '🔴 Déconnecté'}
        </span>
        <button onClick={() => setPaused(p => !p)}>
          {paused ? '▶ Reprendre' : '⏸ Pause'}
        </button>
        <button onClick={clear}>🗑 Effacer</button>
        <span className="debug-console-count">{lines.length} lignes</span>
      </div>
      <pre className="debug-console-output" onScroll={handleScroll}>
        {lines.join('\n')}
        <div ref={bottomRef} />
      </pre>
    </div>
  );
}
