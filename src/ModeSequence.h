#ifndef _MODESEQUENCE_H
#define _MODESEQUENCE_H

#include <Arduino.h>

// Forward declarations
class Mode;
struct curr_mode_t;

class ModeSequence {
   public:
    ModeSequence(Mode *modes, int len, ModeSequence *_submode_seqs = 0);

    Mode mode();
    Mode submode();
    void next();
    void submode_next();
    bool set(int idx);
    void description();
    curr_mode_t get_curr_mode();

   private:
    Mode *_modes;                 // array of modes
    ModeSequence *_submode_seqs;  // array of mode sequences
    int _idx;
    int _len;
};

#endif  // _MODESEQUENCE_H