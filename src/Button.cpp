#include <Arduino.h>
#include <Button.h>
#include <Timer.h>

Button::Button(int button_id) {
    _button_id = button_id;
    _last_button_state = HIGH;

    pinMode(_button_id, INPUT_PULLUP);
}

int Button::read_button(void) {
    int button_state = digitalRead(_button_id);

    if (button_state != _last_button_state) {  // if state has changed
        _hold_timer.reset();                   // reset the hold timer since it is not reset on a hold event
        if (_debounce_timer.has_elapsed()) {   // only acknowledge if debounce time has elapsed
            _last_button_state = button_state;

            return button_state;
        }
    } else {                                   // if state has not changed
        if (_hold_timer.has_elapsed(false)) {  // return the state if hold time has elapsed, and keep firing it by not resetting the timer
            _last_button_state = button_state;
            return button_state;
        }
    }

    return -1;  // no button state change and no hold event
}
