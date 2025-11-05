#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

enum ButtonPress {
    NONE,
    SHORT_PRESS,
    LONG_PRESS
};

class Button {
public:
    Button(int pin, unsigned long long_press_duration = 1000);
    void begin();
    ButtonPress read();

private:
    int pin;
    unsigned long long_press_duration;
    bool pressed;
    unsigned long press_time;
    bool long_press_triggered;
};

#endif // BUTTON_H
