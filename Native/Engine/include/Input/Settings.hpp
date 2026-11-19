#pragma once
// The option settings saved with the game: menu display options, sound,
// vibration, the mapped chain and defense abilities and the save counters.
#include <cstddef>
#include <cstdint>

namespace VS::Input {
inline constexpr std::size_t kMappedAbilitySlots = 4;
inline constexpr std::size_t kSettingsStructBytes = 0x20;

// decomp: vs_main_settings_t (src/SLUS_010.40/main.h:132)
struct Settings {
  // Bit 3 set turns puzzle mode off in the cleared-game save
  // (TITLE _packageGameSaveData).
  /* 0x00 */ std::uint8_t mode_flags : 4;
  /* 0x00 */ std::uint8_t container_empty : 1;
  /* 0x00 */ std::uint8_t ability_timing_display : 1;
  /* 0x00 */ std::uint8_t weapon_stat_change : 1;
  /* 0x00 */ std::uint8_t armor_stat_change : 1;
  /* 0x01 */ std::uint8_t cursor_memory;
  /* 0x02 */ std::uint16_t menu_flags;
  /* 0x04 */ std::uint32_t slot_state;
  /* 0x08 */ std::uint8_t information;
  /* 0x09 */ std::uint8_t simple_map;
  /* 0x0A */ std::uint8_t mono_sound;
  /* 0x0B */ std::uint8_t vibration_on;
  /* 0x0C */ std::uint8_t mapped_chain_abilities[kMappedAbilitySlots];
  /* 0x10 */ std::uint8_t mapped_defense_abilities[kMappedAbilitySlots];
  /* 0x14 */ std::uint32_t key;
  /* 0x18 */ std::uint16_t cumulative_save_count;
  /* 0x1A */ std::uint16_t current_game_save_count;
  /* 0x1C */ std::uint32_t save_file_generation;
};
static_assert(sizeof(Settings) == kSettingsStructBytes);
}  // namespace VS::Input
