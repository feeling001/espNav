/**
 * @file nmea_parser.cpp
 * @brief NMEA 0183 sentence parser and AIS decoder
 *
 * Parses standard NMEA 0183 sentences and AIS VDM/VDO messages.
 * AIS decoding covers:
 *   - Type 1/2/3 : Position Report Class A
 *   - Type 5     : Static and Voyage Related Data (2-sentence reassembly)
 *   - Type 18    : Standard Class B Position Report
 *   - Type 24    : Static Data Report (Class B name)
 */

#include "nmea_parser.h"
#include <stdlib.h>

NMEAParser::NMEAParser(BoatState* bs) : validSentences(0), invalidSentences(0), boatState(bs) {}

bool NMEAParser::parseLine(const char* line, NMEASentence& out) {
    // Accept '$' (NMEA standard) and '!' (AIS)
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

        if (boatState != nullptr) {
            if      (strstr(out.type, "GGA"))                          parseGGA(line);
            else if (strstr(out.type, "RMC"))                          parseRMC(line);
            else if (strstr(out.type, "GLL"))                          parseGLL(line);
            else if (strstr(out.type, "VTG"))                          parseVTG(line);
            else if (strstr(out.type, "HDT"))                          parseHDT(line);
            else if (strstr(out.type, "HDM"))                          parseHDM(line);
            else if (strstr(out.type, "DPT"))                          parseDPT(line);
            else if (strstr(out.type, "DBT"))                          parseDBT(line);
            else if (strstr(out.type, "MWV"))                          parseMWV(line);
            else if (strstr(out.type, "MWD"))                          parseMWD(line);
            else if (strstr(out.type, "MTW"))                          parseMTW(line);
            else if (strstr(out.type, "VHW"))                          parseVHW(line);
            else if (strstr(out.type, "VLW"))                          parseVLW(line);
            else if (strstr(out.type, "AIVDM") || strstr(out.type, "AIVDO")) parseAIVDM(line);
        }
    } else {
        invalidSentences++;
    }

    return out.valid;
}

bool NMEAParser::validateChecksum(const char* line) {
    const char* asterisk = strchr(line, '*');
    if (!asterisk || asterisk - line > 80) return false;

    uint8_t calculated = calculateChecksum(line + 1, asterisk - line - 1);

    uint8_t provided = 0;
    if (sscanf(asterisk + 1, "%02hhx", &provided) != 1) return false;

    return calculated == provided;
}

uint8_t NMEAParser::calculateChecksum(const char* data, size_t len) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; i++) checksum ^= data[i];
    return checksum;
}

void NMEAParser::extractSentenceType(const char* line, char* type, size_t maxLen) {
    const char* ptr = line + 1;  // Skip '$' or '!'
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

    while (*ptr && currentField < fieldIndex) {
        if (*ptr == ',') currentField++;
        ptr++;
    }

    if (currentField != fieldIndex) {
        buffer[0] = '\0';
        return 0;
    }

    size_t i = 0;
    while (*ptr && *ptr != ',' && *ptr != '*' && i < bufferSize - 1) {
        buffer[i++] = *ptr++;
    }
    buffer[i] = '\0';
    return i;
}

float NMEAParser::parseLatitude(const char* lat, const char* ns) {
    if (!lat || !ns || strlen(lat) < 4) return 0.0;
    char degrees[3] = {lat[0], lat[1], 0};
    float result = atof(degrees) + atof(lat + 2) / 60.0;
    if (ns[0] == 'S') result = -result;
    return result;
}

float NMEAParser::parseLongitude(const char* lon, const char* ew) {
    if (!lon || !ew || strlen(lon) < 5) return 0.0;
    char degrees[4] = {lon[0], lon[1], lon[2], 0};
    float result = atof(degrees) + atof(lon + 3) / 60.0;
    if (ew[0] == 'W') result = -result;
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
void NMEAParser::parseGGA(const char* line) {
    char buffer[32];
    char lat[16], ns[2], lon[16], ew[2];

    parseField(line, 2, lat, sizeof(lat));
    parseField(line, 3, ns,  sizeof(ns));
    parseField(line, 4, lon, sizeof(lon));
    parseField(line, 5, ew,  sizeof(ew));

    parseField(line, 6, buffer, sizeof(buffer));
    int fixQuality = atoi(buffer);

    parseField(line, 7, buffer, sizeof(buffer));
    int satellites = atoi(buffer);

    parseField(line, 8, buffer, sizeof(buffer));
    float hdop = atof(buffer);

    if (fixQuality > 0 && strlen(lat) > 0 && strlen(lon) > 0) {
        boatState->setGPSPosition(parseLatitude(lat, ns), parseLongitude(lon, ew));
        boatState->setGPSSatellites(satellites);
        boatState->setGPSFixQuality(fixQuality);
        boatState->setGPSHDOP(hdop);
    }
}

// $GPRMC - Recommended Minimum
void NMEAParser::parseRMC(const char* line) {
    char buffer[32];

    parseField(line, 2, buffer, sizeof(buffer));
    if (buffer[0] != 'A') return;

    char lat[16], ns[2], lon[16], ew[2];
    parseField(line, 3, lat, sizeof(lat));
    parseField(line, 4, ns,  sizeof(ns));
    parseField(line, 5, lon, sizeof(lon));
    parseField(line, 6, ew,  sizeof(ew));

    parseField(line, 7, buffer, sizeof(buffer));
    float sog = parseKnots(buffer);

    parseField(line, 8, buffer, sizeof(buffer));
    float cog = parseDegrees(buffer);

    if (strlen(lat) > 0 && strlen(lon) > 0) {
        boatState->setGPSPosition(parseLatitude(lat, ns), parseLongitude(lon, ew));
    }
    if (sog >= 0)            boatState->setGPSSOG(sog);
    if (cog >= 0 && cog < 360) boatState->setGPSCOG(cog);
}

// $GPGLL - Geographic Position
void NMEAParser::parseGLL(const char* line) {
    char buffer[32];

    parseField(line, 6, buffer, sizeof(buffer));
    if (buffer[0] != 'A') return;

    char lat[16], ns[2], lon[16], ew[2];
    parseField(line, 1, lat, sizeof(lat));
    parseField(line, 2, ns,  sizeof(ns));
    parseField(line, 3, lon, sizeof(lon));
    parseField(line, 4, ew,  sizeof(ew));

    if (strlen(lat) > 0 && strlen(lon) > 0) {
        boatState->setGPSPosition(parseLatitude(lat, ns), parseLongitude(lon, ew));
    }
}

// $GPVTG - Track Made Good and Ground Speed
void NMEAParser::parseVTG(const char* line) {
    char buffer[32];

    parseField(line, 1, buffer, sizeof(buffer));
    float cog = parseDegrees(buffer);

    parseField(line, 5, buffer, sizeof(buffer));
    float sog = parseKnots(buffer);

    if (cog >= 0 && cog < 360) boatState->setGPSCOG(cog);
    if (sog >= 0)               boatState->setGPSSOG(sog);
}

// $HCHDT - True Heading
void NMEAParser::parseHDT(const char* line) {
    char buffer[32];
    parseField(line, 1, buffer, sizeof(buffer));
    float heading = parseDegrees(buffer);
    if (heading >= 0 && heading < 360) boatState->setTrueHeading(heading);
}

// $HCHDM - Magnetic Heading
void NMEAParser::parseHDM(const char* line) {
    char buffer[32];
    parseField(line, 1, buffer, sizeof(buffer));
    float heading = parseDegrees(buffer);
    if (heading >= 0 && heading < 360) boatState->setMagneticHeading(heading);
}

// $SDDPT - Depth
void NMEAParser::parseDPT(const char* line) {
    char buffer[32];

    parseField(line, 1, buffer, sizeof(buffer));
    float depth = atof(buffer);

    parseField(line, 2, buffer, sizeof(buffer));
    float offset = atof(buffer);

    if (depth > 0)   boatState->setDepth(depth);
    if (offset != 0) boatState->setDepthOffset(offset);
}

// $SDDBT - Depth Below Transducer
void NMEAParser::parseDBT(const char* line) {
    char buffer[32];
    parseField(line, 3, buffer, sizeof(buffer));
    float depth = atof(buffer);
    if (depth > 0) boatState->setDepth(depth);
}

// $WIMWV - Wind Speed and Angle
void NMEAParser::parseMWV(const char* line) {
    char buffer[32];

    parseField(line, 1, buffer, sizeof(buffer));
    float angle = parseDegrees(buffer);

    parseField(line, 2, buffer, sizeof(buffer));
    char reference = buffer[0];  // R=Relative(apparent), T=True

    parseField(line, 3, buffer, sizeof(buffer));
    float speed = parseKnots(buffer);

    parseField(line, 4, buffer, sizeof(buffer));
    char unit = buffer[0];  // N=knots, M=m/s, K=km/h

    parseField(line, 5, buffer, sizeof(buffer));
    if (buffer[0] != 'A') return;  // V=invalid

    if (reference == 'R') {
        boatState->setApparentWind(angle, speed);
    } else if (reference == 'T') {
        boatState->setTrueWind(angle, speed);
    }
}

// $WIMWD - Wind Direction & Speed
void NMEAParser::parseMWD(const char* line) {
    char buffer[32];

    parseField(line, 1, buffer, sizeof(buffer));
    float trueDir = parseDegrees(buffer);

    parseField(line, 5, buffer, sizeof(buffer));
    float speed = parseKnots(buffer);

    if (trueDir >= 0 && trueDir < 360) boatState->setTrueWind(trueDir, speed);
}

// $YXMTW - Water Temperature
void NMEAParser::parseMTW(const char* line) {
    char buffer[32];
    parseField(line, 1, buffer, sizeof(buffer));
    float temp = atof(buffer);
    if (temp != 0) boatState->setWaterTemp(temp);
}

// $VWVHW - Water Speed and Heading
void NMEAParser::parseVHW(const char* line) {
    char buffer[32];

    parseField(line, 1, buffer, sizeof(buffer));
    float trueHeading = parseDegrees(buffer);

    parseField(line, 3, buffer, sizeof(buffer));
    float magHeading = parseDegrees(buffer);

    parseField(line, 5, buffer, sizeof(buffer));
    float stw = parseKnots(buffer);

    if (trueHeading >= 0 && trueHeading < 360) boatState->setTrueHeading(trueHeading);
    if (magHeading  >= 0 && magHeading  < 360) boatState->setMagneticHeading(magHeading);
    if (stw >= 0)                               boatState->setSTW(stw);
}

// $VWVLW - Distance Traveled through Water
void NMEAParser::parseVLW(const char* line) {
    char buffer[32];

    parseField(line, 1, buffer, sizeof(buffer));
    float total = atof(buffer);

    parseField(line, 3, buffer, sizeof(buffer));
    float trip = atof(buffer);

    if (total >= 0) boatState->setTotal(total);
    if (trip  >= 0) boatState->setTrip(trip);
}

// ============================================================
// AIS Decoder
// ============================================================

/**
 * Convert an AIS 6-bit ASCII payload character to its 6-bit binary value.
 * Per ITU-R M.1371: subtract 48; if >= 40 subtract another 8.
 */
uint8_t NMEAParser::aisCharTo6Bit(char c) {
    uint8_t v = (uint8_t)c - 48;
    if (v >= 40) v -= 8;
    return v & 0x3F;
}

/**
 * Convert a 6-bit AIS value to its ASCII character representation.
 * AIS 6-bit ASCII table (ITU-R M.1371):
 *   0       -> '@'
 *   1..26   -> 'A'..'Z'
 *   32..63  -> ' '..'?' (space and punctuation)
 *   others  -> ' '
 */
static char ais6BitToChar(uint8_t v) {
    if (v == 0)             return '@';
    if (v >= 1 && v <= 26) return 'A' + v - 1;
    if (v >= 32)            return (char)v;
    return ' ';
}

/**
 * Extract an unsigned integer from the AIS binary payload.
 * Bits are numbered from 0 at the MSB of the first byte (big-endian / MSB-first).
 *
 * @param payload   Packed binary buffer produced by decodeAISPayload()
 * @param start     First bit index (0-based, MSB of payload = bit 0)
 * @param length    Number of bits to extract (max 32)
 * @return          Unsigned integer value
 */
uint32_t NMEAParser::extractBits(const uint8_t* payload, int start, int length) {
    uint32_t result = 0;
    for (int i = 0; i < length; i++) {
        int bitIndex  = start + i;
        int byteIndex = bitIndex / 8;
        int bitInByte = 7 - (bitIndex % 8);  // MSB-first within each byte
        if (payload[byteIndex] & (1 << bitInByte)) {
            result |= (1u << (length - 1 - i));
        }
    }
    return result;
}

/**
 * Decode an AIS payload string (6-bit ASCII) into a packed binary buffer.
 * Each ASCII character encodes 6 bits; bits are placed MSB-first across bytes.
 *
 * @param payload   Null-terminated payload string from NMEA field 5
 * @param out       Output buffer (must be zeroed before call)
 * @param outSize   Size of output buffer in bytes
 * @return          Number of bits decoded
 */
static int decodeAISPayload(const char* payload, uint8_t* out, int outSize) {
    int payloadLen = strlen(payload);
    memset(out, 0, outSize);

    for (int i = 0; i < payloadLen; i++) {
        char c = payload[i];
        uint8_t value = 0;
        if (c >= '0' && c <= 'W')      value = c - 48;
        else if (c >= '`' && c <= 'w') value = c - 56;
        // else value stays 0

        // Place 6 bits MSB-first into output buffer
        int bitOffset = i * 6;
        int byteIndex = bitOffset / 8;
        int bitShift  = 2 - (bitOffset % 8);  // positive = shift left, negative = shift right

        if (byteIndex < outSize) {
            if (bitShift >= 0) {
                out[byteIndex] |= (value << bitShift);
            } else {
                out[byteIndex] |= (value >> (-bitShift));
                if (byteIndex + 1 < outSize) {
                    out[byteIndex + 1] |= (value << (8 + bitShift));
                }
            }
        }
    }
    return payloadLen * 6;
}

// ============================================================
// Multi-sentence reassembly buffer (for AIS type 5, 2 sentences)
// ============================================================
#define AIS_MULTIPART_MAX_PARTS  2
#define AIS_MULTIPART_MAX_LEN    84  // max chars per AIVDM payload part

static struct {
    bool active;
    char seqId;
    int  totalParts;
    int  receivedParts;
    char parts[AIS_MULTIPART_MAX_PARTS][AIS_MULTIPART_MAX_LEN];
} aisMsgBuffer = {false, 0, 0, 0, {}};

// ============================================================
// Helper: compute CPA/TCPA and distance/bearing for a target
// ============================================================
static void computeProximity(AISTarget& target, const GPSData& ownGPS) {
    if (!ownGPS.position.lat.valid || !ownGPS.position.lon.valid) return;
    if (target.lat == 0 && target.lon == 0) return;

    float lat1 = ownGPS.position.lat.value * PI / 180.0f;
    float lon1 = ownGPS.position.lon.value * PI / 180.0f;
    float lat2 = target.lat * PI / 180.0f;
    float lon2 = target.lon * PI / 180.0f;

    float dlat = lat2 - lat1;
    float dlon = lon2 - lon1;

    // Haversine distance (result in nautical miles)
    float a = sinf(dlat / 2) * sinf(dlat / 2) +
              cosf(lat1) * cosf(lat2) * sinf(dlon / 2) * sinf(dlon / 2);
    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    target.distance = 3440.065f * c;

    // Bearing (degrees true)
    float y = sinf(dlon) * cosf(lat2);
    float x = cosf(lat1) * sinf(lat2) - sinf(lat1) * cosf(lat2) * cosf(dlon);
    target.bearing = atan2f(y, x) * 180.0f / PI;
    if (target.bearing < 0) target.bearing += 360.0f;

    // CPA / TCPA
    if (ownGPS.sog.valid && ownGPS.cog.valid && target.sog > 0) {
        float ownVx    = ownGPS.sog.value * sinf(ownGPS.cog.value * PI / 180.0f);
        float ownVy    = ownGPS.sog.value * cosf(ownGPS.cog.value * PI / 180.0f);
        float targetVx = target.sog * sinf(target.cog * PI / 180.0f);
        float targetVy = target.sog * cosf(target.cog * PI / 180.0f);

        float relVx    = targetVx - ownVx;
        float relVy    = targetVy - ownVy;
        float relSpeed = sqrtf(relVx * relVx + relVy * relVy);

        if (relSpeed > 0.1f) {
            float relX = target.distance * sinf(target.bearing * PI / 180.0f);
            float relY = target.distance * cosf(target.bearing * PI / 180.0f);

            target.tcpa = -(relX * relVx + relY * relVy) / (relSpeed * relSpeed) * 60.0f;

            if (target.tcpa > 0) {
                float cpaX = relX + relVx * (target.tcpa / 60.0f);
                float cpaY = relY + relVy * (target.tcpa / 60.0f);
                target.cpa = sqrtf(cpaX * cpaX + cpaY * cpaY);
            } else {
                target.cpa = target.distance;
            }
        }
    }
}

// ============================================================
// Helper: update or create a stub AIS target with a name
// ============================================================
static void updateOrCreateNamedTarget(BoatState* boatState, uint32_t mmsi, const char* name) {
    if (!boatState || mmsi == 0 || name[0] == '\0') return;

    AISData ais = boatState->getAIS();
    for (int i = 0; i < ais.targetCount; i++) {
        if (ais.targets[i].mmsi == mmsi) {
            ais.targets[i].name      = String(name);
            ais.targets[i].timestamp = millis();
            boatState->addOrUpdateAISTarget(ais.targets[i]);
            return;
        }
    }
    // Target not yet known — create a stub so the name is preserved
    AISTarget stub;
    stub.mmsi      = mmsi;
    stub.name      = String(name);
    stub.timestamp = millis();
    boatState->addOrUpdateAISTarget(stub);
}

// ============================================================
// !AIVDM / !AIVDO — AIS VHF Data-link Message
// Format: !AIVDM,totalSentences,sentenceNum,seqId,channel,payload,fillBits*hh
// ============================================================
void NMEAParser::parseAIVDM(const char* line) {
    char buffer[8];

    // Field 1: total sentences in this message
    parseField(line, 1, buffer, sizeof(buffer));
    int totalSentences = atoi(buffer);

    // Field 2: sentence number (1-based)
    parseField(line, 2, buffer, sizeof(buffer));
    int sentenceNum = atoi(buffer);

    // Field 3: sequential message identifier (single char or empty)
    char seqIdBuf[4];
    parseField(line, 3, seqIdBuf, sizeof(seqIdBuf));
    char seqId = seqIdBuf[0];

    // Field 5: encoded payload
    char payloadPart[AIS_MULTIPART_MAX_LEN];
    parseField(line, 5, payloadPart, sizeof(payloadPart));
    if (strlen(payloadPart) == 0) return;

    // ----------------------------------------------------------
    // Multi-sentence reassembly (type 5 always spans 2 sentences)
    // ----------------------------------------------------------
    if (totalSentences > 1) {
        if (sentenceNum == 1) {
            aisMsgBuffer.active        = true;
            aisMsgBuffer.seqId         = seqId;
            aisMsgBuffer.totalParts    = totalSentences;
            aisMsgBuffer.receivedParts = 1;
            strncpy(aisMsgBuffer.parts[0], payloadPart, AIS_MULTIPART_MAX_LEN - 1);
            aisMsgBuffer.parts[0][AIS_MULTIPART_MAX_LEN - 1] = '\0';
        } else if (aisMsgBuffer.active &&
                   aisMsgBuffer.seqId == seqId &&
                   sentenceNum <= AIS_MULTIPART_MAX_PARTS) {
            strncpy(aisMsgBuffer.parts[sentenceNum - 1], payloadPart, AIS_MULTIPART_MAX_LEN - 1);
            aisMsgBuffer.parts[sentenceNum - 1][AIS_MULTIPART_MAX_LEN - 1] = '\0';
            aisMsgBuffer.receivedParts++;

            if (aisMsgBuffer.receivedParts == aisMsgBuffer.totalParts) {
                // Reassemble full payload
                char fullPayload[AIS_MULTIPART_MAX_PARTS * AIS_MULTIPART_MAX_LEN] = {0};
                for (int p = 0; p < aisMsgBuffer.totalParts; p++) {
                    strncat(fullPayload, aisMsgBuffer.parts[p],
                            sizeof(fullPayload) - strlen(fullPayload) - 1);
                }
                aisMsgBuffer.active = false;

                uint8_t binaryPayload[96] = {0};
                decodeAISPayload(fullPayload, binaryPayload, sizeof(binaryPayload));

                uint8_t messageType = (uint8_t)extractBits(binaryPayload, 0, 6);
                if (messageType == 5) {
                    decodeAISType5(binaryPayload, strlen(fullPayload));
                }
            }
        } else {
            // Out-of-sequence or unsupported multi-part — discard
            aisMsgBuffer.active = false;
        }
        return;
    }

    // ----------------------------------------------------------
    // Single-sentence message
    // ----------------------------------------------------------
    uint8_t binaryPayload[64] = {0};
    decodeAISPayload(payloadPart, binaryPayload, sizeof(binaryPayload));

    uint8_t messageType = (uint8_t)extractBits(binaryPayload, 0, 6);
    switch (messageType) {
        case 1: case 2: case 3:
            decodeAISType1(binaryPayload, strlen(payloadPart));
            break;
        case 18:
            decodeAISType18(binaryPayload, strlen(payloadPart));
            break;
        case 24:
            decodeAISType24(binaryPayload, strlen(payloadPart));
            break;
        default:
            break;
    }
}

// ============================================================
// Decode AIS Type 1/2/3 — Position Report Class A
// ============================================================
void NMEAParser::decodeAISType1(const uint8_t* payload, int payloadLen) {
    AISTarget target;

    // MMSI (bits 8–37)
    target.mmsi = extractBits(payload, 8, 30);

    // SOG (bits 50–59) in 1/10 knots; 1023 = not available
    uint32_t sogRaw = extractBits(payload, 50, 10);
    if (sogRaw != 1023) target.sog = sogRaw / 10.0f;

    // Longitude (bits 61–88) in 1/10000 minutes, signed 28-bit
    int32_t lonRaw = (int32_t)extractBits(payload, 61, 28);
    if (lonRaw & 0x08000000) lonRaw |= 0xF0000000;  // sign-extend
    if (lonRaw != 0x6791AC0) target.lon = lonRaw / 600000.0f;  // 181° = not available

    // Latitude (bits 89–115) in 1/10000 minutes, signed 27-bit
    int32_t latRaw = (int32_t)extractBits(payload, 89, 27);
    if (latRaw & 0x04000000) latRaw |= 0xF8000000;  // sign-extend
    if (latRaw != 0x3412140) target.lat = latRaw / 600000.0f;  // 91° = not available

    // COG (bits 116–127) in 1/10 degrees; 3600 = not available
    uint32_t cogRaw = extractBits(payload, 116, 12);
    if (cogRaw != 3600) target.cog = cogRaw / 10.0f;

    // True Heading (bits 128–136); 511 = not available
    uint32_t headingRaw = extractBits(payload, 128, 9);
    if (headingRaw != 511) target.heading = headingRaw;

    target.timestamp = millis();

    computeProximity(target, boatState->getGPS());

    if (target.mmsi != 0) {
        // Preserve existing name if present
        AISData ais = boatState->getAIS();
        for (int i = 0; i < ais.targetCount; i++) {
            if (ais.targets[i].mmsi == target.mmsi && ais.targets[i].name.length() > 0) {
                target.name = ais.targets[i].name;
                break;
            }
        }
        boatState->addOrUpdateAISTarget(target);
    }
}

// ============================================================
// Decode AIS Type 5 — Static and Voyage Related Data (Class A)
// Always 2 sentences; reassembled before this function is called.
// ============================================================
void NMEAParser::decodeAISType5(const uint8_t* payload, int payloadLen) {
    // MMSI (bits 8–37)
    uint32_t mmsi = extractBits(payload, 8, 30);

    // Ship name (bits 112–231) — 20 × 6-bit characters
    char name[21] = {0};
    for (int i = 0; i < 20; i++) {
        name[i] = ais6BitToChar((uint8_t)extractBits(payload, 112 + i * 6, 6));
    }
    // Trim trailing spaces and '@' padding
    for (int i = 19; i >= 0; i--) {
        if (name[i] != ' ' && name[i] != '@') break;
        name[i] = '\0';
    }

    updateOrCreateNamedTarget(boatState, mmsi, name);
}

// ============================================================
// Decode AIS Type 18 — Standard Class B Position Report
// ============================================================
void NMEAParser::decodeAISType18(const uint8_t* payload, int payloadLen) {
    AISTarget target;

    // MMSI (bits 8–37)
    target.mmsi = extractBits(payload, 8, 30);

    // SOG (bits 46–55) in 1/10 knots; 1023 = not available
    uint32_t sogRaw = extractBits(payload, 46, 10);
    if (sogRaw != 1023) target.sog = sogRaw / 10.0f;

    // Longitude (bits 57–84) in 1/10000 minutes, signed 28-bit
    int32_t lonRaw = (int32_t)extractBits(payload, 57, 28);
    if (lonRaw & 0x08000000) lonRaw |= 0xF0000000;
    if (lonRaw != 0x6791AC0) target.lon = lonRaw / 600000.0f;

    // Latitude (bits 85–111) in 1/10000 minutes, signed 27-bit
    int32_t latRaw = (int32_t)extractBits(payload, 85, 27);
    if (latRaw & 0x04000000) latRaw |= 0xF8000000;
    if (latRaw != 0x3412140) target.lat = latRaw / 600000.0f;

    // COG (bits 112–123) in 1/10 degrees; 3600 = not available
    uint32_t cogRaw = extractBits(payload, 112, 12);
    if (cogRaw != 3600) target.cog = cogRaw / 10.0f;

    // True Heading (bits 124–132); 511 = not available
    uint32_t headingRaw = extractBits(payload, 124, 9);
    if (headingRaw != 511) target.heading = headingRaw;

    target.timestamp = millis();

    computeProximity(target, boatState->getGPS());

    if (target.mmsi != 0) {
        // Preserve existing name if present
        AISData ais = boatState->getAIS();
        for (int i = 0; i < ais.targetCount; i++) {
            if (ais.targets[i].mmsi == target.mmsi && ais.targets[i].name.length() > 0) {
                target.name = ais.targets[i].name;
                break;
            }
        }
        boatState->addOrUpdateAISTarget(target);
    }
}

// ============================================================
// Decode AIS Type 24 — Static Data Report (Class B)
// Part A carries the ship name; Part B carries call sign / dimensions.
// ============================================================
void NMEAParser::decodeAISType24(const uint8_t* payload, int payloadLen) {
    // Part Number (bits 38–39)
    uint8_t partNum = (uint8_t)extractBits(payload, 38, 2);

    if (partNum == 0) {
        // Part A — Ship Name (bits 40–159, 20 × 6-bit chars)
        uint32_t mmsi = extractBits(payload, 8, 30);

        char name[21] = {0};
        for (int i = 0; i < 20; i++) {
            name[i] = ais6BitToChar((uint8_t)extractBits(payload, 40 + i * 6, 6));
        }
        // Trim trailing spaces and '@' padding
        for (int i = 19; i >= 0; i--) {
            if (name[i] != ' ' && name[i] != '@') break;
            name[i] = '\0';
        }

        updateOrCreateNamedTarget(boatState, mmsi, name);
    }
    // Part B (call sign, dimensions) — not needed for display
}
