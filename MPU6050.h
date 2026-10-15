#ifndef MPU6050_H
#define MPU6050_H

#include "Arduino.h"
#include <Wire.h>

class MPU6050 {

  public:

    bool init(TwoWire *wire = &Wire);
    void setAccelerometerRange(unsigned char);
    void read();
    int getX();
    int getY();
    int getZ();
    void config();
    // The I2C address ("channel") the chip was found on, or 0 if init()
    // never found a device to talk to.
    uint8_t getAddress() const { return initialized ? _addr : 0; }

  private:
    bool initialized = false;
    int rawAccX = 0, rawAccY = 0, rawAccZ = 0;
    int accelRange = 0;
    // Spike filter state: the first sample after init/range change is always
    // accepted, and a run of rejected samples is treated as a real change.
    bool haveSample = false;
    uint8_t spikeCount[3] = {0, 0, 0};
    bool filterSpike(int newVal, int &held, uint8_t &rejected);
    TwoWire *_wire = nullptr;
    uint8_t _addr = 0x6B;

    uint8_t readReg(uint8_t reg);
    void writeReg(uint8_t reg, uint8_t val);
};

#endif
