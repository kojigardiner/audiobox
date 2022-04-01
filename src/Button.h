#include <Arduino.h>
#include <Timer.h>

#ifndef _BUTTON_H
#define _BUTTON_H

#define DEBOUNCE_TIMEOUT_MS 50
#define BUTTON_HOLD_TIME_MS 500

class Button {
    public:
        Button(int button_id);
        int read_button(void);
    private:
        int _button_id;
        int _last_button_state;
        Timer _debounce_timer = Timer(DEBOUNCE_TIMEOUT_MS);
        Timer _hold_timer = Timer(BUTTON_HOLD_TIME_MS);
        //uint32_t _last_debounce_time_ms;
};

#endif // _BUTTON_H
