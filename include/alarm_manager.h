#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include <Arduino.h>
#include "boat_state.h"
#include "config_manager.h"
#include "seatalk_manager.h"
#include "types.h"

/**
 * @brief Evaluates the depth / AIS proximity / GPS lost alarms and drives
 *        the audible beep on the SeaTalk1 bus.
 *
 * Responsibilities:
 *   1. Periodically evaluate the three alarm conditions from BoatState,
 *      using the thresholds stored in AlarmConfig.
 *   2. Update BoatState's AlarmState (active/acknowledged) accordingly.
 *   3. Trigger a single "beep_on" the moment any alarm becomes active
 *      (edge-triggered — no repeated beeps while the alarm stays active),
 *      and "beep_off" when the operator acknowledges.
 *
 * Thread safety:
 *   update() is intended to be called periodically from a single task
 *   (e.g. the main loop). All BoatState access goes through its own
 *   mutex-protected getters/setters.
 */
class AlarmManager {
public:
    AlarmManager(BoatState* boatState, ConfigManager* configManager,
                 SeatalkManager* seatalkManager);

    /** @brief Load the alarm configuration from NVS. Call once at boot. */
    void init();

    /** @brief Re-evaluate all alarm conditions. Call periodically (e.g. every 1 s). */
    void update();

    /** @brief Return the current in-memory configuration. */
    AlarmConfig getConfig() const { return config; }

    /** @brief Update configuration in memory + persist to NVS. */
    void setConfig(const AlarmConfig& cfg);

    /** @brief Acknowledge all currently active alarms and silence the beep. */
    void acknowledgeAll();

    /** @brief Manually trigger the beep (independent of alarm state). */
    bool beepOn();

    /** @brief Manually silence the beep (independent of alarm state). */
    bool beepOff();

private:
    BoatState*      boatState;
    ConfigManager*  configManager;
    SeatalkManager* seatalkManager;

    AlarmConfig config;

    bool beepActive;  ///< Tracks whether we last sent beep_on (avoids redundant datagrams)

    void evaluateDepth();
    void evaluateAIS();
    void evaluateGPSLost();

    void triggerBeepIfNeeded();
};

#endif // ALARM_MANAGER_H
