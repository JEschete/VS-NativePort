// Hand-maintained (tools/port/owned.csv).
#include "Effects/ParticleTextureAnimation.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

namespace VS::Effects {
namespace {
constexpr std::size_t kWordBytes = 4;
constexpr std::uint32_t kByteBits = 8;
constexpr std::uint32_t kCommandShift = 28;
constexpr std::uint32_t kFrameMask = 0xFFFF;
constexpr std::uint32_t kHoldShift = 16;
constexpr std::uint32_t kFrameFlagsShift = 24;
constexpr std::uint32_t kFrameFlagsMask = 0x3F;
// The offset word's two signed 14-bit fields: x in bits 0-13, y in 14-27.
constexpr std::uint32_t kOffsetXShift = 18;
constexpr std::uint32_t kOffsetYShift = 4;
constexpr std::uint32_t kOffsetFieldShift = 18;
constexpr std::uint8_t kTimerStep = 2;

enum : std::uint32_t {
  kFrameCommand = 0,
  kOffsetCommand = 1,
  kRestartCommand = 2,
  kStopCommand = 3,
};

[[nodiscard]] std::optional<std::uint32_t> ReadWord(
    std::span<const std::byte> program, std::size_t at) noexcept {
  if (at > program.size() || program.size() - at < kWordBytes) {
    return std::nullopt;
  }
  std::uint32_t word = 0;
  for (std::size_t index = 0; index < kWordBytes; ++index) {
    word |= std::to_integer<std::uint32_t>(program[at + index])
            << (index * kByteBits);
  }
  return word;
}

[[nodiscard]] std::int16_t OffsetField(std::uint32_t word,
                                       std::uint32_t shift) noexcept {
  return static_cast<std::int16_t>(static_cast<std::int32_t>(word << shift) >>
                                   kOffsetFieldShift);
}

// The program's cursor as one step reads it. Each return to the first word,
// by a restart or by the halfword cursor wrapping, is counted: a second one
// in a step would repeat the same words forever.
class ProgramCursor {
 public:
  explicit ProgramCursor(std::uint16_t cursor) : cursor_(cursor) {}

  [[nodiscard]] std::uint16_t Value() const noexcept { return cursor_; }
  [[nodiscard]] bool Advance() noexcept {
    const auto next = static_cast<std::uint16_t>(cursor_ + kWordBytes);
    return next > cursor_ ? (cursor_ = next, true) : Restart();
  }
  [[nodiscard]] bool Restart() noexcept {
    cursor_ = 0;
    return !std::exchange(restarted_, true);
  }

 private:
  std::uint16_t cursor_;
  bool restarted_ = false;
};

constexpr auto kRefusedStep = TextureAnimationStep::kRefused;
constexpr TextureAnimationAdvance kRefused{.step = kRefusedStep};
}  // namespace

// decomp: BATTLE.PRG func_800D6CCC (src/BATTLE/BATTLE.PRG/6E4CC.c:17)
void ClearTextureAnimation(TextureAnimationState& state) noexcept {
  state = {};
}

// decomp: BATTLE.PRG func_800D6CF0 (src/BATTLE/BATTLE.PRG/6E4CC.c:29)
void RestartTextureAnimation(TextureAnimationState& state,
                             std::uint8_t texture_index) noexcept {
  state.texture_index = texture_index;
  state.cursor = 0;
  state.frame_timer = 0;
}

// decomp: BATTLE.PRG func_800D6D24 (src/BATTLE/BATTLE.PRG/6E4CC.c:37)
TextureAnimationAdvance AdvanceTextureAnimation(
    TextureAnimationState& state, std::span<const std::byte> program) noexcept {
  if (state.frame_timer != 0) {
    state.frame_timer =
        static_cast<std::uint8_t>(state.frame_timer - kTimerStep);
    if (state.frame_timer != 0) {
      return {.step = TextureAnimationStep::kHolding};
    }
  }
  TextureAnimationState next = state;
  ProgramCursor cursor(state.cursor);
  while (const std::optional<std::uint32_t> word =
             ReadWord(program, cursor.Value())) {
    switch (*word >> kCommandShift) {
      case kFrameCommand:
        if (!cursor.Advance()) {
          return kRefused;
        }
        next.frame_timer = static_cast<std::uint8_t>(*word >> kHoldShift);
        next.frame_flags = static_cast<std::uint8_t>(
            (*word >> kFrameFlagsShift) & kFrameFlagsMask);
        next.cursor = cursor.Value();
        state = next;
        return {.step = TextureAnimationStep::kFrame,
                .frame = static_cast<std::uint16_t>(*word & kFrameMask)};
      case kOffsetCommand:
        next.frame_offset_x = OffsetField(*word, kOffsetXShift);
        next.frame_offset_y = OffsetField(*word, kOffsetYShift);
        if (!cursor.Advance()) {
          return kRefused;
        }
        break;
      case kRestartCommand:
        if (!cursor.Restart()) {
          return kRefused;
        }
        break;
      case kStopCommand:
        next.cursor = cursor.Value();
        state = next;
        return {.step = TextureAnimationStep::kStopped};
      default:
        return kRefused;
    }
  }
  return kRefused;
}
}  // namespace VS::Effects
