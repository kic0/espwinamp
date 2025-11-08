#ifndef ICONS_H
#define ICONS_H

#include <pgmspace.h>

// Bluetooth Icon (8x8 pixels)
const unsigned char PROGMEM bt_icon[] = {
    0b00111000,
    0b01000100,
    0b10010010,
    0b01010100,
    0b01010100,
    0b10010010,
    0b01000100,
    0b00111000
};

// Play Icon (8x8 pixels)
const unsigned char PROGMEM play_icon[] = {
    0b00011000,
    0b00011100,
    0b00011110,
    0b00011111,
    0b00011110,
    0b00011100,
    0b00011000,
    0b00000000
};

// WiFi Icon (8x8 pixels)
const unsigned char PROGMEM wifi_icon[] = {
    0b00011000,
    0b00100100,
    0b01000010,
    0b00011000,
    0b00100100,
    0b01000010,
    0b00000000,
    0b00011000
};

#endif // ICONS_H
