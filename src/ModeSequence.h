#ifndef _MODESEQUENCE_H
#define _MODESEQUENCE_H

#include <Arduino.h>

// Forward declarations
class Mode;
struct curr_mode_t;

// The ModeSequence class encapsulates a hierarchical arrangement of operating modes
// in a fixed sequence. A ModeSequence object has an array of Mode objects as well 
// as an optional pointer to an array ModeSequence objects that represent "submodes"
// for each Mode. Class methods enable querying the current mode and/or submode as well
// as changing to the next mode in the sequence.
//
// In the Audiobox, this structure is used to create a set of "main" modes that consist
// of Album Art and Audio Visualization. Button 1 on the box switches between these two
// main modes. While within a main mode, there are multiple "sub" modes that can be stepped
// through by pressing Button 2. For the Album Art main mode, these sub modes add LED 
// indications for track progress and color palette. For the Audio Visualization main mode, 
// these sub modes consist of the various LEDAudioPatterns. 
class ModeSequence {
   public:
    // Constructor, accepts a pointer to an array of Mode objects, the length of that
    // array, and an optional pointer to ModeSequences associated with each "higher"
    // level mode.
    ModeSequence(Mode *modes, int len, ModeSequence *_submode_seqs = 0);

    // Returns the current mode.
    Mode mode();

    // Returns the current submode.
    Mode submode();

    // Moves the pointer to the next mode, wrapping around if we have reached the
    // end of the Modes in the array.
    void next();

    // Moves the submode pointer to the next submode, wrapping around if we have reached the
    // end of the Modes in the submode ModeSequence.
    void submode_next();

    // Set the pointer to a specific index in the Modes array.
    bool set(int idx);

    // Print a description of hte current mode and submode.
    void description();

    // Return a struct with the current mode and submode.
    curr_mode_t get_curr_mode();

   private:
    Mode *_modes;                 // array of modes
    ModeSequence *_submode_seqs;  // array of mode sequences associated with each mode
    int _idx;                     // index of the currently active mode
    int _len;                     // length of the _modes array
};

#endif  // _MODESEQUENCE_H