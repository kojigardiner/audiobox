#ifndef _MODE_H
#define _MODE_H

#include <Arduino.h>

#include "Constants.h"
#include "Timer.h"

// The Mode class encapsulates an operating mode for the Audiobox. An operating
// mode consists of a servo position for the LED panel, as well as a timer
// that keeps track of how long to remain in particular mode before changing to
// another. Multiple Mode objects can be combined into a ModeSequence, which
// defines the order for switching between modes.
class Mode {
   public:
    // Default constructor, required in order to declare Mode variables prior
    // to initializing them.
    Mode(){};

    // Constructor, accepts an id (see MainModes, ArtSubmode, AudioSubMode in Constants.h),
    // servo position for the LED panel, and a duration prior to switching to another mode.
    Mode(int id, uint8_t servo_pos, int duration_ms = 0);

    // Returns the servo position associated with the mode.
    uint8_t get_servo_pos();

    // Checks if the timer has elapsed.
    bool elapsed();

    // Returns the id.
    int id();

    // Resets the timer to its original value.
    void reset_timer();

    // Prints the details of the mode to serial.
    void description();

   private:
    int _id;
    unsigned long _duration_ms;
    Timer _timer;
    uint8_t _servo_pos;
};

// Structure for associating a main mode with a submode.
struct curr_mode_t {
    Mode main;
    Mode sub;
};

#endif  // _MODE_H