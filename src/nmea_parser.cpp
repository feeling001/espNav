#include "nmea_parser.h"
#include <stdlib.h>

NMEAParser::NMEAParser(BoatState* bs) : validSentences(0), invalidSentences(0), boatState(bs) {}

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
        
        // Parse sentence and update BoatState if available
        if (boatState != nullptr) {
            // Determine sentence type and call appropriate parser
            if (strstr(out.type, "GGA")) {
                parseGGA(line);
            } else if (strstr(out.type, "RMC")) {
                parseRMC(line);
            } else if (strstr(out.type, "GLL")) {
                parseGLL(line);
            } else if (strstr(out.type, "VTG")) {
                parseVTG(line);
            } else if (strstr(out.type, "HDT")) {
                parseHDT(line);
            } else if (strstr(out.type, "HDM")) {
                parseHDM(line);
            } else if (strstr(out.type, "DPT")) {
                parseDPT(line);
            } else if (strstr(out.type, "DBT")) {
                parseDBT(line);
            } else if (strstr(out.type, "MWV")) {
                parseMWV(line);
            } else if (strstr(out.type, "MWD")) {
                parseMWD(line);
            } else if (strstr(out.type, "MTW")) {
                parseMTW(line);
            } else if (strstr(out.type, "VHW")) {
                parseVHW(line);
            } else if (strstr(out.type, "VLW")) {
                parseVLW(line);
            }
        }
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

// ============================================================
// Utility Functions
// ============================================================

int NMEAParser::parseField(const char* line, int fieldIndex, char* buffer, size_t bufferSize) {
    const char* ptr = line;
    int currentField = 0;
    
    // Find start of field
    while (*ptr && currentField < fieldIndex) {
        if (*ptr == ',') {
            currentField++;
        }
        ptr++;
    }
    
    if (currentField != fieldIndex) {
        buffer[0] = '\0';
        return 0;
    }
    
    // Copy field data
    size_t i = 0;
    while (*ptr && *ptr != ',' && *ptr != '*' && i < bufferSize - 1) {
        buffer[i++] = *ptr++;
    }
    buffer[i] = '\0';
    
    return i;
}

float NMEAParser::parseLatitude(const char* lat, const char* ns) {
    if (!lat || !ns || strlen(lat) < 4) return 0.0;
    
    // Format: DDMM.MMMM or DDMM.MM
    char degrees[3] = {0};
    degrees[0] = lat[0];
    degrees[1] = lat[1];
    
    float deg = atof(degrees);
    float min = atof(lat + 2);
    
    float result = deg + (min / 60.0);
    
    if (ns[0] == 'S') {
        result = -result;
    }
    
    return result;
}

float NMEAParser::parseLongitude(const char* lon, const char* ew) {
    if (!lon || !ew || strlen(lon) < 5) return 0.0;
    
    // Format: DDDMM.MMMM or DDDMM.MM
    char degrees[4] = {0};
    degrees[0] = lon[0];
    degrees[1] = lon[1];
    degrees[2] = lon[2];
    
    float deg = atof(degrees);
    float min = atof(lon + 3);
    
    float result = deg + (min / 60.0);
    
    if (ew[0] == 'W') {
        result = -result;
    }
    
    return result;
}

float NMEAParser::parseKnots(const char* speed) {
    if (!speed || strlen(speed) == 0) return 0.0;
    return atof(speed);
}

float NMEAParser::parseDegrees(const char* degrees) {
    if (!degrees || strlen(degrees) == 0) return 0.0;
    return atof(degrees);
}

// ============================================================
// NMEA 0183 Sentence Parsers
// ============================================================

// $GPGGA - GPS Fix Data
// Format: $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
void NMEAParser::parseGGA(const char* line) {
    char buffer[32];
    
    // Field 2,3: Latitude
    char lat[16], ns[2];
    parseField(line, 2, lat, sizeof(lat));
    parseField(line, 3, ns, sizeof(ns));
    
    // Field 4,5: Longitude
    char lon[16], ew[2];
    parseField(line, 4, lon, sizeof(lon));
    parseField(line, 5, ew, sizeof(ew));
    
    // Field 6: Fix quality (0=invalid, 1=GPS, 2=DGPS, etc.)
    parseField(line, 6, buffer, sizeof(buffer));
    int fixQuality = atoi(buffer);
    
    // Field 7: Number of satellites
    parseField(line, 7, buffer, sizeof(buffer));
    int satellites = atoi(buffer);
    
    // Field 8: HDOP
    parseField(line, 8, buffer, sizeof(buffer));
    float hdop = atof(buffer);
    
    // Update BoatState
    if (strlen(lat) > 0 && strlen(lon) > 0) {
        float latitude = parseLatitude(lat, ns);
        float longitude = parseLongitude(lon, ew);
        boatState->setGPSPosition(latitude, longitude);
    }
    
    boatState->setGPSFixQuality(fixQuality);
    boatState->setGPSSatellites(satellites);
    if (hdop > 0) {
        boatState->setGPSHDOP(hdop);
    }
}

// $GPRMC - Recommended Minimum
// Format: $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
void NMEAParser::parseRMC(const char* line) {
    char buffer[32];
    
    // Field 2: Status (A=active, V=void)
    parseField(line, 2, buffer, sizeof(buffer));
    if (buffer[0] != 'A') {
        return;  // Invalid data
    }
    
    // Field 3,4: Latitude
    char lat[16], ns[2];
    parseField(line, 3, lat, sizeof(lat));
    parseField(line, 4, ns, sizeof(ns));
    
    // Field 5,6: Longitude
    char lon[16], ew[2];
    parseField(line, 5, lon, sizeof(lon));
    parseField(line, 6, ew, sizeof(ew));
    
    // Field 7: Speed (knots)
    parseField(line, 7, buffer, sizeof(buffer));
    float sog = parseKnots(buffer);
    
    // Field 8: Course (degrees)
    parseField(line, 8, buffer, sizeof(buffer));
    float cog = parseDegrees(buffer);
    
    // Update BoatState
    if (strlen(lat) > 0 && strlen(lon) > 0) {
        float latitude = parseLatitude(lat, ns);
        float longitude = parseLongitude(lon, ew);
        boatState->setGPSPosition(latitude, longitude);
    }
    
    if (sog >= 0) {
        boatState->setGPSSOG(sog);
    }
    
    if (cog >= 0 && cog < 360) {
        boatState->setGPSCOG(cog);
    }
}

// $GPGLL - Geographic Position
// Format: $GPGLL,llll.ll,a,yyyyy.yy,a,hhmmss.ss,A,a*hh
void NMEAParser::parseGLL(const char* line) {
    char buffer[32];
    
    // Field 1,2: Latitude
    char lat[16], ns[2];
    parseField(line, 1, lat, sizeof(lat));
    parseField(line, 2, ns, sizeof(ns));
    
    // Field 3,4: Longitude
    char lon[16], ew[2];
    parseField(line, 3, lon, sizeof(lon));
    parseField(line, 4, ew, sizeof(ew));
    
    // Field 6: Status (A=valid, V=invalid)
    parseField(line, 6, buffer, sizeof(buffer));
    if (buffer[0] != 'A') {
        return;
    }
    
    // Update BoatState
    if (strlen(lat) > 0 && strlen(lon) > 0) {
        float latitude = parseLatitude(lat, ns);
        float longitude = parseLongitude(lon, ew);
        boatState->setGPSPosition(latitude, longitude);
    }
}

// $GPVTG - Track Made Good and Ground Speed
// Format: $GPVTG,x.x,T,x.x,M,x.x,N,x.x,K,a*hh
void NMEAParser::parseVTG(const char* line) {
    char buffer[32];
    
    // Field 1: True track (degrees)
    parseField(line, 1, buffer, sizeof(buffer));
    float trueCourse = parseDegrees(buffer);
    
    // Field 5: Speed in knots
    parseField(line, 5, buffer, sizeof(buffer));
    float sog = parseKnots(buffer);
    
    // Update BoatState
    if (sog >= 0) {
        boatState->setGPSSOG(sog);
    }
    
    if (trueCourse >= 0 && trueCourse < 360) {
        boatState->setGPSCOG(trueCourse);
    }
}

// $HCHDT - True Heading
// Format: $HCHDT,x.x,T*hh
void NMEAParser::parseHDT(const char* line) {
    char buffer[32];
    
    // Field 1: Heading (degrees true)
    parseField(line, 1, buffer, sizeof(buffer));
    float heading = parseDegrees(buffer);
    
    if (heading >= 0 && heading < 360) {
        boatState->setTrueHeading(heading);
    }
}

// $HCHDM - Magnetic Heading
// Format: $HCHDM,x.x,M*hh
void NMEAParser::parseHDM(const char* line) {
    char buffer[32];
    
    // Field 1: Heading (degrees magnetic)
    parseField(line, 1, buffer, sizeof(buffer));
    float heading = parseDegrees(buffer);
    
    if (heading >= 0 && heading < 360) {
        boatState->setMagneticHeading(heading);
    }
}

// $SDDPT - Depth
// Format: $SDDPT,x.x,x.x*hh
void NMEAParser::parseDPT(const char* line) {
    char buffer[32];
    
    // Field 1: Depth below transducer (meters)
    parseField(line, 1, buffer, sizeof(buffer));
    float depth = atof(buffer);
    
    // Field 2: Offset from transducer (meters)
    parseField(line, 2, buffer, sizeof(buffer));
    float offset = atof(buffer);
    
    if (depth > 0) {
        boatState->setDepth(depth);
    }
    
    if (offset != 0) {
        boatState->setDepthOffset(offset);
    }
}

// $SDDBT - Depth Below Transducer
// Format: $SDDBT,x.x,f,x.x,M,x.x,F*hh
void NMEAParser::parseDBT(const char* line) {
    char buffer[32];
    
    // Field 3: Depth in meters
    parseField(line, 3, buffer, sizeof(buffer));
    float depth = atof(buffer);
    
    if (depth > 0) {
        boatState->setDepth(depth);
    }
}

// $WIMWV - Wind Speed and Angle
// Format: $WIMWV,x.x,R/T,x.x,N/M/K,A*hh
void NMEAParser::parseMWV(const char* line) {
    char buffer[32];
    
    // Field 1: Wind angle (degrees)
    parseField(line, 1, buffer, sizeof(buffer));
    float angle = parseDegrees(buffer);
    
    // Field 2: Reference (R=relative/apparent, T=true)
    parseField(line, 2, buffer, sizeof(buffer));
    bool isRelative = (buffer[0] == 'R');
    
    // Field 3: Wind speed
    parseField(line, 3, buffer, sizeof(buffer));
    float speed = atof(buffer);
    
    // Field 4: Speed units (N=knots, M=m/s, K=km/h)
    parseField(line, 4, buffer, sizeof(buffer));
    
    // Convert to knots if needed
    if (buffer[0] == 'M') {
        speed = speed * 1.94384;  // m/s to knots
    } else if (buffer[0] == 'K') {
        speed = speed * 0.539957; // km/h to knots
    }
    
    // Field 5: Status (A=valid)
    parseField(line, 5, buffer, sizeof(buffer));
    if (buffer[0] != 'A') {
        return;
    }
    
    // Update BoatState
    if (isRelative) {
        // Apparent wind angle: convert to -180 to +180 range
        if (angle > 180) {
            angle = angle - 360;
        }
        boatState->setApparentWind(speed, angle);
    } else {
        // True wind - we need to calculate TWA from TWD and heading
        // For now, just store TWD
        // Note: This is simplified - proper implementation needs heading
    }
}

// $WIMWD - Wind Direction & Speed
// Format: $WIMWD,x.x,T,x.x,M,x.x,N,x.x,M*hh
void NMEAParser::parseMWD(const char* line) {
    char buffer[32];
    
    // Field 1: True wind direction
    parseField(line, 1, buffer, sizeof(buffer));
    float twd = parseDegrees(buffer);
    
    // Field 5: Wind speed in knots
    parseField(line, 5, buffer, sizeof(buffer));
    float tws = parseKnots(buffer);
    
    // We have TWD and TWS, but need heading to calculate TWA
    // For now, store what we have
    // Note: This is simplified - proper implementation needs heading
    if (twd >= 0 && twd < 360 && tws >= 0) {
        boatState->setTrueWind(tws, 0, twd);  // TWA will be calculated if we have heading
    }
}

// $YXMTW - Water Temperature
// Format: $YXMTW,x.x,C*hh
void NMEAParser::parseMTW(const char* line) {
    char buffer[32];
    
    // Field 1: Temperature
    parseField(line, 1, buffer, sizeof(buffer));
    float temp = atof(buffer);
    
    // Field 2: Unit (C=Celsius)
    parseField(line, 2, buffer, sizeof(buffer));
    
    // Convert to Celsius if needed (usually already in C)
    if (buffer[0] == 'C' && temp > -50 && temp < 50) {
        boatState->setWaterTemp(temp);
    }
}

// $VWVHW - Water Speed and Heading
// Format: $VWVHW,x.x,T,x.x,M,x.x,N,x.x,K*hh
void NMEAParser::parseVHW(const char* line) {
    char buffer[32];
    
    // Field 1: True heading
    parseField(line, 1, buffer, sizeof(buffer));
    float trueHeading = parseDegrees(buffer);
    
    // Field 3: Magnetic heading
    parseField(line, 3, buffer, sizeof(buffer));
    float magHeading = parseDegrees(buffer);
    
    // Field 5: Speed through water (knots)
    parseField(line, 5, buffer, sizeof(buffer));
    float stw = parseKnots(buffer);
    
    // Update BoatState
    if (trueHeading >= 0 && trueHeading < 360) {
        boatState->setTrueHeading(trueHeading);
    }
    
    if (magHeading >= 0 && magHeading < 360) {
        boatState->setMagneticHeading(magHeading);
    }
    
    if (stw >= 0) {
        boatState->setSTW(stw);
    }
}

// $VWVLW - Distance Traveled through Water
// Format: $VWVLW,x.x,N,x.x,N*hh
void NMEAParser::parseVLW(const char* line) {
    char buffer[32];
    
    // Field 1: Total distance (nautical miles)
    parseField(line, 1, buffer, sizeof(buffer));
    float total = atof(buffer);
    
    // Field 3: Trip distance (nautical miles)
    parseField(line, 3, buffer, sizeof(buffer));
    float trip = atof(buffer);
    
    // Update BoatState
    if (total >= 0) {
        boatState->setTotal(total);
    }
    
    if (trip >= 0) {
        boatState->setTrip(trip);
    }
}
