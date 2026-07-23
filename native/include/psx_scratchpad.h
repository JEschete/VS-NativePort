#ifndef NATIVE_PSX_SCRATCHPAD_H
#define NATIVE_PSX_SCRATCHPAD_H
/* Host stand-in for the PS1 1KB scratchpad at 0x1F800000.
 * scratch[0] = next-free primitive pointer, scratch[1] = ordering table base. */
unsigned long* _psx_scratchpad(void);
#endif
