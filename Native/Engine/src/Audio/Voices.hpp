#pragma once
// A voice channel record the original declared and the port does not use.
#include <cstddef>
#include <cstdint>

namespace VS::Audio {
// decomp: FSound80094FA0 (src/include/sound.h:304)
struct LegacyVoiceChannelState {
  /* 0x00 */ std::int32_t reserved00;
  /* 0x04 */ std::int32_t reserved04;
  /* 0x08 */ std::int32_t reserved08;
  /* 0x0C */ std::int32_t voices_in_use_mask;
  /* 0x10 */ std::int32_t voice_index;
  /* 0x14 */ std::uint32_t channel_flags;
  /* 0x18 */ std::int32_t reserved18;
  /* 0x1C */ std::int32_t reserved1c;
  /* 0x20 */ std::int32_t reserved20;
  /* 0x24 */ std::int32_t reserved24;
  /* 0x28 */ std::int32_t reserved28;
  /* 0x2C */ std::int32_t reserved2c;
  /* 0x30 */ std::int32_t reserved30;
  /* 0x34 */ std::int32_t reserved34;
  /* 0x38 */ std::int32_t reserved38;
  /* 0x3C */ std::int32_t reserved3c;
  /* 0x40 */ std::int32_t volume;
  /* 0x44 */ std::int32_t reserved44;
  /* 0x48 */ std::int32_t reserved48;
  /* 0x4C */ std::uint8_t reserved4c;
  /* 0x4D */ std::uint8_t reserved4d;
  /* 0x4E */ std::uint8_t reserved4e;
  /* 0x4F */ std::uint8_t reserved4f;
  /* 0x50 */ std::int32_t reserved50;
  /* 0x54 */ std::int32_t reserved54;
  /* 0x58 */ std::int32_t voice_sample_rate;
  /* 0x5C */ std::int32_t control_flags;
};

namespace layout {
inline constexpr std::size_t kLegacyVoiceChannelStateBytes = 0x60;
static_assert(sizeof(LegacyVoiceChannelState) == kLegacyVoiceChannelStateBytes);
}  // namespace layout
}  // namespace VS::Audio
