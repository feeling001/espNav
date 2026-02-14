#ifndef BOAT_STATE_H
#define BOAT_STATE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Timeout values in milliseconds
#define DATA_TIMEOUT_DEFAULT 10000  // 10 seconds for most data
#define DATA_TIMEOUT_AIS     60000  // 60 seconds for AIS

// Maximum AIS targets to keep in memory
#define MAX_AIS_TARGETS 20

/**
 * Structure for storing a single data point with timestamp and unit
 */
struct DataPoint {
    float value;
    const char* unit;
    unsigned long timestamp;
    bool valid;
    
    DataPoint() : value(0.0), unit(""), timestamp(0), valid(false) {}
    
    void set(float val, const char* unitStr) {
        value = val;
        unit = unitStr;
        timestamp = millis();
        valid = true;
    }
    
    void invalidate() {
        valid = false;
    }
    
    bool isStale(unsigned long timeout = DATA_TIMEOUT_DEFAULT) {
        if (!valid) return true;
        return (millis() - timestamp) > timeout;
    }
};

/**
 * GPS Position structure
 */
struct GPSPosition {
    DataPoint lat;
    DataPoint lon;
    
    void set(float latitude, float longitude) {
        lat.set(latitude, "deg");
        lon.set(longitude, "deg");
    }
};

/**
 * GPS data structure
 */
struct GPSData {
    GPSPosition position;
    DataPoint sog;          // Speed Over Ground
    DataPoint cog;          // Course Over Ground
    DataPoint satellites;
    DataPoint fix_quality;
    DataPoint hdop;         // Horizontal Dilution of Precision
};

/**
 * Speed/Log data structure
 */
struct SpeedData {
    DataPoint stw;          // Speed Through Water
    DataPoint trip;         // Trip distance
    DataPoint total;        // Total distance
};

/**
 * Heading data structure
 */
struct HeadingData {
    DataPoint magnetic;
    DataPoint true_heading;
};

/**
 * Depth data structure
 */
struct DepthData {
    DataPoint below_transducer;
    DataPoint offset;       // Transducer offset (configurable)
};

/**
 * Wind data structure
 */
struct WindData {
    DataPoint aws;          // Apparent Wind Speed
    DataPoint awa;          // Apparent Wind Angle
    DataPoint tws;          // True Wind Speed (calculated)
    DataPoint twa;          // True Wind Angle (calculated)
    DataPoint twd;          // True Wind Direction (calculated)
};

/**
 * Environment data structure
 */
struct EnvironmentData {
    DataPoint water_temp;
    DataPoint air_temp;
    DataPoint pressure;
};

/**
 * Calculated data structure
 */
struct CalculatedData {
    DataPoint vmg_wind;
    DataPoint vmg_waypoint;
    DataPoint set;          // Current direction
    DataPoint drift;        // Current speed
};

/**
 * Autopilot data structure (for future SeaTalk1 integration)
 */
struct AutopilotData {
    String mode;            // "standby", "auto", "wind", "track", "manual"
    String status;          // "engaged", "standby", "alarm"
    DataPoint heading_target;
    DataPoint wind_angle_target;
    DataPoint rudder_angle;
    DataPoint locked_heading;
    DataPoint xte;          // Cross Track Error
    String alarm;
    unsigned long timestamp;
    bool valid;
    
    AutopilotData() : mode(""), status(""), alarm(""), timestamp(0), valid(false) {}
    
    bool isStale(unsigned long timeout = DATA_TIMEOUT_DEFAULT) {
        if (!valid) return true;
        return (millis() - timestamp) > timeout;
    }
};

/**
 * AIS Target structure
 */
struct AISTarget {
    uint32_t mmsi;
    String name;
    float lat;
    float lon;
    float cog;
    float sog;
    float heading;
    float distance;         // Distance to target (nm)
    float bearing;          // Bearing to target (deg)
    float cpa;             // Closest Point of Approach (nm)
    float tcpa;            // Time to CPA (minutes)
    unsigned long timestamp;
    
    AISTarget() : mmsi(0), name(""), lat(0), lon(0), cog(0), sog(0), 
                  heading(0), distance(0), bearing(0), cpa(0), tcpa(0), 
                  timestamp(0) {}
};

/**
 * AIS data structure
 */
struct AISData {
    AISTarget targets[MAX_AIS_TARGETS];
    int targetCount;
    
    AISData() : targetCount(0) {}
    
    void addOrUpdateTarget(const AISTarget& target) {
        // Search for existing target with same MMSI
        for (int i = 0; i < targetCount; i++) {
            if (targets[i].mmsi == target.mmsi) {
                targets[i] = target;
                return;
            }
        }
        
        // Add new target if space available
        if (targetCount < MAX_AIS_TARGETS) {
            targets[targetCount++] = target;
        }
    }
    
    void removeStaleTargets(unsigned long timeout = DATA_TIMEOUT_AIS) {
        unsigned long now = millis();
        int writeIndex = 0;
        
        for (int i = 0; i < targetCount; i++) {
            if ((now - targets[i].timestamp) <= timeout) {
                if (writeIndex != i) {
                    targets[writeIndex] = targets[i];
                }
                writeIndex++;
            }
        }
        
        targetCount = writeIndex;
    }
};

/**
 * Main Boat State class
 * Thread-safe storage for all boat data
 */
class BoatState {
public:
    BoatState();
    ~BoatState();
    
    // Initialization
    void init();
    
    // Data access (getters return copies for thread safety)
    GPSData getGPS();
    SpeedData getSpeed();
    HeadingData getHeading();
    DepthData getDepth();
    WindData getWind();
    EnvironmentData getEnvironment();
    CalculatedData getCalculated();
    AutopilotData getAutopilot();
    AISData getAIS();
    
    // Data setters
    void setGPSPosition(float lat, float lon);
    void setGPSSOG(float sog);
    void setGPSCOG(float cog);
    void setGPSSatellites(int count);
    void setGPSFixQuality(int quality);
    void setGPSHDOP(float hdop);
    
    void setSTW(float stw);
    void setTrip(float trip);
    void setTotal(float total);
    
    void setMagneticHeading(float heading);
    void setTrueHeading(float heading);
    
    void setDepth(float depth);
    void setDepthOffset(float offset);
    
    void setApparentWind(float speed, float angle);
    void setTrueWind(float speed, float angle, float direction);
    
    void setWaterTemp(float temp);
    void setAirTemp(float temp);
    void setPressure(float pressure);
    
    void setVMGWind(float vmg);
    void setVMGWaypoint(float vmg);
    void setCurrentSetDrift(float set, float drift);
    
    void setAutopilotMode(const String& mode);
    void setAutopilotStatus(const String& status);
    void setAutopilotHeadingTarget(float heading);
    void setAutopilotWindAngleTarget(float angle);
    void setAutopilotRudderAngle(float angle);
    void setAutopilotXTE(float xte);
    void setAutopilotAlarm(const String& alarm);
    
    void addOrUpdateAISTarget(const AISTarget& target);
    
    // Utility functions
    void cleanupStaleData();
    void calculateDerivedData();
    
    // JSON serialization for API
    String toJSON();
    String getNavigationJSON();
    String getWindJSON();
    String getAISJSON();
    
private:
    // Data structures
    GPSData gps;
    SpeedData speed;
    HeadingData heading;
    DepthData depth;
    WindData wind;
    EnvironmentData environment;
    CalculatedData calculated;
    AutopilotData autopilot;
    AISData ais;
    
    // Thread safety
    SemaphoreHandle_t mutex;
    
    // Helper functions
    void addDataPointToJSON(JsonObject& obj, const char* key, const DataPoint& dp);
};

#endif // BOAT_STATE_H
