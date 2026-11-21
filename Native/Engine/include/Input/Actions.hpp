#pragma once
// The logical actions: the sixteen controls of the original pad, which the
// game's logic reads (plan §6.5). Physical keys and gamepad controls bind to
// them (Bindings.hpp), and each screen gives them its own meaning, such as the
// title's menu commands (TitleMenu.hpp).

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>

namespace VS::Input {

// Each action's bit in the pad word the game reads (libetc PADxxx), so ported
// logic keeps its masks. Player 1 uses bits 0-15 and player 2 bits 16-31 (as
// in vs_main_processPadState).
enum Button : std::uint32_t {
  L2 = 1U << 0U,
  R2 = 1U << 1U,
  L1 = 1U << 2U,
  R1 = 1U << 3U,
  Triangle = 1U << 4U,
  Circle = 1U << 5U,
  Cross = 1U << 6U,
  Square = 1U << 7U,
  Select = 1U << 8U,
  L3 = 1U << 9U,
  R3 = 1U << 10U,  // also set by the game when the right stick is deflected
  Start = 1U << 11U,
  Up = 1U << 12U,
  Right = 1U << 13U,
  Down = 1U << 14U,
  Left = 1U << 15U,
};

// One logical action; the value is its bit index in the pad word.
enum class Action : std::uint8_t {
  kL2,
  kR2,
  kL1,
  kR1,
  kTriangle,
  kCircle,
  kCross,
  kSquare,
  kSelect,
  kL3,
  kR3,
  kStart,
  kUp,
  kRight,
  kDown,
  kLeft,
};
inline constexpr std::size_t kActionCount = 16;

// Every action, in the order logs and the bindings file list them.
inline constexpr std::array<Action, kActionCount> kActions{
    Action::kUp,    Action::kDown,   Action::kLeft,   Action::kRight,
    Action::kCross, Action::kCircle, Action::kSquare, Action::kTriangle,
    Action::kL1,    Action::kR1,     Action::kL2,     Action::kR2,
    Action::kL3,    Action::kR3,     Action::kStart,  Action::kSelect,
};

[[nodiscard]] constexpr std::uint32_t ActionBit(Action action) noexcept {
  return 1U << static_cast<unsigned>(action);
}

// "Up", "Cross", "L1", "Start" ...
[[nodiscard]] std::string_view ActionName(Action action) noexcept;
[[nodiscard]] std::optional<Action> ParseAction(std::string_view name) noexcept;

// A set of one player's actions.
class ActionSet {
 public:
  constexpr ActionSet() noexcept = default;
  constexpr ActionSet(std::initializer_list<Action> actions) noexcept {
    for (const Action action : actions) {
      Insert(action);
    }
  }

  // Player 1's actions in a pad word; player 2's bits are not included.
  [[nodiscard]] static constexpr ActionSet FromPadWord(
      std::uint32_t word) noexcept {
    ActionSet set;
    set.bits_ = static_cast<std::uint16_t>(word);
    return set;
  }

  [[nodiscard]] constexpr bool Contains(Action action) const noexcept {
    return (bits_ & ActionBit(action)) != 0;
  }
  constexpr void Insert(Action action) noexcept {
    bits_ = static_cast<std::uint16_t>(bits_ | ActionBit(action));
  }
  [[nodiscard]] constexpr bool ContainsAny(ActionSet other) const noexcept {
    return (bits_ & other.bits_) != 0;
  }
  [[nodiscard]] constexpr bool Empty() const noexcept { return bits_ == 0; }
  [[nodiscard]] constexpr std::uint16_t PadWord() const noexcept {
    return bits_;
  }

  friend constexpr bool operator==(ActionSet, ActionSet) noexcept = default;

 private:
  std::uint16_t bits_ = 0;
};

// One player's actions for a tick: held now, and newly held since the last.
struct ActionState {
  ActionSet held;
  ActionSet pressed;

  friend constexpr bool operator==(const ActionState&,
                                   const ActionState&) noexcept = default;
};

}  // namespace VS::Input
