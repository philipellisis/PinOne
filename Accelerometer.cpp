#include "Accelerometer.h"
#include <Arduino.h>
#include <Wire.h>
#include "UsbHid.h"
#include "MPU6050.h"
#include "Enums.h"
#include "Globals.h"
#include "Pins.h"



MPU6050 mpu;
Accelerometer::Accelerometer()
{
}

void Accelerometer::init()
{
  uint8_t count = 0;
  bool ok = mpu.init();

  while (!ok && count < 10)
  {
    delay(100);
    count++;
    ok = mpu.init();
  }

  if (!ok)
  {
    config.accelerometer = 0;
    config.accelerometerEprom = 0;
    return;
  }

  resetAccelerometer();
}

void Accelerometer::centerAccelerometer()
{
  delay(400);

  uint8_t count = 0;
  int32_t offsetxCounter = 0;
  int32_t offsetycounter = 0;

  xValueOffset = 0;
  yValueOffset = 0;
  while (count < 10)
  {
    mpu.read();
    offsetxCounter += getRawAccelValue();
    offsetycounter += mpu.getY();

    count++;
  }
  xValueOffset = offsetxCounter / 10;
  yValueOffset = offsetycounter / 10;
}

void Accelerometer::resetAccelerometer()
{
  if (config.orientation > 7) {
    orientation = config.orientation - 8;
  } else {
    orientation = config.orientation;
  }
  localMax = config.accelerometerMax;
  localMaxY = config.accelerometerMaxY;
  mpu.setAccelerometerRange(config.accelerometerSensitivity);
  centerAccelerometer();
}

void Accelerometer::accelerometerRead()
{
  if (config.plungerMoving == true && config.disableAccelOnPlungerMove == 1) {
    return;
  }
  if (config.lightShowState == IN_RANDOM_MODE_WAITING_INPUT)
  {
    if (recentered == false)
    {
      centerAccelerometer();
      recentered = true;
    }
  }
  else
  {
    recentered = false;
  }
  /* Get new sensor events with the readings */
  mpu.read();

  xValue = floor((getRawAccelValue() - xValueOffset));
  yValue = floor((mpu.getY() - yValueOffset));

  if (abs(xValue) < config.accelerometerDeadZone)
  {
    xValue = 0;
  }
  if (abs(yValue) < config.accelerometerDeadZone)
  {
    yValue = 0;
  }

  applyOrientationTransform(xValue, yValue);
  processTiltButton();

  updateXAxis();
  updateYAxis();
}

int16_t Accelerometer::getRawAccelValue() {
  return (config.orientation > 7) ? mpu.getZ() : mpu.getX();
}

void Accelerometer::applyOrientationTransform(int16_t& x, int16_t& y) {
  int16_t temp = x;
  switch (orientation) {
    case RIGHT:
      x = -y; y = temp;
      break;
    case FORWARD:
      x = -x; y = -y;
      break;
    case LEFT:
      x = y; y = -temp;
      break;
    case UP_BACK:
      x = -x;
      break;
    case UP_RIGHT:
      x = -y; y = -temp;
      break;
    case UP_FORWARD:
      y = -y;
      break;
    case UP_LEFT:
      x = y; y = temp;
      break;
  }
}

void Accelerometer::processTiltButton() {
  if (config.tiltButton >= 24 || config.tiltButtonRight >=32 || config.tiltButtonLeft >= 32 || config.tiltButtonUp >= 32 || config.tiltButtonDown >= 32) return;

  if (tiltSuppressTime > 0) {
    tiltSuppressTime--;
    return;
  }

  bool tiltThresholdExceeded = (abs(xValue) > config.accelerometerTilt || abs(yValue) > config.accelerometerTiltY);
  uint8_t currentTiltState = config.lastButtonState[config.tiltButtonRight] ||
                            config.lastButtonState[config.tiltButtonLeft] ||
                            config.lastButtonState[config.tiltButtonUp] ||
                            config.lastButtonState[config.tiltButtonDown];

  if (currentTiltState == 0 && tiltThresholdExceeded) {
    if (xValue > config.accelerometerTilt) {
      config.lastButtonState[config.tiltButtonRight] = buttons.sendButtonPush(config.tiltButtonRight, 1);
    }
    else if (xValue < -config.accelerometerTilt) {
      config.lastButtonState[config.tiltButtonLeft] = buttons.sendButtonPush(config.tiltButtonLeft, 1);
    }
    else if (yValue > config.accelerometerTiltY) {
      config.lastButtonState[config.tiltButtonUp] = buttons.sendButtonPush(config.tiltButtonUp, 1);
    }
    else if (yValue < -config.accelerometerTiltY) {
      config.lastButtonState[config.tiltButtonDown] = buttons.sendButtonPush(config.tiltButtonDown, 1);
    }

  } else if (currentTiltState == 1 && !tiltThresholdExceeded) {
    config.lastButtonState[config.tiltButtonRight] = buttons.sendButtonPush(config.tiltButtonRight, 0);
    config.lastButtonState[config.tiltButtonLeft] = buttons.sendButtonPush(config.tiltButtonLeft, 0);
    config.lastButtonState[config.tiltButtonUp] = buttons.sendButtonPush(config.tiltButtonUp, 0);
    config.lastButtonState[config.tiltButtonDown] = buttons.sendButtonPush(config.tiltButtonDown, 0);
    tiltSuppressTime = config.tiltSuppress;
  }
}

void Accelerometer::updateXAxis() {
  if (priorXValue != xValue) {
    Gamepad1.xAxis(static_cast<int16_t>(static_cast<float>(xValue) / localMax * 32767));
    priorXValue = xValue;
    config.updateUSB = true;
  }
}

void Accelerometer::updateYAxis() {
  if (priorYValue != yValue) {
    Gamepad1.yAxis(static_cast<int16_t>(static_cast<float>(yValue) / localMaxY * 32767));
    priorYValue = yValue;
    config.updateUSB = true;
  }
}

void Accelerometer::sendAccelerometerState()
{
  ComSerial.print(F("A,"));
  ComSerial.print((getRawAccelValue() - xValueOffset));
  ComSerial.print(F(","));
  ComSerial.print((mpu.getY() - yValueOffset));
  ComSerial.print(F(","));
  ComSerial.print(xValue);
  ComSerial.print(F(","));
  ComSerial.print(yValue);
  ComSerial.print(F("\r\n"));
}
