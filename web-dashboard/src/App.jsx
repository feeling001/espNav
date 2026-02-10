import { BrowserRouter, Routes, Route } from 'react-router-dom';
import { Sidebar } from './components/Layout/Sidebar';
import { WiFiConfig } from './components/WiFi/WiFiConfig';
import { SerialConfig } from './components/Serial/SerialConfig';
import { NMEAMonitor } from './components/NMEA/NMEAMonitor';
import { SystemStatus } from './components/System/SystemStatus';
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
          </Routes>
        </div>
      </div>
    </BrowserRouter>
  );
}

export default App;
