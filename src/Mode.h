#ifndef _MODE_H
#define _MODE_H

#include <Arduino.h>

#include "Constants.h"
#include "Timer.h"

class Mode {
   public:
    // Constructors
    Mode(){};
    Mode(int id, uint8_t servo_pos, int duration_ms = 0);

    uint8_t get_servo_pos();  // get servo position associate with the mode
    bool elapsed();           // check if a mode's timer has elapsed
    int id();                 // return the mode id
    void reset_timer();
    void description();

   private:
    int _id;
    unsigned long _duration_ms;
    Timer _timer;
    uint8_t _servo_pos;
};

struct curr_mode_t {
    Mode main;
    Mode sub;
};

#endif  // _MODE_H