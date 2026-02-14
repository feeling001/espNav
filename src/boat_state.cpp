#include "boat_state.h"
#include <math.h>

BoatState::BoatState() {
    mutex = xSemaphoreCreateMutex();
}

BoatState::~BoatState() {
    if (mutex != NULL) {
        vSemaphoreDelete(mutex);
    }
}

void BoatState::init() {
    Serial.println("[BoatState] Initializing boat state manager");
    
    // Initialize default units
    gps.sog.unit = "kn";
    gps.cog.unit = "deg";
    gps.satellites.unit = "count";
    gps.fix_quality.unit = "";
    gps.hdop.unit = "";
    
    speed.stw.unit = "kn";
    speed.trip.unit = "nm";
    speed.total.unit = "nm";
    
    heading.magnetic.unit = "deg";
    heading.true_heading.unit = "deg";
    
    depth.below_transducer.unit = "m";
    depth.offset.unit = "m";
    
    wind.aws.unit = "kn";
    wind.awa.unit = "deg";
    wind.tws.unit = "kn";
    wind.twa.unit = "deg";
    wind.twd.unit = "deg";
    
    environment.water_temp.unit = "C";
    environment.air_temp.unit = "C";
    environment.pressure.unit = "hPa";
    
    calculated.vmg_wind.unit = "kn";
    calculated.vmg_waypoint.unit = "kn";
    calculated.set.unit = "deg";
    calculated.drift.unit = "kn";
    
    Serial.println("[BoatState] ✓ Initialization complete");
}

// ============================================================
// Getters (thread-safe, return copies)
// ============================================================

GPSData BoatState::getGPS() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    GPSData copy = gps;
    xSemaphoreGive(mutex);
    return copy;
}

SpeedData BoatState::getSpeed() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    SpeedData copy = speed;
    xSemaphoreGive(mutex);
    return copy;
}

HeadingData BoatState::getHeading() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    HeadingData copy = heading;
    xSemaphoreGive(mutex);
    return copy;
}

DepthData BoatState::getDepth() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    DepthData copy = depth;
    xSemaphoreGive(mutex);
    return copy;
}

WindData BoatState::getWind() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    WindData copy = wind;
    xSemaphoreGive(mutex);
    return copy;
}

EnvironmentData BoatState::getEnvironment() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    EnvironmentData copy = environment;
    xSemaphoreGive(mutex);
    return copy;
}

CalculatedData BoatState::getCalculated() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    CalculatedData copy = calculated;
    xSemaphoreGive(mutex);
    return copy;
}

AutopilotData BoatState::getAutopilot() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    AutopilotData copy = autopilot;
    xSemaphoreGive(mutex);
    return copy;
}

AISData BoatState::getAIS() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    AISData copy = ais;
    xSemaphoreGive(mutex);
    return copy;
}

// ============================================================
// GPS Setters
// ============================================================

void BoatState::setGPSPosition(float lat, float lon) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    gps.position.set(lat, lon);
    xSemaphoreGive(mutex);
}

void BoatState::setGPSSOG(float sog) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    gps.sog.set(sog, "kn");
    xSemaphoreGive(mutex);
}

void BoatState::setGPSCOG(float cog) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    gps.cog.set(cog, "deg");
    xSemaphoreGive(mutex);
}

void BoatState::setGPSSatellites(int count) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    gps.satellites.set(count, "count");
    xSemaphoreGive(mutex);
}

void BoatState::setGPSFixQuality(int quality) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    gps.fix_quality.set(quality, "");
    xSemaphoreGive(mutex);
}

void BoatState::setGPSHDOP(float hdop) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    gps.hdop.set(hdop, "");
    xSemaphoreGive(mutex);
}

// ============================================================
// Speed Setters
// ============================================================

void BoatState::setSTW(float stw) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    speed.stw.set(stw, "kn");
    xSemaphoreGive(mutex);
}

void BoatState::setTrip(float trip) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    speed.trip.set(trip, "nm");
    xSemaphoreGive(mutex);
}

void BoatState::setTotal(float total) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    speed.total.set(total, "nm");
    xSemaphoreGive(mutex);
}

// ============================================================
// Heading Setters
// ============================================================

void BoatState::setMagneticHeading(float heading_val) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    heading.magnetic.set(heading_val, "deg");
    xSemaphoreGive(mutex);
}

void BoatState::setTrueHeading(float heading_val) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    heading.true_heading.set(heading_val, "deg");
    xSemaphoreGive(mutex);
}

// ============================================================
// Depth Setters
// ============================================================

void BoatState::setDepth(float depth_val) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    depth.below_transducer.set(depth_val, "m");
    xSemaphoreGive(mutex);
}

void BoatState::setDepthOffset(float offset) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    depth.offset.set(offset, "m");
    xSemaphoreGive(mutex);
}

// ============================================================
// Wind Setters
// ============================================================

void BoatState::setApparentWind(float speed, float angle) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    wind.aws.set(speed, "kn");
    wind.awa.set(angle, "deg");
    xSemaphoreGive(mutex);
    
    // Trigger calculation of true wind if we have necessary data
    calculateDerivedData();
}

void BoatState::setTrueWind(float speed, float angle, float direction) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    wind.tws.set(speed, "kn");
    wind.twa.set(angle, "deg");
    wind.twd.set(direction, "deg");
    xSemaphoreGive(mutex);
}

// ============================================================
// Environment Setters
// ============================================================

void BoatState::setWaterTemp(float temp) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    environment.water_temp.set(temp, "C");
    xSemaphoreGive(mutex);
}

void BoatState::setAirTemp(float temp) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    environment.air_temp.set(temp, "C");
    xSemaphoreGive(mutex);
}

void BoatState::setPressure(float pressure) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    environment.pressure.set(pressure, "hPa");
    xSemaphoreGive(mutex);
}

// ============================================================
// Calculated Data Setters
// ============================================================

void BoatState::setVMGWind(float vmg) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    calculated.vmg_wind.set(vmg, "kn");
    xSemaphoreGive(mutex);
}

void BoatState::setVMGWaypoint(float vmg) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    calculated.vmg_waypoint.set(vmg, "kn");
    xSemaphoreGive(mutex);
}

void BoatState::setCurrentSetDrift(float set, float drift) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    calculated.set.set(set, "deg");
    calculated.drift.set(drift, "kn");
    xSemaphoreGive(mutex);
}

// ============================================================
// Autopilot Setters
// ============================================================

void BoatState::setAutopilotMode(const String& mode) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    autopilot.mode = mode;
    autopilot.timestamp = millis();
    autopilot.valid = true;
    xSemaphoreGive(mutex);
}

void BoatState::setAutopilotStatus(const String& status) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    autopilot.status = status;
    autopilot.timestamp = millis();
    autopilot.valid = true;
    xSemaphoreGive(mutex);
}

void BoatState::setAutopilotHeadingTarget(float heading) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    autopilot.heading_target.set(heading, "deg");
    autopilot.timestamp = millis();
    autopilot.valid = true;
    xSemaphoreGive(mutex);
}

void BoatState::setAutopilotWindAngleTarget(float angle) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    autopilot.wind_angle_target.set(angle, "deg");
    autopilot.timestamp = millis();
    autopilot.valid = true;
    xSemaphoreGive(mutex);
}

void BoatState::setAutopilotRudderAngle(float angle) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    autopilot.rudder_angle.set(angle, "deg");
    autopilot.timestamp = millis();
    autopilot.valid = true;
    xSemaphoreGive(mutex);
}

void BoatState::setAutopilotXTE(float xte) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    autopilot.xte.set(xte, "nm");
    autopilot.timestamp = millis();
    autopilot.valid = true;
    xSemaphoreGive(mutex);
}

void BoatState::setAutopilotAlarm(const String& alarm) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    autopilot.alarm = alarm;
    autopilot.timestamp = millis();
    autopilot.valid = true;
    xSemaphoreGive(mutex);
}

// ============================================================
// AIS
// ============================================================

void BoatState::addOrUpdateAISTarget(const AISTarget& target) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    ais.addOrUpdateTarget(target);
    xSemaphoreGive(mutex);
}

// ============================================================
// Utility Functions
// ============================================================

void BoatState::cleanupStaleData() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    
    // Remove stale AIS targets
    ais.removeStaleTargets();
    
    xSemaphoreGive(mutex);
}

void BoatState::calculateDerivedData() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    
    // Calculate True Wind from Apparent Wind if we have STW and COG
    if (wind.aws.valid && wind.awa.valid && speed.stw.valid && heading.true_heading.valid) {
        // Convert AWA to radians (-180 to +180, starboard positive)
        float awa_rad = wind.awa.value * PI / 180.0;
        
        // Boat velocity components
        float boat_speed = speed.stw.value;
        float boat_vx = boat_speed * sin(0);  // Boat moving forward
        float boat_vy = boat_speed * cos(0);
        
        // Apparent wind components
        float aws = wind.aws.value;
        float aw_vx = aws * sin(awa_rad);
        float aw_vy = aws * cos(awa_rad);
        
        // True wind = Apparent wind - Boat velocity
        float tw_vx = aw_vx - boat_vx;
        float tw_vy = aw_vy - boat_vy;
        
        // Calculate TWS and TWA
        float tws = sqrt(tw_vx * tw_vx + tw_vy * tw_vy);
        float twa = atan2(tw_vx, tw_vy) * 180.0 / PI;
        
        // Calculate TWD (True Wind Direction)
        float twd = heading.true_heading.value + twa;
        if (twd < 0) twd += 360;
        if (twd >= 360) twd -= 360;
        
        wind.tws.set(tws, "kn");
        wind.twa.set(twa, "deg");
        wind.twd.set(twd, "deg");
    }
    
    // Calculate VMG to wind
    if (speed.stw.valid && wind.awa.valid) {
        float awa_rad = wind.awa.value * PI / 180.0;
        float vmg_wind = speed.stw.value * cos(awa_rad);
        calculated.vmg_wind.set(vmg_wind, "kn");
    }
    
    // Calculate current (Set & Drift) if we have both SOG/COG and STW/Heading
    if (gps.sog.valid && gps.cog.valid && speed.stw.valid && heading.true_heading.valid) {
        // Convert to radians
        float cog_rad = gps.cog.value * PI / 180.0;
        float hdg_rad = heading.true_heading.value * PI / 180.0;
        
        // SOG components
        float sog_vx = gps.sog.value * sin(cog_rad);
        float sog_vy = gps.sog.value * cos(cog_rad);
        
        // STW components
        float stw_vx = speed.stw.value * sin(hdg_rad);
        float stw_vy = speed.stw.value * cos(hdg_rad);
        
        // Current = SOG - STW
        float current_vx = sog_vx - stw_vx;
        float current_vy = sog_vy - stw_vy;
        
        float drift = sqrt(current_vx * current_vx + current_vy * current_vy);
        float set = atan2(current_vx, current_vy) * 180.0 / PI;
        if (set < 0) set += 360;
        
        calculated.drift.set(drift, "kn");
        calculated.set.set(set, "deg");
    }
    
    xSemaphoreGive(mutex);
}

// ============================================================
// JSON Serialization
// ============================================================

void BoatState::addDataPointToJSON(JsonObject obj, const char* key, const DataPoint& dp) {
    JsonObject point = obj[key].to<JsonObject>();
    if (dp.valid && !dp.isStale()) {
        point["value"] = dp.value;
        point["unit"] = dp.unit;
        point["age"] = (millis() - dp.timestamp) / 1000.0;  // Age in seconds
    } else {
        point["value"] = nullptr;
        point["unit"] = dp.unit;
        point["age"] = nullptr;
    }
}

String BoatState::toJSON() {
    JsonDocument doc;
    
    xSemaphoreTake(mutex, portMAX_DELAY);
    
    // GPS
    JsonObject gpsObj = doc["gps"].to<JsonObject>();
    JsonObject position = gpsObj["position"].to<JsonObject>();
    addDataPointToJSON(position, "lat", gps.position.lat);
    addDataPointToJSON(position, "lon", gps.position.lon);
    addDataPointToJSON(gpsObj, "sog", gps.sog);
    addDataPointToJSON(gpsObj, "cog", gps.cog);
    addDataPointToJSON(gpsObj, "satellites", gps.satellites);
    addDataPointToJSON(gpsObj, "fix_quality", gps.fix_quality);
    addDataPointToJSON(gpsObj, "hdop", gps.hdop);
    
    // Speed
    JsonObject speedObj = doc["speed"].to<JsonObject>();
    addDataPointToJSON(speedObj, "stw", speed.stw);
    addDataPointToJSON(speedObj, "trip", speed.trip);
    addDataPointToJSON(speedObj, "total", speed.total);
    
    // Heading
    JsonObject headingObj = doc["heading"].to<JsonObject>();
    addDataPointToJSON(headingObj, "magnetic", heading.magnetic);
    addDataPointToJSON(headingObj, "true", heading.true_heading);
    
    // Depth
    JsonObject depthObj = doc["depth"].to<JsonObject>();
    addDataPointToJSON(depthObj, "below_transducer", depth.below_transducer);
    addDataPointToJSON(depthObj, "offset", depth.offset);
    
    // Wind
    JsonObject windObj = doc["wind"].to<JsonObject>();
    addDataPointToJSON(windObj, "aws", wind.aws);
    addDataPointToJSON(windObj, "awa", wind.awa);
    addDataPointToJSON(windObj, "tws", wind.tws);
    addDataPointToJSON(windObj, "twa", wind.twa);
    addDataPointToJSON(windObj, "twd", wind.twd);
    
    // Environment
    JsonObject envObj = doc["environment"].to<JsonObject>();
    addDataPointToJSON(envObj, "water_temp", environment.water_temp);
    addDataPointToJSON(envObj, "air_temp", environment.air_temp);
    addDataPointToJSON(envObj, "pressure", environment.pressure);
    
    // Calculated
    JsonObject calcObj = doc["calculated"].to<JsonObject>();
    addDataPointToJSON(calcObj, "vmg_wind", calculated.vmg_wind);
    addDataPointToJSON(calcObj, "vmg_waypoint", calculated.vmg_waypoint);
    addDataPointToJSON(calcObj, "set", calculated.set);
    addDataPointToJSON(calcObj, "drift", calculated.drift);
    
    // Autopilot
    JsonObject apObj = doc["autopilot"].to<JsonObject>();
    if (autopilot.valid && !autopilot.isStale()) {
        apObj["mode"] = autopilot.mode;
        apObj["status"] = autopilot.status;
        addDataPointToJSON(apObj, "heading_target", autopilot.heading_target);
        addDataPointToJSON(apObj, "wind_angle_target", autopilot.wind_angle_target);
        addDataPointToJSON(apObj, "rudder_angle", autopilot.rudder_angle);
        addDataPointToJSON(apObj, "xte", autopilot.xte);
        apObj["alarm"] = autopilot.alarm;
        apObj["age"] = (millis() - autopilot.timestamp) / 1000.0;
    } else {
        apObj["mode"] = nullptr;
        apObj["status"] = nullptr;
        apObj["age"] = nullptr;
    }
    
    // AIS
    JsonArray aisArray = doc["ais"]["targets"].to<JsonArray>();
    for (int i = 0; i < ais.targetCount; i++) {
        AISTarget& target = ais.targets[i];
        unsigned long age = (millis() - target.timestamp) / 1000;
        
        if (age <= DATA_TIMEOUT_AIS / 1000) {
            JsonObject targetObj = aisArray.add<JsonObject>();
            targetObj["mmsi"] = target.mmsi;
            targetObj["name"] = target.name;
            targetObj["lat"] = target.lat;
            targetObj["lon"] = target.lon;
            targetObj["cog"] = target.cog;
            targetObj["sog"] = target.sog;
            targetObj["heading"] = target.heading;
            targetObj["distance"] = target.distance;
            targetObj["bearing"] = target.bearing;
            targetObj["cpa"] = target.cpa;
            targetObj["tcpa"] = target.tcpa;
            targetObj["age"] = age;
        }
    }
    
    xSemaphoreGive(mutex);
    
    String output;
    serializeJson(doc, output);
    return output;
}

String BoatState::getNavigationJSON() {
    JsonDocument doc;
    
    xSemaphoreTake(mutex, portMAX_DELAY);
    
    // Critical navigation data: position, speeds, depth, COG
    JsonObject position = doc["position"].to<JsonObject>();
    addDataPointToJSON(position, "lat", gps.position.lat);
    addDataPointToJSON(position, "lon", gps.position.lon);
    
    JsonObject root = doc.as<JsonObject>();
    addDataPointToJSON(root, "stw", speed.stw);
    addDataPointToJSON(root, "sog", gps.sog);
    addDataPointToJSON(root, "cog", gps.cog);
    addDataPointToJSON(root, "depth", depth.below_transducer);
    
    xSemaphoreGive(mutex);
    
    String output;
    serializeJson(doc, output);
    return output;
}

String BoatState::getWindJSON() {
    JsonDocument doc;
    
    xSemaphoreTake(mutex, portMAX_DELAY);
    
    JsonObject root = doc.as<JsonObject>();
    addDataPointToJSON(root, "aws", wind.aws);
    addDataPointToJSON(root, "awa", wind.awa);
    addDataPointToJSON(root, "tws", wind.tws);
    addDataPointToJSON(root, "twa", wind.twa);
    addDataPointToJSON(root, "twd", wind.twd);
    
    xSemaphoreGive(mutex);
    
    String output;
    serializeJson(doc, output);
    return output;
}

String BoatState::getAISJSON() {
    JsonDocument doc;
    
    xSemaphoreTake(mutex, portMAX_DELAY);
    
    JsonArray aisArray = doc["targets"].to<JsonArray>();
    for (int i = 0; i < ais.targetCount; i++) {
        AISTarget& target = ais.targets[i];
        unsigned long age = (millis() - target.timestamp) / 1000;
        
        if (age <= DATA_TIMEOUT_AIS / 1000) {
            JsonObject targetObj = aisArray.add<JsonObject>();
            targetObj["mmsi"] = target.mmsi;
            targetObj["name"] = target.name;
            targetObj["lat"] = target.lat;
            targetObj["lon"] = target.lon;
            targetObj["cog"] = target.cog;
            targetObj["sog"] = target.sog;
            targetObj["heading"] = target.heading;
            targetObj["distance"] = target.distance;
            targetObj["bearing"] = target.bearing;
            targetObj["cpa"] = target.cpa;
            targetObj["tcpa"] = target.tcpa;
            targetObj["age"] = age;
        }
    }
    
    xSemaphoreGive(mutex);
    
    String output;
    serializeJson(doc, output);
    return output;
}
