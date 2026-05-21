#include "Config.h"
#include <Arduino.h>
#include <Preferences.h>
#include "Globals.h"

static Preferences prefs;

Config::Config() {
}

void Config::init() {
  prefs.begin("pinone", true); // read-only

  bool configured = prefs.getBool("configured", false);

  if (configured) {
    prefs.getBytes("toyOpt", toySpecialOption, 63);
    prefs.getBytes("offState", turnOffState, 63);
    prefs.getBytes("maxOut", maxOutputState, 63);
    prefs.getBytes("maxTime", maxOutputTime, 63);

    plungerMax = prefs.getInt("plgMax", plungerMax);
    plungerMin = prefs.getInt("plgMin", plungerMin);
    plungerMid = prefs.getInt("plgMid", plungerMid);

    prefs.getBytes("solBtnMap", solenoidButtonMap, 4);
    prefs.getBytes("solOutMap", solenoidOutputMap, 4);

    orientation = prefs.getUChar("orient", orientation);
    accelerometer = prefs.getUChar("accel", accelerometer);
    accelerometerEprom = accelerometer;

    accelerometerSensitivity = prefs.getUChar("accelSens", accelerometerSensitivity);
    accelerometerDeadZone = prefs.getInt("accelDZ", accelerometerDeadZone);

    plungerButtonPush = prefs.getUChar("plgBtnPush", plungerButtonPush);

    accelerometerTilt = prefs.getInt("accelTilt", accelerometerTilt);
    accelerometerMax = prefs.getInt("accelMax", accelerometerMax);

    plungerAverageRead = prefs.getUChar("plgAvgRead", plungerAverageRead);
    nightModeButton = prefs.getUChar("nightBtn", nightModeButton);
    plungerLaunchButton = prefs.getUChar("plgLaunch", plungerLaunchButton);
    tiltButton = prefs.getUChar("tiltBtn", tiltButton);
    shiftButton = prefs.getUChar("shiftBtn", shiftButton);

    disableAccelOnPlungerMove = prefs.getBool("disAccelPlg", disableAccelOnPlungerMove);
    enablePlungerQuickRelease = prefs.getUChar("plgQR", enablePlungerQuickRelease);
    disablePlungerWhenNotInUse = prefs.getBool("disPlgIdle", disablePlungerWhenNotInUse);
    disableButtonPressWhenKeyboardEnabled = prefs.getBool("kbdMode", disableButtonPressWhenKeyboardEnabled);

    accelerometerTiltY = prefs.getInt("accelTiltY", accelerometerTiltY);
    accelerometerMaxY = prefs.getInt("accelMaxY", accelerometerMaxY);

    prefs.getBytes("btnKbd", buttonKeyboard, 32);

    prefs.getBytes("btnDebounce", buttonKeyDebounce, 24);
    buttonDebounceCounter = prefs.getUChar("dbncCount", buttonDebounceCounter);
    enablePlunger = prefs.getBool("enPlunger", enablePlunger);

    tiltSuppress = prefs.getUChar("tiltSupp", tiltSuppress);
    lightShowAttractEnabled = prefs.getBool("lsAttract", lightShowAttractEnabled);

    lightShowTime = prefs.getUChar("lsTime", lightShowTime);
    reverseButtonOutputPolarity = prefs.getBool("revBtnPol", reverseButtonOutputPolarity);
    disableUSBSuspend = prefs.getBool("disUSBSusp", disableUSBSuspend);
    bluetoothEnable = prefs.getBool("bleEnable", bluetoothEnable);
    debug = prefs.getBool("debug", debug);
    prefs.getBytes("btnRemap", buttonRemap, 32);
    tiltButtonUp = prefs.getUChar("tiltBtnUp", tiltButtonUp);
    tiltButtonDown = prefs.getUChar("tiltBtnDn", tiltButtonDown);
    tiltButtonLeft = prefs.getUChar("tiltBtnL", tiltButtonLeft);
    tiltButtonRight = prefs.getUChar("tiltBtnR", tiltButtonRight);

    irOutputPin = prefs.getUChar("irOutPin", irOutputPin);
    irProtocol = prefs.getUChar("irProto", irProtocol);
    irCode = prefs.getULong("irCode", irCode);
    irBits = prefs.getUChar("irBits", irBits);
    irButton = prefs.getUChar("irButton", irButton);
    lowLatencyMode = prefs.getBool("lowLatency", lowLatencyMode);
    plungerRestingDeadZone = prefs.getUChar("plgRestDZ", plungerRestingDeadZone);

  } else {
    prefs.end();
    //save default config in case it's never been done before
    saveConfig();
    return;
  }

  prefs.end();
}

void Config::saveConfig() {
    prefs.begin("pinone", false); // read-write

    prefs.putBytes("toyOpt", toySpecialOption, 63);
    prefs.putBytes("offState", turnOffState, 63);
    prefs.putBytes("maxOut", maxOutputState, 63);
    prefs.putBytes("maxTime", maxOutputTime, 63);

    prefs.putInt("plgMax", plungerMax);
    prefs.putInt("plgMin", plungerMin);
    prefs.putInt("plgMid", plungerMid);

    prefs.putBytes("solBtnMap", solenoidButtonMap, 4);
    prefs.putBytes("solOutMap", solenoidOutputMap, 4);

    prefs.putUChar("orient", orientation);
    prefs.putUChar("accel", accelerometer);

    prefs.putUChar("accelSens", accelerometerSensitivity);
    prefs.putInt("accelDZ", accelerometerDeadZone);

    prefs.putUChar("plgBtnPush", plungerButtonPush);
    prefs.putInt("accelTilt", accelerometerTilt);
    prefs.putInt("accelMax", accelerometerMax);

    prefs.putUChar("plgAvgRead", plungerAverageRead);
    prefs.putUChar("nightBtn", nightModeButton);
    prefs.putUChar("plgLaunch", plungerLaunchButton);
    prefs.putUChar("tiltBtn", tiltButton);
    prefs.putUChar("shiftBtn", shiftButton);

    prefs.putBool("disAccelPlg", disableAccelOnPlungerMove);
    prefs.putUChar("plgQR", enablePlungerQuickRelease);
    prefs.putBool("disPlgIdle", disablePlungerWhenNotInUse);
    prefs.putBool("kbdMode", disableButtonPressWhenKeyboardEnabled);

    prefs.putInt("accelTiltY", accelerometerTiltY);
    prefs.putInt("accelMaxY", accelerometerMaxY);

    prefs.putBytes("btnKbd", buttonKeyboard, 32);

    prefs.putBytes("btnDebounce", buttonKeyDebounce, 24);

    prefs.putUChar("dbncCount", buttonDebounceCounter);
    prefs.putBool("enPlunger", enablePlunger);

    prefs.putUChar("tiltSupp", tiltSuppress);
    prefs.putBool("lsAttract", lightShowAttractEnabled);

    prefs.putUChar("lsTime", lightShowTime);
    prefs.putBool("revBtnPol", reverseButtonOutputPolarity);
    prefs.putBool("disUSBSusp", disableUSBSuspend);
    prefs.putBool("bleEnable", bluetoothEnable);
    prefs.putBool("debug", debug);
    prefs.putBytes("btnRemap", buttonRemap, 32);
    prefs.putUChar("tiltBtnUp", tiltButtonUp);
    prefs.putUChar("tiltBtnDn", tiltButtonDown);
    prefs.putUChar("tiltBtnL", tiltButtonLeft);
    prefs.putUChar("tiltBtnR", tiltButtonRight);

    prefs.putUChar("irOutPin", irOutputPin);
    prefs.putUChar("irProto", irProtocol);
    prefs.putULong("irCode", irCode);
    prefs.putUChar("irBits", irBits);
    prefs.putUChar("irButton", irButton);
    prefs.putBool("lowLatency", lowLatencyMode);
    prefs.putUChar("plgRestDZ", plungerRestingDeadZone);

    prefs.putBool("configured", true);
    prefs.end();
}

void Config::updateConfigFromSerial() {
    done = 0;
    // get first 62 bank maximum values
    readConfigArray(toySpecialOption, 63);
    // get next 62 bank maximum values
    readConfigArray(turnOffState, 63);
    // get next 62 bank maximum values
    readConfigArray(maxOutputState, 63);
    // get next 62 bank maximum values
    readConfigArray(maxOutputTime, 63);

    plungerMax = readIntFromByte();
    plungerMin = readIntFromByte();
    plungerMid = readIntFromByte();
    readConfigArray(solenoidButtonMap, 4);
    readConfigArray(solenoidOutputMap, 4);

    orientation = blockRead();
    accelerometer = blockRead();
    accelerometerSensitivity = blockRead();
    accelerometerDeadZone = readIntFromByte();
    plungerButtonPush = blockRead();
    accelerometerTilt = readIntFromByte();
    accelerometerMax = readIntFromByte();
    plungerAverageRead = blockRead();
    nightModeButton = blockRead();
    plungerLaunchButton = blockRead();
    tiltButton = blockRead();
    shiftButton = blockRead();

    readConfigArray(buttonKeyboard, 32);

    disableAccelOnPlungerMove = blockRead();
    enablePlungerQuickRelease = blockRead();
    disablePlungerWhenNotInUse = blockRead();
    disableButtonPressWhenKeyboardEnabled = blockRead();
    accelerometerTiltY = readIntFromByte();
    accelerometerMaxY = readIntFromByte();


    readConfigArray(buttonKeyDebounce, 24);
    buttonDebounceCounter = blockRead();
    enablePlunger = blockRead();

    tiltSuppress = blockRead();
    lightShowAttractEnabled = blockRead();
    lightShowTime = blockRead();
    reverseButtonOutputPolarity = blockRead();
    disableUSBSuspend = blockRead();
    bluetoothEnable = blockRead();
    debug = blockRead();
    readConfigArray(buttonRemap, 32);
    tiltButtonUp = blockRead();
    tiltButtonDown = blockRead();
    tiltButtonLeft = blockRead();
    tiltButtonRight = blockRead();

    irOutputPin = blockRead();
    irProtocol = blockRead();
    irCode = (uint32_t)blockRead() << 24;
    irCode |= (uint32_t)blockRead() << 16;
    irCode |= (uint32_t)blockRead() << 8;
    irCode |= blockRead();
    irBits = blockRead();
    irButton = blockRead();
    lowLatencyMode = blockRead();
    plungerRestingDeadZone = blockRead();

    if(blockRead() != 42) {
      done = 1;
    }

    if (done > 0) {
      printError();
    } else {
      saveConfig();
      printSuccess();
    }
}


void Config::printError() {
  ComSerial.print(F("R,E\r\n"));
}

void Config::sendConfig() {
    ComSerial.print(F("C,"));

    printConfigArray(toySpecialOption, 63);
    printConfigArray(turnOffState, 63);
    printConfigArray(maxOutputState, 63);
    printConfigArray(maxOutputTime, 63);

    printIntComma(plungerMax);
    printIntComma(plungerMin);
    printIntComma(plungerMid);

    printConfigArray(solenoidButtonMap, 4);
    printConfigArray(solenoidOutputMap, 4);

    printComma(orientation);
    printComma(accelerometer);
    printComma(accelerometerSensitivity);
    printIntComma(accelerometerDeadZone);
    printComma(plungerButtonPush);
    printIntComma(accelerometerTilt);
    printIntComma(accelerometerMax);
    printComma(plungerAverageRead);
    printComma(nightModeButton);
    printComma(plungerLaunchButton);
    printComma(tiltButton);
    printComma(shiftButton);
    printConfigArray(buttonKeyboard, 32);

    printComma(disableAccelOnPlungerMove);
    printComma(enablePlungerQuickRelease);
    printComma(disablePlungerWhenNotInUse);
    printComma(disableButtonPressWhenKeyboardEnabled);
    printIntComma(accelerometerTiltY);
    printIntComma(accelerometerMaxY);

    printConfigArray(buttonKeyDebounce, 24);
    printComma(buttonDebounceCounter);
    printComma(enablePlunger);

    printComma(tiltSuppress);
    printComma(lightShowAttractEnabled);
    printComma(lightShowTime);
    printComma(reverseButtonOutputPolarity);
    printComma(disableUSBSuspend);
    printComma(bluetoothEnable);
    printComma(debug);
    printConfigArray(buttonRemap, 32);
    printComma(tiltButtonUp);
    printComma(tiltButtonDown);
    printComma(tiltButtonLeft);
    printComma(tiltButtonRight);

    printComma(irOutputPin);
    printComma(irProtocol);
    printComma((unsigned char)(irCode >> 24));
    printComma((unsigned char)(irCode >> 16));
    printComma((unsigned char)(irCode >> 8));
    printComma((unsigned char)(irCode & 0xFF));
    printComma(irBits);
    printComma(irButton);
    printComma(lowLatencyMode);
    printComma(plungerRestingDeadZone);

    ComSerial.print(F("E\r\n"));
}

void Config::printComma(unsigned char value) {
  ComSerial.print(value);
  ComSerial.print(F(","));
}

void Config::printIntComma(int value) {
  ComSerial.print(value);
  ComSerial.print(F(","));
}

void Config::printSuccess() {
  ComSerial.print(F("R,S\r\n"));
}

unsigned char Config::blockRead() {
    if (done > 0) {
      return 0;
    }
    uint32_t t1 = millis();
    uint32_t t2 = millis();
    while ((t2 - t1) < 5000) {
      if (ComSerial.available() > 0) {
        return ComSerial.read();
      }
      t2 = millis();
      delay(50);
    }
    done = 1;
    return 0;
}

int16_t Config::readIntFromByte() {
  return (blockRead() << 8) + blockRead();
}

void Config::readConfigArray(unsigned char* configArray, unsigned char size) {
  for (uint8_t i = 0; i < size; i++) {
    configArray[i] = blockRead();
  }
}

void Config::printConfigArray(unsigned char* configArray, unsigned char size) {
  for (uint8_t i = 0; i < size; i++) {
    printComma(configArray[i]);
  }
}
