#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

/**
 * @file debug_log.h
 * @brief In-RAM circular buffer capturing serialPrintf() output so it can be
 *        streamed to the web dashboard (see /ws/debug) when the physical
 *        serial/console port is not accessible (e.g. boat in real conditions).
 *
 * Design goals:
 *   - Extremely low overhead on the hot path: push() only copies a
 *     null-terminated string into a fixed-size ring slot behind a short,
 *     non-blocking mutex take. If the mutex is busy the line is dropped.
 *   - No dynamic allocation after init().
 *   - Consumers (WebServer) pull only the lines newer than their last seen
 *     sequence number, so broadcasting can be rate-limited independently.
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define DEBUG_LOG_CAPACITY   80    ///< Number of ring slots kept in RAM
#define DEBUG_LOG_LINE_LEN   128   ///< Max chars per stored line (incl. '\0')

class DebugLog {
public:
    static DebugLog& instance();

    /** Must be called once (from setup()) before push() is used from other tasks. */
    void init();

    /** Append a line (any task). Non-blocking — dropped if buffer is busy. */
    void push(const char* line);

    /**
     * @brief Collect all lines newer than lastSeq (exclusive), joined with '\n'.
     *        Updates lastSeq to the sequence number of the newest returned line.
     * @return Joined string, empty if nothing new.
     */
    String collectSince(uint32_t& lastSeq);

    /** @return Every currently buffered line (oldest to newest), joined with '\n'. */
    String collectAll();

    uint32_t currentSeq() const { return seqCounter; }

private:
    DebugLog() = default;

    struct Slot {
        uint32_t seq = 0;
        char     text[DEBUG_LOG_LINE_LEN] = {0};
    };

    Slot               slots[DEBUG_LOG_CAPACITY];
    volatile uint32_t  seqCounter = 0;   ///< Sequence number of the last pushed line
    volatile size_t    head       = 0;   ///< Next slot to write
    SemaphoreHandle_t  mutex      = nullptr;
    bool               initialized = false;
};

#endif // DEBUG_LOG_H
