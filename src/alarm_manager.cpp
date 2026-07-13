#include "alarm_manager.h"
#include "functions.h"

AlarmManager::AlarmManager(BoatState* bs, ConfigManager* cm, SeatalkManager* stMgr)
    : boatState(bs), configManager(cm), seatalkManager(stMgr), beepActive(false) {
}

void AlarmManager::init() {
    if (configManager) {
        configManager->getAlarmConfig(config);
    }
    serialPrintf("[Alarm] Config loaded — enabled=%s depth(%s,%.1fm) ais(%s,%.1fnm,mmsi=%u) gps_lost(%s,%us)\n",
                 config.alarms_enabled ? "yes" : "no",
                 config.depth_enabled ? "on" : "off", config.depth_threshold_m,
                 config.ais_enabled ? "on" : "off", config.ais_distance_nm, config.own_mmsi,
                 config.gps_lost_enabled ? "on" : "off", config.gps_lost_timeout_s);
}

void AlarmManager::setConfig(const AlarmConfig& cfg) {
    config = cfg;
    // Clamp AIS distance to the nearest 0.1 nm step
    config.ais_distance_nm = roundf(config.ais_distance_nm * 10.0f) / 10.0f;
    if (config.ais_distance_nm < 0.1f)  config.ais_distance_nm = 0.1f;
    if (config.depth_threshold_m < 0.1f) config.depth_threshold_m = 0.1f;
    if (config.gps_lost_timeout_s < 1)   config.gps_lost_timeout_s = 1;

    if (configManager) {
        configManager->setAlarmConfig(config);
    }
    serialPrintf("[Alarm] Config updated\n");
}

void AlarmManager::update() {
    if (!boatState || !config.alarms_enabled) return;

    evaluateDepth();
    evaluateAIS();
    evaluateGPSLost();
    triggerBeepIfNeeded();
}

void AlarmManager::evaluateDepth() {
    if (!config.depth_enabled) {
        boatState->setDepthAlarmActive(false);
        return;
    }

    DepthData depth = boatState->getDepth();
    bool triggered = depth.below_transducer.valid &&
                     !depth.below_transducer.isStale() &&
                     depth.below_transducer.value <= config.depth_threshold_m;

    boatState->setDepthAlarmActive(triggered);
}

void AlarmManager::evaluateAIS() {
    if (!config.ais_enabled) {
        boatState->setAISAlarmActive(false);
        return;
    }

    AISData ais = boatState->getAIS();
    bool triggered = false;
    uint32_t closestMmsi = 0;
    float closestDistance = 1e9f;

    for (int i = 0; i < ais.targetCount; i++) {
        AISTarget& t = ais.targets[i];
        if (t.mmsi == config.own_mmsi) continue;  // exclude own vessel
        unsigned long age = (millis() - t.timestamp) / 1000;
        if (age > DATA_TIMEOUT_AIS / 1000) continue;

        if (t.distance <= config.ais_distance_nm && t.distance < closestDistance) {
            triggered       = true;
            closestDistance = t.distance;
            closestMmsi     = t.mmsi;
        }
    }

    boatState->setAISAlarmActive(triggered, closestMmsi);
}

void AlarmManager::evaluateGPSLost() {
    if (!config.gps_lost_enabled) {
        boatState->setGPSLostAlarmActive(false);
        return;
    }

    GPSData gps = boatState->getGPS();
    unsigned long timeoutMs = (unsigned long)config.gps_lost_timeout_s * 1000UL;

    bool triggered = !gps.position.lat.valid ||
                      gps.position.lat.isStale(timeoutMs);

    boatState->setGPSLostAlarmActive(triggered);
}

void AlarmManager::triggerBeepIfNeeded() {
    AlarmState state = boatState->getAlarmState();
    bool shouldBeep = state.anyUnacked();

    if (shouldBeep && !beepActive) {
        beepOn();
    } else if (!shouldBeep && beepActive) {
        beepOff();
    }
}

void AlarmManager::acknowledgeAll() {
    if (!boatState) return;
    boatState->acknowledgeAllAlarms();
    beepOff();
}

bool AlarmManager::beepOn() {
    beepActive = true;
    if (!seatalkManager) return false;
    return seatalkManager->sendExtraCommand("beep_on");
}

bool AlarmManager::beepOff() {
    beepActive = false;
    if (!seatalkManager) return false;
    return seatalkManager->sendExtraCommand("beep_off");
}
