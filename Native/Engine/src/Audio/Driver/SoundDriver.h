// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Audio_CommandArguments;
struct Audio_DriverFlags;

// decomp: D_80039AFC (D_80039AFC@SLUS_010.40): a view into another global
#define Audio_Driver_mixBehavior (*(int*)(&Audio_Driver_controlLatches.mixBehavior))

// decomp: D_80039B00 (D_80039B00@SLUS_010.40): a view into another global
#define Audio_Driver_pendingUpdates (*(int*)(&Audio_Driver_controlLatches.updateFlags))

// decomp: D_800378C0_t (src/SLUS_010.40/25AC.c:65)
struct Audio_CommandArguments {
    void* param1;
    int param2;
    int param3;
    int param4;
    int extParam1;
    int extParam2;
};

// decomp: FSoundGlobalFlags (src/include/sound.h:203)
struct Audio_DriverFlags {
    u_int controlLatches; // one-shot / transactional engine state flags
    u_int mixBehavior; // global mixing & music-stack behavior flags
    u_int updateFlags; // deferred SPU / voice-mode update flags
};
static_assert(sizeof(Audio_DriverFlags) == 0xC, "Audio_DriverFlags must keep its PS1 layout");

extern int Audio_Driver_legacySpuEvent;
extern int Audio_Driver_recentTickCostTotal;
extern int Audio_Driver_recentTickCosts[4];
extern char Audio_Driver_legacySpuFlushBytes[64];
extern char Audio_Driver_legacySpuAllocatorTable;
extern Audio_CommandArguments Audio_Driver_commandArguments;
extern Audio_DriverFlags Audio_Driver_controlLatches;
