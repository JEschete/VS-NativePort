// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Audio/Driver/SoundDriver.hpp"

#include "Legacy/Psx.hpp"

// decomp: _soundEvent (_soundEvent@SLUS_010.40)
// contents: module image (ModuleData.cpp)
int VS::Audio::Driver::legacySpuEvent;

// decomp: D_8002F548 (D_8002F548@SLUS_010.40)
// contents: module image (ModuleData.cpp)
int VS::Audio::Driver::recentTickCostTotal;

// decomp: D_8002F550 (D_8002F550@SLUS_010.40)
// contents: module image (ModuleData.cpp)
int VS::Audio::Driver::recentTickCosts[VS::Audio::Seq::kRecentTickCostCount];

// decomp: _soundFlush (_soundFlush@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint8_t VS::Audio::Driver::legacySpuFlushBytes
    [VS::Audio::Driver::kLegacySpuFlushByteCount];

// decomp: _spuMemInfo (_spuMemInfo@SLUS_010.40)
std::uint8_t VS::Audio::Driver::legacySpuAllocatorTable;

// decomp: D_800378C0 (D_800378C0@SLUS_010.40)
VS::Audio::LegacyCommandArguments VS::Audio::Driver::commandArguments;

// decomp: g_Sound_GlobalFlags (g_Sound_GlobalFlags@SLUS_010.40)
VS::Audio::DriverFlags VS::Audio::Driver::controlLatches;
