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

  config.mpuState = mpu.getAddress();

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
    offsetycounter += getRawAccelValueY();

    count++;
  }
  xValueOffset = offsetxCounter / 10;
  yValueOffset = offsetycounter / 10;

  // Centering just spent time blocking on delay()/I2C reads, so the elapsed
  // time doesn't reflect real cabinet motion; reset the velocity integrator
  // rather than let it treat this gap as a huge, bogus time step.
  resetVelocity();
  stillSinceMicros = 0;
}

// Slowly walk the zero offsets toward the sensor's resting reading when the
// cabinet has been still. "Still" means both axes stay inside a band a few
// times wider than the dead zone for AUTO_CENTER_STILL_US; any reading outside
// the band (a real nudge) restarts the wait, so nudges are never absorbed.
void Accelerometer::autoCenter(int16_t rawX, int16_t rawY) {
  const unsigned long AUTO_CENTER_STILL_US = 500000UL;  // must be still this long
  const unsigned long AUTO_CENTER_STEP_US = 20000UL;    // then move 1 count per step
  const int16_t AUTO_CENTER_MIN_BAND = 40;

  int16_t band = config.accelerometerDeadZone * 3;
  if (band < AUTO_CENTER_MIN_BAND) band = AUTO_CENTER_MIN_BAND;

  unsigned long now = micros();
  if (abs(rawX) > band || abs(rawY) > band) {
    stillSinceMicros = 0;
    return;
  }
  if (stillSinceMicros == 0) {
    stillSinceMicros = now;
    return;
  }
  if (now - stillSinceMicros < AUTO_CENTER_STILL_US) return;
  if (now - lastAutoCenterMicros < AUTO_CENTER_STEP_US) return;
  lastAutoCenterMicros = now;

  if (rawX > 0) xValueOffset++;
  else if (rawX < 0) xValueOffset--;
  if (rawY > 0) yValueOffset++;
  else if (rawY < 0) yValueOffset--;
}

void Accelerometer::resetAccelerometer()
{
  // Everything derived from config is computed here, once, rather than per
  // sample. This runs at boot and again after every config save (see the
  // GET_CONFIG handler in Communication.cpp).
  pinsFacingUp = (config.orientation & 4) != 0;
  mountedOnSide = (config.orientation & 8) != 0;

  // Rotation is stored in 5 degree steps. Precompute the trig so the read path
  // doesn't need to evaluate it every sample.
  // DEG_TO_RAD comes from Arduino.h
  float angleX = (config.orientationRotation % 72) * 5.0f * DEG_TO_RAD;
  float angleY = ((config.orientationRotation + config.axisRotationOffset) % 72) * 5.0f * DEG_TO_RAD;
  cosX = cosf(angleX);
  sinX = sinf(angleX);
  cosY = cosf(angleY);
  sinY = sinf(angleY);

  localMax = config.accelerometerMax;
  localMaxY = config.accelerometerMaxY;
  xAxisScale = localMax != 0 ? 32767.0f / localMax : 0.0f;
  yAxisScale = localMaxY != 0 ? 32767.0f / localMaxY : 0.0f;

  // Velocity integration constants: counts -> mm/s^2, and the decay rate as
  // halvings per second (0 disables decay).
  velocityCountsToMmPerSec2 = (getGRange() / 32768.0f) * 9806.65f;
  velocityHalvingsPerSec = config.accelerometerVelocityDecayTime > 0
    ? 1000.0f / static_cast<float>(config.accelerometerVelocityDecayTime)
    : 0.0f;

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

  int16_t rawX = getRawAccelValue() - xValueOffset;
  int16_t rawY = getRawAccelValueY() - yValueOffset;
  autoCenter(rawX, rawY);

  xValue = rawX;
  yValue = rawY;

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
  return mpu.getX();
}

// "Mounted on side" swaps Z in for the sensor axis that is vertical (carrying
// gravity). applyOrientationTransform() pre-rotates the readings by 90 degrees
// (x = -y, y = x), so the slot that used to be the original board's "x" is now
// fed by the sensor's Y channel, and that's the channel Z replaces. Z is negated
// so the result matches the original x = Z behaviour after the pre-rotation.
int16_t Accelerometer::getRawAccelValueY() {
  return mountedOnSide ? -mpu.getZ() : mpu.getY();
}

void Accelerometer::applyOrientationTransform(int16_t& x, int16_t& y) {
  // This board revision has the MPU6050 mounted 90 degrees rotated versus the
  // original board, so pre-rotate the raw reading before applying the
  // user-selected orientation below.
  int16_t rawX = x;
  x = -y;
  y = rawX;

  // Pins facing up mirrors the board before it is rotated.
  if (pinsFacingUp) {
    x = -x;
  }

  // Rotate by the calibrated board orientation. The X output uses the board
  // rotation and the Y output uses board rotation + axis offset, so the two
  // axes can be calibrated independently. With no offset this is an ordinary
  // rotation, and 0/90/180/270 degrees match the old fixed orientations.
  float fx = static_cast<float>(x);
  float fy = static_cast<float>(y);
  x = clampToInt16(fx * cosX - fy * sinX);
  y = clampToInt16(fx * sinY + fy * cosY);
}

int16_t Accelerometer::clampToInt16(float value) {
  if (value > 32767.0f) return 32767;
  if (value < -32767.0f) return -32767;
  return static_cast<int16_t>(value >= 0.0f ? value + 0.5f : value - 0.5f);
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
    Gamepad1.xAxis(static_cast<int16_t>(static_cast<float>(xValue) * xAxisScale));
    priorXValue = xValue;
    config.updateUSB = true;
  }
}

void Accelerometer::updateYAxis() {
  if (priorYValue != yValue) {
    Gamepad1.yAxis(static_cast<int16_t>(static_cast<float>(yValue) * yAxisScale));
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

  // half-life decay: 0.5^(dt / halfLife) == 2^(-dt * halvingsPerSec)
  if (velocityHalvingsPerSec > 0.0f) {
    float decayFactor = exp2f(-dt * velocityHalvingsPerSec);
    velocityX *= decayFactor;
    velocityY *= decayFactor;
  }

  // convert a raw (int16 full-scale) acceleration reading to an incremental
  // mm/s contribution over this time step
  float convFactor = velocityCountsToMmPerSec2 * dt;
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
  ConfigOut.print(F("A,"));
  ConfigOut.print((getRawAccelValue() - xValueOffset));
  ConfigOut.print(F(","));
  ConfigOut.print((getRawAccelValueY() - yValueOffset));
  ConfigOut.print(F(","));
  ConfigOut.print(xValue);
  ConfigOut.print(F(","));
  ConfigOut.print(yValue);
  ConfigOut.print(F("\r\n"));
  ConfigOut.flush();
}
