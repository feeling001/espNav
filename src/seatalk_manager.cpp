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
