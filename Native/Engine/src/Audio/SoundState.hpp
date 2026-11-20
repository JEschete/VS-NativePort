#pragma once
// The sound bank slot record (vs_main_soundData), the menu sound effects and
// the wave bank file map that SoundBanksLive.cpp binds to SoundBanks.hpp.
#include <cstddef>
#include <cstdint>

#include "Audio/SoundBanks.hpp"

namespace VS::Content {
struct DiscQueueSlot;
}  // namespace VS::Content

namespace VS::Audio {
// The original declares four id bytes for the three sound-effect bank slots.
inline constexpr std::size_t kSfxBankIdSlots = 4;
inline constexpr std::size_t kMenuSoundEffectWords = 11264;

namespace layout {
inline constexpr std::size_t kSoundReleaseViewLeadBytes = 0x30;
}  // namespace layout

// decomp: func_8004644C_t (src/BATTLE/BATTLE.PRG/30DB0.c:29)
struct SoundReleaseView {
  std::uint8_t reserved0[layout::kSoundReleaseViewLeadBytes];
  std::int32_t* reserved30;
};

// decomp: soundData_t (src/SLUS_010.40/main.h:135)
struct SoundState {
  std::int32_t current_music_slot;
  std::uint32_t playing_music_handle;
  std::uint32_t current_music_id;
  void* current_music_data;
  // The original reads the music ids unsigned (lbu).
  std::uint8_t music_ids[kMusicSlotCount];
  std::uint32_t music_handles[kMusicSlotCount];
  void* music_data[kMusicSlotCount];
  VS::Content::DiscQueueSlot* music_requests[kMusicSlotCount];
  std::uint32_t current_sfx_bank;
  std::int8_t sfx_bank_ids[kSfxBankIdSlots];
  void* sfx_bank_data[kSfxBankSlotCount];
  VS::Content::DiscQueueSlot* sfx_bank_requests[kSfxBankSlotCount];
  void* sfx_stream_data[kEffectStreamSlotCount];
  std::int32_t* reserved70[kEffectStreamSlotCount];
  std::int32_t wave_bank_id;
  void* wave_bank_data;
  VS::Content::DiscQueueSlot* wave_bank_request;
};

// decomp: _sfxData (_sfxData@SLUS_010.40)
extern std::uint16_t menuSoundEffects[kMenuSoundEffectWords];
// decomp: vs_main_soundData (vs_main_soundData@SLUS_010.40)
extern SoundState soundState;
// decomp: _soundFileMap (_soundFileMap@SLUS_010.40)
extern const std::uint8_t kWaveBankFileMap[];
}  // namespace VS::Audio
