#include "Input/Bindings.hpp"

#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "Core/Log.hpp"
#include "Platform/Paths.hpp"
#include "nlohmann/json.hpp"

namespace VS::Input {

namespace {

using Json = nlohmann::json;

constexpr std::int64_t kVersion = 1;
constexpr std::uintmax_t kMaximumFileBytes = std::uintmax_t{64} * 1024;
constexpr int kJsonIndent = 2;

struct NamedKey {
  std::string_view name;
  SDL_Scancode scancode;
};

// Names for the keys a player is likely to bind. The scancodes are USB HID
// usages, which SDL keeps stable; the names are this file format's own.
constexpr NamedKey kKeyNames[] = {
    {"A", SDL_SCANCODE_A},
    {"B", SDL_SCANCODE_B},
    {"C", SDL_SCANCODE_C},
    {"D", SDL_SCANCODE_D},
    {"E", SDL_SCANCODE_E},
    {"F", SDL_SCANCODE_F},
    {"G", SDL_SCANCODE_G},
    {"H", SDL_SCANCODE_H},
    {"I", SDL_SCANCODE_I},
    {"J", SDL_SCANCODE_J},
    {"K", SDL_SCANCODE_K},
    {"L", SDL_SCANCODE_L},
    {"M", SDL_SCANCODE_M},
    {"N", SDL_SCANCODE_N},
    {"O", SDL_SCANCODE_O},
    {"P", SDL_SCANCODE_P},
    {"Q", SDL_SCANCODE_Q},
    {"R", SDL_SCANCODE_R},
    {"S", SDL_SCANCODE_S},
    {"T", SDL_SCANCODE_T},
    {"U", SDL_SCANCODE_U},
    {"V", SDL_SCANCODE_V},
    {"W", SDL_SCANCODE_W},
    {"X", SDL_SCANCODE_X},
    {"Y", SDL_SCANCODE_Y},
    {"Z", SDL_SCANCODE_Z},
    {"1", SDL_SCANCODE_1},
    {"2", SDL_SCANCODE_2},
    {"3", SDL_SCANCODE_3},
    {"4", SDL_SCANCODE_4},
    {"5", SDL_SCANCODE_5},
    {"6", SDL_SCANCODE_6},
    {"7", SDL_SCANCODE_7},
    {"8", SDL_SCANCODE_8},
    {"9", SDL_SCANCODE_9},
    {"0", SDL_SCANCODE_0},
    {"Return", SDL_SCANCODE_RETURN},
    {"Escape", SDL_SCANCODE_ESCAPE},
    {"Backspace", SDL_SCANCODE_BACKSPACE},
    {"Tab", SDL_SCANCODE_TAB},
    {"Space", SDL_SCANCODE_SPACE},
    {"Minus", SDL_SCANCODE_MINUS},
    {"Equals", SDL_SCANCODE_EQUALS},
    {"LeftBracket", SDL_SCANCODE_LEFTBRACKET},
    {"RightBracket", SDL_SCANCODE_RIGHTBRACKET},
    {"Backslash", SDL_SCANCODE_BACKSLASH},
    {"Semicolon", SDL_SCANCODE_SEMICOLON},
    {"Apostrophe", SDL_SCANCODE_APOSTROPHE},
    {"Grave", SDL_SCANCODE_GRAVE},
    {"Comma", SDL_SCANCODE_COMMA},
    {"Period", SDL_SCANCODE_PERIOD},
    {"Slash", SDL_SCANCODE_SLASH},
    {"CapsLock", SDL_SCANCODE_CAPSLOCK},
    {"F1", SDL_SCANCODE_F1},
    {"F2", SDL_SCANCODE_F2},
    {"F3", SDL_SCANCODE_F3},
    {"F4", SDL_SCANCODE_F4},
    {"F5", SDL_SCANCODE_F5},
    {"F6", SDL_SCANCODE_F6},
    {"F7", SDL_SCANCODE_F7},
    {"F8", SDL_SCANCODE_F8},
    {"F9", SDL_SCANCODE_F9},
    {"F10", SDL_SCANCODE_F10},
    {"F11", SDL_SCANCODE_F11},
    {"F12", SDL_SCANCODE_F12},
    {"Insert", SDL_SCANCODE_INSERT},
    {"Home", SDL_SCANCODE_HOME},
    {"PageUp", SDL_SCANCODE_PAGEUP},
    {"Delete", SDL_SCANCODE_DELETE},
    {"End", SDL_SCANCODE_END},
    {"PageDown", SDL_SCANCODE_PAGEDOWN},
    {"Right", SDL_SCANCODE_RIGHT},
    {"Left", SDL_SCANCODE_LEFT},
    {"Down", SDL_SCANCODE_DOWN},
    {"Up", SDL_SCANCODE_UP},
    {"KeypadDivide", SDL_SCANCODE_KP_DIVIDE},
    {"KeypadMultiply", SDL_SCANCODE_KP_MULTIPLY},
    {"KeypadMinus", SDL_SCANCODE_KP_MINUS},
    {"KeypadPlus", SDL_SCANCODE_KP_PLUS},
    {"KeypadEnter", SDL_SCANCODE_KP_ENTER},
    {"Keypad1", SDL_SCANCODE_KP_1},
    {"Keypad2", SDL_SCANCODE_KP_2},
    {"Keypad3", SDL_SCANCODE_KP_3},
    {"Keypad4", SDL_SCANCODE_KP_4},
    {"Keypad5", SDL_SCANCODE_KP_5},
    {"Keypad6", SDL_SCANCODE_KP_6},
    {"Keypad7", SDL_SCANCODE_KP_7},
    {"Keypad8", SDL_SCANCODE_KP_8},
    {"Keypad9", SDL_SCANCODE_KP_9},
    {"Keypad0", SDL_SCANCODE_KP_0},
    {"KeypadPeriod", SDL_SCANCODE_KP_PERIOD},
    {"LeftCtrl", SDL_SCANCODE_LCTRL},
    {"LeftShift", SDL_SCANCODE_LSHIFT},
    {"LeftAlt", SDL_SCANCODE_LALT},
    {"RightCtrl", SDL_SCANCODE_RCTRL},
    {"RightShift", SDL_SCANCODE_RSHIFT},
    {"RightAlt", SDL_SCANCODE_RALT},
};

// Indexed by GamepadControl.
constexpr std::array<std::string_view, kGamepadControlCount> kControlNames{
    "South",        "East",          "West",        "North",
    "DpadUp",       "DpadDown",      "DpadLeft",    "DpadRight",
    "LeftShoulder", "RightShoulder", "LeftStick",   "RightStick",
    "Start",        "Back",          "LeftTrigger", "RightTrigger",
};

constexpr std::size_t Index(Action action) noexcept {
  return static_cast<std::size_t>(action);
}

constexpr Key ToKey(SDL_Scancode scancode) noexcept {
  return static_cast<Key>(scancode);
}

std::string_view NameOf(Key key) { return KeyName(key); }
std::string_view NameOf(GamepadControl control) {
  return GamepadControlName(control);
}

template <typename Control>
std::optional<Control> ParseControl(std::string_view name) {
  if constexpr (std::is_same_v<Control, Key>) {
    return ParseKey(name);
  } else {
    return ParseGamepadControl(name);
  }
}

template <typename Control>
constexpr std::string_view ControlNoun() {
  if constexpr (std::is_same_v<Control, Key>) {
    return "key";
  } else {
    return "control";
  }
}

// A JSON integer as a signed value; nullopt for anything else, including an
// unsigned value too large for it.
std::optional<std::int64_t> Integer(const Json& value) {
  if (value.is_number_unsigned()) {
    const auto unsigned_value = value.get<std::uint64_t>();
    if (unsigned_value >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      return std::nullopt;
    }
    return static_cast<std::int64_t>(unsigned_value);
  }
  if (value.is_number_integer()) return value.get<std::int64_t>();
  return std::nullopt;
}

// Reads one device's section: each listed action's controls replace its
// defaults.
template <typename Control>
void ReadSection(const Json& section, std::string_view device,
                 std::array<std::vector<Control>, kActionCount>& bound,
                 std::vector<std::string>& problems) {
  if (!section.is_object()) {
    problems.push_back(std::format("{} must be an object", device));
    return;
  }
  for (const auto& [action_name, list] : section.items()) {
    const auto action = ParseAction(action_name);
    if (!action) {
      problems.push_back(
          std::format("{}: unknown action \"{}\"", device, action_name));
      continue;
    }
    if (!list.is_array()) {
      problems.push_back(
          std::format("{}.{} must be a list of names", device, action_name));
      continue;
    }
    std::vector<Control> controls;
    for (const Json& entry : list) {
      if (!entry.is_string()) {
        problems.push_back(
            std::format("{}.{} must be a list of names", device, action_name));
        continue;
      }
      const auto& name = entry.get_ref<const std::string&>();
      const auto control = ParseControl<Control>(name);
      if (!control) {
        problems.push_back(std::format("{}.{}: unknown {} \"{}\"", device,
                                       action_name, ControlNoun<Control>(),
                                       name));
      } else if (std::ranges::find(controls, *control) != controls.end()) {
        problems.push_back(std::format("{}.{}: \"{}\" is listed twice", device,
                                       action_name, name));
      } else {
        controls.push_back(*control);
      }
    }
    bound[Index(*action)] = std::move(controls);
  }
}

// One physical input sets one action, as each pad button set one bit.
template <typename Control>
void CheckUnique(const std::array<std::vector<Control>, kActionCount>& bound,
                 std::string_view device, std::vector<std::string>& problems) {
  std::map<Control, Action> owners;
  for (const Action action : kActions) {
    for (const Control control : bound[Index(action)]) {
      const auto [owner, inserted] = owners.try_emplace(control, action);
      if (!inserted && owner->second != action) {
        problems.push_back(std::format(
            "{} \"{}\" is bound to both {} and {}", device, NameOf(control),
            ActionName(owner->second), ActionName(action)));
      }
    }
  }
}

void ReadThreshold(const Json& value, Bindings& bindings,
                   std::vector<std::string>& problems) {
  const auto threshold = Integer(value);
  if (!threshold || *threshold < 1 ||
      *threshold > std::numeric_limits<std::int16_t>::max()) {
    problems.push_back(std::format("trigger_threshold must be 1 to {}",
                                   std::numeric_limits<std::int16_t>::max()));
    return;
  }
  bindings.trigger_threshold = static_cast<std::int16_t>(*threshold);
}

// Parses JSON text, listing object keys that appear twice: the library would
// otherwise keep the last silently.
std::expected<Json, std::string> ParseJson(std::string_view text,
                                           std::vector<std::string>& problems) {
  std::vector<std::vector<std::string>> objects;
  try {
    return Json::parse(text, [&objects, &problems](int /*depth*/,
                                                   Json::parse_event_t event,
                                                   Json& parsed) {
      if (event == Json::parse_event_t::object_start) {
        objects.emplace_back();
      } else if (event == Json::parse_event_t::object_end && !objects.empty()) {
        objects.pop_back();
      } else if (event == Json::parse_event_t::key && !objects.empty()) {
        auto name = parsed.get<std::string>();
        auto& seen = objects.back();
        if (std::ranges::find(seen, name) != seen.end()) {
          problems.push_back(
              std::format("\"{}\" appears twice in one object", name));
        } else {
          seen.push_back(std::move(name));
        }
      }
      return true;
    });
  } catch (const Json::exception& error) {
    return std::unexpected(std::format("not valid JSON: {}", error.what()));
  }
}

std::expected<std::string, std::string> ReadText(
    const std::filesystem::path& path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error) {
    return std::unexpected(std::format("cannot read: {}", error.message()));
  }
  if (size > kMaximumFileBytes) {
    return std::unexpected(
        std::format("larger than {} bytes", kMaximumFileBytes));
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return std::unexpected("cannot open");
  std::string text{std::istreambuf_iterator<char>(stream),
                   std::istreambuf_iterator<char>()};
  if (stream.bad()) return std::unexpected("cannot read");
  return text;
}

std::expected<void, std::string> WriteDefaults(
    const std::filesystem::path& path, const Bindings& bindings) {
  std::error_code error;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      return std::unexpected(
          std::format("cannot create its folder: {}", error.message()));
    }
  }
  auto temporary = path;
  temporary += ".tmp";
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    stream << FormatBindings(bindings);
    stream.flush();
    if (!stream) {
      std::filesystem::remove(temporary, error);
      return std::unexpected("cannot write the defaults");
    }
  }
  std::filesystem::rename(temporary, path, error);
  if (error) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return std::unexpected(
        std::format("cannot write the defaults: {}", error.message()));
  }
  return {};
}

}  // namespace

std::optional<Key> ParseKey(std::string_view name) noexcept {
  for (const auto& key : kKeyNames) {
    if (key.name == name) return ToKey(key.scancode);
  }
  return std::nullopt;
}

std::string_view KeyName(Key key) noexcept {
  for (const auto& named : kKeyNames) {
    if (ToKey(named.scancode) == key) return named.name;
  }
  return {};
}

std::optional<GamepadControl> ParseGamepadControl(
    std::string_view name) noexcept {
  for (std::size_t index = 0; index < kControlNames.size(); ++index) {
    if (kControlNames.at(index) == name) {
      return static_cast<GamepadControl>(index);
    }
  }
  return std::nullopt;
}

std::string_view GamepadControlName(GamepadControl control) noexcept {
  const auto index = static_cast<std::size_t>(control);
  return index < kControlNames.size() ? kControlNames.at(index)
                                      : std::string_view{};
}

Bindings DefaultBindings() {
  Bindings bindings;
  const auto key = [&bindings](Action action, SDL_Scancode scancode) {
    bindings.keys.at(Index(action)).push_back(ToKey(scancode));
  };
  key(Action::kUp, SDL_SCANCODE_UP);
  key(Action::kDown, SDL_SCANCODE_DOWN);
  key(Action::kLeft, SDL_SCANCODE_LEFT);
  key(Action::kRight, SDL_SCANCODE_RIGHT);
  key(Action::kCross, SDL_SCANCODE_X);
  key(Action::kCircle, SDL_SCANCODE_C);
  key(Action::kSquare, SDL_SCANCODE_Z);
  key(Action::kTriangle, SDL_SCANCODE_S);
  key(Action::kL1, SDL_SCANCODE_Q);
  key(Action::kR1, SDL_SCANCODE_E);
  key(Action::kL2, SDL_SCANCODE_1);
  key(Action::kR2, SDL_SCANCODE_3);
  key(Action::kStart, SDL_SCANCODE_RETURN);
  key(Action::kSelect, SDL_SCANCODE_BACKSPACE);
  const auto pad = [&bindings](Action action, GamepadControl control) {
    bindings.gamepad.at(Index(action)).push_back(control);
  };
  pad(Action::kUp, GamepadControl::kDpadUp);
  pad(Action::kDown, GamepadControl::kDpadDown);
  pad(Action::kLeft, GamepadControl::kDpadLeft);
  pad(Action::kRight, GamepadControl::kDpadRight);
  pad(Action::kCross, GamepadControl::kSouth);
  pad(Action::kCircle, GamepadControl::kEast);
  pad(Action::kSquare, GamepadControl::kWest);
  pad(Action::kTriangle, GamepadControl::kNorth);
  pad(Action::kL1, GamepadControl::kLeftShoulder);
  pad(Action::kR1, GamepadControl::kRightShoulder);
  pad(Action::kL2, GamepadControl::kLeftTrigger);
  pad(Action::kR2, GamepadControl::kRightTrigger);
  pad(Action::kL3, GamepadControl::kLeftStick);
  pad(Action::kR3, GamepadControl::kRightStick);
  pad(Action::kStart, GamepadControl::kStart);
  pad(Action::kSelect, GamepadControl::kBack);
  return bindings;
}

ActionSet ResolveKeyboard(const Bindings& bindings,
                          std::span<const bool> keys) {
  ActionSet held;
  for (const Action action : kActions) {
    for (const Key key : bindings.keys.at(Index(action))) {
      const auto scancode = static_cast<std::size_t>(key);
      if (scancode < keys.size() && keys[scancode]) {
        held.Insert(action);
        break;
      }
    }
  }
  return held;
}

ActionSet ResolveGamepad(const Bindings& bindings, const GamepadSnapshot& pad) {
  const auto control_held = [&bindings, &pad](GamepadControl control) {
    switch (control) {
      case GamepadControl::kLeftTrigger:
        return pad.left_trigger > bindings.trigger_threshold;
      case GamepadControl::kRightTrigger:
        return pad.right_trigger > bindings.trigger_threshold;
      default: {
        const auto index = static_cast<std::size_t>(control);
        return index < pad.buttons.size() && pad.buttons.at(index);
      }
    }
  };
  ActionSet held;
  for (const Action action : kActions) {
    if (std::ranges::any_of(bindings.gamepad.at(Index(action)), control_held)) {
      held.Insert(action);
    }
  }
  return held;
}

std::expected<Bindings, std::vector<std::string>> ParseBindings(
    std::string_view text, std::span<const Key> reserved) {
  std::vector<std::string> problems;
  auto root = ParseJson(text, problems);
  if (!root) return std::unexpected(std::vector<std::string>{root.error()});
  if (!root->is_object()) {
    return std::unexpected(
        std::vector<std::string>{"the file must hold a JSON object"});
  }
  Bindings bindings = DefaultBindings();
  for (const auto& [name, value] : root->items()) {
    if (name == "version") {
      if (Integer(value) != kVersion) {
        problems.push_back(std::format("version must be {}", kVersion));
      }
    } else if (name == "keyboard") {
      ReadSection(value, "keyboard", bindings.keys, problems);
    } else if (name == "gamepad") {
      ReadSection(value, "gamepad", bindings.gamepad, problems);
    } else if (name == "trigger_threshold") {
      ReadThreshold(value, bindings, problems);
    } else {
      problems.push_back(std::format("unknown setting \"{}\"", name));
    }
  }
  if (!root->contains("version")) problems.emplace_back("version is missing");
  CheckUnique(bindings.keys, "keyboard key", problems);
  CheckUnique(bindings.gamepad, "gamepad control", problems);
  for (const Action action : kActions) {
    for (const Key key : bindings.keys.at(Index(action))) {
      if (std::ranges::find(reserved, key) != reserved.end()) {
        problems.push_back(
            std::format("keyboard key \"{}\" ({}) is reserved by this program",
                        KeyName(key), ActionName(action)));
      }
    }
  }
  if (!problems.empty()) return std::unexpected(std::move(problems));
  return bindings;
}

std::string FormatBindings(const Bindings& bindings) {
  nlohmann::ordered_json keyboard = nlohmann::ordered_json::object();
  nlohmann::ordered_json gamepad = nlohmann::ordered_json::object();
  for (const Action action : kActions) {
    auto keys = nlohmann::ordered_json::array();
    for (const Key key : bindings.keys.at(Index(action))) {
      keys.push_back(KeyName(key));
    }
    auto controls = nlohmann::ordered_json::array();
    for (const GamepadControl control : bindings.gamepad.at(Index(action))) {
      controls.push_back(GamepadControlName(control));
    }
    const std::string name(ActionName(action));
    keyboard[name] = std::move(keys);
    gamepad[name] = std::move(controls);
  }
  nlohmann::ordered_json root = nlohmann::ordered_json::object();
  root["version"] = kVersion;
  root["keyboard"] = std::move(keyboard);
  root["gamepad"] = std::move(gamepad);
  root["trigger_threshold"] = bindings.trigger_threshold;
  return root.dump(kJsonIndent) + "\n";
}

std::string_view BindingsSourceName(BindingsSource source) noexcept {
  switch (source) {
    case BindingsSource::kFile:
      return "file";
    case BindingsSource::kCreated:
      return "created";
    case BindingsSource::kDefaults:
      return "defaults";
  }
  return {};
}

LoadedBindings LoadBindingsFile(const std::filesystem::path& path,
                                std::span<const Key> reserved) {
  LoadedBindings loaded{.bindings = DefaultBindings(),
                        .source = BindingsSource::kDefaults,
                        .diagnostics = {}};
  const auto text_path = Platform::PathText(path);
  std::error_code error;
  const bool exists = std::filesystem::exists(path, error);
  if (error) {
    loaded.diagnostics.push_back(
        std::format("cannot check for it: {}", error.message()));
  } else if (!exists) {
    if (auto written = WriteDefaults(path, loaded.bindings); written) {
      loaded.source = BindingsSource::kCreated;
    } else {
      loaded.diagnostics.push_back(written.error());
    }
  } else if (auto text = ReadText(path); !text) {
    loaded.diagnostics.push_back(text.error());
  } else if (auto parsed = ParseBindings(*text, reserved); !parsed) {
    loaded.diagnostics = std::move(parsed.error());
  } else {
    loaded.bindings = std::move(*parsed);
    loaded.source = BindingsSource::kFile;
  }
  for (const auto& diagnostic : loaded.diagnostics) {
    Core::LogWarn("input", "bindings {}: {}", text_path, diagnostic);
  }
  if (loaded.source == BindingsSource::kDefaults) {
    Core::LogWarn("input",
                  "bindings {}: using the default bindings; the file is left "
                  "unchanged",
                  text_path);
  } else {
    Core::LogInfo("input", "bindings {}: {}", text_path,
                  BindingsSourceName(loaded.source));
  }
  return loaded;
}

}  // namespace VS::Input
