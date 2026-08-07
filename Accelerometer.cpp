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

  // Centering just spent time blocking on delay()/I2C reads, so the elapsed
  // time doesn't reflect real cabinet motion; reset the velocity integrator
  // rather than let it treat this gap as a huge, bogus time step.
  resetVelocity();
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

  if (config.accelerometerVelocityEnabled)
  {
    updateVelocity();
    updateRxAxis();
    updateRyAxis();
  }
  else if (priorRxValue != 0 || priorRyValue != 0)
  {
    // velocity output was just turned off - zero the axes rather than
    // leaving them stuck at their last reported value
    resetVelocity();
    Gamepad1.rxAxis(0);
    Gamepad1.ryAxis(0);
    priorRxValue = 0;
    priorRyValue = 0;
    config.updateUSB = true;
  }
}

int16_t Accelerometer::getRawAccelValue() {
  return (config.orientation > 7) ? mpu.getZ() : mpu.getX();
}

void Accelerometer::applyOrientationTransform(int16_t& x, int16_t& y) {
  // This board revision has the MPU6050 mounted 90 degrees rotated versus the
  // original board, so pre-rotate the raw reading before applying the
  // user-selected orientation below.
  int16_t rawX = x;
  x = -y;
  y = rawX;

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

// Full-scale range of the accelerometer, in g units, matching the range
// selected by config.accelerometerSensitivity (see MPU6050::setAccelerometerRange).
float Accelerometer::getGRange() const {
  switch (config.accelerometerSensitivity) {
    case 1: return 8.0f;
    case 2: return 4.0f;
    case 3: return 2.0f;
    default: return 16.0f;
  }
}

// Reset the velocity integrator. This should be called whenever a gap in
// readings (centering, mode changes, startup) would otherwise be mistaken
// for real elapsed time by updateVelocity().
void Accelerometer::resetVelocity() {
  velocityX = 0.0f;
  velocityY = 0.0f;
  lastVelocityMicros = 0;
}

// Integrate the current (centered, orientation-corrected) acceleration
// reading into the running velocity estimate, in mm/s. This is the same
// basic technique used by Pinscape Pico's nudge.vx/nudge.vy velocity axes:
// each cycle, the existing velocity is decayed by a configurable half-life
// (to remove the effect of any residual DC bias in the acceleration signal,
// since a stationary cabinet's true average velocity is zero), then the new
// acceleration's contribution is added in.
void Accelerometer::updateVelocity() {
  unsigned long now = micros();
  if (lastVelocityMicros == 0) {
    // first sample since a reset - just establish the time baseline
    lastVelocityMicros = now;
    return;
  }

  float dt = static_cast<float>(now - lastVelocityMicros) / 1000000.0f;
  lastVelocityMicros = now;

  // guard against bogus/huge time steps (e.g. a long stall between reads)
  if (dt <= 0.0f || dt > 1.0f) {
    return;
  }

  if (config.accelerometerVelocityDecayTime > 0) {
    float decayFactor = pow(0.5f, dt / (static_cast<float>(config.accelerometerVelocityDecayTime) / 1000.0f));
    velocityX *= decayFactor;
    velocityY *= decayFactor;
  }

  // convert a raw (int16 full-scale) acceleration reading to mm/s^2, then
  // to an incremental mm/s contribution over this time step
  float convFactor = (getGRange() / 32768.0f) * 9806.65f * dt;
  velocityX += static_cast<float>(xValue) * convFactor;
  velocityY += static_cast<float>(yValue) * convFactor;
}

// Scale a velocity in mm/s to the arbitrary INT16 units used on the HID axes
int16_t Accelerometer::getScaledVelocity(float velocity) const {
  float scaled = velocity * static_cast<float>(config.accelerometerVelocityScale);
  if (scaled > 32767.0f) return 32767;
  if (scaled < -32768.0f) return -32768;
  return static_cast<int16_t>(scaled);
}

void Accelerometer::updateRxAxis() {
  int16_t scaled = getScaledVelocity(velocityX);
  if (priorRxValue != scaled) {
    Gamepad1.rxAxis(scaled);
    priorRxValue = scaled;
    config.updateUSB = true;
  }
}

void Accelerometer::updateRyAxis() {
  int16_t scaled = getScaledVelocity(velocityY);
  if (priorRyValue != scaled) {
    Gamepad1.ryAxis(scaled);
    priorRyValue = scaled;
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
