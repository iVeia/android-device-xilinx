#include "debug.hh"

// Gloval debug class for printing information
//  Note that this is not thread safe, so calling this from multiple
//  threads will result in interleaved printing
// If I want to add file logging to this, then some sort of mutex protection is going
//  to be needed, or else I think it will just fail
iVeiOTA::Debug iVeiOTA::debug;
