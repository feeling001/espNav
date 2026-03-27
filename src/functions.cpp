#include "functions.h"
#include <cstdarg>
#include <cstdio>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"

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
