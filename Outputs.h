#ifndef OUTPUTS_H
#define OUTPUTS_H
#include <Arduino.h>
#include "UsbHid.h"
#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>
#include "Config.h"
#include "Pins.h"



class Outputs {

  public:
    Outputs();
    void init();
    // Change an output from anything that is not the DOF data stream (light show,
    // button -> solenoid, max on time, config tool). It also forgets the last DOF
    // value of that output, so the next DOF value is always applied.
    void updateOutput(unsigned char outputId, unsigned char outputValue);
    // Change an output from a DOF packet. Does nothing when DOF asks for the value
    // it asked for last and nothing else has touched the output since.
    void updateOutputFromDof(unsigned char outputId, unsigned char outputValue);
    // Non-blocking: queues the change for the output task. Use this from the
    // input pass; updateOutput() itself does I2C writes and must only run on
    // the output task (or during setup()).
    void requestOutput(unsigned char outputId, unsigned char outputValue);
    void sendOutputState();
    void checkResetOutputs();
    void turnOff();
    void pulseOutputs(unsigned char* outputIds, unsigned char count, unsigned char times);
    unsigned char outputList[15] = {40,38,39,14,15,48,13,18,12,11,47,21,45,42,10};

  private:
    void applyOutput(unsigned char outputId, unsigned char outputValue);
    void updateOutputInternal(unsigned char outputId, unsigned char outputValue);
    // last value DOF asked for per output; 0xFF = unknown (changed by something else)
    unsigned char dofShadow[63];
    unsigned char numberOutputs = 15;
    unsigned char outputValues[63] = {0};
    long int timeTurnedOn[63] = {0};
    void updateOutputActual(unsigned char outputId, int outputValueStart, int outputValueFinish);
    unsigned char resetOutputNumber = 0;
    unsigned char virtualOutputOn[10] = {0};
};

#endif
