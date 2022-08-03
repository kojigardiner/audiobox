#include "Mode.h"

#include "Utils.h"

// Constructor
Mode::Mode(int id, uint8_t servo_pos, int duration_ms) {
    this->_id = id;
    this->_servo_pos = servo_pos;
    this->_duration_ms = duration_ms;
    this->_timer = Timer(duration_ms);
}

bool Mode::elapsed() {
    if (this->_timer.get_timeout_ms() != 0) {
        return _timer.has_elapsed();
    } else {
        return false;
    }
}

void Mode::reset_timer() {
    _timer.reset();
}

int Mode::id() {
    return _id;
}

uint8_t Mode::get_servo_pos() {
    return _servo_pos;
}

void Mode::description() {
    print("ID: %d, Servo: %d, Duration: %d\n", _id, _servo_pos, _duration_ms);
}