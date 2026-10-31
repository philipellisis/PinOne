#ifndef ACCEL_H
#define ACCEL_H
#include <Arduino.h>
#include "UsbHid.h"
#include "Config.h"



class Accelerometer {

  public:
    Accelerometer();
    void init();
    void accelerometerRead();
    void sendAccelerometerState();
    void resetAccelerometer();
    void centerAccelerometer();
    int16_t getXValue() const { return xValue; }
    int16_t getYValue() const { return yValue; }
    // Full-scale (+/-32767) axis values, as sent over USB: the raw value scaled
    // by 32767 / accelerometerMax
    int16_t getXAxis() const { return clampToInt16(static_cast<float>(xValue) * xAxisScale); }
    int16_t getYAxis() const { return clampToInt16(static_cast<float>(yValue) * yAxisScale); }
    void applyOrientationTransform(int16_t& x, int16_t& y);
    int16_t getRawAccelValue();
    int16_t getRawAccelValueY();
    void processTiltButton();
    void updateXAxis();
    void updateYAxis();
    void updateVelocity();
    void updateRxAxis();
    void updateRyAxis();
    void resetVelocity();
    void zeroOutputs();
    void zeroVelocityOutput();

  private:
    int16_t xValueOffset = 0;
    int16_t yValueOffset = 0;
    int16_t xValue;
    int16_t yValue;
    int16_t priorXValue = 0;
    int16_t priorYValue = 0;
    bool pinsFacingUp = false;
    bool mountedOnSide = false;
    // Axis output scale (32767 / max), the velocity integrator's constants, and
    // the trig below are all derived from config in resetAccelerometer(), so
    // the per-sample path does no division, pow() or switch lookups.
    float xAxisScale = 0.0f;
    float yAxisScale = 0.0f;
    float velocityCountsToMmPerSec2 = 0.0f;
    float velocityHalvingsPerSec = 0.0f;
    // Cached trig for the calibrated board rotation: X uses the board rotation,
    // Y uses board rotation + axis offset (see applyOrientationTransform).
    float cosX = 1.0f;
    float sinX = 0.0f;
    float cosY = 1.0f;
    float sinY = 0.0f;
    static int16_t clampToInt16(float value);
    int16_t softLimit(int16_t value, int limit) const;
    int16_t localMax = 0;
    int16_t localMaxY = 0;
    uint8_t tiltSuppressTime = 0;

    // Auto-centering: the zero offsets continuously drift toward the sensor's
    // current reading, so tilt changes, resting position and thermal drift
    // relax back to center. The fractional remainder is carried between steps.
    unsigned long lastAutoCenterMicros = 0;
    float xDriftRemainder = 0.0f;
    float yDriftRemainder = 0.0f;
    void autoCenter(int16_t rawX, int16_t rawY);

    // Velocity-based accelerometer input. The velocity is tracked in mm/s,
    // by integrating the (centered, orientation-corrected) acceleration
    // readings over time, and decays with a configurable half-life so that
    // any residual DC bias in the acceleration signal doesn't cause the
    // reported velocity to drift or grow unbounded.
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    unsigned long lastVelocityMicros = 0;
    int16_t priorRxValue = 0;
    int16_t priorRyValue = 0;
    float getGRange() const;
    int16_t getScaledVelocity(float velocity) const;
};

#endif
