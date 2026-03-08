import { useState, useEffect, useRef, useCallback } from 'react';
import { WebSocketService } from '../services/websocket';

export function useWebSocket() {
  const [messages, setMessages] = useState([]);
  const [isConnected, setIsConnected] = useState(false);
  const [isPaused, setIsPaused] = useState(false);

  // Use a ref for isPaused so the WebSocket handler always reads
  // the current value without needing to be re-registered.
  // This prevents tearing down and re-creating the WebSocket
  // connection every time the user toggles pause.
  const isPausedRef = useRef(false);
  const wsRef = useRef(null);

  useEffect(() => {
    wsRef.current = new WebSocketService();

    const handleEvent = (type, data) => {
      switch (type) {
        case 'connected':
          setIsConnected(true);
          break;
        case 'disconnected':
          setIsConnected(false);
          break;
        case 'message':
          // Read the ref — never causes the effect to re-run
          if (!isPausedRef.current) {
            setMessages(prev => {
              const next = [...prev, data];
              return next.slice(-100); // Keep last 100 messages
            });
          }
          break;
        default:
          break;
      }
    };

    wsRef.current.addListener(handleEvent);
    wsRef.current.connect();

    // Cleanup: only runs on component unmount, not on every re-render
    return () => {
      if (wsRef.current) {
        wsRef.current.disconnect();
        wsRef.current = null;
      }
    };
  }, []); // Empty deps: connect once, disconnect on unmount

  const clearMessages = useCallback(() => setMessages([]), []);

  const togglePause = useCallback(() => {
    setIsPaused(prev => {
      const next = !prev;
      isPausedRef.current = next; // Keep ref in sync
      return next;
    });
  }, []);

  return {
    messages,
    isConnected,
    isPaused,
    clearMessages,
    togglePause
  };
}
