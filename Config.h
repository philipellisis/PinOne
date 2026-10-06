#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>


class Config {

  public:
    Config();
    void saveConfig();
    void init();
    // Per-output byte, packed so a second array isn't needed to add settings:
    //  - bits 0-2 (mask 0x07): output_type (see Enums.h) - NONE/NOISY/
    //    LIGHT_SHOW_MEDIUM/LIGHT_SHOW_HIGH/SHARED
    //  - bit 3 (mask 0x08): 0 = output is PWM/dimmable (default, matches
    //    existing behavior), 1 = output is on/off only
    // Use getOutputType()/setOutputType() and isOutputPwm()/setOutputPwm()
    // below rather than reading/writing this array directly.
    unsigned char toySpecialOption[63] = {9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

    unsigned char getOutputType(unsigned char outputId) const {
      return toySpecialOption[outputId] & 0x07;
    }
    void setOutputType(unsigned char outputId, unsigned char type) {
      toySpecialOption[outputId] = (toySpecialOption[outputId] & ~0x07) | (type & 0x07);
    }
    bool isOutputPwm(unsigned char outputId) const {
      return (toySpecialOption[outputId] & 0x08) == 0;
    }
    void setOutputPwm(unsigned char outputId, bool pwm) {
      if (pwm) {
        toySpecialOption[outputId] &= ~0x08;
      } else {
        toySpecialOption[outputId] |= 0x08;
      }
    }
    unsigned char turnOffState[63] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,26,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned char maxOutputState[63] = {255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
    unsigned char maxOutputTime[63] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,200,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    // Plunger calibration is stored in millivolts (0-3300 ADC range)
    int plungerMax = 2716;
    int plungerMin = 197;
    int plungerMid = 655;
    unsigned char solenoidButtonMap[4] = {0};
    unsigned char solenoidOutputMap[4] = {0};
    unsigned char irOutputPin = 255;
    unsigned char irButton = 255;
    unsigned char irProtocol = 0;
    uint32_t irCode = 0;
    unsigned char irBits = 0;
    unsigned char orientation = 0;
    unsigned char accelerometerEprom = 0;
    unsigned char accelerometer = 1;
    unsigned char accelerometerSensitivity = 0;
    int accelerometerDeadZone = 30;
    int accelerometerTilt = 250;
    int accelerometerTiltY = 250;
    int accelerometerMax = 280;
    int accelerometerMaxY = 280;
    unsigned char plungerButtonPush = 0;

    unsigned char plungerAverageRead = 10;
    unsigned char nightModeButton = 14;
    unsigned char plungerLaunchButton = 23;
    unsigned char tiltButton = 22;
    unsigned char shiftButton = 2;

    unsigned char tiltButtonUp = 9;
    unsigned char tiltButtonDown = 10;
    unsigned char tiltButtonLeft = 11;
    unsigned char tiltButtonRight = 12;

    unsigned char done = 0;
    bool nightMode = false;
    bool debug = false;
    void updateConfigFromSerial();
    void sendConfig();
    void setPlunger();
    void setAccelerometer();
    bool lowLatencyMode = false;
    unsigned char plungerRestingDeadZone = 161;  // mV

    unsigned char lightShowState = 1;

    unsigned char buttonKeyboard[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned char buttonRemap[32] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32};

    // Input profiles: profile 0 always uses buttonKeyboard[] above. Profiles 1-3 are
    // alternate keyboard maps selected by holding profileSwitchButton - only the
    // keyboard mapping changes between profiles; buttonRemap/buttonKeyDebounce stay
    // shared/global across all profiles.
    unsigned char buttonKeyboardProfile2[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned char buttonKeyboardProfile3[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned char buttonKeyboardProfile4[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    // Number of additional profiles enabled beyond the default (0-3). 0 disables profile switching.
    unsigned char profileCount = 0;
    // 0-based button index that triggers profile switching (default: button 9, index 8).
    unsigned char profileSwitchButton = 8;
    // Milliseconds the switch button must be held before switching; 0 = switch immediately on press.
    int profileSwitchHoldTime = 5000;
    // Up to 4 output IDs (1-based, 0 = unused) to pulse when a profile becomes active.
    // Flattened [profile * 4 + slot], profile 0 = default profile.
    unsigned char profileNotifyOutputs[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    // Number of on/off pulses sent to the configured outputs for each profile.
    unsigned char profileNotifyPulseCount[4] = {3,3,3,3};
    // Currently active profile (0-3). Not persisted; always starts at 0 (default) on boot.
    unsigned char activeProfile = 0;

    unsigned char getButtonKeyboard(unsigned char keyIndex) const {
      switch (activeProfile) {
        case 1: return buttonKeyboardProfile2[keyIndex];
        case 2: return buttonKeyboardProfile3[keyIndex];
        case 3: return buttonKeyboardProfile4[keyIndex];
        default: return buttonKeyboard[keyIndex];
      }
    }
    unsigned char buttonKeyDebounce[24] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    bool lastButtonState[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    bool processedButtonState[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    unsigned char buttonDebounceCounter = 0;
    bool plungerMoving = false;
    long restingStateMax = 160;
    bool updateUSB = false;
    bool disableUSBSuspend = true;
    bool buttonPressed = false;
    // 0 is ignore accel option
    // 1 is quick release option
    // 2 ignore when not in use option
    // 3
    bool disableAccelOnPlungerMove = true;
    unsigned char  enablePlungerQuickRelease = true;
    bool disablePlungerWhenNotInUse = true;
    // 0 = gamepad only (legacy value, no longer user-selectable in the config tool),
    // 1 = keyboard only (falls back to gamepad if no keyboard mapping),
    // 2 = keyboard AND gamepad simultaneously. Global setting, applies across all
    // input profiles - profiles only change which keys are mapped.
    unsigned char disableButtonPressWhenKeyboardEnabled = 1;
    bool enablePlunger = true;
    bool bluetoothEnable = false;

    unsigned char tiltSuppress = 10;
    bool lightShowAttractEnabled = true;
    unsigned char lightShowTime = 10;
    bool reverseButtonOutputPolarity = true;

    // Velocity-based accelerometer input (see VP's "Accelerometer Velocity Input"
    // feature). When enabled, in addition to the usual raw acceleration on the
    // X/Y gamepad axes, the integrated velocity is reported on the Rx/Ry axes.
    bool accelerometerVelocityEnabled = false;
    int accelerometerVelocityDecayTime = 2000;  // velocity half-life, in milliseconds
    int accelerometerVelocityScale = 100;       // INT16 units per mm/s

    // Legacy RS232/CDC serial DOF fallback for un-updated DirectOutput
    // installs (old DOF only knows the old serial protocol; admin/config
    // traffic is USB HID only regardless of this setting). Default on for
    // backward compatibility; disable once DOF is updated, to skip the
    // extra ComSerial polling in Communication::communicate() entirely.
    bool legacySerialDofEnabled = true;

    // Diagnostic-only hardware status, set by firmware at boot - not user
    // configurable and not persisted to preferences. The config tool reads
    // these (sent at the end of sendConfig()) to tell the user whether each
    // expansion board / the accelerometer was actually found, and on which
    // I2C address ("channel") it responded. 0 = not detected.
    unsigned char expansionBoard1State = 0;
    unsigned char expansionBoard2State = 0;
    unsigned char expansionBoard3State = 0;
    unsigned char mpuState = 0;

  private:
    unsigned char blockRead();
    void printError();
    void printComma(unsigned char value);
    void printIntComma(int value);
    void printSuccess();
    int16_t readIntFromByte();
    void readConfigArray(unsigned char* configArray, unsigned char size);
    void printConfigArray(unsigned char* configArray, unsigned char size);

};

#endif
