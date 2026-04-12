import { BrowserRouter, Routes, Route } from 'react-router-dom';
import { Sidebar } from './components/Layout/Sidebar';
import { WiFiConfig } from './components/Config/WiFiConfig';
import { SerialConfig } from './components/Config/SerialConfig';
import { NMEAMonitor } from './components/NMEA/NMEAMonitor';
import { SystemStatus } from './components/System/SystemStatus';
import { BLEConfig } from './components/Config/BLEConfig';
import { Instruments } from './components/Instruments/Instruments';
import { ConfigPage } from './components/Config/ConfigPage';
import { Performance } from './components/Performance/Performance';
import { Autopilot } from './components/Autopilot/Autopilot';
import { Logbook } from './components/Logbook/Logbook';

import './styles/main.css';

function App() {
  return (
    <BrowserRouter>
      <div className="app">
        <Sidebar />
        <div className="main-content">
          <Routes>
            <Route path="/"            element={<SystemStatus />} />
            <Route path="/instruments" element={<Instruments />} />
            <Route path="/autopilot"   element={<Autopilot />} />
            <Route path="/performance" element={<Performance />} />
            <Route path="/logbook"     element={<Logbook />} />
            <Route path="/config"      element={<ConfigPage />} />
            <Route path="/nmea"        element={<NMEAMonitor />} />
          </Routes>
        </div>
      </div>
    </BrowserRouter>
  );
}

export default App;
