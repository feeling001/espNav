# Testing Strategy and Test Plan

Comprehensive testing procedures for the Marine Gateway MVP.

## Testing Philosophy

- **Iterative Testing**: Test each iteration before moving to the next
- **Integration Focus**: Emphasize end-to-end testing over unit tests
- **Real-World Conditions**: Test with actual marine instruments when possible
- **Regression Prevention**: Re-test previous features after changes
- **Performance Validation**: Monitor memory, latency, and reliability

## Test Environment Setup

### Hardware Setup

```
┌──────────────┐
│ GPS/NMEA     │──┐
│ Simulator    │  │ TX (3.3V)
└──────────────┘  │
                  ▼
                ┌────────────┐
                │ ESP32-S3   │
                │ RX (GPIO16)│
                └────────────┘
                      │
                      │ WiFi
                      │
                      ▼
              ┌──────────────┐
              │ WiFi Router  │
              └──────────────┘
                      │
        ┌─────────────┼─────────────┐
        │             │             │
        ▼             ▼             ▼
   ┌─────────┐  ┌─────────┐  ┌─────────┐
   │ OpenCPN │  │ Browser │  │ Netcat  │
   │ (TCP)   │  │ (WebUI) │  │ (TCP)   │
   └─────────┘  └─────────┘  └─────────┘
```

### Software Tools

1. **NMEA Simulator**
   - Free: gpsd's gpsfake
   - Commercial: Tera Term with NMEA macros
   - Hardware: USB GPS module

2. **TCP Clients**
   - netcat: `nc <ESP_IP> 10110`
   - OpenCPN: Marine navigation software
   - Custom Python script

3. **API Testing**
   - curl: Command-line REST testing
   - Postman: GUI REST client
   - Browser Developer Console

4. **Monitoring Tools**
   - PlatformIO Serial Monitor
   - Wireshark: Network packet analysis
   - Browser DevTools: WebSocket/Network inspection

## Test Matrices

### Iteration 1: UART + NMEA Parsing

| Test Case | Input | Expected Output | Status |
|-----------|-------|-----------------|--------|
| Valid NMEA sentence | `$GPGGA,...*47\r\n` | Parsed, checksum valid | ⬜ |
| Invalid checksum | `$GPGGA,...*00\r\n` | Rejected, error logged | ⬜ |
| Incomplete sentence | `$GPGGA,123` | Buffered, waiting for end | ⬜ |
| Sentence too long | 100+ char sentence | Rejected, buffer reset | ⬜ |
| High data rate (38400) | Continuous stream | No data loss | ⬜ |
| Different baud rates | 4800, 9600, 38400 | All work correctly | ⬜ |
| Special characters | Binary data mixed | Rejected, no crash | ⬜ |

### Iteration 2: Configuration + WiFi

| Test Case | Scenario | Expected Behavior | Status |
|-----------|----------|-------------------|--------|
| STA connection success | Valid credentials | Connect within 30s | ⬜ |
| STA connection fail | Invalid credentials | AP mode after 30s | ⬜ |
| Config persistence | Save → Reboot | Settings retained | ⬜ |
| WiFi signal loss | Disconnect router | Reconnect attempt (3x) | ⬜ |
| Reconnection success | Router back online | Reconnect within 60s | ⬜ |
| Reconnection failure | Router offline | AP mode after 3 attempts | ⬜ |
| Factory reset | Trigger reset | Default values loaded | ⬜ |
| Serial config change | 9600 → 38400 | UART reconfigured | ⬜ |

### Iteration 3: TCP Server

| Test Case | Scenario | Expected Behavior | Status |
|-----------|----------|-------------------|--------|
| Single client connect | nc <IP> 10110 | Connection accepted | ⬜ |
| Receive NMEA stream | Connected client | NMEA sentences received | ⬜ |
| Multiple clients (5) | 5 simultaneous | All receive same data | ⬜ |
| Client disconnect | Abrupt close | Server continues | ⬜ |
| Slow client | Slow read rate | Client dropped if buffer full | ⬜ |
| Max clients exceeded | 6th client connects | Connection refused | ⬜ |
| Latency test | UART → TCP | <100ms latency | ⬜ |
| OpenCPN integration | Connect OpenCPN | Navigation data displayed | ⬜ |
| Data integrity | 1000 sentences | All checksums valid | ⬜ |

### Iteration 4: Web Dashboard Backend

| Test Case | Endpoint | Method | Expected Response | Status |
|-----------|----------|--------|-------------------|--------|
| Get WiFi config | /api/config/wifi | GET | JSON with SSID, mode | ⬜ |
| Set WiFi config | /api/config/wifi | POST | Success message | ⬜ |
| Get serial config | /api/config/serial | GET | JSON with baud, etc | ⬜ |
| Set serial config | /api/config/serial | POST | Success message | ⬜ |
| Get system status | /api/status | GET | JSON with uptime, heap | ⬜ |
| Restart system | /api/restart | POST | Success, then reboot | ⬜ |
| WebSocket connect | /ws/nmea | WS | Connection established | ⬜ |
| WebSocket stream | Connected | WS | NMEA sentences received | ⬜ |
| Invalid JSON | /api/config/wifi | POST | 400 error response | ⬜ |
| Concurrent requests | Multiple APIs | Multiple | All handled correctly | ⬜ |

### Iteration 5: Web Dashboard Frontend

| Test Case | Action | Expected Behavior | Status |
|-----------|--------|-------------------|--------|
| Load dashboard | Open browser | Page loads <2s | ⬜ |
| Navigate menu | Click menu items | Pages switch correctly | ⬜ |
| WiFi config form | Enter SSID/pass | Form validation works | ⬜ |
| Save WiFi config | Click save | Success message, saved | ⬜ |
| Serial config | Change baud rate | Dropdown works, saved | ⬜ |
| NMEA monitor | Open monitor | Live stream displayed | ⬜ |
| Pause NMEA | Click pause | Stream pauses | ⬜ |
| Resume NMEA | Click resume | Stream resumes | ⬜ |
| System status | View status | All metrics displayed | ⬜ |
| Auto-refresh | Wait 5s | Status updates | ⬜ |
| Mobile view | Open on phone | Responsive layout | ⬜ |
| Tablet view | Open on tablet | Optimized layout | ⬜ |

### Iteration 6: Final Integration

| Test Case | Duration | Metrics | Pass Criteria | Status |
|-----------|----------|---------|---------------|--------|
| 24h uptime | 24 hours | Heap, crashes | No crashes, stable heap | ⬜ |
| Memory stability | 24 hours | Min free heap | >100KB free | ⬜ |
| Continuous NMEA | 24 hours | Sentence count | No gaps >1 minute | ⬜ |
| Multi-client stress | 1 hour | 5 TCP + 2 WS | No data loss | ⬜ |
| WiFi failover | Multiple | STA ↔ AP | Correct transitions | ⬜ |
| Config persistence | 10 reboots | All settings | Always retained | ⬜ |
| Error recovery | Various | All scenarios | Graceful handling | ⬜ |

## Detailed Test Procedures

### NMEA Checksum Validation Test

**Objective**: Verify NMEA checksum calculation and validation

**Procedure**:
1. Send valid NMEA sentence: `$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n`
2. Send invalid checksum: `$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00\r\n`
3. Send no checksum: `$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,\r\n`

**Expected Results**:
- Valid sentence: Accepted and processed
- Invalid checksum: Rejected with error log
- No checksum: Rejected

**Pass Criteria**: 100% correct validation

---

### TCP Latency Test

**Objective**: Measure end-to-end latency from UART RX to TCP TX

**Procedure**:
1. Modify code to timestamp NMEA sentences:
   ```cpp
   uint32_t uartRxTime = millis();
   // ... process ...
   uint32_t tcpTxTime = millis();
   latency = tcpTxTime - uartRxTime;
   ```
2. Connect GPS simulator at 38400 baud
3. Connect TCP client
4. Measure latency for 1000 sentences
5. Calculate average, min, max

**Expected Results**:
- Average latency: <50ms
- Maximum latency: <100ms
- No outliers >200ms

**Pass Criteria**: Average <50ms, 99th percentile <100ms

---

### WiFi Fallback Test

**Objective**: Verify automatic fallback to AP mode

**Procedure**:
1. Configure invalid WiFi credentials
2. Power on ESP32
3. Monitor serial output
4. Measure time until AP mode activated
5. Verify AP SSID and IP

**Expected Results**:
- AP mode activates within 30-35 seconds
- AP SSID: `MarineGateway-XXXXXX` (MAC-based)
- AP IP: `192.168.4.1`
- Dashboard accessible at `http://192.168.4.1`

**Pass Criteria**: AP mode <35s, dashboard accessible

---

### Multi-Client Stress Test

**Objective**: Verify system handles multiple simultaneous clients

**Procedure**:
1. Start NMEA stream (38400 baud)
2. Connect 5 TCP clients:
   ```bash
   nc <ESP_IP> 10110 > client1.log &
   nc <ESP_IP> 10110 > client2.log &
   nc <ESP_IP> 10110 > client3.log &
   nc <ESP_IP> 10110 > client4.log &
   nc <ESP_IP> 10110 > client5.log &
   ```
3. Connect 2 WebSocket clients (browser)
4. Run for 1 hour
5. Compare logs - all should match

**Expected Results**:
- All clients receive identical data
- No data loss or corruption
- System remains responsive
- Heap usage stable

**Pass Criteria**: Zero data discrepancies between clients

---

### Configuration Persistence Test

**Objective**: Verify settings survive reboots

**Procedure**:
1. Configure unique settings:
   - WiFi: SSID="TestNetwork", Password="test123"
   - Serial: Baud=9600, Data=8, Parity=None, Stop=1
2. Save via web interface
3. Power cycle ESP32 (10 times)
4. After each boot, verify settings via API

**Expected Results**:
- Settings identical after each reboot
- No corruption or reset to defaults

**Pass Criteria**: 10/10 reboots retain settings

---

### 24-Hour Uptime Test

**Objective**: Verify long-term stability

**Setup**:
- Connect GPS simulator (continuous NMEA)
- Connect 2 TCP clients (OpenCPN + netcat)
- Connect 1 WebSocket client (browser dashboard)
- Monitor heap usage every hour

**Data Collection**:
```
Hour | Free Heap | Min Heap | TCP Clients | WS Clients | NMEA Count
-----|-----------|----------|-------------|------------|------------
0    | 245120    | 240000   | 2           | 1          | 0
1    | 244980    | 239800   | 2           | 1          | 3600
...  | ...       | ...      | ...         | ...        | ...
24   | 244750    | 239500   | 2           | 1          | 86400
```

**Pass Criteria**:
- No crashes or reboots
- Min free heap >100KB throughout
- All clients remain connected
- NMEA sentence count matches expected (1 Hz = 86400 in 24h)

---

## Automated Testing Scripts

### NMEA Simulator (Python)

```python
#!/usr/bin/env python3
import serial
import time

NMEA_SENTENCES = [
    "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47",
    "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A",
    "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48",
]

def calculate_checksum(sentence):
    checksum = 0
    for char in sentence[1:sentence.index('*')]:
        checksum ^= ord(char)
    return checksum

def send_nmea(port, baud=38400, rate=1.0):
    ser = serial.Serial(port, baud)
    
    while True:
        for sentence in NMEA_SENTENCES:
            ser.write(f"{sentence}\r\n".encode())
            print(f"Sent: {sentence}")
            time.sleep(1.0 / rate)

if __name__ == "__main__":
    send_nmea("/dev/ttyUSB1", 38400, 1.0)  # 1 Hz
```

### TCP Client Validator (Python)

```python
#!/usr/bin/env python3
import socket
import time

def validate_tcp_stream(host, port=10110, duration=60):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    
    start_time = time.time()
    sentence_count = 0
    invalid_count = 0
    
    buffer = ""
    
    while time.time() - start_time < duration:
        data = sock.recv(1024).decode('utf-8')
        buffer += data
        
        while '\n' in buffer:
            line, buffer = buffer.split('\n', 1)
            line = line.strip()
            
            if line.startswith('$') and '*' in line:
                sentence_count += 1
                
                # Validate checksum
                try:
                    provided = int(line.split('*')[1], 16)
                    calculated = 0
                    for char in line[1:line.index('*')]:
                        calculated ^= ord(char)
                    
                    if provided != calculated:
                        invalid_count += 1
                        print(f"Invalid checksum: {line}")
                except:
                    invalid_count += 1
    
    sock.close()
    
    print(f"\n=== Results ===")
    print(f"Total sentences: {sentence_count}")
    print(f"Invalid checksums: {invalid_count}")
    print(f"Success rate: {(sentence_count - invalid_count) / sentence_count * 100:.2f}%")

if __name__ == "__main__":
    validate_tcp_stream("192.168.1.100", 10110, 60)
```

### API Test Suite (curl)

```bash
#!/bin/bash
ESP_IP="192.168.1.100"

echo "=== API Test Suite ==="

# Test 1: Get WiFi Config
echo "Test 1: GET /api/config/wifi"
curl -s http://${ESP_IP}/api/config/wifi | jq .

# Test 2: Set WiFi Config
echo "Test 2: POST /api/config/wifi"
curl -s -X POST http://${ESP_IP}/api/config/wifi \
  -H "Content-Type: application/json" \
  -d '{"ssid":"TestNet","password":"test123","mode":0}' | jq .

# Test 3: Get Serial Config
echo "Test 3: GET /api/config/serial"
curl -s http://${ESP_IP}/api/config/serial | jq .

# Test 4: Set Serial Config
echo "Test 4: POST /api/config/serial"
curl -s -X POST http://${ESP_IP}/api/config/serial \
  -H "Content-Type: application/json" \
  -d '{"baudRate":9600,"dataBits":8,"parity":0,"stopBits":1}' | jq .

# Test 5: Get Status
echo "Test 5: GET /api/status"
curl -s http://${ESP_IP}/api/status | jq .

echo "=== Tests Complete ==="
```

## Test Reporting Template

### Test Report Format

```markdown
# Test Report: [Feature Name]

**Date**: YYYY-MM-DD
**Tester**: [Name]
**Iteration**: [1-6]
**Firmware Version**: [Version]

## Test Environment
- Hardware: ESP32-S3
- NMEA Source: [GPS/Simulator]
- Network: [WiFi details]

## Tests Executed
- [ ] Test 1: [Description]
- [ ] Test 2: [Description]
- [ ] Test 3: [Description]

## Results Summary
- Tests Passed: X/Y
- Tests Failed: Z
- Pass Rate: XX%

## Failed Tests
### Test Name
- **Expected**: [Expected behavior]
- **Actual**: [Actual behavior]
- **Logs**: [Relevant log output]
- **Severity**: [Critical/High/Medium/Low]

## Issues Found
1. [Issue description]
2. [Issue description]

## Recommendations
- [Recommendation 1]
- [Recommendation 2]

## Conclusion
[Overall assessment]
```

## Continuous Testing Checklist

Before each commit:
- [ ] Code compiles without warnings
- [ ] All iteration tests pass
- [ ] No memory leaks observed
- [ ] Serial output clean (no errors)

Before each release:
- [ ] Full test suite executed
- [ ] 24-hour uptime test passed
- [ ] Documentation updated
- [ ] Version number incremented

## Known Limitations

Document known issues that don't block MVP:

1. **WiFi reconnection**: May take up to 60s after router restart
2. **WebSocket clients**: Limited to 3 simultaneous connections
3. **NMEA buffer**: 50 sentences max (older sentences dropped)
4. **Configuration**: Requires manual reboot after changes

## Future Test Enhancements

Post-MVP testing improvements:

1. **Unit Tests**: Add Google Test framework
2. **Automated CI**: GitHub Actions for build/test
3. **Load Testing**: Apache JMeter for HTTP/WS
4. **Real Instruments**: Test with actual marine electronics
5. **Environmental**: Temperature, voltage variation testing
