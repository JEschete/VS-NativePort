#pragma once
// The option settings saved with the game: menu display options, sound,
// vibration, the mapped chain and defense abilities and the save counters.
// Two of them are device settings (plan §6.5): the sound output and the
// vibration, which the audio output and the pad motors apply as soon as they
// change. Native code reads and writes them as DeviceSettings; the rest are
// read by the menus and gameplay when they next consult the record.
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

enum class SoundOutput : std::uint8_t { kStereo, kMono };
enum class Vibration : std::uint8_t { kOff, kOn };

// The defaults are what a soft reset and a first title visit select.
struct DeviceSettings {
  SoundOutput sound_output = SoundOutput::kStereo;
  Vibration vibration = Vibration::kOn;

  friend constexpr bool operator==(const DeviceSettings&,
                                   const DeviceSettings&) noexcept = default;
};

// The saved byte as the audio output applies it: only 1 selects mono.
// decomp: SLUS_010.40 Sound_SetSpeakerMode (src/SLUS_010.40/25AC.c:451)
[[nodiscard]] constexpr SoundOutput DecodeSoundOutput(
    std::uint8_t mono_sound) noexcept {
  return mono_sound == 1 ? SoundOutput::kMono : SoundOutput::kStereo;
}

// The saved byte as the motors apply it: any nonzero value drives them.
// decomp: SLUS_010.40 vs_main_setVibrateParams (src/SLUS_010.40/main.c:1653)
[[nodiscard]] constexpr Vibration DecodeVibration(
    std::uint8_t vibration_on) noexcept {
  return vibration_on != 0 ? Vibration::kOn : Vibration::kOff;
}

struct DecodedDeviceSettings {
  DeviceSettings settings;
  // False when a byte is neither 0 nor 1, which only an altered save or
  // snapshot holds. The title normalized such bytes with `!= 0`
  // (TITLE.PRG initEnvironment, 22C.c:1487); the port keeps the values the
  // devices applied, so a mono byte of 2 stays stereo.
  bool exact = true;
};

[[nodiscard]] constexpr DecodedDeviceSettings DecodeDeviceSettings(
    const Settings& record) noexcept {
  const auto is_flag = [](std::uint8_t byte) { return byte <= 1; };
  return {
      .settings = {.sound_output = DecodeSoundOutput(record.mono_sound),
                   .vibration = DecodeVibration(record.vibration_on)},
      .exact = is_flag(record.mono_sound) && is_flag(record.vibration_on),
  };
}

// Writes the two bytes as 0 or 1.
constexpr void EncodeDeviceSettings(DeviceSettings device,
                                    Settings& record) noexcept {
  record.mono_sound = device.sound_output == SoundOutput::kMono ? 1 : 0;
  record.vibration_on = device.vibration == Vibration::kOn ? 1 : 0;
}
}  // namespace VS::Input
