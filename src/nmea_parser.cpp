#include "nmea_parser.h"

NMEAParser::NMEAParser() : validSentences(0), invalidSentences(0) {}

bool NMEAParser::parseLine(const char* line, NMEASentence& out) {
    if (!line || line[0] != '$') {
        invalidSentences++;
        return false;
    }
    
    // Copy raw sentence
    strncpy(out.raw, line, sizeof(out.raw) - 1);
    out.raw[sizeof(out.raw) - 1] = '\0';
    
    // Extract sentence type
    extractSentenceType(line, out.type, sizeof(out.type));
    
    // Validate checksum
    out.valid = validateChecksum(line);
    out.timestamp = millis();
    
    if (out.valid) {
        validSentences++;
    } else {
        invalidSentences++;
    }
    
    return out.valid;
}

bool NMEAParser::validateChecksum(const char* line) {
    // Find asterisk
    const char* asterisk = strchr(line, '*');
    if (!asterisk || asterisk - line > 80) {
        return false;
    }
    
    // Calculate checksum
    uint8_t calculated = calculateChecksum(line + 1, asterisk - line - 1);
    
    // Parse provided checksum
    uint8_t provided = 0;
    if (sscanf(asterisk + 1, "%02hhx", &provided) != 1) {
        return false;
    }
    
    return calculated == provided;
}

uint8_t NMEAParser::calculateChecksum(const char* data, size_t len) {
    uint8_t checksum = 0;
    
    for (size_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    
    return checksum;
}

void NMEAParser::extractSentenceType(const char* line, char* type, size_t maxLen) {
    // Skip '$'
    const char* ptr = line + 1;
    
    // Extract until ',' or end
    size_t i = 0;
    while (*ptr && *ptr != ',' && i < maxLen - 1) {
        type[i++] = *ptr++;
    }
    type[i] = '\0';
}
