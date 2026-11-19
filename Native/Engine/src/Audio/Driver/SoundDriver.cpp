// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Driver/SoundDriver.hpp"

// decomp: _soundEvent (_soundEvent@SLUS_010.40)
int VS::Audio::Driver::legacySpuEvent; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F548 (D_8002F548@SLUS_010.40)
int VS::Audio::Driver::recentTickCostTotal; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F550 (D_8002F550@SLUS_010.40)
int VS::Audio::Driver::recentTickCosts[VS::Audio::Seq::kRecentTickCostCount]; /* contents: module image (ModuleData.cpp) */

// decomp: _soundFlush (_soundFlush@SLUS_010.40)
u_char VS::Audio::Driver::legacySpuFlushBytes[VS::Audio::Driver::kLegacySpuFlushByteCount]; /* contents: module image (ModuleData.cpp) */

// decomp: _spuMemInfo (_spuMemInfo@SLUS_010.40)
u_char VS::Audio::Driver::legacySpuAllocatorTable;

// decomp: D_800378C0 (D_800378C0@SLUS_010.40)
VS::Audio::LegacyCommandArguments VS::Audio::Driver::commandArguments;

// decomp: g_Sound_GlobalFlags (g_Sound_GlobalFlags@SLUS_010.40)
VS::Audio::DriverFlags VS::Audio::Driver::controlLatches;

