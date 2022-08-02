#include "ModeSequence.h"

#include "Mode.h"

// Constructor
ModeSequence::ModeSequence(Mode *modes, int len, ModeSequence *submode_seqs) {
    this->_modes = modes;
    this->_len = len;
    this->_idx = 0;
    this->_submode_seqs = submode_seqs;
}

void ModeSequence::next() {
    _idx = (_idx + 1) % _len;
    _modes[_idx].reset_timer();
}

Mode ModeSequence::mode() {
    return _modes[_idx];
}

Mode ModeSequence::submode() {
    if (_submode_seqs == 0) {
        return _modes[_idx];  // just return the current mode
    }
    return _submode_seqs[_idx].mode();
}

void ModeSequence::submode_next() {
    if (_submode_seqs == 0) {
        _submode_seqs[_idx].next();
    }
}

bool ModeSequence::set(int idx) {
    if (idx < _len) {
        _idx = idx;
        return true;
    }
    return false;
}