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
    void updateOutput(unsigned char outputId, unsigned char outputValue);
    void sendOutputState();
    void checkResetOutputs();
    void turnOff();
    unsigned char outputList[15] = {40,38,39,14,15,48,12,11,47,21,18,17,45,42,10};

  private:
    void updateOutputInternal(unsigned char outputId, unsigned char outputValue);
    unsigned char numberOutputs = 15;
    unsigned char outputValues[63] = {0};
    long int timeTurnedOn[63] = {0};
    void updateOutputActual(unsigned char outputId, int outputValueStart, int outputValueFinish);
    unsigned char resetOutputNumber = 0;
    unsigned char virtualOutputOn[10] = {0};
};

#endif
