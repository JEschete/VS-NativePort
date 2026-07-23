/* Host stand-in for the PS1 scratchpad (1KB fast RAM at 0x1F800000). */
#include <psx_scratchpad.h>
static unsigned long _scratch[1024 / sizeof(unsigned long)];
unsigned long* _psx_scratchpad(void) { return _scratch; }
