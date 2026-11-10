// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/ActorSettings.hpp"

namespace VS::Battle::Ai {
namespace {
// Saved-flag bits 1-11 land on settings bits 21-31.
constexpr unsigned kFlagsShift = 20;
constexpr std::uint32_t kFlagBits = kSavedAiSettingsMask >> kFlagsShift;
}  // namespace

// decomp: BATTLE.PRG func_800E5A74 (src/BATTLE/BATTLE.PRG/7CD68.c:595).
std::uint32_t ApplySavedAiSettingsFlags(std::uint32_t settings,
                                        std::uint16_t saved_flags) {
  return ((saved_flags & kFlagBits) << kFlagsShift) |
         (settings & ~kSavedAiSettingsMask);
}
}  // namespace VS::Battle::Ai
