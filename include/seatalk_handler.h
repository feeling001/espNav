#ifndef SEATALK_HANDLER_H
#define SEATALK_HANDLER_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "config.h"
#include "types.h"

class SeaTalkHandler {
public:
    SeaTalkHandler();
    void init(SeaTalkConfig config);
    void start();
    void loop(); // Logique interne pour la tâche FreeRTOS

    // Commandes
    void sendCommand(const uint16_t* datagram);
    
    // Getters de données
    float getAWS() { return _aws; }
    String getAPStatus() { return _apStatus; }

private:
    SoftwareSerial _stSerial;
    SeaTalkConfig _config;
    float _aws = 0.0;
    String _apStatus = "Unknown";
    
    void parseDatagram(uint16_t* buffer, size_t length);
    bool _initialized = false;
    uint16_t _incomingBuffer[18];
    size_t _bufferIdx = 0;
};

// Instance globale pour accès via WebServer/Main
extern SeaTalkHandler stHandler;

#endif