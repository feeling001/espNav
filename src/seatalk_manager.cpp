#include "seatalk_manager.h"
#include "functions.h"
#include <math.h>

// ── Autopilot command → key-code table (ST4000+, command 0x86, X=2) ──────────
// Reference: http://www.thomasknauf.de/rap/seatalk2.htm
struct CmdEntry {
    const char* name;
    uint8_t     keyCode;
};

static const CmdEntry CMD_TABLE[] = {
    { "auto",            0x01 },
    { "standby",         0x02 },
    { "track",           0x03 },
    { "adjust-1",        0x05 },
    { "adjust-10",       0x06 },
    { "adjust+1",        0x07 },
    { "adjust+10",       0x08 },
    { "wind",            0x23 },
    { "tack-port",       0x21 },
    { "tack-starboard",  0x22 },
    { nullptr,           0x00 },   // sentinel
};

// ── Lamp intensity nibbles (datagram 0x30 0x00 0x0X) ─────────────────────────
// X=0: off, X=4: L1, X=8: L2, X=C: L3
// Reference: datagram 30  00  0X
static const struct { const char* name; uint8_t nibble; } LAMP_TABLE[] = {
    { "lamp:0", 0x00 },
    { "lamp:1", 0x04 },
    { "lamp:2", 0x08 },
    { "lamp:3", 0x0C },
    { nullptr,  0x00 },
};

// ── Constructor / Destructor ──────────────────────────────────────────────────

SeatalkManager::SeatalkManager(SeatalkRMT* r, BoatState* bs)
    : rmt(r), boatState(bs) {
    txMutex   = xSemaphoreCreateMutex();
    _convMutex = xSemaphoreCreateMutex();
    memset(_convLastSent, 0, sizeof(_convLastSent));
}

SeatalkManager::~SeatalkManager() {
    if (txMutex)    vSemaphoreDelete(txMutex);
    if (_convMutex) vSemaphoreDelete(_convMutex);
}

// ── Public: sendAutopilotCommand ──────────────────────────────────────────────

bool SeatalkManager::sendAutopilotCommand(const char* command) {
    if (!command || command[0] == '\0') {
        serialPrintf("[ST1Mgr] Empty command\n");
        return false;
    }

    // Lookup key code
    uint8_t keyCode = 0;
    bool found = false;
    for (int i = 0; CMD_TABLE[i].name != nullptr; i++) {
        if (strcmp(command, CMD_TABLE[i].name) == 0) {
            keyCode = CMD_TABLE[i].keyCode;
            found   = true;
            break;
        }
    }

    if (!found) {
        serialPrintf("[ST1Mgr] Unknown autopilot command: %s\n", command);
        return false;
    }

    serialPrintf("[ST1Mgr] → autopilot %s (key=0x%02X)\n", command, keyCode);
    return sendCmd86(keyCode);
}

// ── Public: sendExtraCommand ──────────────────────────────────────────────────

bool SeatalkManager::sendExtraCommand(const char* command) {
    if (!command || command[0] == '\0') {
        serialPrintf("[ST1Mgr] Empty extra command\n");
        return false;
    }

    serialPrintf("[ST1Mgr] → extra command: %s\n", command);

    // ── Lamp intensity: "lamp:0" … "lamp:3" ──────────────────────────────────
    // Datagram 30  00  0X  (3 bytes)
    // Ref: "30  00  0X  Set lamp Intensity; X=0: L0, X=4: L1, X=8: L2, X=C: L3"
    for (int i = 0; LAMP_TABLE[i].name != nullptr; i++) {
        if (strcmp(command, LAMP_TABLE[i].name) == 0) {
            uint8_t buf[3] = { 0x30, 0x00, LAMP_TABLE[i].nibble };
            serialPrintf("[ST1Mgr]   lamp datagram: 30 00 %02X\n", buf[2]);
            return sendRaw(buf, 3);
        }
    }

    // ── Alarm acknowledge ─────────────────────────────────────────────────────
    // Datagram 68  41  15  00  (from ST40 Wind Instrument — generic acknowledge)
    // Ref: "68  41  15  00  Alarm acknowledgment keystroke (from ST40 Wind Instrument)"
    if (strcmp(command, "alarm-ack") == 0) {
        uint8_t buf[4] = { 0x68, 0x41, 0x15, 0x00 };
        serialPrintf("[ST1Mgr]   alarm-ack datagram: 68 41 15 00\n");
        return sendRaw(buf, 4);
    }

    // ── Beep: trigger via "Disp" keystroke (0x04) ────────────────────────────
    // The ST4000+ emits an audible beep on every valid keystroke reception.
    // We send the "Disp/page" key (0x04) which produces a single beep without
    // changing autopilot state.
    // Datagram 86  21  04  FB  (command 0x86, X=2, key=0x04, checksum=0xFF^0x04)
    // Ref: "X1  04  FB  disp (in display mode or page in auto chapter = advance)"
    if (strcmp(command, "beep_on") == 0) {
        serialPrintf("[ST1Mgr]   beep on datagram: A8  53  80 00 00 D3\n");
        //return sendCmd86(0x04);
        uint8_t buf[6] = {
            0xA8,
            0x53,
            0x80,
            0x00,
            0x00,
            0xD3
            };
        return sendRaw(buf, 6);
    }

    if (strcmp(command, "beep_off") == 0) {
        serialPrintf("[ST1Mgr]   beep off datagram: A8  43  80 00 00 C3\n");
        //return sendCmd86(0x04);
        uint8_t buf[6] = {
            0xA8,
            0x43,
            0x80,
            0x00,
            0x00,
            0xC3
            };
        return sendRaw(buf, 6);
    }

    serialPrintf("[ST1Mgr] Unknown extra command: %s\n", command);
    return false;
}

// ── Public: update ────────────────────────────────────────────────────────────

void SeatalkManager::update() {
    if (rmt) rmt->task();
}

// ── Private: sendCmd86 ────────────────────────────────────────────────────────

bool SeatalkManager::sendCmd86(uint8_t keyCode) {
    uint8_t buf[4] = {
        0x86,
        0x21,
        keyCode,
        static_cast<uint8_t>(0xFF ^ keyCode)
    };
    return sendRaw(buf, 4);
}

// ── Private: sendRaw ─────────────────────────────────────────────────────────

bool SeatalkManager::sendRaw(uint8_t* buf, uint8_t len) {
    if (!rmt) {
        serialPrintf("[ST1Mgr] No RMT — datagram dropped\n");
        return false;
    }

    bool ok = false;
    if (xSemaphoreTake(txMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        ok = rmt->sendDatagram(buf, len);
        xSemaphoreGive(txMutex);
    } else {
        serialPrintf("[ST1Mgr] TX mutex timeout — datagram dropped\n");
    }

    if (ok) {
        serialPrintf("[ST1Mgr] ✓ datagram sent (%u bytes)\n", len);
    } else {
        serialPrintf("[ST1Mgr] ✗ datagram failed (%u bytes)\n", len);
    }
    return ok;
}

// ── Public: setConversionConfig / getConversionConfig ────────────────────────

void SeatalkManager::setConversionConfig(const ConversionConfig& cfg) {
    if (xSemaphoreTake(_convMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        _convCfg = cfg;
        xSemaphoreGive(_convMutex);
    }
}

ConversionConfig SeatalkManager::getConversionConfig() const {
    ConversionConfig copy;
    if (xSemaphoreTake(_convMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        copy = _convCfg;
        xSemaphoreGive(_convMutex);
    }
    return copy;
}

// ── Public: runConversions ────────────────────────────────────────────────────

void SeatalkManager::runConversions(BoatState* bs) {
    if (!bs) return;

    ConversionConfig cfg;
    if (xSemaphoreTake(_convMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        cfg = _convCfg;
        xSemaphoreGive(_convMutex);
    } else {
        return;
    }

    uint32_t now = millis();

    auto due = [&](int idx) -> bool {
        if (!cfg.rules[idx].enabled) return false;
        uint32_t interval_ms = (uint32_t)cfg.rules[idx].interval_s * 1000UL;
        return (now - _convLastSent[idx]) >= interval_ms;
    };

    // ── 0: GPS COG ─────────────────────────────────────────────────────────
    if (due(CONV_GPS_COG_TO_ST)) {
        GPSData gps = bs->getGPS();
        if (gps.cog.valid && !gps.cog.isStale()) {
            if (convSendCOG(gps.cog.value))
                _convLastSent[CONV_GPS_COG_TO_ST] = now;
        }
    }
    // ── 1: GPS SOG ─────────────────────────────────────────────────────────
    if (due(CONV_GPS_SOG_TO_ST)) {
        GPSData gps = bs->getGPS();
        if (gps.sog.valid && !gps.sog.isStale()) {
            if (convSendSOG(gps.sog.value))
                _convLastSent[CONV_GPS_SOG_TO_ST] = now;
        }
    }
    // ── 2: GPS Position ────────────────────────────────────────────────────
    if (due(CONV_GPS_POS_TO_ST)) {
        GPSData gps = bs->getGPS();
        if (gps.position.lat.valid && !gps.position.lat.isStale() &&
            gps.position.lon.valid && !gps.position.lon.isStale()) {
            if (convSendPosition(gps.position.lat.value, gps.position.lon.value))
                _convLastSent[CONV_GPS_POS_TO_ST] = now;
        }
    }
    // ── 3: Depth ───────────────────────────────────────────────────────────
    if (due(CONV_DEPTH_TO_ST)) {
        DepthData depth = bs->getDepth();
        if (depth.below_transducer.valid && !depth.below_transducer.isStale()) {
            if (convSendDepth(depth.below_transducer.value))
                _convLastSent[CONV_DEPTH_TO_ST] = now;
        }
    }
    // ── 4: STW ─────────────────────────────────────────────────────────────
    if (due(CONV_STW_TO_ST)) {
        SpeedData speed = bs->getSpeed();
        if (speed.stw.valid && !speed.stw.isStale()) {
            if (convSendSTW(speed.stw.value))
                _convLastSent[CONV_STW_TO_ST] = now;
        }
    }
    // ── 5: AWA ─────────────────────────────────────────────────────────────
    if (due(CONV_AWA_TO_ST)) {
        WindData wind = bs->getWind();
        if (wind.awa.valid && !wind.awa.isStale()) {
            if (convSendAWA(wind.awa.value))
                _convLastSent[CONV_AWA_TO_ST] = now;
        }
    }
    // ── 6: AWS ─────────────────────────────────────────────────────────────
    if (due(CONV_AWS_TO_ST)) {
        WindData wind = bs->getWind();
        if (wind.aws.valid && !wind.aws.isStale()) {
            if (convSendAWS(wind.aws.value))
                _convLastSent[CONV_AWS_TO_ST] = now;
        }
    }
    // ── 7: Water Temperature ───────────────────────────────────────────────
    if (due(CONV_WATER_TEMP_TO_ST)) {
        EnvironmentData env = bs->getEnvironment();
        if (env.water_temp.valid && !env.water_temp.isStale()) {
            if (convSendWaterTemp(env.water_temp.value))
                _convLastSent[CONV_WATER_TEMP_TO_ST] = now;
        }
    }
    // ── 8: Heading ─────────────────────────────────────────────────────────
    if (due(CONV_HDG_TO_ST)) {
        HeadingData hdg = bs->getHeading();
        if (hdg.magnetic.valid && !hdg.magnetic.isStale()) {
            if (convSendHeading(hdg.magnetic.value))
                _convLastSent[CONV_HDG_TO_ST] = now;
        }
    }
}

// ── Private: encodeAngle ──────────────────────────────────────────────────────
// Encode a 0-359° integer angle to the SeaTalk U-nibble/VW-byte format used
// by datagrams 0x53 (COG), 0x89 and 0x9C (compass heading).
// Formula (decode): (U & 0x3)*90 + (VW & 0x3F)*2 + (U & 0xC)/8
// Resolution: 1 degree.

void SeatalkManager::encodeAngle(uint16_t deg, uint8_t& U_nibble, uint8_t& VW) {
    deg = deg % 360;
    uint8_t  quadrant = deg / 90;
    uint16_t rem      = deg % 90;
    uint8_t  vw_val   = rem / 2;
    uint8_t  half     = rem % 2;           // 0 or 1 extra degree
    U_nibble = (quadrant & 0x3) | (half ? 0x08 : 0x00);
    VW       = vw_val & 0x3F;
}

// ── Private: convSendCOG ── datagram 0x53 ────────────────────────────────────
// Ref: 53  U0  VW   COG: (U&3)*90 + (VW&0x3F)*2 + (U&0xC)/8
bool SeatalkManager::convSendCOG(float cog_deg) {
    if (cog_deg < 0.0f) cog_deg += 360.0f;
    uint8_t U_nib, VW;
    encodeAngle((uint16_t)roundf(cog_deg) % 360, U_nib, VW);
    uint8_t buf[3] = { 0x53, (uint8_t)((U_nib << 4) | 0x00), VW };
    serialPrintf("[ST1Conv] COG %.1f° → 53 %02X %02X\n", cog_deg, buf[1], buf[2]);
    return sendRaw(buf, 3);
}

// ── Private: convSendSOG ── datagram 0x52 ────────────────────────────────────
// Ref: 52  01  XX  XX   SOG: XXXX/10 knots
bool SeatalkManager::convSendSOG(float sog_knots) {
    if (sog_knots < 0.0f) sog_knots = 0.0f;
    uint16_t val = (uint16_t)roundf(sog_knots * 10.0f);
    uint8_t buf[4] = { 0x52, 0x01, (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };
    serialPrintf("[ST1Conv] SOG %.1f kn → 52 01 %02X %02X\n", sog_knots, buf[2], buf[3]);
    return sendRaw(buf, 4);
}

// ── Private: convSendPosition ── datagrams 0x50 + 0x51 ───────────────────────
// Ref: 50  Z2  XX  YY  YY   LAT: XX deg, (YYYY&0x7FFF)/100 min, South if bit15
//      51  Z2  XX  YY  YY   LON: XX deg, (YYYY&0x7FFF)/100 min, East  if bit15
bool SeatalkManager::convSendPosition(float lat, float lon) {
    // ── LAT ──
    float absLat   = fabsf(lat);
    uint8_t latDeg = (uint8_t)absLat;
    float   latMin = (absLat - latDeg) * 60.0f;
    uint16_t latYYYY = (uint16_t)roundf(latMin * 100.0f) & 0x7FFF;
    if (lat < 0.0f) latYYYY |= 0x8000;  // South
    uint8_t bufLat[5] = {
        0x50, 0x02,
        latDeg,
        (uint8_t)(latYYYY & 0xFF),
        (uint8_t)(latYYYY >> 8)
    };
    bool okLat = sendRaw(bufLat, 5);

    // ── LON ──
    float absLon   = fabsf(lon);
    uint8_t lonDeg = (uint8_t)absLon;
    float   lonMin = (absLon - lonDeg) * 60.0f;
    uint16_t lonYYYY = (uint16_t)roundf(lonMin * 100.0f) & 0x7FFF;
    if (lon >= 0.0f) lonYYYY |= 0x8000;  // East
    uint8_t bufLon[5] = {
        0x51, 0x02,
        lonDeg,
        (uint8_t)(lonYYYY & 0xFF),
        (uint8_t)(lonYYYY >> 8)
    };
    bool okLon = sendRaw(bufLon, 5);

    serialPrintf("[ST1Conv] POS %.4f,%.4f → LAT %s LON %s\n",
                 lat, lon, okLat ? "OK" : "FAIL", okLon ? "OK" : "FAIL");
    return okLat && okLon;
}

// ── Private: convSendDepth ── datagram 0x00 ───────────────────────────────────
// Ref: 00  02  YZ  XX XX   Depth: XXXX/10 feet
bool SeatalkManager::convSendDepth(float depth_m) {
    if (depth_m < 0.0f) depth_m = 0.0f;
    float depth_ft = depth_m * 3.28084f;
    uint16_t val = (uint16_t)roundf(depth_ft * 10.0f);
    uint8_t buf[5] = { 0x00, 0x02, 0x00,
                       (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };
    serialPrintf("[ST1Conv] Depth %.1f m → 00 02 00 %02X %02X\n", depth_m, buf[3], buf[4]);
    return sendRaw(buf, 5);
}

// ── Private: convSendSTW ── datagram 0x20 ────────────────────────────────────
// Ref: 20  01  XX  XX   STW: XXXX/10 knots
bool SeatalkManager::convSendSTW(float stw_knots) {
    if (stw_knots < 0.0f) stw_knots = 0.0f;
    uint16_t val = (uint16_t)roundf(stw_knots * 10.0f);
    uint8_t buf[4] = { 0x20, 0x01, (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };
    serialPrintf("[ST1Conv] STW %.1f kn → 20 01 %02X %02X\n", stw_knots, buf[2], buf[3]);
    return sendRaw(buf, 4);
}

// ── Private: convSendAWA ── datagram 0x10 ────────────────────────────────────
// Ref: 10  01  XX  YY   AWA: XXYY/2 degrees right of bow (big-endian)
bool SeatalkManager::convSendAWA(float awa_deg) {
    // Normalise to 0-360 (right of bow convention)
    while (awa_deg < 0.0f)    awa_deg += 360.0f;
    while (awa_deg >= 360.0f) awa_deg -= 360.0f;
    uint16_t val = (uint16_t)roundf(awa_deg * 2.0f);
    uint8_t buf[4] = { 0x10, 0x01,
                       (uint8_t)(val >> 8),
                       (uint8_t)(val & 0xFF) };
    serialPrintf("[ST1Conv] AWA %.1f° → 10 01 %02X %02X\n", awa_deg, buf[2], buf[3]);
    return sendRaw(buf, 4);
}

// ── Private: convSendAWS ── datagram 0x11 ────────────────────────────────────
// Ref: 11  01  XX  0Y   AWS: (XX&0x7F) + Y/10 knots; XX&0x80=0 → knots
bool SeatalkManager::convSendAWS(float aws_knots) {
    if (aws_knots < 0.0f) aws_knots = 0.0f;
    uint8_t integer  = (uint8_t)floorf(aws_knots);
    uint8_t frac     = (uint8_t)roundf((aws_knots - integer) * 10.0f);
    if (frac > 9) { frac = 0; integer++; }
    uint8_t buf[4] = { 0x11, 0x01,
                       (uint8_t)(integer & 0x7F),   // units = knots (bit7=0)
                       (uint8_t)(frac & 0x0F) };
    serialPrintf("[ST1Conv] AWS %.1f kn → 11 01 %02X %02X\n", aws_knots, buf[2], buf[3]);
    return sendRaw(buf, 4);
}

// ── Private: convSendWaterTemp ── datagram 0x27 ───────────────────────────────
// Ref: 27  01  XX  XX   Water temp: (XXXX-100)/10 °C
bool SeatalkManager::convSendWaterTemp(float temp_c) {
    int16_t val = (int16_t)roundf(temp_c * 10.0f) + 100;
    if (val < 0) val = 0;
    uint8_t buf[4] = { 0x27, 0x01,
                       (uint8_t)(val & 0xFF),
                       (uint8_t)((val >> 8) & 0xFF) };
    serialPrintf("[ST1Conv] WaterTemp %.1f°C → 27 01 %02X %02X\n", temp_c, buf[2], buf[3]);
    return sendRaw(buf, 4);
}

// ── Private: convSendHeading ── datagram 0x9C ─────────────────────────────────
// Ref: 9C  U1  VW  RR   Compass heading + rudder (RR=0 → unknown)
// Heading encoding identical to 0x53 (COG).
bool SeatalkManager::convSendHeading(float hdg_deg) {
    if (hdg_deg < 0.0f) hdg_deg += 360.0f;
    uint8_t U_nib, VW;
    encodeAngle((uint16_t)roundf(hdg_deg) % 360, U_nib, VW);
    uint8_t buf[4] = { 0x9C, (uint8_t)((U_nib << 4) | 0x01), VW, 0x00 };
    serialPrintf("[ST1Conv] HDG %.1f° → 9C %02X %02X 00\n", hdg_deg, buf[1], buf[2]);
    return sendRaw(buf, 4);
}

// ── Private: parseFrame ───────────────────────────────────────────────────────

void SeatalkManager::parseFrame(const uint8_t* frame, uint8_t len) {
    if (!boatState || !frame || len < 3) return;

    uint8_t cmd = frame[0];

    switch (cmd) {

        // ── 0x84: Autopilot heading, mode, status ─────────────────────────
        case 0x84: {
            if (len < 11) break;

            uint8_t am = frame[5];
            const char* mode = "standby";
            if      (am & 0x08) mode = "track";
            else if (am & 0x04) mode = "wind";
            else if (am & 0x02) mode = "auto";

            boatState->setAutopilotMode(String(mode));

            uint16_t thRaw = ((uint16_t)(frame[9] & 0x03) << 8) | frame[10];
            float targetHeading = thRaw / 10.0f;
            boatState->setAutopilotHeadingTarget(targetHeading);

            int8_t rudder = (int8_t)frame[4];
            boatState->setAutopilotRudderAngle((float)rudder);

            uint8_t ar = frame[3];
            bool offCourse = (ar & 0x10) != 0;
            boatState->setAutopilotStatus(offCourse ? String("alarm") : String("engaged"));
            break;
        }

        // ── 0x9C: Compass heading + rudder position ───────────────────────
        case 0x9C: {
            if (len < 5) break;

            uint16_t headingRaw = ((uint16_t)(frame[2] & 0x03) << 8) | frame[3];
            float heading = headingRaw / 2.0f;
            if (heading >= 0.0f && heading < 360.0f) {
                boatState->setMagneticHeading(heading);
            }

            int8_t rudder = (int8_t)frame[4];
            boatState->setAutopilotRudderAngle((float)rudder);
            break;
        }

        // ── 0x10: Speed Through Water ─────────────────────────────────────
        case 0x10: {
            if (len < 4) break;
            uint16_t stwRaw = ((uint16_t)frame[2] << 8) | frame[3];
            float stw = stwRaw / 10.0f;
            if (stw >= 0.0f && stw < 100.0f) {
                boatState->setSTW(stw);
            }
            break;
        }

        // ── 0x20: Apparent Wind Angle ─────────────────────────────────────
        case 0x20: {
            if (len < 4) break;
            uint16_t awaRaw = ((uint16_t)(frame[2] & 0x03) << 8) | frame[3];
            float awa = awaRaw / 2.0f;
            if (awa > 180.0f) awa -= 360.0f;
            WindData w = boatState->getWind();
            float aws = w.aws.valid ? w.aws.value : 0.0f;
            boatState->setApparentWind(aws, awa);
            break;
        }

        // ── 0x11: Apparent Wind Speed ─────────────────────────────────────
        case 0x11: {
            if (len < 4) break;
            float aws = (float)frame[2];
            uint8_t frac = (frame[3] >> 4) & 0x0F;
            aws += frac * 0.1f;
            WindData w = boatState->getWind();
            float awa = w.awa.valid ? w.awa.value : 0.0f;
            boatState->setApparentWind(aws, awa);
            break;
        }

        default:
            break;
    }
}

// ── Static helpers ────────────────────────────────────────────────────────────

float SeatalkManager::st1ToDegrees(uint8_t hi, uint8_t lo) {
    uint16_t raw = ((uint16_t)(hi & 0x03) << 8) | lo;
    return raw / 2.0f;
}

float SeatalkManager::st1ToKnots(uint8_t byte) {
    return byte / 10.0f;
}
