#ifndef WIFI_DOF_H
#define WIFI_DOF_H
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// DOF over Wi-Fi: joins the configured network and listens for ArtNet (DMX over
// UDP), which libdof can send on platforms that have no USB/serial/HID drivers,
// such as Android on a Meta Quest. Each ArtDMX frame for the configured universe
// sets outputs 1-63 from DMX channels artnetFirstChannel onwards, through the
// same path as USB DOF packets.
//
// begin() runs in setup(); service() and the rest run on the output task, which
// owns the outputs and the light show.
class WifiDof {

  public:
    void begin();
    // Applies received ArtNet frames; call from the output task loop
    void service();
    // After a config upload: reconnects if Wi-Fi was turned on/off or the
    // network changed. Universe and channel changes apply without it.
    void applyConfig();
    // Answers the config tool's WIFI_STATUS command:
    // W,<state>,<ip0>,<ip1>,<ip2>,<ip3>,<rssi>,<artnet>,<otherUniverse>
    //  state: see WifiState; rssi: signal in -dBm (0 = not connected)
    //  artnet: 0 = no ArtNet in the last few seconds, 1 = receiving the
    //  configured universe, 2 = receiving only other universes (the last one
    //  seen is otherUniverse)
    void sendStatus();

    enum WifiState : uint8_t {
      WIFI_DISABLED = 0,
      WIFI_NO_SSID = 1,
      WIFI_CONNECTING = 2,
      WIFI_CONNECTED = 3,
      WIFI_NETWORK_NOT_FOUND = 4,
      WIFI_AUTH_FAILED = 5,
    };

  private:
    static const uint16_t ARTNET_PORT = 6454;
    static const uint16_t ARTNET_HEADER_SIZE = 18;
    static const uint16_t DMX_CHANNELS = 512;
    // How long after the last frame ArtNet still counts as being received
    static const uint32_t ARTNET_ACTIVE_MS = 5000;
    // Packets read per service() pass, so a flood can't hold up the output task
    static const uint8_t MAX_PACKETS_PER_PASS = 8;
    static const uint8_t NUM_OUTPUTS = 63;

    void start();
    void stop();
    bool parseFrame(int size);
    void applyFrame();
    void turnOffArtnetOutputs();
    WifiState currentState();
    static void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info);

    WiFiUDP udp;
    bool started = false;
    bool udpOpen = false;
    // Settings the running connection was started with, to spot changes
    bool runningEnabled = false;
    char runningSsid[33] = "";
    char runningPassword[65] = "";

    uint8_t packet[ARTNET_HEADER_SIZE + DMX_CHANNELS];
    // Last frame for the configured universe, as output values
    uint8_t frame[NUM_OUTPUTS] = {0};
    uint8_t frameOutputs = 0;  // outputs the frame reached (it may stop short of 512 channels)
    bool frameReady = false;
    // Outputs the last applied frame left on, turned off if the link drops
    bool artnetOutputOn[NUM_OUTPUTS] = {false};
    uint32_t lastFrameMs = 0;
    bool frameSeen = false;
    uint32_t lastOtherUniverseMs = 0;
    bool otherUniverseSeen = false;
    uint16_t lastOtherUniverse = 0;

    // Set from the Wi-Fi event task
    static volatile uint8_t disconnectReason;
    static volatile bool linkLost;
};

#endif
