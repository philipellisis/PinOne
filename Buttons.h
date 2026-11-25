#ifndef BUTTON_H
#define BUTTON_H
#include <Arduino.h>
#include "Config.h"
#include "Outputs.h"


class Buttons {

  public:
    Buttons();
    void readInputs();
    bool checkChanged();
    unsigned char numberButtonsPressed = 0;
    void init();
    // True if any physical button is held down. For the power-up check in
    // setup(): holding a button boots in normal USB mode even when XInput mode
    // is selected. Takes about 20 ms.
    bool anyButtonHeldAtBoot();
    void sendButtonState();
    bool sendButtonPush(unsigned char i, bool currentButtonState);
    // Safety net: when no physical button is held any more, nothing may stay pressed in
    // the USB reports either. Call every pass.
    void reconcile();


  private:
    unsigned char buttonOffset = 0;
    unsigned long profileSwitchPressStart = 0;
    unsigned long idleSince = 0;
    bool profileSwitchImmediateFired = false;

    bool isButtonLocked(unsigned char button);
    void sendActualButtonPress(unsigned char buttonOffset, bool currentButtonState);
    void handleMediaKey(unsigned char keyCode, bool pressed);
    void processKeyboardAction(unsigned char keyCode, bool pressed);
    void checkProfileSwitchButton(unsigned char buttonIndex, bool currentButtonState);
    void switchProfile();
    void notifyProfileChange(unsigned char profileIndex);
};

#endif
