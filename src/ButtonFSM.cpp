#include "ButtonFSM.h"

// Constructor
ButtonFSM::ButtonFSM(int button_id) {
    _button_id = button_id;

    pinMode(_button_id, INPUT_PULLUP);
}

// Advance the state machine
void ButtonFSM::advance() {
    _prev_state = _state;

    _read();  // read the current button state

    switch (_state) {
        case START:  // Initial state, wait for button to go low and start the timer
            if (_button_read == LOW) {
                _state = WAIT;
                _t0_ms = millis();
            }
            break;
        case WAIT:  // Restart if button goes high, otherwise, only advance if we have exceeded the debounce time
            if (_button_read == HIGH) {
                _state = START;
            } else if ((millis() - _t0_ms) > DEBOUNCE_TIMEOUT_MS) {
                _state = ARMED;
            }
            break;
        case ARMED:  // Once we are armed, if button goes high, flag it immediately and reset; otherwise, keep track until the hold time
            if (_button_read == HIGH) {
                _state = MOMENTARY_TRIGGERED;
            } else if ((millis() - _t0_ms) > BUTTON_HOLD_TIME_MS) {
                _state = HOLD;
            }
            break;
        case HOLD:  // Once the hold timer has exceeded, wait for the button to go high
            if (_button_read == HIGH) {
                _state = HOLD_TRIGGERED;
            }
            break;
        case MOMENTARY_TRIGGERED:  // We need a state here so that the calling function can recognize what has happened before we reset
            _state = START;
            break;
        case HOLD_TRIGGERED:  // We need a state here so that the calling function can recognize what has happened before we reset
            _state = START;
            break;
    }
}

// Read the button
void ButtonFSM::_read() {
    _button_read = digitalRead(_button_id);
}

// Return the current state
ButtonFSM::button_fsm_state_t ButtonFSM::get_state() {
    return _state;
}