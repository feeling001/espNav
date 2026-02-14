import { BrowserRouter, Routes, Route } from 'react-router-dom';
import { Sidebar } from './components/Layout/Sidebar';
import { WiFiConfig } from './components/WiFi/WiFiConfig';
import { SerialConfig } from './components/Serial/SerialConfig';
import { NMEAMonitor } from './components/NMEA/NMEAMonitor';
import { SystemStatus } from './components/System/SystemStatus';
import { BLEConfig } from './components/BLE/BLEConfig';
import { Instruments } from './components/Instruments/Instruments';

import './styles/main.css';

function App() {
  return (
    <BrowserRouter>
      <div className="app">
        <Sidebar />
        <div className="main-content">
          <Routes>
            <Route path="/" element={<SystemStatus />} />
            <Route path="/wifi" element={<WiFiConfig />} />
            <Route path="/serial" element={<SerialConfig />} />
            <Route path="/nmea" element={<NMEAMonitor />} />
            <Route path="/ble" element={<BLEConfig />} />
            <Route path="/instruments" element={<Instruments />} />
          </Routes>
        </div>
      </div>
    </BrowserRouter>
  );
}

export default App;
