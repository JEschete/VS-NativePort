// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


#include "Effects/Loading.h"
#include "Effects/Runtime.h"

extern u_char Effects_effectUsesTextures[];
extern u_short Effects_effectEntryPointIndices[400];
extern Effects_EffectRenderer Effects_effectEntryPoints[];
extern Effects_FileLoadContext Effects_fbcFiles[];
extern u_char Effects_fbcSlots[];
extern u_char Effects_fbtCounts[];
extern Effects_FileLoadContext Effects_fbtFiles[];
extern u_char Effects_fbtSlots[];
extern Effects_FileLoadContext Effects_packageFiles[];
extern Effects_FileLoadContext Effects_pluginFiles[];
