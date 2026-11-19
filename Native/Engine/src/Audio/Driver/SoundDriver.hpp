#pragma once
// The sound driver's command arguments, control latches, tick costs and the
// libspu state the original kept beside them.
#include <cstddef>
#include <cstdint>

#include "Audio/SeqSequencer.hpp"

namespace VS::Audio {
// decomp: D_800378C0_t (src/SLUS_010.40/25AC.c:65)
struct LegacyCommandArguments {
  void* param1;
  std::int32_t param2;
  std::int32_t param3;
  std::int32_t param4;
  std::int32_t ext_param1;
  // Also the music transition timer (the decomp's D_800378D4 view).
  std::int32_t ext_param2;
};

// decomp: FSoundGlobalFlags (src/include/sound.h:203)
struct DriverFlags {
  std::uint32_t control_latches;  // one-shot / transactional engine state flags
  std::uint32_t mix_behavior;     // global mixing & music-stack behavior flags
  std::uint32_t update_flags;     // deferred SPU / voice-mode update flags
};

namespace layout {
inline constexpr std::size_t kDriverFlagsBytes = 0xC;
static_assert(sizeof(DriverFlags) == kDriverFlagsBytes);
}  // namespace layout
}  // namespace VS::Audio

namespace VS::Audio::Driver {
inline constexpr std::size_t kLegacySpuFlushByteCount = 64;

// decomp: _soundEvent (_soundEvent@SLUS_010.40)
extern std::int32_t legacySpuEvent;
// decomp: D_8002F548 (D_8002F548@SLUS_010.40)
extern std::int32_t recentTickCostTotal;
// decomp: D_8002F550 (D_8002F550@SLUS_010.40)
extern std::int32_t recentTickCosts[Seq::kRecentTickCostCount];
// decomp: _soundFlush (_soundFlush@SLUS_010.40)
extern std::uint8_t legacySpuFlushBytes[kLegacySpuFlushByteCount];
// decomp: _spuMemInfo (_spuMemInfo@SLUS_010.40)
extern std::uint8_t legacySpuAllocatorTable;
// decomp: D_800378C0 (D_800378C0@SLUS_010.40)
extern LegacyCommandArguments commandArguments;
// The decomp's views into it: D_80039AFC (mix_behavior) and D_80039B00
// (update_flags).
// decomp: g_Sound_GlobalFlags (g_Sound_GlobalFlags@SLUS_010.40)
extern DriverFlags controlLatches;
}  // namespace VS::Audio::Driver
