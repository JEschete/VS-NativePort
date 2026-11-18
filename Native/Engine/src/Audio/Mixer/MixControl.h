// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Audio_CommandParams;

// decomp: FSoundCommandParams (src/include/sound.h:501)
struct Audio_CommandParams {
    uintptr_t param1;
    u_int param2;
    u_int param3; // Seems to be usually flags
    u_int param4;
    u_int extParam1;
    u_int extParam2;
};

// decomp: FSoundCommandHandler (src/SLUS_010.40/25AC.c:77)
using Audio_CommandHandler = void (*)(Audio_CommandParams*);
