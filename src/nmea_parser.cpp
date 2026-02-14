#include "nmea_parser.h"
#include <stdlib.h>

NMEAParser::NMEAParser(BoatState* bs) : validSentences(0), invalidSentences(0), boatState(bs) {}

bool NMEAParser::parseLine(const char* line, NMEASentence& out) {
    // CORRECTION: Accepter les messages commençant par '$' (NMEA standard) ou '!' (AIS)
    if (!line || (line[0] != '$' && line[0] != '!')) {
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
            } else if (strstr(out.type, "AIVDM") || strstr(out.type, "AIVDO")) {
                // Messages AIS - décoder
                parseAIVDM(line);
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
    // Skip '$' or '!' 
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
    
    if (fixQuality > 0 && strlen(lat) > 0 && strlen(lon) > 0) {
        float latitude = parseLatitude(lat, ns);
        float longitude = parseLongitude(lon, ew);
        
        boatState->setGPSPosition(latitude, longitude);
        boatState->setGPSSatellites(satellites);
        boatState->setGPSFixQuality(fixQuality);
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
    
    // Field 7: Speed over ground (knots)
    parseField(line, 7, buffer, sizeof(buffer));
    float sog = parseKnots(buffer);
    
    // Field 8: Course over ground (degrees)
    parseField(line, 8, buffer, sizeof(buffer));
    float cog = parseDegrees(buffer);
    
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
    
    // Field 6: Status (A=valid, V=invalid)
    parseField(line, 6, buffer, sizeof(buffer));
    if (buffer[0] != 'A') {
        return;
    }
    
    // Field 1,2: Latitude
    char lat[16], ns[2];
    parseField(line, 1, lat, sizeof(lat));
    parseField(line, 2, ns, sizeof(ns));
    
    // Field 3,4: Longitude
    char lon[16], ew[2];
    parseField(line, 3, lon, sizeof(lon));
    parseField(line, 4, ew, sizeof(ew));
    
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
    
    // Field 1: Course (degrees true)
    parseField(line, 1, buffer, sizeof(buffer));
    float cog = parseDegrees(buffer);
    
    // Field 5: Speed (knots)
    parseField(line, 5, buffer, sizeof(buffer));
    float sog = parseKnots(buffer);
    
    if (cog >= 0 && cog < 360) {
        boatState->setGPSCOG(cog);
    }
    
    if (sog >= 0) {
        boatState->setGPSSOG(sog);
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
    
    // Field 4: Units (N=knots, M=m/s, K=km/h)
    parseField(line, 4, buffer, sizeof(buffer));
    char unit = buffer[0];
    
    // Field 5: Status (A=valid)
    parseField(line, 5, buffer, sizeof(buffer));
    if (buffer[0] != 'A') {
        return;
    }
    
    // Convert speed to knots if needed
    if (unit == 'M') {
        speed = speed * 1.94384;  // m/s to knots
    } else if (unit == 'K') {
        speed = speed * 0.539957;  // km/h to knots
    }
    
    if (isRelative) {
        boatState->setApparentWind(speed, angle);
    } else {
        // Pour le vent vrai, on utilise l'angle comme direction aussi
        // car MWV ne donne pas séparément l'angle et la direction
        boatState->setTrueWind(speed, angle, angle);
    }
}

// $WIMWD - Wind Direction & Speed
// Format: $WIMWD,x.x,T,x.x,M,x.x,N,x.x,M*hh
void NMEAParser::parseMWD(const char* line) {
    char buffer[32];
    
    // Field 1: Wind direction (degrees true)
    parseField(line, 1, buffer, sizeof(buffer));
    float direction = parseDegrees(buffer);
    
    // Field 5: Wind speed (knots)
    parseField(line, 5, buffer, sizeof(buffer));
    float speed = parseKnots(buffer);
    
    if (direction >= 0 && direction < 360 && speed >= 0) {
        // MWD donne la direction du vent, pas l'angle relatif
        // On calcule l'angle relatif si on a un cap (heading)
        // Pour l'instant on met 0 pour l'angle
        boatState->setTrueWind(speed, 0, direction);
    }
}

// $YXMTW - Water Temperature
// Format: $YXMTW,x.x,C*hh
void NMEAParser::parseMTW(const char* line) {
    char buffer[32];
    
    // Field 1: Water temperature (degrees)
    parseField(line, 1, buffer, sizeof(buffer));
    float temp = atof(buffer);
    
    // Field 2: Units (C/F)
    parseField(line, 2, buffer, sizeof(buffer));
    char unit = buffer[0];
    
    // Convert to Celsius if needed
    if (unit == 'F') {
        temp = (temp - 32.0) * 5.0 / 9.0;
    }
    
    if (temp > -10 && temp < 50) {  // Sanity check
        boatState->setWaterTemp(temp);
    }
}

// $VWVHW - Water Speed and Heading
// Format: $VWVHW,x.x,T,x.x,M,x.x,N,x.x,K*hh
void NMEAParser::parseVHW(const char* line) {
    char buffer[32];
    
    // Field 1: Heading (degrees true)
    parseField(line, 1, buffer, sizeof(buffer));
    float trueHeading = parseDegrees(buffer);
    
    // Field 3: Heading (degrees magnetic)
    parseField(line, 3, buffer, sizeof(buffer));
    float magHeading = parseDegrees(buffer);
    
    // Field 5: Speed through water (knots)
    parseField(line, 5, buffer, sizeof(buffer));
    float stw = parseKnots(buffer);
    
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
    
    if (total >= 0) {
        boatState->setTotal(total);
    }
    
    if (trip >= 0) {
        boatState->setTrip(trip);
    }
}

// ============================================================
// AIS Decoder
// ============================================================

// Convertit un caractère AIS 6-bit ASCII en valeur binaire
uint8_t NMEAParser::aisCharTo6Bit(char c) {
    if (c >= '0' && c <= 'W') {
        return c - 48;
    } else if (c >= '`' && c <= 'w') {
        return c - 56;
    }
    return 0;
}

// Extrait des bits d'un payload AIS
uint32_t NMEAParser::extractBits(const uint8_t* payload, int start, int length) {
    uint32_t result = 0;
    
    for (int i = 0; i < length; i++) {
        int bitIndex = start + i;
        int byteIndex = bitIndex / 8;
        int bitInByte = 7 - (bitIndex % 8);
        
        if (payload[byteIndex] & (1 << bitInByte)) {
            result |= (1 << (length - 1 - i));
        }
    }
    
    return result;
}

// !AIVDM - AIS VHF Data-link Message
// Format: !AIVDM,x,x,x,a,payload,x*hh
void NMEAParser::parseAIVDM(const char* line) {
    char buffer[128];
    
    // Field 1: Total number of sentences
    parseField(line, 1, buffer, sizeof(buffer));
    int totalSentences = atoi(buffer);
    
    // Field 2: Sentence number
    parseField(line, 2, buffer, sizeof(buffer));
    int sentenceNum = atoi(buffer);
    
    // Pour l'instant, on traite seulement les messages single-sentence
    if (totalSentences > 1) {
        // Multi-sentence messages nécessitent un buffer pour assembler
        // TODO: Implémenter l'assemblage multi-sentence si nécessaire
        return;
    }
    
    // Field 5: Payload encapsulé
    char payload[82];  // Max 82 caractères pour AIS
    parseField(line, 5, payload, sizeof(payload));
    
    int payloadLen = strlen(payload);
    if (payloadLen == 0) {
        return;
    }
    
    // Convertir le payload ASCII 6-bit en bytes
    uint8_t binaryPayload[64];  // Max ~60 bytes pour AIS
    memset(binaryPayload, 0, sizeof(binaryPayload));
    
    for (int i = 0; i < payloadLen; i++) {
        uint8_t value = aisCharTo6Bit(payload[i]);
        int bitOffset = i * 6;
        int byteIndex = bitOffset / 8;
        int bitInByte = bitOffset % 8;
        
        // Placer les 6 bits dans le bon emplacement
        binaryPayload[byteIndex] |= (value >> bitInByte);
        if (bitInByte > 2 && byteIndex + 1 < sizeof(binaryPayload)) {
            binaryPayload[byteIndex + 1] |= (value << (8 - bitInByte));
        }
    }
    
    // Extraire le Message Type (bits 0-5)
    uint8_t messageType = extractBits(binaryPayload, 0, 6);
    
    // Décoder selon le type de message
    switch (messageType) {
        case 1:
        case 2:
        case 3:
            // Position Report Class A
            decodeAISType1(binaryPayload, payloadLen);
            break;
            
        case 5:
            // Static and Voyage Related Data
            decodeAISType5(binaryPayload, payloadLen);
            break;
            
        case 18:
            // Standard Class B Position Report
            decodeAISType18(binaryPayload, payloadLen);
            break;
            
        case 24:
            // Static Data Report
            decodeAISType24(binaryPayload, payloadLen);
            break;
            
        default:
            // Type non supporté pour l'instant
            break;
    }
}

// Décode AIS Type 1/2/3 - Position Report Class A
void NMEAParser::decodeAISType1(const uint8_t* payload, int payloadLen) {
    AISTarget target;
    
    // MMSI (bits 8-37)
    target.mmsi = extractBits(payload, 8, 30);
    
    // SOG (bits 50-59) en 1/10 knots
    uint32_t sogRaw = extractBits(payload, 50, 10);
    if (sogRaw != 1023) {  // 1023 = not available
        target.sog = sogRaw / 10.0;
    }
    
    // Longitude (bits 61-88) en 1/10000 minutes
    int32_t lonRaw = extractBits(payload, 61, 28);
    if (lonRaw & 0x08000000) {  // Sign extend si négatif
        lonRaw |= 0xF0000000;
    }
    if (lonRaw != 0x6791AC0) {  // 181° = not available
        target.lon = lonRaw / 600000.0;
    }
    
    // Latitude (bits 89-115) en 1/10000 minutes
    int32_t latRaw = extractBits(payload, 89, 27);
    if (latRaw & 0x04000000) {  // Sign extend si négatif
        latRaw |= 0xF8000000;
    }
    if (latRaw != 0x3412140) {  // 91° = not available
        target.lat = latRaw / 600000.0;
    }
    
    // COG (bits 116-127) en 1/10 degrees
    uint32_t cogRaw = extractBits(payload, 116, 12);
    if (cogRaw != 3600) {  // 3600 = not available
        target.cog = cogRaw / 10.0;
    }
    
    // True Heading (bits 128-136)
    uint32_t headingRaw = extractBits(payload, 128, 9);
    if (headingRaw != 511) {  // 511 = not available
        target.heading = headingRaw;
    }
    
    target.timestamp = millis();
    
    // Si on a notre propre position GPS, calculer distance et bearing
    GPSData ownGPS = boatState->getGPS();
    if (ownGPS.position.lat.valid && ownGPS.position.lon.valid && 
        target.lat != 0 && target.lon != 0) {
        
        // Calcul de distance et bearing (formule haversine simplifiée)
        float lat1 = ownGPS.position.lat.value * PI / 180.0;
        float lon1 = ownGPS.position.lon.value * PI / 180.0;
        float lat2 = target.lat * PI / 180.0;
        float lon2 = target.lon * PI / 180.0;
        
        float dlat = lat2 - lat1;
        float dlon = lon2 - lon1;
        
        // Distance (haversine)
        float a = sin(dlat/2) * sin(dlat/2) + 
                  cos(lat1) * cos(lat2) * sin(dlon/2) * sin(dlon/2);
        float c = 2 * atan2(sqrt(a), sqrt(1-a));
        target.distance = 3440.065 * c;  // En nautical miles (rayon terre = 3440.065 nm)
        
        // Bearing
        float y = sin(dlon) * cos(lat2);
        float x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
        target.bearing = atan2(y, x) * 180.0 / PI;
        if (target.bearing < 0) target.bearing += 360;
        
        // CPA / TCPA calculation (simplifié)
        if (ownGPS.sog.valid && ownGPS.cog.valid && target.sog > 0) {
            // Vecteurs de vitesse
            float ownVx = ownGPS.sog.value * sin(ownGPS.cog.value * PI / 180.0);
            float ownVy = ownGPS.sog.value * cos(ownGPS.cog.value * PI / 180.0);
            float targetVx = target.sog * sin(target.cog * PI / 180.0);
            float targetVy = target.sog * cos(target.cog * PI / 180.0);
            
            // Vitesse relative
            float relVx = targetVx - ownVx;
            float relVy = targetVy - ownVy;
            float relSpeed = sqrt(relVx * relVx + relVy * relVy);
            
            if (relSpeed > 0.1) {
                // Position relative
                float relX = target.distance * sin(target.bearing * PI / 180.0);
                float relY = target.distance * cos(target.bearing * PI / 180.0);
                
                // TCPA
                target.tcpa = -(relX * relVx + relY * relVy) / (relSpeed * relSpeed);
                target.tcpa *= 60.0;  // Convertir en minutes
                
                if (target.tcpa > 0) {
                    // CPA
                    float cpaX = relX + relVx * (target.tcpa / 60.0);
                    float cpaY = relY + relVy * (target.tcpa / 60.0);
                    target.cpa = sqrt(cpaX * cpaX + cpaY * cpaY);
                } else {
                    target.cpa = target.distance;  // Déjà passé le CPA
                }
            }
        }
    }
    
    // Ajouter ou mettre à jour la cible
    if (target.mmsi != 0) {
        boatState->addOrUpdateAISTarget(target);
    }
}

// Décode AIS Type 5 - Static and Voyage Related Data
void NMEAParser::decodeAISType5(const uint8_t* payload, int payloadLen) {
    // MMSI (bits 8-37)
    uint32_t mmsi = extractBits(payload, 8, 30);
    
    // Call sign (bits 70-111) - 7 caractères 6-bit
    char callsign[8] = {0};
    for (int i = 0; i < 7; i++) {
        uint8_t c = extractBits(payload, 70 + i * 6, 6);
        if (c >= 1 && c <= 26) {
            callsign[i] = 'A' + c - 1;
        } else if (c >= 32) {
            callsign[i] = c;
        } else {
            callsign[i] = ' ';
        }
    }
    
    // Ship name (bits 112-231) - 20 caractères 6-bit
    char name[21] = {0};
    for (int i = 0; i < 20; i++) {
        uint8_t c = extractBits(payload, 112 + i * 6, 6);
        if (c >= 1 && c <= 26) {
            name[i] = 'A' + c - 1;
        } else if (c >= 32 && c < 64) {
            name[i] = c;
        } else if (c == 0) {
            name[i] = ' ';
        } else {
            name[i] = ' ';
        }
    }
    
    // Trim spaces
    for (int i = 19; i >= 0; i--) {
        if (name[i] != ' ') break;
        name[i] = '\0';
    }
    
    // Chercher la cible existante et mettre à jour le nom
    AISData ais = boatState->getAIS();
    for (int i = 0; i < ais.targetCount; i++) {
        if (ais.targets[i].mmsi == mmsi) {
            ais.targets[i].name = String(name);
            ais.targets[i].timestamp = millis();
            boatState->addOrUpdateAISTarget(ais.targets[i]);
            break;
        }
    }
}

// Décode AIS Type 18 - Standard Class B Position Report
void NMEAParser::decodeAISType18(const uint8_t* payload, int payloadLen) {
    // Structure similaire au Type 1, mais format Class B
    AISTarget target;
    
    // MMSI (bits 8-37)
    target.mmsi = extractBits(payload, 8, 30);
    
    // SOG (bits 46-55) en 1/10 knots
    uint32_t sogRaw = extractBits(payload, 46, 10);
    if (sogRaw != 1023) {
        target.sog = sogRaw / 10.0;
    }
    
    // Longitude (bits 57-84) en 1/10000 minutes
    int32_t lonRaw = extractBits(payload, 57, 28);
    if (lonRaw & 0x08000000) {
        lonRaw |= 0xF0000000;
    }
    if (lonRaw != 0x6791AC0) {
        target.lon = lonRaw / 600000.0;
    }
    
    // Latitude (bits 85-111) en 1/10000 minutes
    int32_t latRaw = extractBits(payload, 85, 27);
    if (latRaw & 0x04000000) {
        latRaw |= 0xF8000000;
    }
    if (latRaw != 0x3412140) {
        target.lat = latRaw / 600000.0;
    }
    
    // COG (bits 112-123) en 1/10 degrees
    uint32_t cogRaw = extractBits(payload, 112, 12);
    if (cogRaw != 3600) {
        target.cog = cogRaw / 10.0;
    }
    
    // True Heading (bits 124-132)
    uint32_t headingRaw = extractBits(payload, 124, 9);
    if (headingRaw != 511) {
        target.heading = headingRaw;
    }
    
    target.timestamp = millis();
    
    // Calculs de distance/bearing/CPA/TCPA (même code que Type 1)
    GPSData ownGPS = boatState->getGPS();
    if (ownGPS.position.lat.valid && ownGPS.position.lon.valid && 
        target.lat != 0 && target.lon != 0) {
        
        float lat1 = ownGPS.position.lat.value * PI / 180.0;
        float lon1 = ownGPS.position.lon.value * PI / 180.0;
        float lat2 = target.lat * PI / 180.0;
        float lon2 = target.lon * PI / 180.0;
        
        float dlat = lat2 - lat1;
        float dlon = lon2 - lon1;
        
        float a = sin(dlat/2) * sin(dlat/2) + 
                  cos(lat1) * cos(lat2) * sin(dlon/2) * sin(dlon/2);
        float c = 2 * atan2(sqrt(a), sqrt(1-a));
        target.distance = 3440.065 * c;
        
        float y = sin(dlon) * cos(lat2);
        float x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
        target.bearing = atan2(y, x) * 180.0 / PI;
        if (target.bearing < 0) target.bearing += 360;
        
        if (ownGPS.sog.valid && ownGPS.cog.valid && target.sog > 0) {
            float ownVx = ownGPS.sog.value * sin(ownGPS.cog.value * PI / 180.0);
            float ownVy = ownGPS.sog.value * cos(ownGPS.cog.value * PI / 180.0);
            float targetVx = target.sog * sin(target.cog * PI / 180.0);
            float targetVy = target.sog * cos(target.cog * PI / 180.0);
            
            float relVx = targetVx - ownVx;
            float relVy = targetVy - ownVy;
            float relSpeed = sqrt(relVx * relVx + relVy * relVy);
            
            if (relSpeed > 0.1) {
                float relX = target.distance * sin(target.bearing * PI / 180.0);
                float relY = target.distance * cos(target.bearing * PI / 180.0);
                
                target.tcpa = -(relX * relVx + relY * relVy) / (relSpeed * relSpeed) * 60.0;
                
                if (target.tcpa > 0) {
                    float cpaX = relX + relVx * (target.tcpa / 60.0);
                    float cpaY = relY + relVy * (target.tcpa / 60.0);
                    target.cpa = sqrt(cpaX * cpaX + cpaY * cpaY);
                } else {
                    target.cpa = target.distance;
                }
            }
        }
    }
    
    if (target.mmsi != 0) {
        boatState->addOrUpdateAISTarget(target);
    }
}

// Décode AIS Type 24 - Static Data Report
void NMEAParser::decodeAISType24(const uint8_t* payload, int payloadLen) {
    // Part Number (bits 38-39)
    uint8_t partNum = extractBits(payload, 38, 2);
    
    if (partNum == 0) {
        // Part A - Ship Name
        uint32_t mmsi = extractBits(payload, 8, 30);
        
        char name[21] = {0};
        for (int i = 0; i < 20; i++) {
            uint8_t c = extractBits(payload, 40 + i * 6, 6);
            if (c >= 1 && c <= 26) {
                name[i] = 'A' + c - 1;
            } else if (c >= 32 && c < 64) {
                name[i] = c;
            } else {
                name[i] = ' ';
            }
        }
        
        // Trim spaces
        for (int i = 19; i >= 0; i--) {
            if (name[i] != ' ') break;
            name[i] = '\0';
        }
        
        // Mettre à jour le nom
        AISData ais = boatState->getAIS();
        for (int i = 0; i < ais.targetCount; i++) {
            if (ais.targets[i].mmsi == mmsi) {
                ais.targets[i].name = String(name);
                ais.targets[i].timestamp = millis();
                boatState->addOrUpdateAISTarget(ais.targets[i]);
                break;
            }
        }
    }
    // Part B contient call sign et dimensions - on peut l'ignorer pour l'instant
}