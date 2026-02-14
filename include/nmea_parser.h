#ifndef NMEA_PARSER_H
#define NMEA_PARSER_H

#include <Arduino.h>
#include "types.h"
#include "boat_state.h"

class NMEAParser {
public:
    NMEAParser(BoatState* boatState = nullptr);
    
    bool parseLine(const char* line, NMEASentence& out);
    bool validateChecksum(const char* line);
    
    uint32_t getValidSentences() const { return validSentences; }
    uint32_t getInvalidSentences() const { return invalidSentences; }
    
private:
    uint8_t calculateChecksum(const char* data, size_t len);
    void extractSentenceType(const char* line, char* type, size_t maxLen);
    
    // NMEA 0183 sentence parsers
    void parseGGA(const char* line);  // GPS Fix Data
    void parseRMC(const char* line);  // Recommended Minimum
    void parseGLL(const char* line);  // Geographic Position
    void parseVTG(const char* line);  // Track Made Good and Ground Speed
    void parseHDT(const char* line);  // True Heading
    void parseHDM(const char* line);  // Magnetic Heading
    void parseDPT(const char* line);  // Depth
    void parseDBT(const char* line);  // Depth Below Transducer
    void parseMWV(const char* line);  // Wind Speed and Angle
    void parseMWD(const char* line);  // Wind Direction & Speed
    void parseMTW(const char* line);  // Water Temperature
    void parseVHW(const char* line);  // Water Speed and Heading
    void parseVLW(const char* line);  // Distance Traveled through Water
    
    // Utility functions
    float parseLatitude(const char* lat, const char* ns);
    float parseLongitude(const char* lon, const char* ew);
    float parseKnots(const char* speed);
    float parseDegrees(const char* degrees);
    int parseField(const char* line, int fieldIndex, char* buffer, size_t bufferSize);
    
    uint32_t validSentences;
    uint32_t invalidSentences;
    BoatState* boatState;
};

#endif // NMEA_PARSER_H
