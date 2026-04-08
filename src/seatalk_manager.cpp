#include "seatalk_manager.h"
#include "functions.h"
#include <math.h>

// ── Command → key-code table (ST4000+, command 0x86, X=2) ────────────────────
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

// ── Constructor / Destructor ──────────────────────────────────────────────────

SeatalkManager::SeatalkManager(SeatalkRMT* r, BoatState* bs)
    : rmt(r), boatState(bs) {
    txMutex = xSemaphoreCreateMutex();
}

SeatalkManager::~SeatalkManager() {
    if (txMutex) vSemaphoreDelete(txMutex);
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
        serialPrintf("[ST1Mgr] Unknown command: %s\n", command);
        return false;
    }

    serialPrintf("[ST1Mgr] → %s (0x%02X)\n", command, keyCode);
    return sendCmd86(keyCode);
}

// ── Public: update ────────────────────────────────────────────────────────────

void SeatalkManager::update() {
    // Drive the RMT low-level reception.
    // SeatalkRMT::task() assembles frames and calls handleframe() which currently
    // just logs them.  To intercept assembled frames we rely on the fact that
    // SeatalkRMT exposes the last assembled frame via public members _frame /
    // _framelen — but that requires patching seatalk_rmt.h.
    //
    // For now we simply drive the RMT pump; full frame parsing can be wired in
    // once SeatalkRMT exposes a callback or the frame buffer publicly.
    if (rmt) rmt->task();
}

// ── Private: sendCmd86 ────────────────────────────────────────────────────────

bool SeatalkManager::sendCmd86(uint8_t keyCode) {
    if (!rmt) {
        serialPrintf("[ST1Mgr] No RMT — command dropped\n");
        return false;
    }

    uint8_t buf[4] = {
        0x86,
        0x21,
        keyCode,
        static_cast<uint8_t>(0xFF ^ keyCode)
    };

    bool ok = false;
    if (xSemaphoreTake(txMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        ok = rmt->sendDatagram(buf, sizeof(buf));
        xSemaphoreGive(txMutex);
    } else {
        serialPrintf("[ST1Mgr] TX mutex timeout — command dropped\n");
    }

    if (ok) {
        serialPrintf("[ST1Mgr] ✓ cmd86 sent (key=0x%02X)\n", keyCode);
    } else {
        serialPrintf("[ST1Mgr] ✗ cmd86 failed (key=0x%02X)\n", keyCode);
    }
    return ok;
}

// ── Private: parseFrame ───────────────────────────────────────────────────────
// SeaTalk1 frame format: byte[0]=CMD, byte[1]=ATTR (upper nibble=X, lower=length-3),
// byte[2..] = data bytes.

void SeatalkManager::parseFrame(const uint8_t* frame, uint8_t len) {
    if (!boatState || !frame || len < 3) return;

    uint8_t cmd = frame[0];

    switch (cmd) {

        // ── 0x84: Autopilot heading, mode, status ─────────────────────────
        // Format: 84  X6  VW  AP  AR  AM  xx  yy  MV  TH  AL
        // (11-byte datagram, X=1 → len byte = 6, so total = 3+8=11 bytes)
        case 0x84: {
            if (len < 11) break;

            // Autopilot mode byte (AM)
            uint8_t am = frame[5];
            // Mode bits: bit0=standby, bit1=auto, bit2=wind, bit3=track
            const char* mode = "standby";
            if      (am & 0x08) mode = "track";
            else if (am & 0x04) mode = "wind";
            else if (am & 0x02) mode = "auto";

            boatState->setAutopilotMode(String(mode));

            // Target heading (TH) – bytes 9-10, 1/10 degree units
            // TH = (byte9 & 0x03)<<8 | byte10   (in 1/10°, 0-359.9)
            uint16_t thRaw = ((uint16_t)(frame[9] & 0x03) << 8) | frame[10];
            float targetHeading = thRaw / 10.0f;
            boatState->setAutopilotHeadingTarget(targetHeading);

            // Rudder angle (AR): signed, in degrees
            int8_t rudder = (int8_t)frame[4];
            boatState->setAutopilotRudderAngle((float)rudder);

            // Status
            uint8_t ar = frame[3];
            bool offCourse = (ar & 0x10) != 0;
            boatState->setAutopilotStatus(offCourse ? String("alarm") : String("engaged"));

            break;
        }

        // ── 0x9C: Compass heading + rudder position ───────────────────────
        // Format: 9C  X1  HH  HR  RR
        // Heading = ((HH & 0x03)<<8 | HR) / 2  degrees
        // Rudder  = (signed) (RR - 0x80) or similar vendor encoding
        case 0x9C: {
            if (len < 5) break;

            uint16_t headingRaw = ((uint16_t)(frame[2] & 0x03) << 8) | frame[3];
            float heading = headingRaw / 2.0f;
            if (heading >= 0.0f && heading < 360.0f) {
                boatState->setMagneticHeading(heading);
            }

            // Rudder: frame[4] encodes signed rudder, 0x00=0°, positive=stbd
            // Some AP versions encode as offset from 0x00 or 0x80; treat as signed
            int8_t rudder = (int8_t)frame[4];
            boatState->setAutopilotRudderAngle((float)rudder);
            break;
        }

        // ── 0x10: Speed Through Water ─────────────────────────────────────
        // Format: 10  01  SS  SS  (SS = speed in 1/10 knot, 16-bit)
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
        // Format: 20  01  AW  AW
        // AWA = ((AW_hi & 0x3)<<8 | AW_lo) / 2  (0–359.5°, then normalise to ±180)
        case 0x20: {
            if (len < 4) break;
            uint16_t awaRaw = ((uint16_t)(frame[2] & 0x03) << 8) | frame[3];
            float awa = awaRaw / 2.0f;
            // Normalise to ±180°
            if (awa > 180.0f) awa -= 360.0f;
            // setApparentWind expects (speed, angle); speed unknown here → skip if 0 
            // We'll update only wind.awa via a dedicated setter path
            // For now update the boat state wind field directly through setTrueWind
            // using current AWS if valid, or simply update awa only via a workaround:
            // call setApparentWind with the current AWS value if available.
            WindData w = boatState->getWind();
            float aws = w.aws.valid ? w.aws.value : 0.0f;
            boatState->setApparentWind(aws, awa);
            break;
        }

        // ── 0x11: Apparent Wind Speed ─────────────────────────────────────
        // Format: 11  01  SS  0x0x
        // AWS = SS (integer knots) + (x/10) from low nibble of byte4?
        // Simplified: integer knots in byte[2]
        case 0x11: {
            if (len < 4) break;
            float aws = (float)frame[2];
            // fractional part in upper nibble of frame[3]
            uint8_t frac = (frame[3] >> 4) & 0x0F;
            aws += frac * 0.1f;
            WindData w = boatState->getWind();
            float awa = w.awa.valid ? w.awa.value : 0.0f;
            boatState->setApparentWind(aws, awa);
            break;
        }

        default:
            // Unknown or unhandled sentence — silently ignore
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
