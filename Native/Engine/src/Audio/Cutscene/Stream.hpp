#pragma once
// The cutscene audio stream's playback state.
#include <cstddef>
#include <cstdint>

namespace VS::Audio {
struct CutsceneStreamData;

// decomp: unnamed union (src/include/sound.h:589)
// One stream word, read as an integer or as its four bytes.
union StreamWordBytes {
  std::int32_t word;
  std::uint8_t bytes[sizeof(std::int32_t)];
};

// decomp: FSoundCutsceneStreamState (src/include/sound.h:597)
struct CutsceneStreamState {
  /* 0x00 */ void* current_chunk;
  // Where a repeating stream restarts (header word 5 past the first chunk);
  // null when format_flags bit 1 is clear (25AC.c:6461).
  /* 0x04 */ void* repeat_chunk;
  // Bit 0 stereo, bit 1 the stream repeats from repeat_chunk.
  /* 0x08 */ std::int32_t format_flags;
  /* 0x0C */ std::int32_t voice_mask;
  /* 0x10 */ std::int32_t voice_index;
  // Probably a bad name, judged on its use in Sound_Cutscene_BeginPlayback.
  /* 0x14 */ std::uint32_t total_pages;
  /* 0x18 */ std::int32_t current_page;
  // The page count the refill restores when it jumps to repeat_chunk.
  /* 0x1C */ std::int32_t repeat_pages;
  /* 0x20 */ std::int32_t reserved20;
  /* 0x24 */ std::int32_t reserved24;
  /* 0x28 */ std::int32_t page_index;
  /* 0x2C */ CutsceneStreamData* stream_base;
  /* 0x30 */ std::int32_t reserved30;
  /* 0x34 */ std::uint32_t stream_page_index;
  /* 0x38 */ std::int32_t reserved38;
  /* 0x3C */ std::uint32_t page_ring_size;
  /* 0x40 */ std::int32_t volume;
  /* 0x44 */ std::int32_t volume_fade_step;
  /* 0x48 */ std::int32_t volume_fade_ticks_left;
  /* 0x4C */ StreamWordBytes pan;
  /* 0x50 */ std::int32_t reserved50;
  /* 0x54 */ std::int32_t reserved54;
  /* 0x58 */ std::int32_t sample_rate;
  /* 0x5C */ std::int32_t control_flags;
};

namespace layout {
inline constexpr std::size_t kStreamWordBytes = 0x4;
static_assert(sizeof(StreamWordBytes) == kStreamWordBytes);
}  // namespace layout
}  // namespace VS::Audio

namespace VS::Audio::Cutscene {
// decomp: _isSpuTransfer (_isSpuTransfer@SLUS_010.40)
extern volatile std::int32_t legacySpuTransferBusy;
// The decomp's views into it: D_80039B14 (voice_mask), D_80039B48 (volume)
// and D_80039B64 (control_flags).
// decomp: g_Sound_Cutscene_StreamState
// (g_Sound_Cutscene_StreamState@SLUS_010.40)
extern CutsceneStreamState streamState;
}  // namespace VS::Audio::Cutscene
