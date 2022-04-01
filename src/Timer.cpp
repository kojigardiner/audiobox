#include <Timer.h>
#include <Arduino.h>

Timer::Timer(uint32_t timeout_ms) {
    _timeout_ms = timeout_ms;
    _last_time_ms = millis();
}

bool Timer::has_elapsed(bool reset) {
    if ((millis() - _last_time_ms) > _timeout_ms) {
        if (reset) {
            this->reset();
        }
        return true;
    }
    return false;
}

uint32_t Timer::get_time_elapsed_ms(void) {
    return (millis() - _last_time_ms);
}

void Timer::reset(void) {
    _last_time_ms = millis();
}

void Timer::set_timeout_ms(uint32_t timeout_ms) {
    _timeout_ms = timeout_ms;
}

uint32_t Timer::get_timeout_ms(void) {
    return _timeout_ms;
}