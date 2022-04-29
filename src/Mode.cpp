#include "Mode.h"

// Constructor
Mode::Mode() {
    _durations = NULL;
    _num_modes = 0;

    set_mode(0);
}

Mode::Mode(int initial_mode, int num_modes, const unsigned long *durations) {
    _durations = durations;
    _num_modes = num_modes;

    set_mode(initial_mode);
}

void Mode::cycle_mode() {
    set_mode((_mode + 1) % _num_modes);  // cycle through the modes
}

void Mode::set_mode(int mode) {
    _mode = mode;
    if (_durations != NULL) {
        _timer.set_timeout_ms(_durations[_mode]);
    }
}

int Mode::get_mode() {
    return _mode;
}

bool Mode::mode_elapsed() {
    return _timer.has_elapsed();
}