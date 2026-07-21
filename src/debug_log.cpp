/**
 * @file debug_log.cpp
 * @brief Implementation of the in-RAM debug log ring buffer.
 */

#include "debug_log.h"

DebugLog& DebugLog::instance() {
    static DebugLog inst;
    return inst;
}

void DebugLog::init() {
    if (initialized) return;
    mutex = xSemaphoreCreateMutex();
    initialized = true;
}

void DebugLog::push(const char* line) {
    if (!initialized || !mutex || !line) return;

    // Non-blocking: if another task holds the mutex, drop the line rather
    // than risk stalling a real-time producer (UART/SeaTalk/NMEA tasks).
    if (xSemaphoreTake(mutex, 0) != pdTRUE) return;

    Slot& slot = slots[head];
    strncpy(slot.text, line, DEBUG_LOG_LINE_LEN - 1);
    slot.text[DEBUG_LOG_LINE_LEN - 1] = '\0';
    slot.seq = ++seqCounter;

    head = (head + 1) % DEBUG_LOG_CAPACITY;

    xSemaphoreGive(mutex);
}

String DebugLog::collectSince(uint32_t& lastSeq) {
    if (!initialized || !mutex) return String();

    String out;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) != pdTRUE) return out;

    uint32_t newestSeq = lastSeq;
    // Walk the ring in chronological order (oldest first) starting at head.
    for (size_t i = 0; i < DEBUG_LOG_CAPACITY; i++) {
        Slot& slot = slots[(head + i) % DEBUG_LOG_CAPACITY];
        if (slot.seq > lastSeq) {
            if (!out.isEmpty()) out += '\n';
            out += slot.text;
            if (slot.seq > newestSeq) newestSeq = slot.seq;
        }
    }
    lastSeq = newestSeq;

    xSemaphoreGive(mutex);
    return out;
}

String DebugLog::collectAll() {
    uint32_t none = 0;
    return collectSince(none);
}
