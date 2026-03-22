import { useState } from 'react';
import { WiFiConfig } from './WiFiConfig';
import { SerialConfig } from './SerialConfig';
import { BLEConfig } from './BLEConfig';


const TABS = [
  { id: 'wifi',    label: '📶 WiFi' },
  { id: 'serial',  label: '🔌 Serial' },
  { id: 'ble',     label: '🔵 Bluetooth' },
];

export function ConfigPage() {
  const [activeTab, setActiveTab] = useState('wifi');

  return (
    <div className="page">
      <h2>Configuration</h2>

      <div className="config-tabs">
        {TABS.map(tab => (
          <button
            key={tab.id}
            className={`tab-btn ${activeTab === tab.id ? 'active' : ''}`}
            onClick={() => setActiveTab(tab.id)}
          >
            {tab.label}
          </button>
        ))}
      </div>

      <div className="tab-content">
        {activeTab === 'wifi'   && <WiFiConfig />}
        {activeTab === 'serial' && <SerialConfig />}
        {activeTab === 'ble'    && <BLEConfig />}
      </div>
    </div>
  );
}