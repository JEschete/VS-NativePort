#pragma once
// Physical keyboard keys and gamepad controls bound to the logical actions
// (Actions.hpp; plan §6.5, #52). Hosts read them from a JSON file:
//
//   {"version": 1,
//    "keyboard": {"Cross": ["X"], "Start": ["Return"], ...},
//    "gamepad": {"Cross": ["South"], "L2": ["LeftTrigger"], ...},
//    "trigger_threshold": 16384}
//
// An action the file leaves out keeps its default binding and an empty list
// unbinds it. Key names are this module's own: SDL's display names differ
// between platforms (SDL_GetScancodeName), so they cannot round-trip a file.

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Input/Actions.hpp"

namespace VS::Input {

// A keyboard key by position: the USB HID usage SDL reports as its scancode,
// so a binding stays on the same key whatever the keyboard layout.
enum class Key : std::uint16_t {};

// A gamepad control by position in SDL's standard layout: South is the bottom
// face button whatever its label says.
enum class GamepadControl : std::uint8_t {
  kSouth,
  kEast,
  kWest,
  kNorth,
  kDpadUp,
  kDpadDown,
  kDpadLeft,
  kDpadRight,
  kLeftShoulder,
  kRightShoulder,
  kLeftStick,
  kRightStick,
  kStart,
  kBack,
  // The triggers are axes: held past the bindings' trigger threshold.
  kLeftTrigger,
  kRightTrigger,
};
// The digital controls, kSouth..kBack.
inline constexpr std::size_t kGamepadButtonCount = 14;
inline constexpr std::size_t kGamepadControlCount = 16;

[[nodiscard]] std::optional<Key> ParseKey(std::string_view name) noexcept;
// The key's name in the bindings file; empty for a key without one.
[[nodiscard]] std::string_view KeyName(Key key) noexcept;
[[nodiscard]] std::optional<GamepadControl> ParseGamepadControl(
    std::string_view name) noexcept;
[[nodiscard]] std::string_view GamepadControlName(
    GamepadControl control) noexcept;

struct Bindings {
  static constexpr std::int16_t kDefaultTriggerThreshold = 16384;

  // Each action's keys and gamepad controls, indexed by its Action value.
  std::array<std::vector<Key>, kActionCount> keys;
  std::array<std::vector<GamepadControl>, kActionCount> gamepad;
  // A trigger holds its action while its axis (0..32767) exceeds this.
  std::int16_t trigger_threshold = kDefaultTriggerThreshold;

  friend bool operator==(const Bindings&, const Bindings&) = default;
};

// The layout used before bindings were configurable: the arrows, X Cross,
// C Circle, Z Square, S Triangle, Q/E L1/R1, 1/3 L2/R2, Return Start and
// Backspace Select; the gamepad by position, with the triggers as L2/R2 and
// the stick clicks as L3/R3.
[[nodiscard]] Bindings DefaultBindings();

// One tick's gamepad controls.
struct GamepadSnapshot {
  std::array<bool, kGamepadButtonCount> buttons{};  // by GamepadControl
  std::int16_t left_trigger = 0;
  std::int16_t right_trigger = 0;
};

// `keys` is indexed by scancode, as SDL_GetKeyboardState reports it.
[[nodiscard]] ActionSet ResolveKeyboard(const Bindings& bindings,
                                        std::span<const bool> keys);
[[nodiscard]] ActionSet ResolveGamepad(const Bindings& bindings,
                                       const GamepadSnapshot& pad);

// Validates a whole bindings file. Any problem rejects it, and every problem
// found is listed. `reserved` keys belong to the host (Combat Lab's controls).
[[nodiscard]] std::expected<Bindings, std::vector<std::string>> ParseBindings(
    std::string_view text, std::span<const Key> reserved = {});
// The file text, listing every action in kActions order.
[[nodiscard]] std::string FormatBindings(const Bindings& bindings);

enum class BindingsSource : std::uint8_t {
  kFile,      // read and valid
  kCreated,   // absent, so written with the defaults
  kDefaults,  // unreadable, invalid or not creatable: the file is untouched
};
[[nodiscard]] std::string_view BindingsSourceName(
    BindingsSource source) noexcept;

struct LoadedBindings {
  Bindings bindings;
  BindingsSource source = BindingsSource::kDefaults;
  std::vector<std::string> diagnostics;
};

// Reads `path`, creating it with the defaults when it is absent. An invalid
// file is never overwritten: the defaults apply and each problem is logged.
[[nodiscard]] LoadedBindings LoadBindingsFile(
    const std::filesystem::path& path, std::span<const Key> reserved = {});

}  // namespace VS::Input
