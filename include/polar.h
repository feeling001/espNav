#ifndef POLAR_H
#define POLAR_H

#include <Arduino.h>
#include <LittleFS.h>

// Maximum polar table dimensions
#define POLAR_MAX_TWS    20   // max wind speed columns
#define POLAR_MAX_TWA    40   // max wind angle rows
#define POLAR_FILE_PATH  "/polar.pol"

/**
 * @brief Boat polar diagram — loaded from a tab-delimited file on LittleFS.
 *
 * File format (tab-separated):
 *   Row 0, col 0 : label (e.g. "TWA" or "TWA\TWS") — ignored
 *   Row 0, col 1…N : TWS breakpoints in knots
 *   Row 1…M, col 0 : TWA breakpoints in degrees (0–180)
 *   Row 1…M, col 1…N : target STW in knots
 *
 * TWA is treated as an absolute value (port/starboard symmetry).
 * Interpolation is bilinear (linear on both TWS and TWA axes).
 * TWS and TWA values are clamped to the table range — no extrapolation.
 */
class PolarData {
public:
    PolarData();

    // -----------------------------------------------------------------------
    // Loading
    // -----------------------------------------------------------------------

    /**
     * @brief Load polar from a file on LittleFS.
     * @param path  File path (default: POLAR_FILE_PATH)
     * @return true on success
     */
    bool loadFromFile(const char* path = POLAR_FILE_PATH);

    // -----------------------------------------------------------------------
    // Query
    // -----------------------------------------------------------------------

    /**
     * @brief Bilinear interpolation of target STW.
     * @param tws  True Wind Speed in knots
     * @param twa  True Wind Angle in degrees — absolute value used (0–180)
     * @return     Target STW in knots, or -1.0f if polar not loaded
     */
    float getTargetSTW(float tws, float twa) const;

    // -----------------------------------------------------------------------
    // State accessors
    // -----------------------------------------------------------------------

    bool    isLoaded()  const { return loaded; }
    uint8_t twsCount()  const { return numTWS; }
    uint8_t twaCount()  const { return numTWA; }
    size_t  fileSize()  const { return storedFileSize; }

    /** @brief Space-separated list of TWS breakpoints for status display. */
    String  twsString() const;

private:
    bool    loaded;
    uint8_t numTWS;
    uint8_t numTWA;
    float   twsBreaks[POLAR_MAX_TWS];
    float   twaBreaks[POLAR_MAX_TWA];
    float   stwTable[POLAR_MAX_TWA][POLAR_MAX_TWS];
    size_t  storedFileSize;

    bool  parseBuffer(char* buf, size_t len);

    /** @return largest index i such that arr[i] <= value, or -1 if none */
    int   lowerBound(const float* arr, uint8_t count, float value) const;

    static float lerp(float a, float b, float t) { return a + (b - a) * t; }
};

#endif // POLAR_H
