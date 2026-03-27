#include "functions.h"
#include "seatalk_handler.h"


// Datagrammes standards [cite: 24, 25, 26, 27, 28, 29]
const uint16_t ST_AUTO[]    = {0x186, 0x21, 0x01, 0xFE};
const uint16_t ST_STBY[]    = {0x186, 0x21, 0x02, 0xFD};
const uint16_t ST_M1[]      = {0x186, 0x21, 0x05, 0xFA};
const uint16_t ST_P1[]      = {0x186, 0x21, 0x07, 0xF8};

SeaTalkHandler stHandler;

SeaTalkHandler::SeaTalkHandler() : _stSerial() {}

void SeaTalkHandler::init(SeaTalkConfig config) {
    _config = config;
    if (_config.enabled) {
        _stSerial.begin(_config.baud, SWSERIAL_8N1, _config.rxPin, _config.txPin, false);
        _initialized = true;
    }
}

void SeaTalkHandler::start() {
    if (!_initialized) return;
    // La tâche est créée dans le main.cpp sur le Core 0
}

void SeaTalkHandler::loop() {
    if (!_initialized) return;

    while (_stSerial.available()) {
        uint16_t c = _stSerial.read(); // Lecture incluant le bit de commande [cite: 69, 70]
        
        if (c & 0x100) { // Bit de commande détecté
            _bufferIdx = 0;
        }
        
        if (_bufferIdx < 18) {
            _incomingBuffer[_bufferIdx++] = c;
            
            if (_bufferIdx >= 3) {
                // Calcul de la longueur : 3 + nibble de poids faible du 2ème octet [cite: 50]
                size_t expectedLen = (_incomingBuffer[1] & 0x0F) + 3;
                if (_bufferIdx == expectedLen) {
                    parseDatagram(_incomingBuffer, expectedLen);
                    _bufferIdx = 0;
                }
            }
        }
    }
}

void SeaTalkHandler::parseDatagram(uint16_t* buffer, size_t length) {
    uint8_t cmd = buffer[0] & 0xFF;
    switch (cmd) {
        case 0x11: // Wind Speed [cite: 71, 77]
            _aws = (buffer[2] & 0x7F) + (buffer[3] / 10.0);
            break;
        case 0x84: // AP Status
            if (buffer[2] == 0x00) _apStatus = "Standby";
            else if (buffer[2] == 0x40) _apStatus = "Auto";
            break;
    }
}

void SeaTalkHandler::sendCommand(const uint16_t* data) {
    size_t len = (data[1] & 0x0F) + 3;
    // Logique anti-collision CDMA/CD [cite: 51, 57, 58]
    for (int retry = 0; retry < 5; retry++) {
        while (_stSerial.available()) { _stSerial.read(); delay(2); } // Attente silence
        
        bool ok = true;
        for (size_t i = 0; i < len; i++) {
            _stSerial.write(data[i]);
            delay(3);
            if (_stSerial.available() && _stSerial.read() != data[i]) {
                ok = false; // Collision ! [cite: 54, 57]
                break;
            }
        }
        if (ok) return;
        delay(random(5, 50));
    }
}