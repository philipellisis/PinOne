#ifndef THREADING_H
#define THREADING_H
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// The firmware runs two tasks: the input pass (loop(), core 1) and the output
// task (OutputTask.cpp, core 0). These mutexes cover what they share:
//  - inputMutex: held by each input pass. The config tool's GET_CONFIG and
//    SET_BLE_MAP take it while they rewrite config, accelerometer and BLE
//    state, so the input pass never reads half-written values.
//  - i2cMutex: Wire is not thread-safe. The MPU6050 (input) and the PCA9685
//    expansion boards (output) both take it around each bus transaction.
extern SemaphoreHandle_t inputMutex;
extern SemaphoreHandle_t i2cMutex;

void threadingInit();  // creates the mutexes; call first in setup()

// Takes the mutex for the lifetime of the object. A null mutex is ignored so
// code that runs before threadingInit() still works.
class ScopedLock {
  public:
    explicit ScopedLock(SemaphoreHandle_t mutex) : _mutex(mutex) {
      if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    }
    ~ScopedLock() {
      if (_mutex) xSemaphoreGive(_mutex);
    }
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;

  private:
    SemaphoreHandle_t _mutex;
};

#endif
