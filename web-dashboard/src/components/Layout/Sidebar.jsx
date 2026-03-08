import { Link, useLocation } from 'react-router-dom';

// ESPNav logo as inline SVG component
const ESPNavLogo = () => (
  <svg width="56" height="56" viewBox="0 0 512 512" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="seaGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#0ea5e9"/>
        <stop offset="100%" stopColor="#1e3a8a"/>
      </linearGradient>
    </defs>
    <rect x="0" y="0" width="512" height="512" rx="90" fill="url(#seaGrad)"/>
    {/* ESP Chip */}
    <g transform="scale(1.5) translate(70,75)">
      <rect x="0" y="0" width="200" height="150" rx="20" fill="#0f172a" stroke="#38bdf8" strokeWidth="4"/>
      <g stroke="#38bdf8" strokeWidth="4">
        <line x1="0" y1="15" x2="-26" y2="15"/>
        <line x1="0" y1="45" x2="-26" y2="45"/>
        <line x1="0" y1="75" x2="-26" y2="75"/>
        <line x1="0" y1="105" x2="-26" y2="105"/>
        <line x1="0" y1="135" x2="-26" y2="135"/>
        <line x1="200" y1="15" x2="226" y2="15"/>
        <line x1="200" y1="45" x2="226" y2="45"/>
        <line x1="200" y1="75" x2="226" y2="75"/>
        <line x1="200" y1="105" x2="226" y2="105"/>
        <line x1="200" y1="135" x2="226" y2="135"/>
      </g>
      <circle cx="100" cy="75" r="55" fill="none" stroke="#7dd3fc" strokeWidth="3"/>
      <polygon points="100,25 112,75 100,125 88,75" fill="#38bdf8"/>
    </g>
    {/* Sailboat */}
    <g transform="translate(300,380) scale(1.2)">
      <path d="M -60 19 L -45 16 L 10 16 Q 45 16 55 20 L 55 28 Q 35 30 -10 30 L -50 28 Q -60 26 -60 22 Z"
        fill="#0f172a" stroke="#e0f2fe" strokeWidth="2"/>
      <line x1="5" y1="-110" x2="5" y2="16" stroke="#e0f2fe" strokeWidth="3"/>
      <path d="M 5 -110 L 5 14 L 50 13 Q 40 -50 18 -95 Q 12 -108 5 -110 Z" fill="#e0f2fe"/>
      <path d="M 5 -95 L -8 14 L -61 18 Q -30 -40 +0 -90 Z" fill="#e0f2fe"/>
    </g>
    {/* Waves */}
    <g transform="translate(80,410) scale(1.5)">
      <path d="M0 20 Q40 0 80 20 T160 20 T240 20"
        fill="none" stroke="#7dd3fc" strokeWidth="4" opacity="0.7"/>
    </g>
  </svg>
);

export function Sidebar() {
  const location = useLocation();

  const isActive = (path) => location.pathname === path ? 'active' : '';

  return (
    <div className="sidebar">
      <div className="sidebar-brand">
        <ESPNavLogo />
        <span className="sidebar-title">Marine Gateway</span>
      </div>
      <nav>
        <Link to="/" className={isActive('/')}>System Status</Link>
        <Link to="/instruments" className={isActive('/instruments')}>Instruments</Link>
        <Link to="/wifi" className={isActive('/wifi')}>WiFi Config</Link>
        <Link to="/serial" className={isActive('/serial')}>Serial Config</Link>
        <Link to="/nmea" className={isActive('/nmea')}>NMEA Monitor</Link>
        <Link to="/ble" className={isActive('/ble')}>Bluetooth Config</Link>
      </nav>
    </div>
  );
}
