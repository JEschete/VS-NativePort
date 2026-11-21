#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Input/Actions.hpp"
#include "Input/Bindings.hpp"

namespace {
namespace fs = std::filesystem;
namespace Input = VS::Input;
using Input::Action;
using Input::GamepadControl;

using KeyState = std::array<bool, SDL_SCANCODE_COUNT>;

Input::ActionSet PressKey(const Input::Bindings& bindings, SDL_Scancode key) {
  KeyState keys{};
  keys.at(key) = true;
  return Input::ResolveKeyboard(bindings, keys);
}

Input::ActionSet PressControl(const Input::Bindings& bindings,
                              GamepadControl control) {
  Input::GamepadSnapshot pad;
  pad.buttons.at(static_cast<std::size_t>(control)) = true;
  return Input::ResolveGamepad(bindings, pad);
}

bool Mentions(const std::vector<std::string>& problems, std::string_view text) {
  return std::ranges::any_of(problems, [text](const std::string& problem) {
    return problem.find(text) != std::string::npos;
  });
}

std::vector<std::string> Problems(std::string_view text,
                                  std::span<const Input::Key> reserved = {}) {
  auto parsed = Input::ParseBindings(text, reserved);
  if (parsed) return {};
  return std::move(parsed.error());
}

std::string ReadAll(const fs::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

struct ScratchDirectory {
  fs::path root;

  ScratchDirectory() {
    static std::atomic<unsigned> sequence{0};
    root =
        fs::temp_directory_path() /
        std::format("vs-native-bindings-{}-{}",
                    std::chrono::steady_clock::now().time_since_epoch().count(),
                    sequence.fetch_add(1));
  }
  ~ScratchDirectory() {
    try {
      std::error_code ignored;
      fs::remove_all(root, ignored);
    } catch (const std::exception& failure) {
      std::fprintf(stderr, "bindings scratch left files: %s\n", failure.what());
    }
  }
  ScratchDirectory(const ScratchDirectory&) = delete;
  ScratchDirectory& operator=(const ScratchDirectory&) = delete;
};

}  // namespace

TEST_CASE("The default bindings read the original keyboard layout",
          "[input-bindings]") {
  const auto bindings = Input::DefaultBindings();
  const std::array<std::pair<SDL_Scancode, Action>, 14> layout{{
      {SDL_SCANCODE_UP, Action::kUp},
      {SDL_SCANCODE_DOWN, Action::kDown},
      {SDL_SCANCODE_LEFT, Action::kLeft},
      {SDL_SCANCODE_RIGHT, Action::kRight},
      {SDL_SCANCODE_X, Action::kCross},
      {SDL_SCANCODE_C, Action::kCircle},
      {SDL_SCANCODE_Z, Action::kSquare},
      {SDL_SCANCODE_S, Action::kTriangle},
      {SDL_SCANCODE_Q, Action::kL1},
      {SDL_SCANCODE_E, Action::kR1},
      {SDL_SCANCODE_1, Action::kL2},
      {SDL_SCANCODE_3, Action::kR2},
      {SDL_SCANCODE_RETURN, Action::kStart},
      {SDL_SCANCODE_BACKSPACE, Action::kSelect},
  }};
  for (const auto& [key, action] : layout) {
    CAPTURE(Input::ActionName(action));
    CHECK(PressKey(bindings, key) == Input::ActionSet{action});
  }
  CHECK(PressKey(bindings, SDL_SCANCODE_SPACE).Empty());
  // A key-state array shorter than a bound scancode reads that key as up.
  const std::array<bool, 1> short_state{true};
  CHECK(Input::ResolveKeyboard(bindings, short_state).Empty());
}

TEST_CASE("The default bindings read the gamepad by position",
          "[input-bindings]") {
  const auto bindings = Input::DefaultBindings();
  const std::array<std::pair<GamepadControl, Action>, 14> layout{{
      {GamepadControl::kSouth, Action::kCross},
      {GamepadControl::kEast, Action::kCircle},
      {GamepadControl::kWest, Action::kSquare},
      {GamepadControl::kNorth, Action::kTriangle},
      {GamepadControl::kDpadUp, Action::kUp},
      {GamepadControl::kDpadDown, Action::kDown},
      {GamepadControl::kDpadLeft, Action::kLeft},
      {GamepadControl::kDpadRight, Action::kRight},
      {GamepadControl::kLeftShoulder, Action::kL1},
      {GamepadControl::kRightShoulder, Action::kR1},
      {GamepadControl::kLeftStick, Action::kL3},
      {GamepadControl::kRightStick, Action::kR3},
      {GamepadControl::kStart, Action::kStart},
      {GamepadControl::kBack, Action::kSelect},
  }};
  for (const auto& [control, action] : layout) {
    CAPTURE(Input::GamepadControlName(control));
    CHECK(PressControl(bindings, control) == Input::ActionSet{action});
  }
}

TEST_CASE("A trigger holds its action only past the threshold",
          "[input-bindings]") {
  auto bindings = Input::DefaultBindings();
  Input::GamepadSnapshot pad;
  pad.left_trigger = Input::Bindings::kDefaultTriggerThreshold;
  pad.right_trigger = Input::Bindings::kDefaultTriggerThreshold + 1;
  CHECK(Input::ResolveGamepad(bindings, pad) == Input::ActionSet{Action::kR2});

  bindings.trigger_threshold = 100;
  pad.left_trigger = 101;
  pad.right_trigger = 100;
  CHECK(Input::ResolveGamepad(bindings, pad) == Input::ActionSet{Action::kL2});
}

TEST_CASE("A bindings file replaces only the actions it lists",
          "[input-bindings]") {
  const auto parsed = Input::ParseBindings(R"({
    "version": 1,
    "keyboard": {"Cross": ["Space", "K"], "Circle": []},
    "gamepad": {"Cross": ["East"], "Circle": ["South"]},
    "trigger_threshold": 32767
  })");
  REQUIRE(parsed.has_value());
  const auto& bindings = *parsed;
  CHECK(PressKey(bindings, SDL_SCANCODE_SPACE) ==
        Input::ActionSet{Action::kCross});
  CHECK(PressKey(bindings, SDL_SCANCODE_K) == Input::ActionSet{Action::kCross});
  CHECK(PressKey(bindings, SDL_SCANCODE_X).Empty());
  CHECK(PressKey(bindings, SDL_SCANCODE_C).Empty());
  CHECK(PressKey(bindings, SDL_SCANCODE_UP) == Input::ActionSet{Action::kUp});
  CHECK(PressControl(bindings, GamepadControl::kEast) ==
        Input::ActionSet{Action::kCross});
  CHECK(PressControl(bindings, GamepadControl::kSouth) ==
        Input::ActionSet{Action::kCircle});
  CHECK(bindings.trigger_threshold == 32767);
}

TEST_CASE("A bindings file round-trips through its text", "[input-bindings]") {
  const auto defaults = Input::DefaultBindings();
  const auto text = Input::FormatBindings(defaults);
  CHECK(
      text.starts_with("{\n  \"version\": 1,\n  \"keyboard\": {\n"
                       "    \"Up\": [\n      \"Up\"\n    ],"));
  CHECK(Input::ParseBindings(text) == defaults);

  auto custom = defaults;
  custom.keys.at(static_cast<std::size_t>(Action::kL3))
      .push_back(*Input::ParseKey("KeypadEnter"));
  custom.gamepad.at(static_cast<std::size_t>(Action::kSelect)).clear();
  custom.trigger_threshold = 1;
  CHECK(Input::ParseBindings(Input::FormatBindings(custom)) == custom);
}

TEST_CASE("Every problem in a bindings file is listed and rejects it",
          "[input-bindings]") {
  const auto problems = Problems(R"({
    "version": 2,
    "keyboard": {"Jump": ["X"], "Cross": ["Nope", "Space", "Space"]},
    "gamepad": {"Circle": "East", "Square": [3]},
    "trigger_threshold": 0,
    "mouse": {}
  })");
  CHECK(Mentions(problems, "version must be 1"));
  CHECK(Mentions(problems, "keyboard: unknown action \"Jump\""));
  CHECK(Mentions(problems, "keyboard.Cross: unknown key \"Nope\""));
  CHECK(Mentions(problems, "keyboard.Cross: \"Space\" is listed twice"));
  CHECK(Mentions(problems, "gamepad.Circle must be a list of names"));
  CHECK(Mentions(problems, "gamepad.Square must be a list of names"));
  CHECK(Mentions(problems, "trigger_threshold must be 1 to 32767"));
  CHECK(Mentions(problems, "unknown setting \"mouse\""));
  CHECK(problems.size() == 8);
}

TEST_CASE("Bindings files with a bad shape or values are refused",
          "[input-bindings]") {
  CHECK(Mentions(Problems("{\"version\": 1,"), "not valid JSON"));
  CHECK(Mentions(Problems("[1]"), "must hold a JSON object"));
  CHECK(Mentions(Problems("{}"), "version is missing"));
  CHECK(Mentions(Problems(R"({"version": 1.0})"), "version must be 1"));
  CHECK(Mentions(Problems(R"({"version": 1, "keyboard": []})"),
                 "keyboard must be an object"));
  for (const std::string_view threshold :
       {"-1", "32768", "18446744073709551615", "\"16\"", "1.5"}) {
    CAPTURE(threshold);
    CHECK(Mentions(
        Problems(std::format(R"({{"version": 1, "trigger_threshold": {}}})",
                             threshold)),
        "trigger_threshold must be 1 to 32767"));
  }
  CHECK(Mentions(Problems(R"({"version": 1, "gamepad": {"L1": ["Paddle"]}})"),
                 "gamepad.L1: unknown control \"Paddle\""));
}

TEST_CASE("One physical input cannot hold two actions", "[input-bindings]") {
  CHECK(Problems(R"({"version": 1, "keyboard": {"Circle": ["X"]}})") ==
        std::vector<std::string>{
            "keyboard key \"X\" is bound to both Cross and Circle"});
  CHECK(Problems(R"({"version": 1, "gamepad": {"L3": ["LeftShoulder"]}})") ==
        std::vector<std::string>{
            "gamepad control \"LeftShoulder\" is bound to both L1 and L3"});
  // Moving a key frees it: Cross takes C once Circle gives it up.
  CHECK(Problems(R"({"version": 1,
                     "keyboard": {"Cross": ["C"], "Circle": ["X"]}})")
            .empty());
}

TEST_CASE("A key named twice in one object is refused", "[input-bindings]") {
  CHECK(Mentions(
      Problems(
          R"({"version": 1, "keyboard": {"Cross": ["K"], "Cross": ["X"]}})"),
      "\"Cross\" appears twice in one object"));
  CHECK(Mentions(Problems(R"({"version": 1, "version": 1})"),
                 "\"version\" appears twice in one object"));
  // The same name in different objects is fine.
  CHECK(Problems(R"({"version": 1, "keyboard": {"Cross": ["X"]},
                     "gamepad": {"Cross": ["South"]}})")
            .empty());
}

TEST_CASE("A host's reserved keys cannot be bound", "[input-bindings]") {
  const std::array reserved{*Input::ParseKey("F1"), *Input::ParseKey("F10")};
  CHECK(Problems(R"({"version": 1, "keyboard": {"Select": ["F10"]}})",
                 reserved) ==
        std::vector<std::string>{
            "keyboard key \"F10\" (Select) is reserved by this program"});
  CHECK(Problems(R"({"version": 1, "keyboard": {"Select": ["F9"]}})", reserved)
            .empty());
  // The defaults leave the Combat Lab's function keys free.
  CHECK(Problems(R"({"version": 1})", reserved).empty());
}

TEST_CASE("Key and control names round-trip", "[input-bindings]") {
  for (const std::string_view name :
       {"A", "Z", "0", "9", "F12", "Return", "Keypad0", "KeypadEnter",
        "RightAlt", "Grave", "PageDown"}) {
    const auto key = Input::ParseKey(name);
    REQUIRE(key.has_value());
    CHECK(Input::KeyName(*key) == name);
  }
  CHECK_FALSE(Input::ParseKey("return").has_value());
  CHECK(Input::KeyName(static_cast<Input::Key>(SDL_SCANCODE_MUTE)).empty());
  for (std::size_t index = 0; index < Input::kGamepadControlCount; ++index) {
    const auto control = static_cast<GamepadControl>(index);
    CHECK(Input::ParseGamepadControl(Input::GamepadControlName(control)) ==
          control);
  }
}

TEST_CASE("A missing bindings file is created with the defaults",
          "[input-bindings]") {
  const ScratchDirectory scratch;
  const auto path = scratch.root / "config" / "input.json";
  const auto created = Input::LoadBindingsFile(path);
  CHECK(created.source == Input::BindingsSource::kCreated);
  CHECK(created.diagnostics.empty());
  CHECK(created.bindings == Input::DefaultBindings());
  CHECK(ReadAll(path) == Input::FormatBindings(Input::DefaultBindings()));
  CHECK_FALSE(fs::exists(fs::path(path) += ".tmp"));

  const auto reread = Input::LoadBindingsFile(path);
  CHECK(reread.source == Input::BindingsSource::kFile);
  CHECK(reread.bindings == Input::DefaultBindings());
}

TEST_CASE("An invalid bindings file is kept and the defaults apply",
          "[input-bindings]") {
  const ScratchDirectory scratch;
  fs::create_directories(scratch.root);
  const auto path = scratch.root / "input.json";
  const std::string text =
      R"({"version": 1, "keyboard": {"Cross": ["Space"], "Circle": ["Space"]}})";
  {
    std::ofstream stream(path, std::ios::binary);
    stream << text;
  }
  const auto loaded = Input::LoadBindingsFile(path);
  CHECK(loaded.source == Input::BindingsSource::kDefaults);
  CHECK(loaded.bindings == Input::DefaultBindings());
  CHECK(loaded.diagnostics ==
        std::vector<std::string>{
            "keyboard key \"Space\" is bound to both Cross and Circle"});
  CHECK(ReadAll(path) == text);

  // A valid file replaces the defaults; so does fixing the rejected one.
  {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << R"({"version": 1, "keyboard": {"Cross": ["Space"]}})";
  }
  const auto fixed = Input::LoadBindingsFile(path);
  CHECK(fixed.source == Input::BindingsSource::kFile);
  CHECK(PressKey(fixed.bindings, SDL_SCANCODE_SPACE) ==
        Input::ActionSet{Action::kCross});
}

TEST_CASE("A bindings file over 64 KiB is not read", "[input-bindings]") {
  const ScratchDirectory scratch;
  fs::create_directories(scratch.root);
  const auto path = scratch.root / "input.json";
  const std::string text =
      R"({"version": 1})" + std::string(std::size_t{64} * 1024, ' ');
  {
    std::ofstream stream(path, std::ios::binary);
    stream << text;
  }
  const auto loaded = Input::LoadBindingsFile(path);
  CHECK(loaded.source == Input::BindingsSource::kDefaults);
  CHECK(loaded.diagnostics ==
        std::vector<std::string>{"larger than 65536 bytes"});
  CHECK(ReadAll(path) == text);
}

TEST_CASE("A bindings path that cannot be created leaves the defaults",
          "[input-bindings]") {
  const ScratchDirectory scratch;
  fs::create_directories(scratch.root);
  // A file where the config folder should be.
  const auto blocker = scratch.root / "config";
  {
    std::ofstream stream(blocker, std::ios::binary);
    stream << "not a folder";
  }
  const auto loaded = Input::LoadBindingsFile(blocker / "input.json");
  CHECK(loaded.source == Input::BindingsSource::kDefaults);
  CHECK(loaded.bindings == Input::DefaultBindings());
  CHECK(Mentions(loaded.diagnostics, "cannot create its folder"));
  CHECK(ReadAll(blocker) == "not a folder");
}
