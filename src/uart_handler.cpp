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
    
    // Configure GPIO with pull-up
    gpio_set_pull_mode(UART_RX_PIN, GPIO_PULLUP_ONLY);

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
    
    // Flush any existing data in UART buffer
    uart_flush(UART_NUM);
    
    // Create stream buffer (AUGMENTÉ: 4096 au lieu de 1024)
    streamBuffer = xStreamBufferCreate(UART_BUFFER_SIZE, 1);
    
    initialized = true;
    
    Serial.printf("[UART] Initialized: Baud=%u, Data=%u, Parity=%u, Stop=%u, RX=GPIO%u, TX=GPIO%u\n",
                  config.baudRate, config.dataBits, config.parity, config.stopBits, 
                  UART_RX_PIN, UART_TX_PIN);
    Serial.printf("[UART] Stream buffer size: %u bytes\n", UART_BUFFER_SIZE);
}

void UARTHandler::start() {
    if (!initialized || running) {
        return;
    }
    
    running = true;
    xTaskCreate(uartTask, "UART_RX", TASK_STACK_UART, this, TASK_PRIORITY_UART, &taskHandle);
    
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
            // Send to stream buffer
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
    uint32_t timeoutMs = pdTICKS_TO_MS(timeout);
    
    while (millis() - startTime < timeoutMs) {
        uint8_t byte;
        size_t received = xStreamBufferReceive(streamBuffer, &byte, 1, pdMS_TO_TICKS(10));
        
        if (received == 0) {
            continue;
        }
        
        // ═══════════════════════════════════════════════════════════════
        // CORRECTION 1: Détection de début de message NMEA/AIS
        // ═══════════════════════════════════════════════════════════════
        if (byte == '$' || byte == '!') {
            // Nouveau message détecté - reset buffer
            linePos = 0;
            lineBuffer[linePos++] = byte;
            continue;
        }
        
        // ═══════════════════════════════════════════════════════════════
        // CORRECTION 2: Protection améliorée contre buffer overflow
        // ═══════════════════════════════════════════════════════════════
        if (linePos < sizeof(lineBuffer) - 1) {
            // Espace disponible - ajouter le caractère
            lineBuffer[linePos++] = byte;
        } else {
            // Buffer plein - comportement selon le caractère
            if (byte == '\n' || byte == '\r') {
                // Fin de ligne sur un message trop long
                // On reset et on compte comme erreur
                linePos = 0;
                errors++;
                Serial.println("[UART] ⚠️  Line too long, dropped");
                continue;
            }
            // Sinon, ignorer le caractère et continuer
            // (attendre le prochain $ ou fin de ligne)
            continue;
        }
        
        // ═══════════════════════════════════════════════════════════════
        // Détection de fin de ligne
        // ═══════════════════════════════════════════════════════════════
        if (byte == '\n') {
            // Remove line ending
            if (linePos > 1 && lineBuffer[linePos - 2] == '\r') {
                // CRLF ending
                lineBuffer[linePos - 2] = '\0';
                size_t copyLen = min(linePos - 2, maxLen - 1);
                strncpy(buffer, lineBuffer, copyLen);
                buffer[copyLen] = '\0';
            } else {
                // LF only ending
                lineBuffer[linePos - 1] = '\0';
                size_t copyLen = min(linePos - 1, maxLen - 1);
                strncpy(buffer, lineBuffer, copyLen);
                buffer[copyLen] = '\0';
            }
            
            linePos = 0;
            
            // ═══════════════════════════════════════════════════════════════
            // CORRECTION 3: Validation basique avant de retourner
            // Vérifier que le message commence bien par $ ou !
            // ═══════════════════════════════════════════════════════════════
            if (buffer[0] == '$' || buffer[0] == '!') {
                sentencesReceived++;
                return true;
            } else {
                // Message invalide (ne commence pas par $ ou !)
                // Probablement dû à une synchronisation perdue
                errors++;
                Serial.printf("[UART] ⚠️  Invalid message start: '%c' (expected $ or !)\n", buffer[0]);
                continue;
            }
        }
    }
    
    return false;
}