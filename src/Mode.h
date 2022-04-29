#ifndef _MODE_H
#define _MODE_H

#include <Arduino.h>

#include "Constants.h"
#include "Timer.h"

class Mode {
   public:
    Mode();
    Mode(int initial_mode, int num_modes, const unsigned long *durations = NULL);

    void cycle_mode();
    void set_mode(int mode);
    int get_mode();
    bool mode_elapsed();

   private:
    int _num_modes;
    int _mode;
    const unsigned long *_durations;
    Timer _timer = Timer();
};

#endif  // _MODE_H