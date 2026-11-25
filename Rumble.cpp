#include "Rumble.h"
#include "Globals.h"

void RumbleOutputs::apply(uint8_t left, uint8_t right, uint8_t maxMagnitude) {
    uint8_t rightOutput = config.bleRumbleRightOutput;
    // Both motors on one output: drive it from whichever is stronger
    if (rightOutput == config.bleRumbleLeftOutput) {
        left = max(left, right);
        rightOutput = 0;
    }
    applyOne(left, maxMagnitude, config.bleRumbleLeftOutput, _left);
    applyOne(right, maxMagnitude, rightOutput, _right);
}

// Scaled to 0-255 in PWM mode, otherwise fully on once the magnitude reaches the
// threshold (a percentage). Only changes are sent, as each one restarts the
// output's max-on-time (solenoid life extender) timer.
void RumbleOutputs::applyOne(uint8_t magnitude, uint8_t maxMagnitude, uint8_t output, Applied& applied) {
    uint8_t value;
    if (config.bleRumblePwm) {
        value = ((uint16_t)magnitude * 255 + maxMagnitude / 2) / maxMagnitude;
    } else {
        value = (uint16_t)magnitude * 100 >= (uint16_t)config.bleRumbleThreshold * maxMagnitude ? 255 : 0;
    }
    if (output != applied.output) {
        // Reassigned by the config tool: don't leave the old output running
        if (applied.output > 0 && applied.value > 0) {
            outputs.requestOutput(applied.output - 1, 0);
        }
        applied.output = output;
        applied.value  = 0;
    }
    if (output == 0 || value == applied.value) return;
    outputs.requestOutput(output - 1, value);
    applied.value = value;
}
