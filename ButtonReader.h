#ifndef ShiftIn_h
#define ShiftIn_h

#include "Arduino.h"
#include "Pins.h"

class ButtonReader {
private:

  uint32_t lastState = 0;
  uint32_t currentState = 0;
public:
  ButtonReader() {}

  // setup all pins
  void init() {
    pinMode(PIN_SHIFT_LATCH, OUTPUT);
    pinMode(PIN_SHIFT_CLOCK, OUTPUT);
    pinMode(PIN_SHIFT_DATA, INPUT);
  }

  inline boolean state(int id) { return bitRead(currentState, id); }

  uint32_t read() {
    lastState = currentState;
    uint32_t result = 0;

    digitalWrite(PIN_SHIFT_CLOCK, HIGH);
    digitalWrite(PIN_SHIFT_LATCH, LOW);
    digitalWrite(PIN_SHIFT_LATCH, HIGH);
    digitalWrite(PIN_SHIFT_CLOCK, LOW);

    for(uint16_t i = 0; i < 24; i++) {
      uint32_t value = digitalRead(PIN_SHIFT_DATA);
      result |= (value << ((24-1) - i));
      digitalWrite(PIN_SHIFT_CLOCK, HIGH);
      digitalWrite(PIN_SHIFT_CLOCK, LOW);
    }
    currentState = result;
    return result;
  }

  boolean update() {
    return read() != lastState;
  }
};

#endif
