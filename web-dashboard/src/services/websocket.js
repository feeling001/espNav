export class WebSocketService {
  constructor() {
    this.ws = null;
    this.listeners = [];
    this.reconnectTimeout = null;
    this.shouldReconnect = true;
  }

  connect() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws/nmea`;

    try {
      this.ws = new WebSocket(wsUrl);

      this.ws.onopen = () => {
        console.log('[WebSocket] Connected');
        this.notifyListeners('connected', null);
      };

      this.ws.onmessage = (event) => {
        this.notifyListeners('message', event.data);
      };

      this.ws.onerror = (error) => {
        console.error('[WebSocket] Error:', error);
        this.notifyListeners('error', error);
      };

      this.ws.onclose = () => {
        console.log('[WebSocket] Disconnected');
        this.notifyListeners('disconnected', null);
        
        if (this.shouldReconnect) {
          this.reconnectTimeout = setTimeout(() => this.connect(), 3000);
        }
      };
    } catch (error) {
      console.error('[WebSocket] Connection error:', error);
    }
  }

  disconnect() {
    this.shouldReconnect = false;
    if (this.reconnectTimeout) {
      clearTimeout(this.reconnectTimeout);
    }
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }

  addListener(callback) {
    this.listeners.push(callback);
  }

  removeListener(callback) {
    this.listeners = this.listeners.filter(l => l !== callback);
  }

  notifyListeners(type, data) {
    this.listeners.forEach(listener => listener(type, data));
  }

  send(data) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(data);
    }
  }
}
