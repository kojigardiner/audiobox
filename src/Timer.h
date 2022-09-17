#include <Arduino.h>

#ifndef _TIMER_H
#define _TIMER_H

// A lightweight software Timer object. Accepts a single timeout 
// parameter in milliseconds and exposes methods for checking if
// the timer has elapsed.
class Timer {
   public:
    // Constructor, accepts a timeout parameter in milliseconds.
    Timer(uint32_t timeout_ms = 0);

    // Returns true if the time that has elapsed since the last reset is > than the timeout.
    // Returns false otherwise. Accepts an optional argument to reset the timer if the it has
    // indeed elapsed.
    bool has_elapsed(bool reset = true);

    // Resets the timer to start counting from when the method is called.
    void reset();

    // Sets the timeout value explicitly, in milliseconds.
    void set_timeout_ms(uint32_t timeout_ms);

    // Returns the timeout value in milliseconds.
    uint32_t get_timeout_ms();

    // Returns how much time has elapsed since the last reset, in milliseconds.
    uint32_t get_time_elapsed_ms();

   private:
    uint32_t _timeout_ms;   // timeout period, in milliseconds
    uint32_t _last_time_ms; // last time of reset, in milliseconds
};

#endif  // _TIMER_H
