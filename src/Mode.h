#ifndef _MODE_H
#define _MODE_H

#include <Arduino.h>

#include "Constants.h"
#include "Timer.h"

class Mode {
   public:
    // Constructors
    Mode(int id, int servo_pos, int duration_ms = 0);

    int get_servo_pos();  // get servo position associate with the mode
    bool elapsed();       // check if a mode's timer has elapsed
    int id();             // return the mode id
    void reset_timer();

   private:
    int _id;
    unsigned long _duration_ms;
    Timer _timer;
    int _servo_pos;
};

#endif  // _MODE_H