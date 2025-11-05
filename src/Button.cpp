#include "Button.h"

Button::Button(int pin, unsigned long long_press_duration)
    : pin(pin), long_press_duration(long_press_duration), pressed(false), press_time(0), long_press_triggered(false) {}

void Button::begin() {
    pinMode(pin, INPUT_PULLUP);
}

ButtonPress Button::read() {
    bool current_state = !digitalRead(pin);
    ButtonPress event = NONE;

    if (current_state && !pressed) {
        pressed = true;
        press_time = millis();
        long_press_triggered = false;
    } else if (!current_state && pressed) {
        pressed = false;
        if (!long_press_triggered) {
            event = SHORT_PRESS;
        }
    }

    if (pressed && !long_press_triggered && (millis() - press_time >= long_press_duration)) {
        long_press_triggered = true;
        event = LONG_PRESS;
    }

    return event;
}
