#include "polar.h"
#include "functions.h"
#include <stdlib.h>
#include <string.h>

// ============================================================
// Constructor
// ============================================================

PolarData::PolarData()
    : loaded(false), numTWS(0), numTWA(0), storedFileSize(0) {
    memset(twsBreaks, 0, sizeof(twsBreaks));
    memset(twaBreaks, 0, sizeof(twaBreaks));
    memset(stwTable,  0, sizeof(stwTable));
}

// ============================================================
// loadFromFile
// ============================================================

bool PolarData::loadFromFile(const char* path) {
    loaded = false;
    numTWS = 0;
    numTWA = 0;

    // Guard against VFS error log when file doesn't exist
    if (!LittleFS.exists(path)) {
        serialPrintf("[Polar] File not found: %s\n", path);
        return false;
    }

    File f = LittleFS.open(path, "r");
    if (!f) {
        serialPrintf("[Polar] Failed to open: %s\n", path);
        return false;
    }

    storedFileSize = f.size();
    if (storedFileSize == 0 || storedFileSize > 65536) {
        serialPrintf("[Polar] Invalid file size: %zu bytes\n", storedFileSize);
        f.close();
        return false;
    }

    char* buf = (char*)malloc(storedFileSize + 1);
    if (!buf) {
        serialPrintf("[Polar] malloc failed\n");
        f.close();
        return false;
    }

    size_t bytesRead = f.readBytes(buf, storedFileSize);
    buf[bytesRead] = '\0';
    f.close();

    bool ok = parseBuffer(buf, bytesRead);
    free(buf);

    if (ok) {
        serialPrintf("[Polar] Loaded %u TWA × %u TWS entries from %s\n",
                      numTWA, numTWS, path);
        serialPrintf("[Polar] TWA range: %.0f°–%.0f°  |  TWS range: %.0f–%.0f kn\n",
                      twaBreaks[0], twaBreaks[numTWA - 1],
                      twsBreaks[0], twsBreaks[numTWS - 1]);
    }
    return ok;
}

// ============================================================
// parseBuffer
// ============================================================

bool PolarData::parseBuffer(char* buf, size_t len) {
    // Normalise line endings: replace \r with \n
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\r') buf[i] = '\n';
    }

    // Split into lines
    char* lines[POLAR_MAX_TWA + 2] = {};
    uint8_t lineCount = 0;

    char* p = buf;
    while (*p && lineCount < POLAR_MAX_TWA + 2) {
        // Skip blank lines
        while (*p == '\n') p++;
        if (*p == '\0') break;

        lines[lineCount++] = p;

        // Advance to end of line
        while (*p && *p != '\n') p++;
        if (*p == '\n') *p++ = '\0';
    }

    if (lineCount < 2) {
        serialPrintf("[Polar] Too few lines in file\n");
        return false;
    }

    // -------------------------------------------------------
    // Row 0: header — extract TWS breakpoints
    // -------------------------------------------------------
    {
        char* tok = strtok(lines[0], "\t");  // skip first cell (label)
        numTWS = 0;

        while ((tok = strtok(nullptr, "\t")) != nullptr && numTWS < POLAR_MAX_TWS) {
            // Skip empty tokens
            while (*tok == ' ') tok++;
            if (*tok == '\0') continue;

            float v = strtof(tok, nullptr);
            if (v > 0.0f) {
                twsBreaks[numTWS++] = v;
            }
        }
    }

    if (numTWS < 2) {
        serialPrintf("[Polar] Too few TWS columns: %u\n", numTWS);
        return false;
    }

    // -------------------------------------------------------
    // Rows 1…: TWA + STW values
    // -------------------------------------------------------
    numTWA = 0;

    for (uint8_t row = 1; row < lineCount && numTWA < POLAR_MAX_TWA; row++) {
        char* tok = strtok(lines[row], "\t");
        if (!tok) continue;

        // First cell = TWA
        float twaVal = strtof(tok, nullptr);
        if (twaVal < 0.0f || twaVal > 180.0f) continue;  // ignore out-of-range

        twaBreaks[numTWA] = twaVal;

        // Remaining cells = STW targets
        uint8_t col = 0;
        while ((tok = strtok(nullptr, "\t")) != nullptr && col < numTWS) {
            while (*tok == ' ') tok++;
            stwTable[numTWA][col] = (*tok == '\0') ? 0.0f : strtof(tok, nullptr);
            col++;
        }
        // Fill missing columns with 0
        while (col < numTWS) {
            stwTable[numTWA][col++] = 0.0f;
        }

        numTWA++;
    }

    if (numTWA < 2) {
        serialPrintf("[Polar] Too few TWA rows: %u\n", numTWA);
        return false;
    }

    loaded = true;
    return true;
}

// ============================================================
// getTargetSTW — bilinear interpolation
// ============================================================

float PolarData::getTargetSTW(float tws, float twa) const {
    if (!loaded) return -1.0f;

    // Use absolute TWA (polar is symmetric port/starboard)
    twa = fabsf(twa);

    // Clamp to table range
    tws = constrain(tws, twsBreaks[0],       twsBreaks[numTWS - 1]);
    twa = constrain(twa, twaBreaks[0],       twaBreaks[numTWA - 1]);

    // Find surrounding indices
    int ti = lowerBound(twsBreaks, numTWS, tws);
    int ai = lowerBound(twaBreaks, numTWA, twa);

    // Clamp indices so we always have a valid upper neighbour
    if (ti < 0) ti = 0;
    if (ai < 0) ai = 0;
    if (ti > numTWS - 2) ti = numTWS - 2;
    if (ai > numTWA - 2) ai = numTWA - 2;

    // Fractional positions in [0, 1]
    float ft = (twsBreaks[ti + 1] - twsBreaks[ti] > 0.0f)
               ? (tws - twsBreaks[ti]) / (twsBreaks[ti + 1] - twsBreaks[ti])
               : 0.0f;

    float fa = (twaBreaks[ai + 1] - twaBreaks[ai] > 0.0f)
               ? (twa - twaBreaks[ai]) / (twaBreaks[ai + 1] - twaBreaks[ai])
               : 0.0f;

    // Bilinear interpolation over the four surrounding table cells
    float v00 = stwTable[ai    ][ti    ];
    float v01 = stwTable[ai    ][ti + 1];
    float v10 = stwTable[ai + 1][ti    ];
    float v11 = stwTable[ai + 1][ti + 1];

    float result = lerp(lerp(v00, v01, ft),
                        lerp(v10, v11, ft),
                        fa);

    return result;
}

// ============================================================
// lowerBound
// ============================================================

int PolarData::lowerBound(const float* arr, uint8_t count, float value) const {
    int idx = -1;
    for (uint8_t i = 0; i < count; i++) {
        if (arr[i] <= value) idx = i;
        else break;
    }
    return idx;
}

// ============================================================
// twsString
// ============================================================

String PolarData::twsString() const {
    String s;
    for (uint8_t i = 0; i < numTWS; i++) {
        if (i > 0) s += ' ';
        s += String((int)twsBreaks[i]);
    }
    return s;
}
