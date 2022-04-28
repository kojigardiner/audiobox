#ifndef _BUTTONFSM_H
#define _BUTTONFSM_H

#include <Arduino.h>

#define DEBOUNCE_TIMEOUT_MS 50
#define BUTTON_HOLD_TIME_MS 500
#define BUTTON_FSM_CYCLE_TIME_MS 1

enum ButtonFSMState {
    START,
    WAIT,
    ARMED,
    HOLD,
    MOMENTARY_TRIGGERED,
    HOLD_TRIGGERED
};

class ButtonFSM {
   public:
    ButtonFSM(int button_id);

    void advance();
    ButtonFSMState get_state();

   private:
    void _read();

    ButtonFSMState _state = START;
    ButtonFSMState _prev_state = START;

    int _button_read = HIGH;
    int _button_id = -1;

    unsigned long _t0_ms = 0;
};

#endif  // _BUTTONFSM_H
