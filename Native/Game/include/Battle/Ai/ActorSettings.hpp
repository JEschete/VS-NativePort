#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// How a battle actor's packed AI settings word is assembled before its AI
// state is created. These policies only transform settings values, so they
// reach no actor, AI state table or PS1 address.
namespace VS::Battle::Ai {
// Settings bits 21-31 (relation category and above) that the actor's saved
// settings flags can replace; bits 0-20 always keep the enemy template's value.
inline constexpr std::uint32_t kSavedAiSettingsMask = 0xFFE00000u;

// Replaces settings bits 21-31 with bits 1-11 of the actor's saved settings
// flags (func_800E5A74). Bit 0, which the caller tests to enable the override,
// and bits 12-15 are ignored.
[[nodiscard]] std::uint32_t ApplySavedAiSettingsFlags(
    std::uint32_t settings, std::uint16_t saved_flags);
}  // namespace VS::Battle::Ai
