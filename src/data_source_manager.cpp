#include "data_source_manager.h"
#include "functions.h"
#include <string.h>

// ── Static catalogue — id strings must match the front-end DataSourceConfig.jsx ──
static const char* DS_FIELD_IDS[DS_FIELD_COUNT] = {
    "gps_position",   // DS_GPS_POSITION
    "gps_sog",        // DS_GPS_SOG
    "gps_cog",        // DS_GPS_COG
    "heading_mag",    // DS_HEADING_MAG
    "heading_true",   // DS_HEADING_TRUE
    "stw",            // DS_STW
    "depth",          // DS_DEPTH
    "wind_apparent",  // DS_WIND_APPARENT
    "wind_true",      // DS_WIND_TRUE
    "water_temp",     // DS_WATER_TEMP
    "trip",           // DS_TRIP
    "total",          // DS_TOTAL
};

static const char* DS_SUB_IDS[DS_SUB_COUNT] = {
    "nmea_gga",   // DS_SUB_NMEA_GGA
    "nmea_rmc",   // DS_SUB_NMEA_RMC
    "nmea_gll",   // DS_SUB_NMEA_GLL
    "nmea_vtg",   // DS_SUB_NMEA_VTG
    "nmea_hdt",   // DS_SUB_NMEA_HDT
    "nmea_hdm",   // DS_SUB_NMEA_HDM
    "nmea_vhw",   // DS_SUB_NMEA_VHW
    "nmea_dpt",   // DS_SUB_NMEA_DPT
    "nmea_dbt",   // DS_SUB_NMEA_DBT
    "nmea_mwv",   // DS_SUB_NMEA_MWV
    "nmea_mwd",   // DS_SUB_NMEA_MWD
    "nmea_mtw",   // DS_SUB_NMEA_MTW
    "nmea_vlw",   // DS_SUB_NMEA_VLW
    "seatalk",    // DS_SUB_SEATALK
    "compute",    // DS_SUB_COMPUTE
};

DataSourceManager::DataSourceManager() {}

void DataSourceManager::setConfig(const DataSourceConfig& cfg) {
    config = cfg;
}

DataSourceConfig DataSourceManager::getConfig() const {
    return config;
}

bool DataSourceManager::isActive(uint8_t field, DataSubSource sub) const {
    if (field >= DS_FIELD_COUNT) return false;
    return config.source[field] == (uint8_t)sub;
}

float DataSourceManager::getMagneticVariation() const {
    return config.magneticVariation;
}

const char* DataSourceManager::fieldId(uint8_t field) {
    if (field >= DS_FIELD_COUNT) return "";
    return DS_FIELD_IDS[field];
}

const char* DataSourceManager::subId(DataSubSource sub) {
    if (sub >= DS_SUB_COUNT) return "";
    return DS_SUB_IDS[sub];
}

bool DataSourceManager::fieldFromId(const char* id, uint8_t& field) {
    if (!id) return false;
    for (int i = 0; i < DS_FIELD_COUNT; i++) {
        if (strcmp(id, DS_FIELD_IDS[i]) == 0) {
            field = (uint8_t)i;
            return true;
        }
    }
    return false;
}

bool DataSourceManager::subFromId(const char* id, DataSubSource& sub) {
    if (!id) return false;
    for (int i = 0; i < DS_SUB_COUNT; i++) {
        if (strcmp(id, DS_SUB_IDS[i]) == 0) {
            sub = (DataSubSource)i;
            return true;
        }
    }
    return false;
}
