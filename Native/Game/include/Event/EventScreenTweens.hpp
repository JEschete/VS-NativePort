#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event camera's roll tweens and the captured screen (framebuffer effect)
// tweens: the script opcodes that start them and the per-frame advances.
// Each tween keeps the original's 16-bit fields and wrapping; the framebuffer
// effect, the radial warp and the easing curve are injected ports.
// EventScreenTweensLive.hpp binds them to Event_camera and the live services.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Effects/ScreenDistortion/RadialWarp.hpp"
#include "Event/ScriptCommand.hpp"

namespace VS::Event {
// Opcode 0xE1: the opcode byte and the enable byte.
inline constexpr std::size_t kSetScreenCaptureLength = 2;
// Opcodes 0xE2 and 0xE3: the opcode byte, a 16-bit angle, the direction and
// easing byte and the duration byte.
inline constexpr std::size_t kTweenRollLength = 5;
inline constexpr std::uint8_t kTweenCameraRollOpcode = 0xE2;
inline constexpr std::uint8_t kTweenScreenRollOpcode = 0xE3;
// A full turn in the 12-bit angle units the rolls use.
inline constexpr std::int16_t kFullTurn = 0x1000;

// The screen transform opcodes (func_800BD6C4's jump table).
enum class ScreenTransformOpcode : std::uint8_t {
  kSetDepth = 0xB9,
  kSetPreviousFrame = 0xBA,
  kConfigureCapture = 0xBF,
  kTweenScale = 0xE4,
  kTweenTint = 0xE5,
  kTweenOffset = 0xE6,
  kSetBlendMode = 0xE7,
  kTweenRadialWarp = 0xED,
};

// The bytes a screen transform opcode reads, or nothing for another opcode.
[[nodiscard]] std::optional<std::size_t> ScreenTransformLength(
    std::uint8_t opcode) noexcept;

// AngleTween (func_800BD57C_t): a 12-bit angle eased from `start` by
// `delta` over `duration` half-ticks. `changed` tells the camera to apply it.
struct EventAngleTween {
  std::uint8_t changed{};
  std::uint8_t easing{};
  std::int16_t angle{};
  std::int16_t start{};
  std::int16_t delta{};
  std::int16_t elapsed{};
  std::int16_t duration{};

  friend bool operator==(const EventAngleTween&,
                         const EventAngleTween&) = default;
};

// TripleTween (func_800BDBB4_t): up to three values eased together.
// The two-value tweens still advance their third value from whatever start
// and delta it holds, as the original did.
inline constexpr std::size_t kScreenTweenValues = 3;

struct EventScreenTween {
  std::uint8_t easing{};
  std::array<std::int16_t, kScreenTweenValues> current{};
  std::array<std::int16_t, kScreenTweenValues> start{};
  std::array<std::int16_t, kScreenTweenValues> delta{};
  std::int16_t elapsed{};
  std::int16_t duration{};

  friend bool operator==(const EventScreenTween&,
                         const EventScreenTween&) = default;
};

// The event camera's four captured screen tweens, in update order.
struct EventScreenTweens {
  EventScreenTween scale{};
  EventScreenTween tint{};
  EventScreenTween translation{};
  EventScreenTween radial_warp{};

  friend bool operator==(const EventScreenTweens&,
                         const EventScreenTweens&) = default;
};

// A framebuffer effect scale or offset
// (VS::Render::FramebufferEffectTranslation's first two words).
struct EventScreenPair {
  std::int32_t x{};
  std::int32_t y{};

  friend bool operator==(const EventScreenPair&,
                         const EventScreenPair&) = default;
};

struct EventScreenTint {
  std::uint8_t red{};
  std::uint8_t green{};
  std::uint8_t blue{};

  friend bool operator==(const EventScreenTint&,
                         const EventScreenTint&) = default;
};

// The captured screen (BATTLE.PRG func_8007DD50..func_8007DECC) and the
// screen overlay's radial warp (GIM/SCREFF2.PRG func_800F9BC0).
class EventScreenEffectPort {
 public:
  virtual ~EventScreenEffectPort() = default;
  EventScreenEffectPort() = default;
  EventScreenEffectPort(const EventScreenEffectPort&) = delete;
  EventScreenEffectPort& operator=(const EventScreenEffectPort&) = delete;
  EventScreenEffectPort(EventScreenEffectPort&&) = delete;
  EventScreenEffectPort& operator=(EventScreenEffectPort&&) = delete;

  virtual void SetCaptureEnabled(std::int32_t enabled) = 0;
  virtual void SetDepth(std::int32_t depth) = 0;
  virtual void SetPreviousFrame(std::int32_t use_previous_frame) = 0;
  virtual void SetScale(EventScreenPair scale) = 0;
  virtual void SetTint(EventScreenTint tint) = 0;
  virtual void SetOffset(EventScreenPair offset) = 0;
  virtual void SetBlendMode(std::int32_t blend_mode) = 0;
  [[nodiscard]] virtual EventScreenPair ReadScale() = 0;
  [[nodiscard]] virtual EventScreenTint ReadTint() = 0;
  [[nodiscard]] virtual EventScreenPair ReadOffset() = 0;
  virtual void SetRadialWarp(
      Effects::ScreenDistortion::RadialWarpSettings settings) = 0;
};

// The event easing curve (func_800BFEBC) and the scratchpad word the original
// kept each eased fraction in (0x1F800088).
class EventTweenEasing {
 public:
  virtual ~EventTweenEasing() = default;
  EventTweenEasing() = default;
  EventTweenEasing(const EventTweenEasing&) = delete;
  EventTweenEasing& operator=(const EventTweenEasing&) = delete;
  EventTweenEasing(EventTweenEasing&&) = delete;
  EventTweenEasing& operator=(EventTweenEasing&&) = delete;

  // A 12-bit fraction (0x1000 = done) of `mode`'s curve at `frame`.
  [[nodiscard]] virtual std::int32_t EaseFraction(std::uint8_t mode,
                                                  std::int16_t frame,
                                                  std::int16_t duration) = 0;
  virtual void RecordEasedFraction(std::int32_t fraction) = 0;
};

// decomp: BATTLE.PRG func_800BB450 (F01198, 4A0A8.c:2793) Opcode 0xE1.
// False for a command shorter than kSetScreenCaptureLength, with nothing set.
[[nodiscard]] bool CommandSetScreenCapture(ScriptCommand command,
                                           EventScreenEffectPort& screen);

// decomp: BATTLE.PRG func_800BD444 (F01226, 4A0A8.c:3601) Opcodes 0xE2 (the
// camera roll) and 0xE3 (the captured screen roll). A zero duration sets the
// angle at once; otherwise the tween takes the shorter way round unless the
// direction nibble forces a negative (0) or positive (2) turn. False, with
// nothing changed, for a short command or another opcode (the original wrote
// through an unset pointer).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the two rolls.
[[nodiscard]] bool CommandTweenCameraRoll(ScriptCommand command,
                                          EventAngleTween& camera_roll,
                                          EventAngleTween& screen_roll);

// decomp: BATTLE.PRG func_800BD57C (F01227, 4A0A8.c:3647) Advances a running
// roll by half of `tick_speed` and marks it changed; it stops once the elapsed
// time reaches the duration.
void UpdateEventAngleTween(EventAngleTween& tween, std::int32_t tick_speed,
                           EventTweenEasing& easing);

// decomp: BATTLE.PRG func_800BD6C4 (F01229, 4A0A8.c:3691) Opcodes 0xB9, 0xBA,
// 0xBF, 0xE4-0xE7 and 0xED. Returns the script result: 1 while a radial warp
// tween is still running (0xED waits), else 0. Nothing for a short command
// or another opcode, with nothing changed.
[[nodiscard]] std::optional<std::int32_t> CommandScreenTransform(
    ScriptCommand command, EventScreenTweens& tweens,
    EventScreenEffectPort& screen);

// decomp: BATTLE.PRG func_800BDBB4 (F01231, 4A0A8.c:3838) Advances a running
// tween by half of `tick_speed` and returns its eased fraction, or nothing for
// an idle tween. It stops only when the elapsed time equals the duration.
[[nodiscard]] std::optional<std::int32_t> UpdateEventTripleTween(
    EventScreenTween& tween, std::int32_t tick_speed, EventTweenEasing& easing);

// decomp: BATTLE.PRG func_800BDAB4 (F01230, 4A0A8.c:3812) Advances the four
// tweens in order and applies each running one to the captured screen or the
// radial warp.
void UpdateEventScreenTransforms(EventScreenTweens& tweens,
                                 std::int32_t tick_speed,
                                 EventTweenEasing& easing,
                                 EventScreenEffectPort& screen);
}  // namespace VS::Event
