// The map headers' include cycles (sdk/internals/mapping.md) must compile in either include order:
// each of this test's translation units includes one cycle's headers in the order opposite to the
// one most code uses. The cycle headers break themselves with after-the-entries includes and forward
// declarations, so this file takes Footstep.h before GhostAI.h (most code reaches Footstep.h through
// GhostAI.h and GhostSenses.h). Compiling all three translation units is the check; main only
// reports that the build reached them. Run: build\map_headers_compile_in_any_order_test.exe
#include "Footstep.h"
#include "GhostAI.h"

int main() { return 0; }
