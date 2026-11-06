#include "Communication.h"
#include <Arduino.h>
#include "Plunger.h"
#include "UsbHid.h"
#include "Buttons.h"
#include "Accelerometer.h"
#include "Enums.h"
#include "Globals.h"
#include "Threading.h"

Communication::Communication() {
}



void Communication::communicate() {
  outputs.checkResetOutputs();
  // Read everything that has arrived (up to a limit so the light show and the
  // rest of this task keep running). The old limit of 9 bytes per pass took
  // about 7 ms for each 63 byte HID report, so a burst from the PC overflowed
  // the receive queue and bytes were lost in the middle of reports.
  for (uint16_t n = 0; n < MAX_BYTES_PER_PASS && HidConfig.available(); n++) {
    incomingData[dataLocation] = HidConfig.read();
    if (dataLocation == 0 && incomingData[0] != firstNumber) {
      continue; // not the start of a packet
    }
    if (dataLocation == 1 && !validSecondByte(incomingData[1])) {
      // Not a packet after all. The byte may itself be the start of the real
      // packet (the reports are padded with zeros), so do not throw it away.
      dataLocation = (incomingData[1] == firstNumber) ? 1 : 0;
      continue;
    }
    // wait until we have filled 9 slots of data, then do what needs to be done
    if (dataLocation == 8) {
      uint8_t kind = incomingData[1];
      if (kind == adminNumber) {
        config.lightShowState = LS_DISABLED;
        admin = incomingData[2];
      } else if (kind == connectionNumber) {
        ConfigOut.print(connectedString);
        ConfigOut.flush();
      } else if (kind == outputSingleNumber) {
        if (incomingData[2] < 63) {
          outputs.updateOutput(incomingData[2], incomingData[3]);
        }
      } else if (kind >= bankOffset && kind < bankOffset + 9) {
        //normal operation
        if (config.lightShowState != OUTPUT_RECEIVED) {
          lightShow.setLightsOff();
        }
        config.lightShowState = OUTPUT_RECEIVED_RESET_TIMER;
        updateOutputs();
      }
      dataLocation = 0;
      // Stop at an admin command so sendAdmin() runs before anything that follows it.
      // GET_CONFIG's padding and the config data come right behind the command, and
      // the parser must not eat them.
      if (kind == adminNumber) break;
    } else {
      dataLocation++;
    }
  }
  if (config.legacySerialDofEnabled) {
    communicateLegacySerial();
  }
  sendAdmin();
}

void Communication::communicateLegacySerial() {
  for (uint8_t i = 0; i < 64; i++) {
    if (!ComSerial.available()) break;
    legacyIncomingData[legacyDataLocation] = ComSerial.read();
    if (legacyDataLocation == 0 && legacyIncomingData[0] != firstNumber) {
      continue;
    }
    if (legacyDataLocation == 1 && !validSecondByte(legacyIncomingData[1])) {
      legacyDataLocation = (legacyIncomingData[1] == firstNumber) ? 1 : 0;
      continue;
    }
    if (legacyDataLocation == 8) {
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
      {
        // Discard the rest of the GET_CONFIG admin trigger's own OUTPUT report
        // (it's always zero-padded out to the full 63-byte HID payload, even
        // though only 9 bytes were meaningful). Without this,
        // receiveConfig()'s exact-byte-count reads would consume that
        // padding as if it were the start of the config data, shifting every
        // field read for the rest of the transfer. Only that known amount is
        // skipped, and before waiting on the input pass, so config bytes that
        // have already started arriving (e.g. if the lock below is slow to
        // come free) are never thrown away with it.
        const uint8_t COMMAND_PADDING = 63 - 9;
        for (uint8_t i = 0; i < COMMAND_PADDING && HidConfig.available(); i++) {
          HidConfig.read();
        }
        // Read and check the whole transfer first, with the input pass still running.
        // A bad transfer is rejected here and leaves the live config as it was.
        if (!config.receiveConfig()) {
          admin = 0;
          break;
        }
        // Pause the input pass while config is rewritten, so it never reads
        // a half-updated profile or accelerometer setting
        ScopedLock inputPause(inputMutex);
        config.updateConfigFromSerial();
        // Button map is part of the main config now; pick up the new mapping
        bleController.applyConfig();
        plunger.resetPlunger();
        config.accelerometerEprom = config.accelerometer;
        if (config.accelerometerEprom > 0) {
          accel.init();
        }
        // The axis assignments may have changed: clear the USB axes so none is
        // left holding a stale value, then have each input resend its own
        Gamepad1.resetAxes();
        plunger.refreshAxis();
        accel.refreshAxes();
        config.updateUSB = true;
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
      ConfigOut.print(F("V," FIRMWARE_VERSION "\r\n"));
      ConfigOut.flush();
      admin = 0;
      break;
    case CLEAR_BLE_BONDS:
      bleController.clearBonds();
      admin = 0;
      break;
    case RESET:
      ESP.restart();
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
  if (packet[1] < bankOffset || packet[1] >= bankOffset + 9) return;
  uint8_t baseIndex = (packet[1] - bankOffset) * 7;
  for (uint8_t i = 2; i < 9; i++) {
    uint8_t outputIndex = baseIndex + i - 2;
    if (outputIndex >= 63) break;
    // skips values DOF already asked for, but never when something else (light
    // show, button, max on time) changed the output since
    outputs.updateOutputFromDof(outputIndex, packet[i]);
  }
}

// The second byte of a packet is a bank (200-208) or one of the command numbers.
bool Communication::validSecondByte(uint8_t b) {
  return (b >= bankOffset && b < bankOffset + 9) || b == adminNumber || b == connectionNumber ||
         b == outputSingleNumber || b == outputButtonNumber;
}
