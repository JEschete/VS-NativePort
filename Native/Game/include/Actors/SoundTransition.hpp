#pragma once

#include <cstdint>

// An actor's sound pan/volume glide toward a target, one step per frame. The
// step is a pure function of the current and target states and the frame's
// tick speed, so it reads no actor record, timer global or PS1 address.
namespace VS::Actors {
// Mode of a gliding transition; any other mode holds the current state.
inline constexpr std::uint8_t kSoundTransitionGliding = 1;

// An actor sound state (original Actors_ModelEffectSlot byte order).
struct SoundTransition {
  std::uint8_t mode = 0;
  std::uint8_t pan = 0;
  std::uint8_t volume = 0;
  std::uint8_t frames_remaining = 0;

  friend bool operator==(const SoundTransition&,
                         const SoundTransition&) = default;
};

// Advances a gliding transition by half the tick speed (clamped to the frames
// remaining), moving pan and volume proportionally toward the target with the
// original signed division; the target's mode and length are not read
// (func_800AEEC4).
[[nodiscard]] SoundTransition StepSoundTransition(SoundTransition current,
                                                  SoundTransition target,
                                                  int tick_speed);
}  // namespace VS::Actors
