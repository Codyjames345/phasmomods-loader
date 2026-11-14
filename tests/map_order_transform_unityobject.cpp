// Translation unit 3 of map_headers_compile_in_any_order: UnityObject.h before Transform.h, the
// order opposite to the one most code uses (the alphabetical includes every header writes take
// Transform.h first). Compiling this file is the check.
#include "UnityObject.h"
#include "Transform.h"
