#include "uart_handler.h"
#include <driver/uart.h>

UARTHandler::UARTHandler() 
    : streamBuffer(NULL), taskHandle(NULL), initialized(false), running(false),
      sentencesReceived(0), errors(0), linePos(0) {
    memset(lineBuffer, 0, sizeof(lineBuffer));
}

UARTHandler::~UARTHandler() {
    stop();
    if (streamBuffer) {
        vStreamBufferDelete(streamBuffer);
    }
}

void UARTHandler::init(const UARTConfig& cfg) {
    if (initialized) {
        return;
    }
    
    config = cfg;
    
    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = (int)config.baudRate,
        .data_bits = (uart_word_length_t)(config.dataBits - 5),
        .parity = (uart_parity_t)config.parity,
        .stop_bits = (uart_stop_bits_t)config.stopBits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
    };
    
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, UART_BUFFER_SIZE, 0, 0, NULL, 0);
    
    // Create stream buffer
    streamBuffer = xStreamBufferCreate(UART_BUFFER_SIZE, 1);
    
    initialized = true;
    
    Serial.printf("[UART] Initialized: Baud=%u, Data=%u, Parity=%u, Stop=%u\n",
                  config.baudRate, config.dataBits, config.parity, config.stopBits);
}

void UARTHandler::start() {
    if (!initialized || running) {
        return;
    }
    
    running = true;
    xTaskCreate(uartTask, "UART", TASK_STACK_UART, this, TASK_PRIORITY_UART, &taskHandle);
    
    Serial.println("[UART] Started");
}

void UARTHandler::stop() {
    if (!running) {
        return;
    }
    
    running = false;
    
    if (taskHandle) {
        vTaskDelete(taskHandle);
        taskHandle = NULL;
    }
    
    Serial.println("[UART] Stopped");
}

void UARTHandler::uartTask(void* parameter) {
    UARTHandler* handler = static_cast<UARTHandler*>(parameter);
    
    uint8_t data[128];
    
    while (handler->running) {
        int len = uart_read_bytes(UART_NUM, data, sizeof(data), pdMS_TO_TICKS(100));
        
        if (len > 0) {
            xStreamBufferSend(handler->streamBuffer, data, len, 0);
        }
    }
    
    vTaskDelete(NULL);
}

bool UARTHandler::readLine(char* buffer, size_t maxLen, TickType_t timeout) {
    if (!initialized || !streamBuffer) {
        return false;
    }
    
    uint32_t startTime = millis();
    
    while (millis() - startTime < pdTICKS_TO_MS(timeout)) {
        uint8_t byte;
        size_t received = xStreamBufferReceive(streamBuffer, &byte, 1, pdMS_TO_TICKS(10));
        
        if (received == 0) {
            continue;
        }
        
        // Add to line buffer
        if (linePos < sizeof(lineBuffer) - 1) {
            lineBuffer[linePos++] = byte;
        }
        
        // Check for line ending
        if (byte == '\n' && linePos > 1 && lineBuffer[linePos - 2] == '\r') {
            // Complete line received
            lineBuffer[linePos - 2] = '\0';  // Remove \r\n
            
            // Copy to output buffer
            size_t copyLen = min(linePos - 2, maxLen - 1);
            strncpy(buffer, lineBuffer, copyLen);
            buffer[copyLen] = '\0';
            
            linePos = 0;
            sentencesReceived++;
            
            return true;
        }
        
        // Prevent buffer overflow
        if (linePos >= sizeof(lineBuffer)) {
            linePos = 0;
            errors++;
        }
    }
    
    return false;
}
