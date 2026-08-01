#pragma once

#include <Arduino.h>
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "log_manager.h"

#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"


// ─────────────────────────────────────────────────────────────────────────────
// Timing constants — move to config.h if you need per-build overrides
// ─────────────────────────────────────────────────────────────────────────────
#define SEATALK_BIT_US          208
#define HALF_BIT_US             104
#define SEATALK_FRAME_TIMOUT    2080
#define IDLE_THRESHOLD_US       3000    // 3 ms of silence separates messages


typedef void (*seatalk_rx_callback_t)(uint16_t data);

class SeatalkRMT {
public:
    explicit SeatalkRMT(LogManager* logManager);

    /**
     * @brief Initialise the RMT RX and TX channels.
     *
     * @param rxPin      GPIO used for SeaTalk1 reception.
     * @param txPin      GPIO used for SeaTalk1 transmission.
     * @param rxChannel  RMT channel for reception  (default RMT_CHANNEL_4).
     * @param txChannel  RMT channel for transmission (default RMT_CHANNEL_1).
     * @param invertRx   When true, the ESP32 GPIO-matrix hardware inversion is
     *                   applied to rxPin before the signal enters the RMT
     *                   peripheral.  Use true when your circuit (opto-coupler,
     *                   level-shifter …) already inverts the SeaTalk bus once,
     *                   so that the RMT sees a corrected logic level.
     *                   Corresponds to ST1_INVERT_RX in config.h.
     * @param invertTx   When true, the RMT TX idle level is set to HIGH and
     *                   mark bits (start bit, command bit) are encoded as HIGH
     *                   pulses — the standard configuration for a single
     *                   inverting transistor driver stage.
     *                   When false, idle = LOW and marks are LOW pulses.
     *                   Corresponds to ST1_INVERT_TX in config.h.
     */
    void init(gpio_num_t rxPin, gpio_num_t txPin,
              rmt_channel_t rxChannel = RMT_CHANNEL_4,
              rmt_channel_t txChannel = RMT_CHANNEL_1,
              bool invertRx = true,
              bool invertTx = true);

    void task();                            ///< Call regularly from a FreeRTOS task.
    bool sendDatagram(uint8_t* buffer, uint8_t len);

    /**
     * @brief Pop the oldest fully-decoded RX frame from the internal queue,
     *        if any is pending.
     *
     * Thread-safe (guarded by _rxMutex). Each completed frame is delivered
     * exactly once, in FIFO order. Call this repeatedly (in a loop, until it
     * returns false) after task() to feed SeatalkManager::parseFrame() —
     * several frames may have completed since the last poll.
     *
     * @param outFrame  Buffer to receive the frame bytes (must hold >= 18 bytes).
     * @param outLen    Set to the frame length when a frame is returned.
     * @return true if a frame was copied into outFrame.
     */
    bool getFrame(uint8_t* outFrame, uint8_t& outLen);

private:
    // ── Pin / channel configuration ───────────────────────────────────────────
    gpio_num_t          _rxPin;
    gpio_num_t          _txPin;
    rmt_channel_t       _rxChannel;
    rmt_channel_t       _txChannel;

    /// Polarity flags stored at init time and used during TX encoding.
    bool                _invertRx;
    bool                _invertTx;

    // ── RMT driver handles ────────────────────────────────────────────────────
    rmt_config_t        rmt_rx;
    rmt_config_t        rmt_tx;
    RingbufHandle_t     _rb = nullptr;

    seatalk_rx_callback_t _callback = nullptr;

    /// Guards the RX ring-buffer + decode state machine below, since both
    /// task() (called from the SeaTalk FreeRTOS task) and sendDatagram()'s
    /// collision-check wait loop (which may be invoked from a different
    /// task, e.g. the web server) call processIncoming() concurrently.
    SemaphoreHandle_t   _rxMutex = nullptr;

    // ── RX state machine ──────────────────────────────────────────────────────
    uint32_t            _lasttransition;
    uint8_t             _inframe;
    uint8_t             _bitpos;
    uint8_t             _charpos;
    uint16_t            _shiftreg;
    uint8_t             _framelen;
    uint8_t             _frame[18];

    /// Small FIFO queue of fully-decoded frames awaiting consumption by
    /// getFrame(). A single-slot buffer would silently drop frames whenever
    /// several datagrams complete between two getFrame() polls (e.g. rare
    /// datagrams like 0x21/0x22 trip/total getting overwritten by frequent
    /// ones such as 0x53 COG or 0x9C heading before they can be read).
    static const uint8_t kFrameQueueSize = 8;
    uint8_t             _frameQueue[kFrameQueueSize][18];
    uint8_t             _frameQueueLen[kFrameQueueSize];
    uint8_t             _frameQueueHead = 0;   ///< Next slot to pop from.
    uint8_t             _frameQueueTail = 0;   ///< Next slot to push into.
    uint8_t             _frameQueueCount = 0;  ///< Number of frames currently queued.

    // ── TX item buffer ────────────────────────────────────────────────────────
    rmt_item32_t        _items[128];
    uint8_t             _itemcount1;
    uint8_t             _itemcount0;
    uint8_t             _itemtransitions;
    uint8_t             _itemlastlevel;

    LogManager*         _logManager;

    // ── TX helpers ────────────────────────────────────────────────────────────

    /**
     * @brief Append one bit to the RMT item buffer, respecting _invertTx.
     *
     * @param bit        Logical bit value (1 = mark, 0 = space) before any
     *                   polarity inversion.
     * @param closeframe Set to 1 only for the synthetic closing pulse that
     *                   terminates the item list.
     */
    void addItemBit(uint8_t bit, uint8_t closeframe = 0);

    void sendDatagramNoCD(uint8_t* buffer, uint8_t len);

    // ── RX helpers ────────────────────────────────────────────────────────────
    void addbit(uint8_t level, uint8_t count);
    void addchar();
    void handleframe();

    /**
     * @brief Drain and process whatever is currently available in the RMT RX
     *        ring buffer, and finalize a stalled frame after
     *        SEATALK_FRAME_TIMOUT of silence.
     *
     * This is the single source of truth for RX processing — both task()
     * and sendDatagram()'s collision-check wait loop call it so that RX
     * decoding (and therefore `_frame`) keeps advancing in real time even
     * while sendDatagram() is busy-waiting for its own echo.
     *
     * @param rbTimeoutMs  Max time to block waiting on the ring buffer.
     */
    void processIncoming(uint32_t rbTimeoutMs);

    uint8_t reverse8(uint8_t x);
};