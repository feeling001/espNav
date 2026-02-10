import { useState, useEffect, useRef } from 'react';
import { WebSocketService } from '../services/websocket';

export function useWebSocket() {
  const [messages, setMessages] = useState([]);
  const [isConnected, setIsConnected] = useState(false);
  const [isPaused, setIsPaused] = useState(false);
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
          if (!isPaused) {
            setMessages(prev => {
              const newMessages = [...prev, data];
              return newMessages.slice(-100); // Keep last 100
            });
          }
          break;
        default:
          break;
      }
    };

    wsRef.current.addListener(handleEvent);
    wsRef.current.connect();

    return () => {
      if (wsRef.current) {
        wsRef.current.disconnect();
      }
    };
  }, [isPaused]);

  const clearMessages = () => setMessages([]);
  const togglePause = () => setIsPaused(prev => !prev);

  return {
    messages,
    isConnected,
    isPaused,
    clearMessages,
    togglePause
  };
}
