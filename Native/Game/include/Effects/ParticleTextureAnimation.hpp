#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

// A particle's texture animation (BATTLE.PRG 5BF94.c:8501-8521): a program of
// words in the effect package's animation table (block 2) that selects frames
// of one texture table, holds each for a number of ticks and moves the frame
// offset. The functions work on the animation's value fields;
// ParticleTextureAnimationLive.hpp binds the LiveParticleTextureAnimation
// records and their program and frame pointers.
namespace VS::Effects {
struct TextureAnimationState {
  // Ticks the frame is held, counted down by two.
  std::uint8_t frame_timer{};
  // Bits 24-29 of the last frame word, which the original stored with the
  // timer.
  std::uint8_t frame_flags{};
  std::int16_t frame_offset_x{};
  std::int16_t frame_offset_y{};
  // The program's next word, in bytes.
  std::uint16_t cursor{};
  std::uint8_t texture_index{};

  friend bool operator==(const TextureAnimationState&,
                         const TextureAnimationState&) = default;
};

// func_800D6CCC (5BF94.c:8501): the cleared animation.
void ClearTextureAnimation(TextureAnimationState& state) noexcept;

// func_800D6CF0 (5BF94.c:8513): restarts the animation on texture table
// `texture_index` at its program's first word with no frame held. The
// program is the animation table entry ParticleAnimationData selects.
void RestartTextureAnimation(TextureAnimationState& state,
                             std::uint8_t texture_index) noexcept;

enum class TextureAnimationStep : std::uint8_t {
  // The timer still holds the frame.
  kHolding,
  // The program selected a new frame.
  kFrame,
  // The program stopped; the frame stays.
  kStopped,
  // An unknown command, a second restart in one step (both looped forever
  // in the original) or a word past the program; the state is as before the
  // program was read.
  kRefused,
};

struct TextureAnimationAdvance {
  TextureAnimationStep step{};
  // The frame's index in the texture table, for kFrame.
  std::uint16_t frame{};

  friend bool operator==(const TextureAnimationAdvance&,
                         const TextureAnimationAdvance&) = default;
};

// func_800D6D24 (5BF94.c:8521): counts the held frame down by two ticks
// (an odd count never ends, as in the original), then runs `program` from the
// cursor: command 0 selects a frame and its hold, 1 sets the frame offset,
// 2 restarts the program and 3 stops it.
[[nodiscard]] TextureAnimationAdvance AdvanceTextureAnimation(
    TextureAnimationState& state, std::span<const std::byte> program) noexcept;
}  // namespace VS::Effects
