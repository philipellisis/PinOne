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
    if (HidConfig.available()) {
      incomingData[dataLocation] = HidConfig.read();
      if ((dataLocation == 0 && incomingData[0] != firstNumber) || (dataLocation == 1 && (incomingData[1] < bankOffset))) {
        dataLocation = 0;
      } else {
        // wait until we have filled 9 slots of data, then do what needs to be done
        if (dataLocation == 8) {
          if (incomingData[1] == adminNumber) {
            config.lightShowState = LS_DISABLED;
            admin = incomingData[2];
          } else if (incomingData[1] == connectionNumber) {
            ConfigOut.print(connectedString);
            ConfigOut.flush();
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
  if (config.legacySerialDofEnabled) {
    communicateLegacySerial();
  }
  sendAdmin();
}

void Communication::communicateLegacySerial() {
  for (uint8_t i = 0; i < 9; i++) {
    if (!ComSerial.available()) break;
    legacyIncomingData[legacyDataLocation] = ComSerial.read();
    if ((legacyDataLocation == 0 && legacyIncomingData[0] != firstNumber) ||
        (legacyDataLocation == 1 && legacyIncomingData[1] < bankOffset)) {
      legacyDataLocation = 0;
    } else if (legacyDataLocation == 8) {
      if (legacyIncomingData[1] == connectionNumber) {
        ComSerial.print(connectedString);
      } else if (legacyIncomingData[1] >= bankOffset && legacyIncomingData[1] < bankOffset + 9) {
        // matches the HID path's light-show suppression on real DOF activity
        if (config.lightShowState != OUTPUT_RECEIVED) {
          lightShow.setLightsOff();
        }
        config.lightShowState = OUTPUT_RECEIVED_RESET_TIMER;
        updateOutputsFromPacket(legacyIncomingData);
      }
      legacyDataLocation = 0;
    } else {
      legacyDataLocation++;
    }
  }
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
      // Discard any bytes still queued from the GET_CONFIG admin trigger's
      // own OUTPUT report (it's always zero-padded out to the full 63-byte
      // HID payload, even though only 9 bytes were meaningful). Without
      // this, updateConfigFromSerial()'s exact-byte-count reads would
      // consume that padding as if it were the start of the config data,
      // shifting every field read for the rest of the transfer.
      while (HidConfig.available()) { HidConfig.read(); }
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
      ConfigOut.print(connectedString);
      ConfigOut.flush();
      admin = 0;
      break;
    case VERSION:
      ConfigOut.print(F("V,3.0.0\r\n"));
      ConfigOut.flush();
      admin = 0;
      break;
    case RESET:
      ESP.restart();
      admin = 0;
      break;
    case SET_BLE_MAP:
      {
        // Same leftover-padding hazard as GET_CONFIG above.
        while (HidConfig.available()) { HidConfig.read(); }
        uint8_t bleData[64];
        for (uint8_t i = 0; i < 64; i++) {
          uint32_t t1 = millis();
          while (!HidConfig.available() && (millis() - t1 < 5000)) { delay(1); }
          bleData[i] = HidConfig.available() ? HidConfig.read() : 0;
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
  updateOutputsFromPacket(incomingData);
}

void Communication::updateOutputsFromPacket(uint8_t* packet) {
  uint8_t baseIndex = (packet[1] - bankOffset) * 7;
  for (uint8_t i = 2; i < 9; i++) {
    uint8_t outputIndex = baseIndex + i - 2;
    if (previousDOFValues[outputIndex] != packet[i]) {
      outputs.updateOutput(outputIndex, packet[i]);
      previousDOFValues[outputIndex] = packet[i];
    }
  }
}
