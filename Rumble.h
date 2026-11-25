#pragma once

#include <Arduino.h>

// Drives the outputs chosen for an Xbox controller's two rumble motors
// (config.bleRumble*: outputs, PWM or on/off, on threshold). Shared by the
// Bluetooth Xbox profile and USB XInput mode.
class RumbleOutputs {
public:
    // left = strong (low frequency) motor, right = weak (high frequency) motor,
    // each 0..maxMagnitude (100 over Bluetooth, 255 over USB XInput).
    // Only changes are sent to the outputs. Call every pass.
    void apply(uint8_t left, uint8_t right, uint8_t maxMagnitude);

private:
    // The output (1-based, 0 = none) a motor last drove and the value it set
    struct Applied {
        uint8_t output = 0;
        uint8_t value  = 0;
    };
    Applied _left;
    Applied _right;
    void applyOne(uint8_t magnitude, uint8_t maxMagnitude, uint8_t output, Applied& applied);
};
