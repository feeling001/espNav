#ifndef SEATALK_MANAGER_H
#define SEATALK_MANAGER_H

#include <Arduino.h>
#include "seatalk_rmt.h"
#include "boat_state.h"

/**
 * @brief Semantic SeaTalk1 layer — sits above SeatalkRMT.
 *
 * Responsibilities:
 *   1. Translate semantic autopilot commands ("auto", "adjust+1", …) into
 *      SeaTalk1 datagrams and call SeatalkRMT::sendDatagram().
 *   2. Parse incoming SeaTalk1 frames (autopilot status, heading locked,
 *      rudder angle …) and update BoatState accordingly.
 *
 * Usage:
 *   Instantiate once in main.cpp.  Pass a pointer to both WebServer and
 *   BLEManager so each can call sendAutopilotCommand() without duplicating
 *   the datagram logic.
 *
 * Thread safety:
 *   sendAutopilotCommand() can be called from different tasks/cores.
 *   A FreeRTOS mutex serialises access to SeatalkRMT::sendDatagram().
 */

// ── Accepted command strings ──────────────────────────────────────────────────
// "standby"        → disengage (ST4000+ key 0x02)
// "auto"           → compass lock (0x01)
// "wind"           → wind vane mode (0x23)
// "track"          → GPS track (0x03)
// "adjust-1"       → course −1° (0x05)
// "adjust-10"      → course −10° (0x06)
// "adjust+1"       → course +1° (0x07)
// "adjust+10"      → course +10° (0x08)
// "tack-port"      → port tack (0x21)
// "tack-starboard" → starboard tack (0x22)

class SeatalkManager {
public:
    /**
     * @param rmt       Pointer to the already-initialised SeatalkRMT instance.
     * @param boatState Pointer to the shared BoatState; updated when AP frames
     *                  are received.  May be nullptr (parsing disabled).
     */
    SeatalkManager(SeatalkRMT* rmt, BoatState* boatState = nullptr);
    ~SeatalkManager();

    // ── Command dispatch ──────────────────────────────────────────────────────

    /**
     * @brief Send a semantic autopilot command over the SeaTalk1 bus.
     *
     * @param command  One of the command strings listed above.
     * @return true on successful transmission, false on unknown command or
     *         transmission failure (collision after retries).
     */
    bool sendAutopilotCommand(const char* command);

    // ── Incoming frame processing ─────────────────────────────────────────────

    /**
     * @brief Process pending bytes from SeatalkRMT and update BoatState.
     *
     * Call this regularly from the SeaTalk FreeRTOS task (replaces the direct
     * seatalkHandler.task() call).  Internally calls SeatalkRMT::task() then
     * inspects any complete frame that was assembled.
     */
    void update();

private:
    SeatalkRMT*  rmt;
    BoatState*   boatState;
    SemaphoreHandle_t txMutex;

    // ── Low-level helpers ─────────────────────────────────────────────────────

    /**
     * @brief Build and transmit a 4-byte command-86 datagram (ST4000+, X=2).
     *
     * Datagram layout: 0x86  0x21  keyCode  (0xFF ^ keyCode)
     */
    bool sendCmd86(uint8_t keyCode);

    // ── Incoming frame parser ─────────────────────────────────────────────────

    /**
     * @brief Decode a complete SeaTalk frame and update BoatState.
     *
     * Currently decoded sentences:
     *   0x84  — autopilot heading/mode/status
     *   0x9C  — compass heading + rudder position
     *   0x10  — speed through water (VHW)
     *   0x20  — wind angle (apparent)
     *   0x11  — apparent wind speed
     */
    void parseFrame(const uint8_t* frame, uint8_t len);

    // Helpers for fixed-point SeaTalk fields
    static float st1ToDegrees(uint8_t hi, uint8_t lo);
    static float st1ToKnots(uint8_t byte);
};

#endif // SEATALK_MANAGER_H
