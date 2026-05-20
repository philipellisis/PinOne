#include "UsbHid.h"
#include <Wire.h>
#include "Globals.h"

Plunger plunger;
Buttons buttons;
LightShow lightShow;
Accelerometer accel;
Communication comm;
Outputs outputs;
Config config;
BleController bleController;

unsigned char toggle = 0;
// Arduino IDE board settings:
//   USB Mode:        USB-OTG (TinyUSB)
//   USB CDC On Boot: Enabled   ← framework starts CDC before setup(), no USB.begin() needed here
void setup() {
  Serial.begin(9600);

  config.init();
  config.debug = false;
  outputs.init();
  buttons.init();
  plunger.init();
  lightShow.init();

  if (config.accelerometer > 0) {
    accel.init();
  }

  if (config.bluetoothEnable) {
    bleController.begin();
  }
}

void loop() {
  // Handle input processing (same 4-task rotation as V1)
  if (toggle == 0) {
    plunger.plungerRead();
  } else if (toggle == 1 && config.accelerometerEprom > 0) {
    accel.accelerometerRead();
  } else if (toggle == 2) {
    lightShow.checkSetLights();
  } else if (toggle == 3) {
    comm.communicate();
  }
  toggle++;
  if (toggle > 3) {
    toggle = 0;
  }

  // Check for button changes
  buttons.checkChanged();
  if (config.updateUSB || buttons.numberButtonsPressed > 0) {
    buttons.readInputs();
    Gamepad1.write();
    config.updateUSB = false;
    config.buttonPressed = false;
  }

  // Update BLE controller (reads directly from globals, no SPI needed)
  if (config.bluetoothEnable) {
    bleController.update();
  }
}
