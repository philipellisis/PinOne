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
    void applyOrientationTransform(int16_t& x, int16_t& y);
    int16_t getRawAccelValue();
    void processTiltButton();
    void updateXAxis();
    void updateYAxis();
    void updateVelocity();
    void updateRxAxis();
    void updateRyAxis();
    void resetVelocity();

  private:
    int16_t xValueOffset = 0;
    int16_t yValueOffset = 0;
    int16_t xValue;
    int16_t yValue;
    int16_t priorXValue = 0;
    int16_t priorYValue = 0;
    bool recentered = false;
    uint8_t orientation = 0;
    int16_t localMax = 0;
    int16_t localMaxY = 0;
    uint8_t tiltSuppressTime = 0;

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
