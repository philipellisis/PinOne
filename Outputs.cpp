#include "Outputs.h"
#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>
#include "Enums.h"
#include "Globals.h"
#include "Pins.h"

#define CONFIG_MCPWM_SUPPRESS_DEPRECATE_WARN 1
#include "driver/mcpwm.h"

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(0x41);
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x42);

// Outputs 8-14 get real hardware PWM via the MCPWM peripheral (separate from
// LEDC, which is maxed out at 8 channels on outputs 0-7). Each entry maps
// output index (8..14) to its MCPWM unit/timer/generator.
static const mcpwm_unit_t MCPWM_UNIT_FOR_OUTPUT[NUM_MCPWM_OUTPUTS] = {
  MCPWM_UNIT_0, MCPWM_UNIT_0, MCPWM_UNIT_0, MCPWM_UNIT_0, MCPWM_UNIT_0, MCPWM_UNIT_0, MCPWM_UNIT_1
};
static const mcpwm_timer_t MCPWM_TIMER_FOR_OUTPUT[NUM_MCPWM_OUTPUTS] = {
  MCPWM_TIMER_0, MCPWM_TIMER_0, MCPWM_TIMER_1, MCPWM_TIMER_1, MCPWM_TIMER_2, MCPWM_TIMER_2, MCPWM_TIMER_0
};
static const mcpwm_generator_t MCPWM_GEN_FOR_OUTPUT[NUM_MCPWM_OUTPUTS] = {
  MCPWM_GEN_A, MCPWM_GEN_B, MCPWM_GEN_A, MCPWM_GEN_B, MCPWM_GEN_A, MCPWM_GEN_B, MCPWM_GEN_A
};
static const mcpwm_io_signals_t MCPWM_SIGNAL_FOR_OUTPUT[NUM_MCPWM_OUTPUTS] = {
  MCPWM0A, MCPWM0B, MCPWM1A, MCPWM1B, MCPWM2A, MCPWM2B, MCPWM0A
};

Outputs::Outputs() {
   for (int index = 0; index < numberOutputs; index++) {
    pinMode(outputList[index], OUTPUT);
   }
}

void Outputs::init() {
  // Configure LEDC PWM channels for outputs 0-7
  for (int i = 0; i < NUM_PWM_OUTPUTS; i++) {
    ledcAttach(outputList[i], 1000, 8); // pin, 1kHz, 8-bit resolution
  }

  // Configure MCPWM channels for outputs 8-14
  mcpwm_config_t mcpwmConf;
  mcpwmConf.frequency = 1000; // match the LEDC outputs' 1kHz
  mcpwmConf.cmpr_a = 0;
  mcpwmConf.cmpr_b = 0;
  mcpwmConf.duty_mode = MCPWM_DUTY_MODE_0;
  mcpwmConf.counter_mode = MCPWM_UP_COUNTER;
  for (int i = 0; i < NUM_MCPWM_OUTPUTS; i++) {
    mcpwm_gpio_init(MCPWM_UNIT_FOR_OUTPUT[i], MCPWM_SIGNAL_FOR_OUTPUT[i], outputList[NUM_PWM_OUTPUTS + i]);
    if (MCPWM_GEN_FOR_OUTPUT[i] == MCPWM_GEN_A) {
      // Generator B on the same timer is initialized alongside A, so only init once per timer
      mcpwm_init(MCPWM_UNIT_FOR_OUTPUT[i], MCPWM_TIMER_FOR_OUTPUT[i], &mcpwmConf);
    }
  }

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(90);
  pwm1.begin();
  pwm1.setOscillatorFrequency(27000000);
  pwm1.setPWMFreq(90);
  pwm2.begin();
  pwm2.setOscillatorFrequency(27000000);
  pwm2.setPWMFreq(90);
  Wire.setClock(400000);
  Wire.setTimeout(3000);

  turnOff();

}

void Outputs::turnOff() {
  for (int i = 0; i < 62; i++) {
    updateOutput(i, 0);
  }
}

void Outputs::updateOutput(unsigned char outputId, unsigned char outputValue) {
  if (outputValue > config.maxOutputState[outputId]) {
    outputValue = config.maxOutputState[outputId];
  }
  if (config.nightMode == true && (config.toySpecialOption[outputId] == NOISY || config.toySpecialOption[outputId] == SHARED)) {
    outputValue = 0;
  }
  if (config.toySpecialOption[outputId] == SHARED && outputValue > 0 && outputValues[outputId] > 0) {
    for (int i = 0; i < 9; i++) {
      if(config.toySpecialOption[i] == SHARED && outputValues[i] == 0) {
        outputId = i;
        virtualOutputOn[i] = 1;
        break;
      }
    }
  }
  if (config.toySpecialOption[outputId] == SHARED && outputValue == 0 && outputId < 10) {
    if (virtualOutputOn[outputId] == 0) {
      for (int i = 0; i < 9; i++) {
        if(config.toySpecialOption[i] == SHARED && outputValues[i] > 0 && virtualOutputOn[i] > 0) {
          updateOutputInternal(i, 0);
          virtualOutputOn[i] = 0;
        }
      }
    }
    updateOutputInternal(outputId, outputValue);
  } else {
    updateOutputInternal(outputId, outputValue);
  }
}

void Outputs::updateOutputInternal(unsigned char outputId, unsigned char outputValue) {
  outputValues[outputId] = outputValue;
  if (outputValue != 0) {
    timeTurnedOn[outputId] = millis();
  }
  if (outputId < 15) {
    if (outputId < NUM_PWM_OUTPUTS) {
      // PWM output via LEDC (outputs 0-7)
      ledcWrite(outputList[outputId], outputValue);
    } else {
      // PWM output via MCPWM (outputs 8-14)
      int mcpwmIndex = outputId - NUM_PWM_OUTPUTS;
      mcpwm_set_duty(MCPWM_UNIT_FOR_OUTPUT[mcpwmIndex], MCPWM_TIMER_FOR_OUTPUT[mcpwmIndex], MCPWM_GEN_FOR_OUTPUT[mcpwmIndex], (outputValue / 255.0f) * 100.0f);
    }
  } else {
    // PCA9685 expansion board outputs
    // if the output is the button board output, then invert the value
    if (outputId < 31 && config.reverseButtonOutputPolarity == true) {
      outputValue = 255 - outputValue;
    }
    if (outputValue == 255) {
      updateOutputActual(outputId - 15, 4096, 0);
    } else if (outputValue == 0) {
      updateOutputActual(outputId - 15, 0, 4096);
    } else {
      updateOutputActual(outputId - 15, 1, outputValue * 16);
    }
  }
}

void Outputs::checkResetOutputs() {
  long int t1 = millis();
  for(int i = 0; i < 5; i++) {
    resetOutputNumber = (resetOutputNumber + 1) % 63;
    if (outputValues[resetOutputNumber] > config.turnOffState[resetOutputNumber] && config.maxOutputTime[resetOutputNumber] > 0) {
      if (t1 - timeTurnedOn[resetOutputNumber] >= config.maxOutputTime[resetOutputNumber] * 100) {
        updateOutput(resetOutputNumber, config.turnOffState[resetOutputNumber]);
      }
    }
  }
}

void Outputs::updateOutputActual(unsigned char outputId, int outputValueStart, int outputValueFinish) {
  if (outputId < 16) {
    pwm.setPWM(outputId, outputValueStart, outputValueFinish);
  }
  else if (outputId >= 16 && outputId < 32) {
    pwm1.setPWM(outputId - 16, outputValueStart, outputValueFinish);
  }
  else if (outputId >= 32 && outputId < 48) {
    pwm2.setPWM(outputId - 32, outputValueStart, outputValueFinish);
  }
}

void Outputs::sendOutputState() {
  ConfigOut.print(F("O,"));
  for (int i = 0; i < 63; i++) {
    ConfigOut.print(outputValues[i]);
    ConfigOut.print(F(","));
  }
  ConfigOut.print(config.nightMode);
  ConfigOut.print(F("\r\n"));
  ConfigOut.flush();
}
