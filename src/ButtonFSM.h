#ifndef _BUTTONFSM_H
#define _BUTTONFSM_H

#include <Arduino.h>

#define DEBOUNCE_TIMEOUT_MS 50
#define BUTTON_HOLD_TIME_MS 500
#define BUTTON_FSM_CYCLE_TIME_MS 10

class ButtonFSM {
   public:
    // Constructor
    ButtonFSM(int button_id);

    enum button_fsm_state_t {
        START,
        WAIT,
        ARMED,
        HOLD,
        MOMENTARY_TRIGGERED,
        HOLD_TRIGGERED
    };

    // Move to next state in state machine
    void advance();

    // Return current state in state machine
    button_fsm_state_t get_state();

    // Get id of button
    int get_id();

   private:
    // Read physical button pin
    void _read();

    button_fsm_state_t _state = START;
    button_fsm_state_t _prev_state = START;

    int _button_read = HIGH;
    int _button_id = -1;

    unsigned long _t0_ms = 0;
};

#endif  // _BUTTONFSM_H
