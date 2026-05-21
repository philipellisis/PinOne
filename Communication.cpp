#include "Communication.h"
#include <Arduino.h>
#include "Plunger.h"
#include "UsbHid.h"
#include "Buttons.h"
#include "Accelerometer.h"
#include "Enums.h"
#include "Globals.h"

Communication::Communication() {
}



void Communication::communicate() {
  outputs.checkResetOutputs();
  for (uint8_t i = 0; i < 9; i++) {
    if (Serial.available()) {
      incomingData[dataLocation] = Serial.read();
      if ((dataLocation == 0 && incomingData[0] != firstNumber) || (dataLocation == 1 && (incomingData[1] < bankOffset))) {
        dataLocation = 0;
      } else {
        // wait until we have filled 9 slots of data, then do what needs to be done
        if (dataLocation == 8) {
          if (incomingData[1] == adminNumber) {
            config.lightShowState = LS_DISABLED;
            admin = incomingData[2];
          } else if (incomingData[1] == connectionNumber) {
            Serial.print(connectedString);
          } else if (incomingData[1] == outputSingleNumber) {
            outputs.updateOutput(incomingData[2], incomingData[3]);
          } else {
            //normal operation
            if (config.lightShowState != OUTPUT_RECEIVED) {
              lightShow.setLightsOff();
            }
            config.lightShowState = OUTPUT_RECEIVED_RESET_TIMER;
            updateOutputs();

          }
          dataLocation = 0;
        } else {
          dataLocation++;
        }
      }
    } else {
      break;
    }
  }
  sendAdmin();
}

void Communication::sendAdmin() {

  if (admin > 0) {

    switch (admin)
    {
    case BUTTONS:
    case OUTPUTS:
    case PLUNGER:
    case ACCEL:
      handleDelayedAdmin(admin);
      break;
    case SEND_CONFIG:
      config.sendConfig();
      admin = 0;
      break;
    case GET_CONFIG:
      config.updateConfigFromSerial();
      plunger.resetPlunger();
      config.accelerometerEprom = config.accelerometer;
      if (config.accelerometerEprom > 0) {
        accel.init();
      }
      admin = 0;
      break;
    case OFF:
      admin = 0;
      config.lightShowState = OUTPUT_RECEIVED_RESET_TIMER;
      outputs.turnOff();
      break;
    case CONNECT:
      Serial.print(connectedString);
      admin = 0;
      break;
    case VERSION:
      Serial.print(F("V,3.0.0\r\n"));
      admin = 0;
      break;
    case RESET:
      ESP.restart();
      admin = 0;
      break;
    case SET_BLE_MAP:
      {
        uint8_t bleData[64];
        for (uint8_t i = 0; i < 64; i++) {
          uint32_t t1 = millis();
          while (!Serial.available() && (millis() - t1 < 5000)) { delay(1); }
          bleData[i] = Serial.available() ? Serial.read() : 0;
        }
        // Apply button map directly to BLE controller
        bleController.updateButtonMap(bleData, 32);
        // Apply device name directly to BLE controller
        bleController.updateDeviceName(bleData + 32, 32);
      }
      admin = 0;
      break;
    }

  }
}

void Communication::handleDelayedAdmin(uint8_t adminType) {
  if (shouldDelay()) return;

  switch (adminType) {
    case BUTTONS:
      buttons.sendButtonState();
      break;
    case OUTPUTS:
      outputs.sendOutputState();
      break;
    case PLUNGER:
      plunger.sendPlungerState();
      break;
    case ACCEL:
      accel.sendAccelerometerState();
      break;
  }
}

bool Communication::shouldDelay() {
  if (delayIncrementor < 20) {
    delayIncrementor++;
    return true;
  } else {
    delayIncrementor = 0;
    return false;
  }
}

void Communication::updateOutputs() {
  uint8_t baseIndex = (incomingData[1] - bankOffset) * 7;
  for (uint8_t i = 2; i < 9; i++) {
    uint8_t outputIndex = baseIndex + i - 2;
    if (previousDOFValues[outputIndex] != incomingData[i]) {
      outputs.updateOutput(outputIndex, incomingData[i]);
      previousDOFValues[outputIndex] = incomingData[i];
    }
  }
}
