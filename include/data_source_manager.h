#ifndef DATA_SOURCE_MANAGER_H
#define DATA_SOURCE_MANAGER_H

#include <Arduino.h>
#include "types.h"

/**
 * @brief Runtime holder for the "which source feeds which BoatState field"
 *        configuration (NMEA sentence / SeaTalk datagram / locally computed).
 *
 * Loaded from ConfigManager at boot, and live-updated (no reboot needed)
 * whenever the web UI POSTs a new configuration.
 *
 * NMEAParser and SeatalkManager hold a pointer to this instance and call
 * isActive(field, sub) before writing to BoatState so that only the
 * currently-selected source updates a given field.
 */
class DataSourceManager {
public:
    DataSourceManager();

    /** Replace the whole configuration (called at boot and after a POST). */
    void setConfig(const DataSourceConfig& cfg);

    /** Return a copy of the current configuration. */
    DataSourceConfig getConfig() const;

    /** True if `sub` is the currently configured source for `field`. */
    bool isActive(uint8_t field, DataSubSource sub) const;

    /** Magnetic variation (degrees, +E/-W) for HEADING_TRUE compute. */
    float getMagneticVariation() const;

    // ── Static catalogue helpers (used by the REST API layer) ─────────────
    static const char* fieldId(uint8_t field);
    static const char* subId(DataSubSource sub);
    static bool fieldFromId(const char* id, uint8_t& field);
    static bool subFromId(const char* id, DataSubSource& sub);

private:
    DataSourceConfig config;
};

#endif // DATA_SOURCE_MANAGER_H
