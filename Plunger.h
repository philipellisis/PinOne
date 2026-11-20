#ifndef PLUNGER_H
#define PLUNGER_H
#include <Arduino.h>
#include "UsbHid.h"
#include "Config.h"

class Plunger {

  public:
    Plunger();
    void init();
    void plungerRead();
    void sendPlungerState();
    void resetPlunger();
    int16_t getAdjustedValue() const { return adjustedValue; }
    // The value last put on the USB gamepad axis: 0 at rest (when the plunger is disabled when not in use),
    // 0 after a quick release and delayed like the USB axis. BLE reports it too, so both links behave the same.
    int16_t getReportedValue() const { return reportedValue; }
    // Make the next read resend the plunger axis (after the axis assignment changes)
    void refreshAxis() { priorValue = INT16_MIN; }

  private:
    bool plungerReleased = false;
    float plungerScaleFactor;
    int16_t adjustedValue;
    int16_t reportedValue = 0;
    uint8_t buttonState;
    uint8_t buttonState2;
    int16_t priorValue = 0;
    float truePriorValue = 0;
    uint32_t restingStartTime;
    uint32_t priorTime = 0;

    int16_t plungerData[35] = {0};
    uint8_t plungerDataTime[35] = {0};
    int8_t plungerDataCounter = 0;
    int16_t getDelayedPlungerValue(int16_t sensorValue, uint32_t currentTime);
    int16_t currentPlungerMax = 0;
    void updateButtonState(uint8_t& buttonState, bool condition, bool pressed);
    void updateGamepadAxis(int16_t value, bool forceUpdate = false);

};

#endif
