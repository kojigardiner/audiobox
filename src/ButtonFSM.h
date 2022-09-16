#ifndef _BUTTONFSM_H
#define _BUTTONFSM_H

#include <Arduino.h>

#define DEBOUNCE_TIMEOUT_MS 50
#define BUTTON_HOLD_TIME_MS 500
#define BUTTON_FSM_CYCLE_TIME_MS 10

// The ButtonFSM class encapsulates a finite state machine for a push-button, and can
// detect momentary button presses as well as held buttons. The class is *not* interrupt
// based and therefore should be called in a loop with the timeout specified by 
// BUTTON_FSM_CYCLE_TIME_MS in order to ensure button presses are not missed. This is
// most easily accommodated by spawning a separate FreeRTOS task to handle ButtonFSM
// objects. Code inspired by: https://gist.github.com/NormalUniverse/5b3a8a8d909b56b58b67bd3ec2808f4b
class ButtonFSM {
   public:
    // Constructor, accepts a pin number to be used as the button ID
    ButtonFSM(int button_id);

    // FSM states
    enum button_fsm_state_t {
        START,
        WAIT,
        ARMED,
        HOLD,
        MOMENTARY_TRIGGERED,
        HOLD_TRIGGERED
    };

    // Moves to next state in state machine.
    void advance();

    // Returns the current state in state machine.
    button_fsm_state_t get_state();

    // Gets the ID of the button object.
    int get_id();

   private:
    // Reads the physical button pin.
    void _read();

    // Private variables
    button_fsm_state_t _state = START;
    button_fsm_state_t _prev_state = START;

    int _button_read = HIGH;
    int _button_id = -1;

    unsigned long _t0_ms = 0;   // used to measure time between events
};

#endif  // _BUTTONFSM_H
