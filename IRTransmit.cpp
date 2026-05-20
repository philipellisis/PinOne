#include "IRTransmit.h"
#include "Config.h"
#include "Globals.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"

IRTransmit::IRTransmit() {}

IRTransmit::~IRTransmit() {
  releaseChannel();
}

void IRTransmit::ensureChannel(uint8_t pin) {
  if (_channel != nullptr && _lastPin == pin) return;

  releaseChannel();

  rmt_tx_channel_config_t txConfig = {};
  txConfig.gpio_num = (gpio_num_t)pin;
  txConfig.clk_src = RMT_CLK_SRC_DEFAULT;
  txConfig.resolution_hz = 1000000; // 1 MHz = 1us per tick
  txConfig.mem_block_symbols = 64;
  txConfig.trans_queue_depth = 1;

  esp_err_t err = rmt_new_tx_channel(&txConfig, &_channel);
  if (err != ESP_OK) {
    _channel = nullptr;
    return;
  }

  // Configure carrier
  rmt_carrier_config_t carrierConfig = {};
  carrierConfig.duty_cycle = _carrierDuty;
  carrierConfig.frequency_hz = _carrierFreqHz;
  carrierConfig.flags.polarity_active_low = false;
  rmt_apply_carrier(_channel, &carrierConfig);

  // Create copy encoder for raw symbols
  rmt_copy_encoder_config_t copyConfig = {};
  rmt_new_copy_encoder(&copyConfig, &_encoder);

  rmt_enable(_channel);
  _lastPin = pin;
}

void IRTransmit::releaseChannel() {
  if (_channel) {
    rmt_disable(_channel);
    rmt_del_channel(_channel);
    _channel = nullptr;
  }
  if (_encoder) {
    rmt_del_encoder(_encoder);
    _encoder = nullptr;
  }
  _lastPin = 0xFF;
}

void IRTransmit::addItem(uint16_t markUs, uint16_t spaceUs) {
  if (_itemCount >= MAX_ITEMS) return;
  _items[_itemCount].duration0 = markUs;
  _items[_itemCount].level0 = 1;    // carrier on during mark
  _items[_itemCount].duration1 = spaceUs;
  _items[_itemCount].level1 = 0;    // carrier off during space
  _itemCount++;
}

void IRTransmit::transmit(uint8_t pin) {
  ensureChannel(pin);
  if (!_channel || !_encoder || _itemCount == 0) return;

  rmt_transmit_config_t txCfg = {};
  txCfg.loop_count = 0;
  rmt_transmit(_channel, _encoder, _items, _itemCount * sizeof(rmt_symbol_word_t), &txCfg);
  rmt_tx_wait_all_done(_channel, 1000);

  _itemCount = 0;
}

void IRTransmit::sendCommand(uint8_t outputPin) {
  if (config.irProtocol == IR_PROTOCOL_NONE) return;

  // Set carrier frequency based on protocol
  uint32_t newFreq = (config.irProtocol == IR_PROTOCOL_SONY) ? 40000 : 38000;
  if (newFreq != _carrierFreqHz) {
    _carrierFreqHz = newFreq;
    releaseChannel(); // force re-create with new carrier freq
  }

  pinMode(outputPin, OUTPUT);
  digitalWrite(outputPin, LOW);

  switch (config.irProtocol) {
    case IR_PROTOCOL_NEC:
      sendNEC(config.irCode, outputPin);
      delay(40);
      // Send repeat codes
      _itemCount = 0;
      necAddRepeat();
      transmit(outputPin);
      delay(40);
      _itemCount = 0;
      necAddRepeat();
      transmit(outputPin);
      break;

    case IR_PROTOCOL_SAMSUNG:
      sendSamsung(config.irCode, outputPin);
      delay(40);
      sendSamsung(config.irCode, outputPin);
      delay(40);
      sendSamsung(config.irCode, outputPin);
      break;

    case IR_PROTOCOL_SONY:
      sendSony(config.irCode, config.irBits, outputPin);
      delay(45);
      sendSony(config.irCode, config.irBits, outputPin);
      break;

    default:
      break;
  }
}

// ===== NEC =====
void IRTransmit::sendNEC(uint32_t word, uint8_t pin) {
  uint8_t b0 = (word      ) & 0xFF;
  uint8_t b1 = (word >>  8) & 0xFF;
  uint8_t b2 = (word >> 16) & 0xFF;
  uint8_t b3 = (word >> 24) & 0xFF;

  bool cmdValid = ((uint8_t)~b2 == b3);
  bool addrStandard = ((uint8_t)~b0 == b1);

  _itemCount = 0;

  if (cmdValid && addrStandard) {
    necAddFrame(b0, b2);
  } else if (cmdValid) {
    uint16_t addr16 = ((uint16_t)b1 << 8) | b0;
    necAddFrameExtended(addr16, b2);
  } else {
    necAddFrame(b0, b2);
  }

  transmit(pin);
}

void IRTransmit::necAddFrame(uint8_t addr, uint8_t cmd) {
  // Leader: 9ms mark, 4.5ms space
  addItem(9000, 4500);

  necAddByteLSB(addr);
  necAddByteLSB((uint8_t)~addr);
  necAddByteLSB(cmd);
  necAddByteLSB((uint8_t)~cmd);

  // Stop bit
  addItem(560, 560);
}

void IRTransmit::necAddFrameExtended(uint16_t addr16, uint8_t cmd) {
  // Leader
  addItem(9000, 4500);

  necAddByteLSB((uint8_t)(addr16 & 0xFF));
  necAddByteLSB((uint8_t)(addr16 >> 8));
  necAddByteLSB(cmd);
  necAddByteLSB((uint8_t)~cmd);

  // Stop
  addItem(560, 560);
}

void IRTransmit::necAddRepeat() {
  // NEC repeat: 9ms mark, 2.25ms space, 560us mark
  addItem(9000, 2250);
  addItem(560, 560);
}

void IRTransmit::necAddByteLSB(uint8_t b) {
  for (uint8_t i = 0; i < 8; i++) {
    addItem(560, (b & 0x01) ? 1690 : 560);
    b >>= 1;
  }
}

// ===== Samsung =====
void IRTransmit::sendSamsung(uint32_t word, uint8_t pin) {
  _itemCount = 0;

  // Leader (4.5ms/4.5ms)
  addItem(4500, 4500);

  // 32 bits LSB-first
  uint32_t data = word;
  for (uint8_t i = 0; i < 32; i++) {
    addItem(560, (data & 0x1) ? 1690 : 560);
    data >>= 1;
  }

  // Stop
  addItem(560, 560);

  transmit(pin);
}

// ===== Sony SIRC =====
void IRTransmit::sendSony(uint32_t data, uint8_t bits, uint8_t pin) {
  _itemCount = 0;

  // Leader: 2.4ms mark, 600us space
  addItem(2400, 600);

  // LSB-first
  for (uint8_t i = 0; i < bits; i++) {
    addItem(600, (data & 0x1) ? 1200 : 600);
    data >>= 1;
  }

  // End with a mark to ensure proper termination
  addItem(600, 0);

  transmit(pin);
}
