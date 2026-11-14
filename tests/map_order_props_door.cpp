// Translation unit 2 of map_headers_compile_in_any_order: Props.h before Door.h, the order opposite
// to the one most code uses (Door.h and GhostAI.h include Props.h, and all.h takes Door.h first).
// Compiling this file is the check.
#include "Props.h"
#include "Door.h"
