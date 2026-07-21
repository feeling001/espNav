#include "functions.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"
#include "debug_log.h"

// IMPORTANT : déclaration externe du mutex
extern SemaphoreHandle_t g_serialMutex;

void serialPrintf(const char* fmt, ...) {


	    if (!fmt) return;

    char buf[256];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);  // ✅ format correct
    va_end(args);

    Serial.print(buf);  // ✅ safe avec HWCDC

    // Mirror into the in-RAM ring buffer so it can be streamed to the web
    // dashboard (/ws/debug) — useful when the physical serial port is not
    // reachable (boat in real conditions). Cheap, non-blocking, drops on
    // contention.
    //
    // Many call sites build a single logical line across several
    // serialPrintf() calls without a trailing '\n' (e.g. printing a frame
    // byte-by-byte in a loop). To keep the web console compact (one line per
    // log entry instead of one line per call), we accumulate characters in a
    // small static buffer and only push to the ring buffer when a '\n' is
    // seen, splitting on embedded newlines as needed.
    {
        static char     lineBuf[DEBUG_LOG_LINE_LEN];
        static size_t   lineLen = 0;
        static SemaphoreHandle_t lineMutex = xSemaphoreCreateMutex();

        if (lineMutex && xSemaphoreTake(lineMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            for (const char* p = buf; *p; ++p) {
                if (*p == '\n' || *p == '\r') {
                    if (lineLen > 0) {
                        lineBuf[lineLen] = '\0';
                        DebugLog::instance().push(lineBuf);
                        lineLen = 0;
                    }
                } else if (lineLen + 1 < DEBUG_LOG_LINE_LEN) {
                    lineBuf[lineLen++] = *p;
                }
            }
            xSemaphoreGive(lineMutex);
        }
    }


/*    
	if (!fmt) return;  // garde contre NULL
    va_list args;
    va_start(args, fmt);
    Serial.printf(fmt, args);  // ou vprintf selon ton implémentation
    va_end(args);
    */

    /*
    char buf[256];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (g_serialMutex && xSemaphoreTake(g_serialMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        DEBUG_SERIAL.print(buf);
        xSemaphoreGive(g_serialMutex);
    }
  */
}
