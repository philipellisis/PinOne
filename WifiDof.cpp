#include "WifiDof.h"
#include "esp_bt.h"
#include "Enums.h"
#include "Globals.h"

volatile uint8_t WifiDof::disconnectReason = 0;
volatile bool WifiDof::linkLost = false;

void WifiDof::begin() {
  applyConfig();
}

void WifiDof::applyConfig() {
  bool enabled = config.wifiEnabled && config.wifiSsid[0] != '\0';
  bool unchanged = enabled == runningEnabled &&
                   (!enabled || (strcmp(config.wifiSsid, runningSsid) == 0 &&
                                 strcmp(config.wifiPassword, runningPassword) == 0));
  if (unchanged) return;
  if (runningEnabled) stop();
  if (enabled) start();
}

void WifiDof::start() {
  static bool eventsRegistered = false;
  if (!eventsRegistered) {
    WiFi.onEvent(onWifiEvent);
    eventsRegistered = true;
  }
  strncpy(runningSsid, config.wifiSsid, sizeof(runningSsid) - 1);
  strncpy(runningPassword, config.wifiPassword, sizeof(runningPassword) - 1);
  runningEnabled = true;
  disconnectReason = 0;
  linkLost = false;

  // The credentials live in the main config; don't let the Wi-Fi driver keep
  // its own copy in NVS as well
  WiFi.persistent(false);
  WiFi.setHostname("pinone");  // must come before the station starts
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(runningSsid, runningPassword[0] ? runningPassword : nullptr);
  // Modem sleep holds received frames until the next beacon (up to ~100 ms).
  // The driver refuses to turn it off while Bluetooth shares the radio, so it
  // can only go when Bluetooth isn't running.
  if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
    WiFi.setSleep(false);
  }
  started = true;
}

void WifiDof::stop() {
  if (udpOpen) {
    udp.stop();
    udpOpen = false;
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  turnOffArtnetOutputs();
  started = false;
  runningEnabled = false;
  runningSsid[0] = '\0';
  runningPassword[0] = '\0';
  frameSeen = false;
  otherUniverseSeen = false;
}

void WifiDof::onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      disconnectReason = 0;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      disconnectReason = info.wifi_sta_disconnected.reason;
      linkLost = true;
      break;
    default:
      break;
  }
}

void WifiDof::service() {
  if (!started) return;

  if (linkLost) {
    // Nothing can turn these off while the link is down, and DOF may be gone
    // for good, so don't leave solenoids or lights on
    linkLost = false;
    turnOffArtnetOutputs();
    if (udpOpen) {
      udp.stop();
      udpOpen = false;
    }
  }

  if (WiFi.status() != WL_CONNECTED) return;
  if (!udpOpen) {
    udpOpen = udp.begin(ARTNET_PORT);
    if (!udpOpen) return;
  }

  // Each frame carries every channel, so only the newest one needs applying
  for (uint8_t n = 0; n < MAX_PACKETS_PER_PASS; n++) {
    int size = udp.parsePacket();
    if (size <= 0) break;
    if (parseFrame(size)) frameReady = true;
    // parsePacket() returns nothing while any of a datagram is left unread, so
    // drop the rest of every one (e.g. a 14-byte ArtPoll that parseFrame skips)
    udp.clear();
  }
  if (frameReady) {
    frameReady = false;
    applyFrame();
  }
}

// Reads one datagram; true if it is an ArtDMX frame for the configured universe
bool WifiDof::parseFrame(int size) {
  if (size < ARTNET_HEADER_SIZE) return false;
  int len = udp.read(packet, sizeof(packet));
  if (len < ARTNET_HEADER_SIZE) return false;
  if (memcmp(packet, "Art-Net", 8) != 0) return false;  // includes the terminating zero
  uint16_t opcode = packet[8] | (packet[9] << 8);
  if (opcode != 0x5000) return false;  // OpDmx

  uint16_t universe = (packet[14] | (packet[15] << 8)) & 0x7FFF;
  uint32_t now = millis();
  if (universe != config.artnetUniverse) {
    lastOtherUniverse = universe;
    lastOtherUniverseMs = now;
    otherUniverseSeen = true;
    return false;
  }
  lastFrameMs = now;
  frameSeen = true;

  uint16_t dataLength = (packet[16] << 8) | packet[17];
  dataLength = min<uint16_t>(dataLength, min<uint16_t>(len - ARTNET_HEADER_SIZE, DMX_CHANNELS));
  // A frame may stop short of 512 channels; outputs past its end keep their values
  frameOutputs = 0;
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    uint16_t channel = config.artnetFirstChannel - 1 + i;
    if (channel >= dataLength) break;
    frame[i] = packet[ARTNET_HEADER_SIZE + channel];
    frameOutputs = i + 1;
  }
  return true;
}

void WifiDof::applyFrame() {
  // Same light-show handling as a USB DOF packet
  if (config.lightShowState != OUTPUT_RECEIVED) {
    lightShow.setLightsOff();
  }
  config.lightShowState = OUTPUT_RECEIVED_RESET_TIMER;
  for (uint8_t i = 0; i < frameOutputs; i++) {
    outputs.updateOutputFromDof(i, frame[i]);
    artnetOutputOn[i] = frame[i] > 0;
  }
}

void WifiDof::turnOffArtnetOutputs() {
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    if (artnetOutputOn[i]) {
      outputs.updateOutput(i, 0);
      artnetOutputOn[i] = false;
    }
  }
}

WifiDof::WifiState WifiDof::currentState() {
  if (!config.wifiEnabled) return WIFI_DISABLED;
  if (config.wifiSsid[0] == '\0') return WIFI_NO_SSID;
  if (!started) return WIFI_CONNECTING;
  if (WiFi.status() == WL_CONNECTED) return WIFI_CONNECTED;
  uint8_t reason = disconnectReason;
  if (reason == WIFI_REASON_NO_AP_FOUND || (reason >= 210 && reason <= 212)) {
    return WIFI_NETWORK_NOT_FOUND;  // 210-212: found only with the wrong security or too weak
  }
  if (reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
      reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
    return WIFI_AUTH_FAILED;
  }
  return WIFI_CONNECTING;
}

void WifiDof::sendStatus() {
  WifiState state = currentState();
  bool connected = state == WIFI_CONNECTED;
  IPAddress ip = connected ? WiFi.localIP() : IPAddress(0, 0, 0, 0);
  int rssi = connected ? -WiFi.RSSI() : 0;

  uint32_t now = millis();
  uint8_t artnet = 0;
  if (frameSeen && now - lastFrameMs < ARTNET_ACTIVE_MS) {
    artnet = 1;
  } else if (otherUniverseSeen && now - lastOtherUniverseMs < ARTNET_ACTIVE_MS) {
    artnet = 2;
  }

  ConfigOut.print(F("W,"));
  ConfigOut.print(state);
  for (uint8_t i = 0; i < 4; i++) {
    ConfigOut.print(F(","));
    ConfigOut.print(ip[i]);
  }
  ConfigOut.print(F(","));
  ConfigOut.print(rssi);
  ConfigOut.print(F(","));
  ConfigOut.print(artnet);
  ConfigOut.print(F(","));
  ConfigOut.print(lastOtherUniverse);
  ConfigOut.print(F("\r\n"));
  ConfigOut.flush();
}
