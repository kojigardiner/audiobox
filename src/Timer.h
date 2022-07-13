#include <Arduino.h>

#ifndef _TIMER_H
#define _TIMER_H

class Timer {
   public:
    // Constructor
    Timer(uint32_t timeout_ms = 0);

    bool has_elapsed(bool reset = true);
    void reset();
    void set_timeout_ms(uint32_t timeout_ms);
    uint32_t get_timeout_ms();
    uint32_t get_time_elapsed_ms();

   private:
    uint32_t _timeout_ms;
    uint32_t _last_time_ms;
};

#endif  // _TIMER_H
