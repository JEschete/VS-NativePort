// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_MIXER_MIXCONTROL_H_TYPES
#define VS_AUDIO_MIXER_MIXCONTROL_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_CommandParams Audio_CommandParams;

// decomp: FSoundCommandParams (src/include/sound.h:501)
typedef struct Audio_CommandParams {
    uintptr_t param1;
    u_int param2;
    u_int param3; // Seems to be usually flags
    u_int param4;
    u_int extParam1;
    u_int extParam2;
} Audio_CommandParams;

// decomp: FSoundCommandHandler (src/SLUS_010.40/25AC.c:77)
typedef void (*Audio_CommandHandler)(Audio_CommandParams*);

#endif // VS_AUDIO_MIXER_MIXCONTROL_H_TYPES
