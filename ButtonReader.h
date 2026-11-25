#ifndef ShiftIn_h
#define ShiftIn_h

#include "Arduino.h"
#include "Pins.h"

// =============================================================================
// Phantom press guard (EMI from speakers / coils coupling into button wiring)
// =============================================================================
// A noise spike can pull an input low for a moment, which looks exactly like a
// press. A real switch closure stays closed; a spike doesn't. So a new press is
// only accepted once it has been seen to stay pressed:
//
//  1. Glitch burst (all buttons): the moment a new press shows up, the shift
//     register is re-read a few times, back to back. The press has to be there
//     in every sample. This drops short spikes and corrupted shift-register
//     reads, and costs ~GLITCH_CONFIRM_SAMPLES * GLITCH_CONFIRM_GAP_US once,
//     on the pass where the press happens.
//  2. Hold window: the press must stay down for a set time before it is
//     reported. Flippers get their own (default 0 = burst only, no added lag)
//     so they stay as fast as possible; every other button gets a longer
//     window, which also catches the slower, audio-rate coupling from speakers.
//  3. Mass-press check: a spike usually hits many wires at once. If
//     MASS_PRESS_THRESHOLD or more buttons go down in the same sample, they all
//     get the longer window, flippers included, so a big hit can't flip.
//
// Releases always go through at once: noise can't open a closed switch, and
// release bounce is already handled by the per-button debounce in Buttons.cpp.

// 1-based button number, as the config tool shows it
#define PINONE_BUTTON_BIT(n)      (1UL << ((n) - 1))

// Flipper buttons: buttons 1 and 3. Add more with | PINONE_BUTTON_BIT(n).
#define FLIPPER_BUTTON_MASK       (PINONE_BUTTON_BIT(1) | PINONE_BUTTON_BIT(3))

// How long a press must stay down before it's reported, in microseconds
#define FLIPPER_QUALIFY_US        0      // 0 = glitch burst only, no added lag
#define BUTTON_QUALIFY_US         5000   // all other buttons

// Glitch burst: extra re-reads, and the gap between them, when a new press appears
#define GLITCH_CONFIRM_SAMPLES    3
#define GLITCH_CONFIRM_GAP_US     60

// This many new presses in a single sample counts as noise and gets the long window
#define MASS_PRESS_THRESHOLD      3

class ButtonReader {
private:
  static const uint8_t NUM_INPUTS = 24;
  static const uint32_t INPUT_MASK = (1UL << NUM_INPUTS) - 1;

  // currentState keeps the shift register's active-low layout (bit clear =
  // pressed) so callers are unchanged. It's built from the filtered state.
  uint32_t lastState = 0;
  uint32_t currentState = 0;

  uint32_t accepted = 0;     // presses that passed the guard (bit set = pressed)
  uint32_t pending = 0;      // presses still waiting out their window
  uint32_t forcedSlow = 0;   // pending presses from a mass event (long window)
  uint32_t pendingSince[NUM_INPUTS] = {0};

  // One raw read of the shift register (active low: bit clear = pressed)
  uint32_t sample() {
    uint32_t result = 0;

    digitalWrite(PIN_SHIFT_CLOCK, HIGH);
    digitalWrite(PIN_SHIFT_LATCH, LOW);
    digitalWrite(PIN_SHIFT_LATCH, HIGH);
    digitalWrite(PIN_SHIFT_CLOCK, LOW);

    for(uint16_t i = 0; i < NUM_INPUTS; i++) {
      uint32_t value = digitalRead(PIN_SHIFT_DATA);
      result |= (value << ((NUM_INPUTS-1) - i));
      digitalWrite(PIN_SHIFT_CLOCK, HIGH);
      digitalWrite(PIN_SHIFT_CLOCK, LOW);
    }
    return result;
  }

  inline uint32_t samplePressed() { return ~sample() & INPUT_MASK; }

public:
  ButtonReader() {}

  // setup all pins
  void init() {
    pinMode(PIN_SHIFT_LATCH, OUTPUT);
    pinMode(PIN_SHIFT_CLOCK, OUTPUT);
    pinMode(PIN_SHIFT_DATA, INPUT);
  }

  inline boolean state(int id) { return bitRead(currentState, id); }

  // Inputs held down right now (bit set = pressed), read straight from the shift
  // register for the power-up check, before the normal reads start. An input only
  // counts if it reads pressed in every one of `samples` reads, `gapMs` apart, so
  // a noise spike can't fake a press.
  uint32_t steadyPressed(uint8_t samples, uint16_t gapMs) {
    init();
    uint32_t held = INPUT_MASK;
    for (uint8_t k = 0; held && k < samples; k++) {
      if (k > 0) delay(gapMs);
      held &= samplePressed();
    }
    return held;
  }

  uint32_t read() {
    lastState = currentState;
    uint32_t now = micros();
    uint32_t pressed = samplePressed();

    // Releases are immediate, and a pending press that let go was noise
    accepted &= pressed;
    pending &= pressed;
    forcedSlow &= pending;

    // New presses must survive a quick burst of re-reads
    uint32_t fresh = pressed & ~accepted & ~pending;
    for (uint8_t k = 0; fresh && k < GLITCH_CONFIRM_SAMPLES; k++) {
      delayMicroseconds(GLITCH_CONFIRM_GAP_US);
      fresh &= samplePressed();
    }

    if (fresh) {
      if (__builtin_popcount(fresh) >= MASS_PRESS_THRESHOLD) {
        forcedSlow |= fresh;
      }
      pending |= fresh;
      for (uint8_t i = 0; i < NUM_INPUTS; i++) {
        if (fresh & (1UL << i)) pendingSince[i] = now;
      }
    }

    // Accept presses that have stayed down for their whole window
    for (uint8_t i = 0; pending && i < NUM_INPUTS; i++) {
      uint32_t bit = 1UL << i;
      if (!(pending & bit)) continue;
      bool fast = (FLIPPER_BUTTON_MASK & bit) && !(forcedSlow & bit);
      uint32_t window = fast ? FLIPPER_QUALIFY_US : BUTTON_QUALIFY_US;
      if ((uint32_t)(now - pendingSince[i]) >= window) {
        accepted |= bit;
        pending &= ~bit;
        forcedSlow &= ~bit;
      }
    }

    currentState = ~accepted & INPUT_MASK;
    return currentState;
  }

  boolean update() {
    return read() != lastState;
  }
};

#endif
