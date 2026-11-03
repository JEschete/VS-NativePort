// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Driver/SoundDriver.h"

// decomp: _soundEvent (_soundEvent@SLUS_010.40)
int Audio_Driver_legacySpuEvent; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F548 (D_8002F548@SLUS_010.40)
int Audio_Driver_recentTickCostTotal; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F550 (D_8002F550@SLUS_010.40)
int Audio_Driver_recentTickCosts[4]; /* contents: module image (ModuleData.cpp) */

// decomp: _soundFlush (_soundFlush@SLUS_010.40)
char Audio_Driver_legacySpuFlushBytes[64]; /* contents: module image (ModuleData.cpp) */

// decomp: _spuMemInfo (_spuMemInfo@SLUS_010.40)
char Audio_Driver_legacySpuAllocatorTable;

// decomp: D_800378C0 (D_800378C0@SLUS_010.40)
Audio_CommandArguments Audio_Driver_commandArguments;

// decomp: g_Sound_GlobalFlags (g_Sound_GlobalFlags@SLUS_010.40)
Audio_DriverFlags Audio_Driver_controlLatches;

}  // extern "C"
