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
 *   2. Translate extra utility commands ("lamp:0"…"lamp:3", "alarm-ack",
 *      "beep") into their respective datagrams.
 *   3. Parse incoming SeaTalk1 frames (autopilot status, heading locked,
 *      rudder angle …) and update BoatState accordingly.
 *
 * Usage:
 *   Instantiate once in main.cpp.  Pass a pointer to both WebServer and
 *   BLEManager so each can call sendAutopilotCommand() / sendExtraCommand()
 *   without duplicating the datagram logic.
 *
 * Thread safety:
 *   sendAutopilotCommand() and sendExtraCommand() can be called from
 *   different tasks/cores.  A FreeRTOS mutex serialises access to
 *   SeatalkRMT::sendDatagram().
 */

// ── Accepted autopilot command strings ───────────────────────────────────────
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

// ── Accepted extra command strings ───────────────────────────────────────────
// "lamp:0"         → Set lamp intensity off     (datagram 30 00 00)
// "lamp:1"         → Set lamp intensity level 1 (datagram 30 00 04)
// "lamp:2"         → Set lamp intensity level 2 (datagram 30 00 08)
// "lamp:3"         → Set lamp intensity level 3 (datagram 30 00 0C)
// "alarm-ack"      → Acknowledge alarm (ST40 keystroke 68 41 15 00)
// "beep"           → Trigger a key beep via Disp keystroke (86 21 04 FB)

class SeatalkManager {
public:
    /**
     * @param rmt       Pointer to the already-initialised SeatalkRMT instance.
     * @param boatState Pointer to the shared BoatState; updated when AP frames
     *                  are received.  May be nullptr (parsing disabled).
     */
    SeatalkManager(SeatalkRMT* rmt, BoatState* boatState = nullptr);
    ~SeatalkManager();

    // ── Autopilot command dispatch ────────────────────────────────────────────

    /**
     * @brief Send a semantic autopilot command over the SeaTalk1 bus.
     *
     * @param command  One of the autopilot command strings listed above.
     * @return true on successful transmission, false on unknown command or
     *         transmission failure (collision after retries).
     */
    bool sendAutopilotCommand(const char* command);

    // ── Extra command dispatch ────────────────────────────────────────────────

    /**
     * @brief Send a utility / instrument command over the SeaTalk1 bus.
     *
     * Supported commands:
     *   "lamp:0"   — lamp off         (datagram 0x30 0x00 0x00)
     *   "lamp:1"   — lamp level 1     (datagram 0x30 0x00 0x04)
     *   "lamp:2"   — lamp level 2     (datagram 0x30 0x00 0x08)
     *   "lamp:3"   — lamp level 3     (datagram 0x30 0x00 0x0C)
     *   "alarm-ack"— acknowledge alarm (datagram 0x68 0x41 0x15 0x00)
     *   "beep"     — key beep via Disp keystroke (datagram 0x86 0x21 0x04 0xFB)
     *
     * @param command  One of the extra command strings listed above.
     * @return true on successful transmission, false on unknown command or
     *         transmission failure.
     */
    bool sendExtraCommand(const char* command);

    // ── Incoming frame processing ─────────────────────────────────────────────

    /**
     * @brief Process pending bytes from SeatalkRMT and update BoatState.
     *
     * Call this regularly from the SeaTalk FreeRTOS task.  Internally calls
     * SeatalkRMT::task() then inspects any complete frame that was assembled.
     */
    void update();

private:
    SeatalkRMT*       rmt;
    BoatState*        boatState;
    SemaphoreHandle_t txMutex;

    // ── Low-level helpers ─────────────────────────────────────────────────────

    /**
     * @brief Build and transmit a 4-byte command-86 datagram (ST4000+, X=2).
     *
     * Datagram layout: 0x86  0x21  keyCode  (0xFF ^ keyCode)
     */
    bool sendCmd86(uint8_t keyCode);

    /**
     * @brief Send an arbitrary raw datagram, acquiring the TX mutex.
     *
     * @param buf   Datagram bytes (already fully formed).
     * @param len   Number of bytes (3–18).
     * @return true on successful transmission.
     */
    bool sendRaw(uint8_t* buf, uint8_t len);

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
