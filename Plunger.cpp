#include "Plunger.h"
#include <Arduino.h>
#include "UsbHid.h"
#include "Globals.h"
#include "Pins.h"

// Plunger output uses the same full signed 16-bit range as the accelerometer axes
static const float PLUNGER_AXIS_SCALE = 32768.0f;
static const float PLUNGER_AXIS_MAX   = 32767.0f;


Plunger::Plunger() {
  pinMode(PIN_PLUNGER, INPUT); // plunger (analog ADC input)

  restingStartTime = millis(); // Initialize the timestamp
}

void Plunger::init() {
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);
  resetPlunger();
  priorTime = millis();
}

void Plunger::resetPlunger() {
  plungerScaleFactor = (float)(config.plungerMid - config.plungerMin) / (float)(config.plungerMax - config.plungerMid);
  restingStartTime = millis(); // Reset the resting start time
}

void Plunger::plungerRead() {
  if (config.enablePlunger == false) {
    updateGamepadZAxis(0);
    return;
  }

  // Readings are in millivolts (0-3300). Averaging is done in floating point so
  // the sub-mV fraction survives and the mapping can use it.
  float sensorValue = 0;
  int16_t newReading;

  // Remove any sensor value that does not agree with the prior value (+/-32 mV)
  uint8_t goodReadings = 0;
  for (uint8_t i = 0; i < 5; i++) {
    newReading = (int16_t)analogReadMilliVolts(PIN_PLUNGER);
    if (newReading < truePriorValue + 32 && newReading > truePriorValue - 32) {
      goodReadings++;
      sensorValue += newReading;
    }
  }
  if (goodReadings > 0) {
    sensorValue = sensorValue / (goodReadings);
  } else {
    sensorValue = newReading;
  }

  if (config.plungerMoving == true || config.disablePlungerWhenNotInUse == 0) {
    for (uint8_t i = 0; i < config.plungerAverageRead; i++) {
      sensorValue += (int16_t)analogReadMilliVolts(PIN_PLUNGER);
    }
    sensorValue = sensorValue / (config.plungerAverageRead + 1);
  }
  truePriorValue = sensorValue;

  // this checks that the plunger is sitting stationary. If so, it will enable the accelerometer. It also checks if there is nothing connected. to ensure the accelerometer still works even if the plunger is disconnected
  uint32_t currentTime = millis();

  if ((sensorValue < config.plungerMid + config.plungerRestingDeadZone && sensorValue > config.plungerMid - config.plungerRestingDeadZone) || sensorValue < 32) {
    if (currentTime - restingStartTime >= config.restingStateMax) {
      // Plunger is in the resting state when the timer exceeds restingStateMax
      config.plungerMoving = false;
    }
  } else {
    // Reset the timer if the plunger is moving
    restingStartTime = currentTime;
    config.plungerMoving = true;
  }

  // Handle plunger button push logic for max position
  if (config.plungerButtonPush == 1 || config.plungerButtonPush == 3) {
    updateButtonState(buttonState, sensorValue >= config.plungerMax - 64, true);
    updateButtonState(buttonState, sensorValue < config.plungerMax - 64, false);
  }

  // Handle plunger button push logic for min position
  if (config.plungerButtonPush >= 2) {
    updateButtonState(buttonState2, sensorValue <= config.plungerMin + 32, true);
    updateButtonState(buttonState2, sensorValue > config.plungerMin + 32, false);
  }

  // Map the calibrated reading to -32767..32767. Readings beyond the calibrated
  // min/max clamp to full scale.
  float fraction;
  if (sensorValue <= config.plungerMid) {
    fraction = -(1 - (float)(sensorValue - config.plungerMin) / (config.plungerMid - config.plungerMin));
  } else {
    fraction = (float)(sensorValue - config.plungerMid) / (config.plungerMax - config.plungerMid);
  }
  float scaled = fraction * PLUNGER_AXIS_SCALE;
  if (scaled > PLUNGER_AXIS_MAX) scaled = PLUNGER_AXIS_MAX;
  if (scaled < -PLUNGER_AXIS_MAX) scaled = -PLUNGER_AXIS_MAX;
  adjustedValue = static_cast<int16_t>(scaled);


  int16_t currentDelayedValue = getDelayedPlungerValue(adjustedValue, currentTime);

  if (priorValue != currentDelayedValue) {
    if (config.plungerMoving) {
      if (adjustedValue > 0 && !plungerReleased) {
        currentPlungerMax = currentDelayedValue;
      }
      updateGamepadZAxis(currentDelayedValue, true);
    } else {
      currentPlungerMax = 0;
      plungerReleased = false;
      if (config.disablePlungerWhenNotInUse == 1) {
        updateGamepadZAxis(0);
      } else {
        updateGamepadZAxis(currentDelayedValue, true);
      }
    }
    priorValue = currentDelayedValue;
  }

  plungerData[plungerDataCounter] = adjustedValue;
  plungerDataTime[plungerDataCounter] = (uint8_t)(currentTime - priorTime);
  plungerDataCounter = (plungerDataCounter + 1) % 35;
  priorTime = currentTime;
}

int16_t Plunger::getDelayedPlungerValue(int16_t sensorValue, uint32_t currentTime) {

  if (config.enablePlungerQuickRelease == 0) {
    return sensorValue;
  }
  if (config.plungerMoving == false && plungerReleased == true) {
    plungerReleased = false;
    config.updateUSB = true;
    return 0;
  }
  if ((sensorValue < 0 && config.plungerMoving == true && currentPlungerMax > 0 && truePriorValue > 161) || plungerReleased == true) {
    if (plungerReleased == false) {
      config.lastButtonState[config.plungerLaunchButton] = buttons.sendButtonPush(config.plungerLaunchButton, 1);
    } else {
      config.lastButtonState[config.plungerLaunchButton] = buttons.sendButtonPush(config.plungerLaunchButton, 0);
    }
    plungerReleased = true;
    return 0;
  }

  // 100ms delay
  uint16_t accumulatedTime = 0;
  int8_t index = (plungerDataCounter - 1 + 35) % 35;

  while (accumulatedTime < config.enablePlungerQuickRelease && index != plungerDataCounter) {
      accumulatedTime += plungerDataTime[index];
      index = (index - 1 + 35) % 35;
  }

  return plungerData[index];
}

void Plunger::updateButtonState(uint8_t& buttonState, bool condition, bool pressed) {
  if (condition && buttonState != pressed) {
    buttonState = buttons.sendButtonPush(config.plungerLaunchButton, pressed);
    config.lastButtonState[config.plungerLaunchButton] = buttonState;
  }
}

void Plunger::updateGamepadZAxis(int16_t value, bool forceUpdate) {
  Gamepad1.zAxis(value);
  if (forceUpdate) {
    config.updateUSB = true;
  }
}

void Plunger::sendPlungerState() {
  ConfigOut.print(F("P,"));
  ConfigOut.print((int)(truePriorValue + 0.5f));
  ConfigOut.print(F("\r\n"));
  ConfigOut.flush();
}
