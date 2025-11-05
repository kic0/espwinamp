#ifndef LOG_H
#define LOG_H

#include <Arduino.h>

class Log {
public:
    template<typename ... Args>
    static void printf(const char * format, Args ... args) {
        Serial.printf(format, args...);
    }
};

#endif // LOG_H
