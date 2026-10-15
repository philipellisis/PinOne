#include "OutputTask.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "Enums.h"
#include "Globals.h"
#include "IRTransmit.h"
#include "Threading.h"

SemaphoreHandle_t inputMutex = nullptr;
SemaphoreHandle_t i2cMutex = nullptr;

IRTransmit irTransmit;

namespace {

struct OutputCmd {
  uint8_t type;
  uint8_t a;
  uint8_t b;
};

const uint8_t OUTPUT_QUEUE_LENGTH = 128;
// Priority 2 is above the loop task (1), so when the output task is waiting
// on inputMutex during a config transfer it gets the lock as soon as the
// input pass releases it, instead of being starved by the loop re-taking it.
const UBaseType_t OUTPUT_TASK_PRIORITY = 2;
const uint32_t OUTPUT_TASK_STACK = 8192;
// Light-show steps are paced in time rather than per pass, so the output task
// can run faster than the old round-robin without speeding the animation up.
// This is an estimate of the old cadence; tune by eye on the cabinet.
const uint32_t LIGHT_SHOW_STEP_MS = 4;

QueueHandle_t outputQueue = nullptr;

// Profile-change feedback: pulse the configured outputs, or flash the light
// show if none are configured. Runs on the output task because it blocks.
void runProfileNotify(uint8_t profileIndex) {
  unsigned char* outs = &config.profileNotifyOutputs[profileIndex * 4];
  bool anyConfigured = false;
  for (uint8_t i = 0; i < 4; i++) {
    if (outs[i] > 0) {
      anyConfigured = true;
      break;
    }
  }
  if (!anyConfigured) {
    lightShow.flashLights();
    return;
  }
  outputs.pulseOutputs(outs, 4, config.profileNotifyPulseCount[profileIndex]);
}

void dispatch(const OutputCmd& cmd) {
  switch (cmd.type) {
    case CMD_SET_OUTPUT:
      outputs.updateOutput(cmd.a, cmd.b);
      break;
    case CMD_BUTTON_PRESSED:
      lightShow.onButtonPressed();
      break;
    case CMD_BUTTON_RELEASED:
      lightShow.onButtonReleased();
      break;
    case CMD_IR_SEND:
      irTransmit.sendCommand(outputs.outputList[cmd.a]);
      break;
    case CMD_PROFILE_NOTIFY:
      runProfileNotify(cmd.a);
      break;
  }
}

void outputTaskFn(void*) {
  OutputCmd cmd;
  uint32_t lastLightStep = 0;
  for (;;) {
    while (xQueueReceive(outputQueue, &cmd, 0) == pdTRUE) {
      dispatch(cmd);
    }

    comm.communicate();

    uint32_t now = millis();
    if (now - lastLightStep >= LIGHT_SHOW_STEP_MS) {
      lastLightStep = now;
      lightShow.checkSetLights();
    }

    // Yield so the idle task and the BLE/USB tasks on this core get time.
    vTaskDelay(1);
  }
}

}  // namespace

void threadingInit() {
  inputMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();
}

void outputTaskStart() {
  outputQueue = xQueueCreate(OUTPUT_QUEUE_LENGTH, sizeof(OutputCmd));
  xTaskCreatePinnedToCore(outputTaskFn, "outputs", OUTPUT_TASK_STACK, nullptr,
                          OUTPUT_TASK_PRIORITY, nullptr, 0);
}

bool postOutputCommand(OutputCmdType type, uint8_t a, uint8_t b) {
  OutputCmd cmd = {type, a, b};
  if (!outputQueue) {
    dispatch(cmd);
    return true;
  }
  return xQueueSend(outputQueue, &cmd, 0) == pdTRUE;
}
