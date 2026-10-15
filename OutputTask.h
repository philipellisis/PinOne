#ifndef OUTPUT_TASK_H
#define OUTPUT_TASK_H
#include <Arduino.h>

// Work the input pass hands to the output task. Input code posts one of these
// and returns; anything slow (I2C expansion writes, IR transmit, light-show
// delays) happens on the output task so it can't hold up button or plunger
// reads.
enum OutputCmdType : uint8_t {
  CMD_SET_OUTPUT,       // a = output id, b = value
  CMD_BUTTON_PRESSED,   // light show: an input went high
  CMD_BUTTON_RELEASED,  // light show: an input went low
  CMD_IR_SEND,          // a = output index whose pin drives the IR LED
  CMD_PROFILE_NOTIFY,   // a = profile index to pulse / flash for
};

// Starts the output task. Call last in setup(), after every init() is done.
void outputTaskStart();

// Queues a command for the output task. Before outputTaskStart() has run it
// executes inline, which is safe because setup() is still single-threaded.
// Returns false if the queue is full and the command was dropped.
bool postOutputCommand(OutputCmdType type, uint8_t a = 0, uint8_t b = 0);

#endif
