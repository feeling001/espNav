#ifndef NMEA_PARSER_H
#define NMEA_PARSER_H

#include <Arduino.h>
#include "types.h"

class NMEAParser {
public:
    NMEAParser();
    
    bool parseLine(const char* line, NMEASentence& out);
    bool validateChecksum(const char* line);
    
    uint32_t getValidSentences() const { return validSentences; }
    uint32_t getInvalidSentences() const { return invalidSentences; }
    
private:
    uint8_t calculateChecksum(const char* data, size_t len);
    void extractSentenceType(const char* line, char* type, size_t maxLen);
    
    uint32_t validSentences;
    uint32_t invalidSentences;
};

#endif // NMEA_PARSER_H
