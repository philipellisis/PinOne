#ifndef IRTRANSMIT_H
#define IRTRANSMIT_H
#include <Arduino.h>
#include "driver/rmt_tx.h"

enum IR_PROTOCOL : uint8_t {
  IR_PROTOCOL_NONE = 0,
  IR_PROTOCOL_NEC,
  IR_PROTOCOL_SAMSUNG,
  IR_PROTOCOL_SONY
};

class IRTransmit {
public:
  IRTransmit();
  ~IRTransmit();

  // Uses global 'config' (protocol, code, bits) and the provided output pin.
  void sendCommand(uint8_t outputPin);

private:
  // Protocol implementations — build RMT items and transmit
  void sendNEC(uint32_t word, uint8_t pin);
  void sendSamsung(uint32_t word, uint8_t pin);
  void sendSony(uint32_t data, uint8_t bits, uint8_t pin);

  // NEC helpers
  void necAddByteLSB(uint8_t b);
  void necAddFrame(uint8_t addr, uint8_t cmd);
  void necAddFrameExtended(uint16_t addr16, uint8_t cmd);
  void necAddRepeat();

  // RMT helpers
  void addItem(uint16_t markUs, uint16_t spaceUs);
  void transmit(uint8_t pin);
  void ensureChannel(uint8_t pin);
  void releaseChannel();

  // RMT state
  rmt_channel_handle_t _channel = nullptr;
  rmt_encoder_handle_t _encoder = nullptr;
  uint8_t _lastPin = 0xFF;

  // Item buffer for building IR frames
  static const int MAX_ITEMS = 80;
  rmt_symbol_word_t _items[MAX_ITEMS];
  int _itemCount = 0;

  // Carrier parameters
  uint32_t _carrierFreqHz = 38000;
  float _carrierDuty = 0.33f;
};

#endif
