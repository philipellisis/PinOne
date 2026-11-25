#ifndef COMMUNICATION_H
#define COMMUNICATION_H
#include <Arduino.h>
#include "Plunger.h"
#include "UsbHid.h"
#include "Buttons.h"
#include "Accelerometer.h"
#include "Config.h"
#include "Outputs.h"


class Communication {

  public:
    Communication();
    void communicate();

    // DOF over Bluetooth: a 9-byte frame as on the USB vendor channel ([0, 200+bank, v0..v6] or [0, 252, output, value, ...]),
    // queued from the NimBLE host task (the HID report callbacks) and applied by communicate() on the output task, like USB frames.
    // Returns false when the frame is not a bank or single-output frame, or the queue is full.
    bool queueBleFrame(const uint8_t* frame);

  private:
    char* connectedString = "DEBUG,CSD Board Connected\r\n";
    uint8_t firstNumber = 0; //0 normally
    uint8_t bankOffset = 200; //200 normally
    uint8_t adminNumber = 250; //250 normally
    uint8_t connectionNumber = 251;//251 normally
    uint8_t outputSingleNumber = 252;//252 normally
    uint8_t outputButtonNumber = 253;//252 normally
    uint8_t maxNumber = 255; //255 normally
    uint8_t delayIncrementor = 0;

    uint8_t incomingData[10];
    uint8_t dataLocation = 0;
    void updateOutputs();
    void updateOutputsFromPacket(uint8_t* packet);
    uint8_t admin = 0;
    void sendAdmin();
    bool shouldDelay();
    void handleDelayedAdmin(uint8_t adminType);

    // Legacy RS232/CDC serial DOF fallback (see Config::legacySerialDofEnabled)
    uint8_t legacyIncomingData[10];
    uint8_t legacyDataLocation = 0;
    void communicateLegacySerial();

    static const uint16_t MAX_BYTES_PER_PASS = 256;
    bool validSecondByte(uint8_t b);

    // Bluetooth DOF frames (see queueBleFrame)
    static const uint8_t BLE_FRAME_QUEUE_LENGTH = 64;
    QueueHandle_t bleFrames = nullptr;
    void communicateBle();
};

#endif
