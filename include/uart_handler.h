#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include <Arduino.h>
#include "config.h"
#include "types.h"
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

class UARTHandler {
public:
    UARTHandler();
    ~UARTHandler();
    
    void init(const SerialConfig& config);
    void start();
    void stop();
    bool readLine(char* buffer, size_t maxLen, TickType_t timeout);
    
    uint32_t getSentencesReceived() const { return sentencesReceived; }
    uint32_t getErrors() const { return errors; }
    
private:
    static void uartTask(void* parameter);
    void handleUART();
    
    StreamBufferHandle_t streamBuffer;
    TaskHandle_t taskHandle;
    SerialConfig config;
    bool initialized;
    bool running;
    
    uint32_t sentencesReceived;
    uint32_t errors;
    
    char lineBuffer[NMEA_MAX_LENGTH];
    size_t linePos;
};

#endif // UART_HANDLER_H
