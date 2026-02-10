import { Link, useLocation } from 'react-router-dom';

export function Sidebar() {
  const location = useLocation();
  
  const isActive = (path) => location.pathname === path ? 'active' : '';
  
  return (
    <div className="sidebar">
      <h1>⚓ Marine Gateway</h1>
      <nav>
        <Link to="/" className={isActive('/')}>System Status</Link>
        <Link to="/wifi" className={isActive('/wifi')}>WiFi Config</Link>
        <Link to="/serial" className={isActive('/serial')}>Serial Config</Link>
        <Link to="/nmea" className={isActive('/nmea')}>NMEA Monitor</Link>
      </nav>
    </div>
  );
}
