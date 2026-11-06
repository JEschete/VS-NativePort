#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Input/Controller.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/LegacyPad.hpp"
#include "Input/Pad.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Input/ControllerLiveState.h"
#include "Input/Controllers.h"
#include "Input/Vibration.h"
}

namespace {
struct Command {
  Uint16 low;
  Uint16 high;
};

struct GamepadSubsystem {
  bool ready = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
  ~GamepadSubsystem() {
    if (ready) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
  }
};

struct VirtualGamepad {
  SDL_JoystickID id = 0;

  explicit VirtualGamepad(std::vector<Command>& commands) {
    SDL_VirtualJoystickDesc desc{};
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    desc.name = "Native vibration regression";
    desc.userdata = &commands;
    desc.Rumble = [](void* context, Uint16 low, Uint16 high) {
      static_cast<std::vector<Command>*>(context)->push_back({low, high});
      return true;
    };
    id = SDL_AttachVirtualJoystick(&desc);
  }

  ~VirtualGamepad() {
    if (id != 0) SDL_DetachVirtualJoystick(id);
  }
};

struct ActuatorLease {
  char saved_setting = Input_settings.vibrationOn;
  std::array<unsigned char, 34> response{};
  std::array<unsigned char, 2> levels{};
  std::array<unsigned char, 6> alignment{0, 1, 255, 255, 255, 255};

  ActuatorLease() {
    Input_settings.vibrationOn = 1;
    PadInitDirect(response.data(), nullptr);
    VS::Input::PadState state;
    state.analog = true;
    VS::Legacy::FeedPad(0, state, true);
    PadSetAct(0, levels.data(), static_cast<int>(levels.size()));
    PadSetActAlign(0, alignment.data());
  }

  ~ActuatorLease() {
    PadInitDirect(nullptr, nullptr);
    Input_settings.vibrationOn = saved_setting;
  }
};

void Connect(VS::Input::InputSystem& input, SDL_JoystickID id) {
  SDL_Event event{};
  event.type = SDL_EVENT_GAMEPAD_ADDED;
  event.gdevice.which = id;
  input.HandleEvent(event);
  REQUIRE(input.HasGamepad());
}

void CheckLast(const std::vector<Command>& commands, Uint16 low, Uint16 high) {
  REQUIRE_FALSE(commands.empty());
  CHECK(commands.back().low == low);
  CHECK(commands.back().high == high);
}

struct RumbleFixture {
  GamepadSubsystem subsystem;
  std::vector<Command> commands;
  VirtualGamepad device{commands};
  VS::Input::InputSystem input;
  ActuatorLease lease;

  RumbleFixture() {
    REQUIRE(subsystem.ready);
    REQUIRE(device.id != 0);
    Connect(input, device.id);
    commands.clear();
  }

  void Apply(unsigned char small, unsigned char large) {
    lease.levels = {small, large};
    input.Update();
  }
};

struct PatternState {
  VS::Input::VibrationState saved_vibration = VS::Input::LiveVibrationState();
  std::array<Input_ControllerPortState, 2> saved_ports =
      std::to_array(Input_controllerPorts);
  std::array<unsigned char, 11> pattern{5, 2, 1, 0, 0, 2, 128, 2, 128, 0, 0};

  PatternState() {
    VS::Input::LiveVibrationState() = {};
    VS::Input::ConfigureLiveControllerActuators(
        0, Input_legacyPadResponseBytes[0]);
  }

  ~PatternState() {
    VS::Input::LiveVibrationState() = saved_vibration;
    std::copy(saved_ports.begin(), saved_ports.end(), Input_controllerPorts);
  }

  int Start(unsigned int slot, int owner_tag = 0x80) {
    return VS::Input::StartLiveVibrationPattern(
        std::as_bytes(std::span(pattern)), slot, owner_tag);
  }

  static const VS::Input::VibrationSlot& Slot(std::size_t slot_index) {
    return VS::Input::LiveVibrationState().slots.at(slot_index);
  }

  void CheckSlot(std::size_t slot_index, bool active, bool stored = true) {
    CHECK(Slot(slot_index).active == active);
    CHECK(Slot(slot_index).pattern.Bytes().data() ==
          (stored ? reinterpret_cast<const std::byte*>(pattern.data())
                  : nullptr));
  }

  static void CheckPresetSlot(std::size_t slot_index) {
    CHECK(Slot(slot_index).pattern.Bytes().data() ==
          reinterpret_cast<const std::byte*>(Input_vibrationPresets + 8));
    CHECK(Slot(slot_index).owner == 0x80);
    CHECK(Slot(slot_index).active);
  }
};
}  // namespace

TEST_CASE("Original stick axis mapping preserves dead-zone boundaries",
          "[input][stick]") {
  constexpr std::array<int, 9> kAxes{0, 63, 64, 127, 128, 191, 192, 193, 255};
  constexpr std::array<int, 9> kOffsets{-64, -1, 0, 0, 0, 0, 0, 1, 63};
  for (std::size_t index = 0; index < kAxes.size(); ++index) {
    CHECK(VS::Input::MapStickDeadZone(kAxes[index]) == kOffsets[index]);
  }
}

TEST_CASE("Original vibration preset uses its directory and preview owner",
          "[input][vibration][pattern]") {
  PatternState fixture;
  REQUIRE(Input_vibrationPresets[0] == 1);
  VS::Input::PlayLiveVibrationPreset(0);
  PatternState::CheckPresetSlot(0);
  CHECK(PatternState::Slot(1).pattern.Bytes().data() == nullptr);
  VS::Input::PlayLiveVibrationPreset(1);
  CHECK(PatternState::Slot(1).pattern.Bytes().data() == nullptr);
  VS::Input::PlayLiveVibrationPreset(0);
  PatternState::CheckPresetSlot(1);
  VS::Input::PlayLiveVibrationPreset(0);
  PatternState::CheckPresetSlot(0);
  PatternState::CheckPresetSlot(1);
}

TEST_CASE("Live registered actuator bytes reach actual SDL rumble output",
          "[input][vibration]") {
  RumbleFixture fixture;
  fixture.Apply(1, 128);
  CheckLast(fixture.commands, 32896, 65535);
  fixture.Apply(0, 255);
  CheckLast(fixture.commands, 65535, 0);
  fixture.Apply(0, 0);
  CheckLast(fixture.commands, 0, 0);
}

TEST_CASE("Vibration Off and scripted input stop an active SDL effect",
          "[input][vibration]") {
  RumbleFixture fixture;
  fixture.Apply(1, 255);
  CheckLast(fixture.commands, 65535, 65535);
  Input_settings.vibrationOn = 0;
  fixture.input.Update();
  CheckLast(fixture.commands, 0, 0);
  Input_settings.vibrationOn = 1;
  fixture.input.Update();
  CheckLast(fixture.commands, 65535, 65535);
  fixture.input.SetDevicesEnabled(false);
  fixture.input.Update();
  CheckLast(fixture.commands, 0, 0);
}

TEST_CASE("Disconnect and pad reset invalidate active actuator output",
          "[input][vibration]") {
  RumbleFixture fixture;
  fixture.Apply(1, 255);
  CheckLast(fixture.commands, 65535, 65535);
  VS::Legacy::FeedPad(0, {}, false);
  fixture.input.Update();
  CheckLast(fixture.commands, 0, 0);
  VS::Input::PadState state;
  state.analog = true;
  VS::Legacy::FeedPad(0, state, true);
  fixture.input.Update();
  CheckLast(fixture.commands, 65535, 65535);
  PadInitDirect(nullptr, nullptr);
  fixture.input.Update();
  CheckLast(fixture.commands, 0, 0);
}

TEST_CASE("Closing and removing a gamepad stop its active rumble",
          "[input][vibration]") {
  GamepadSubsystem subsystem;
  REQUIRE(subsystem.ready);
  std::vector<Command> commands;
  VirtualGamepad device(commands);
  REQUIRE(device.id != 0);
  ActuatorLease lease;
  lease.levels = {1, 255};
  {
    VS::Input::InputSystem input;
    Connect(input, device.id);
    input.Update();
    CheckLast(commands, 65535, 65535);
  }
  CheckLast(commands, 0, 0);
  VS::Input::InputSystem input;
  Connect(input, device.id);
  input.Update();
  CheckLast(commands, 65535, 65535);
  SDL_Event event{};
  event.type = SDL_EVENT_GAMEPAD_REMOVED;
  event.gdevice.which = device.id;
  input.HandleEvent(event);
  CHECK_FALSE(input.HasGamepad());
  CheckLast(commands, 0, 0);
}

TEST_CASE("Active vibration survives the SDL refresh interval",
          "[input][vibration]") {
  RumbleFixture fixture;
  fixture.Apply(1, 255);
  CheckLast(fixture.commands, 65535, 65535);
  for (int repeat = 0; repeat < 6; ++repeat) {
    SDL_Delay(80);
    SDL_PumpEvents();
    fixture.input.Update();
    CheckLast(fixture.commands, 65535, 65535);
  }
  fixture.Apply(0, 0);
  CheckLast(fixture.commands, 0, 0);
}

TEST_CASE("Unbound and unsupported actuator commands remain silent",
          "[input][vibration]") {
  ActuatorLease lease;
  lease.levels = {1, 255};
  PadSetAct(0, nullptr, 0);
  CHECK(VS::Legacy::ReadActuators(0).low_frequency == 0);
  CHECK(VS::Legacy::ReadActuators(0).high_frequency == 0);
  PadSetAct(0, lease.levels.data(), 2);
  lease.alignment[0] = 1;
  CHECK(PadSetActAlign(0, lease.alignment.data()) == 0);
  CHECK(VS::Legacy::ReadActuators(0).low_frequency == 0);
  CHECK(VS::Legacy::ReadActuators(0).high_frequency == 0);
}

TEST_CASE("No selected gamepad safely ignores registered vibration",
          "[input][vibration]") {
  GamepadSubsystem subsystem;
  REQUIRE(subsystem.ready);
  VS::Input::InputSystem input;
  ActuatorLease lease;
  lease.levels = {1, 255};
  CHECK_FALSE(input.HasGamepad());
  CHECK_FALSE(input.Update().analog);
}

TEST_CASE("Original vibration bytecode retains unsigned motor intensities",
          "[input][vibration][pattern]") {
  RumbleFixture fixture;
  PatternState pattern;
  REQUIRE(pattern.Start(1) == 1);
  VS::Input::UpdateLiveVibrationPatterns();
  fixture.input.Update();
  CheckLast(fixture.commands, 32896, 65535);
}

TEST_CASE("Both original vibration slots have independent complete envelopes",
          "[input][vibration][pattern]") {
  REQUIRE(VS::Input::LiveVibrationState().slots[1].envelopes.size() == 2);
  RumbleFixture fixture;
  PatternState pattern;
  pattern.pattern[2] = 0;
  REQUIRE(pattern.Start(1) == 1);
  REQUIRE(pattern.Start(2) == 2);
  VS::Input::UpdateLiveVibrationPatterns();
  fixture.input.Update();
  CheckLast(fixture.commands, 65535, 0);
}

TEST_CASE("Original vibration allocation and selectors preserve slot IDs",
          "[input][vibration][pattern]") {
  PatternState pattern;
  REQUIRE(pattern.Start(0xFF) == 1);
  REQUIRE(pattern.Start(0xFF, 2) == 2);
  CHECK(pattern.Start(0xFF) == 0);
  CHECK(VS::Input::StopLiveVibrationPatterns(3, 0) == 0);
  CHECK(VS::Input::PauseVibrationPatterns(VS::Input::LiveVibrationState(), 3) ==
        0U);
  CHECK(VS::Input::ResumeVibrationPatterns(VS::Input::LiveVibrationState(),
                                           3) == 0U);
  pattern.CheckSlot(0, true);
  pattern.CheckSlot(1, true);
}

TEST_CASE("Original vibration pause and resume retain pattern storage",
          "[input][vibration][pattern]") {
  PatternState pattern;
  REQUIRE(pattern.Start(1) == 1);
  REQUIRE(pattern.Start(2) == 2);
  CHECK(VS::Input::PauseVibrationPatterns(VS::Input::LiveVibrationState(), 0) ==
        1U);
  pattern.CheckSlot(0, false);
  pattern.CheckSlot(1, false);
  CHECK(VS::Input::ResumeVibrationPatterns(VS::Input::LiveVibrationState(),
                                           2) == 2U);
  pattern.CheckSlot(0, false);
  pattern.CheckSlot(1, true);
  CHECK(VS::Input::ResumeVibrationPatterns(VS::Input::LiveVibrationState(),
                                           0) == 1U);
  pattern.CheckSlot(0, true);
  pattern.CheckSlot(1, true);
}

TEST_CASE("Original vibration stop selectors preserve owner filtering",
          "[input][vibration][pattern]") {
  PatternState pattern;
  REQUIRE(pattern.Start(1) == 1);
  REQUIRE(pattern.Start(2, 2) == 2);
  CHECK(VS::Input::StopLiveVibrationPatterns(0, 0x80) == 1);
  pattern.CheckSlot(0, false, false);
  pattern.CheckSlot(1, true);
  CHECK(VS::Input::StopLiveVibrationPatterns(2, 0x80) == 2);
  pattern.CheckSlot(1, false, false);

  REQUIRE(pattern.Start(1) == 1);
  REQUIRE(pattern.Start(2) == 2);
  CHECK(VS::Input::StopLiveVibrationPatterns(0, 0) == 1);
  pattern.CheckSlot(0, false, false);
  pattern.CheckSlot(1, false, false);
}

TEST_CASE("Original vibration reset clears slots and controller connections",
          "[input][vibration][pattern]") {
  PatternState pattern;
  REQUIRE(pattern.Start(1) == 1);
  REQUIRE(pattern.Start(2) == 2);
  Input_controllerPorts[0].connected = 1;
  Input_controllerPorts[1].connected = 1;
  VS::Input::ResetLiveVibrationPatterns();
  for (const auto& slot : VS::Input::LiveVibrationState().slots) {
    CHECK(slot.pattern.Bytes().empty());
    CHECK_FALSE(slot.active);
    CHECK(slot.envelopes[0].level == 0);
    CHECK(slot.envelopes[1].level == 0);
  }
  CHECK(Input_controllerPorts[0].connected == 0);
  CHECK(Input_controllerPorts[1].connected == 0);
}

namespace {
SDL_JoystickID FindApprovedGamepad(const std::string& target) {
  int count = 0;
  const std::unique_ptr<SDL_JoystickID, decltype(&SDL_free)> devices(
      SDL_GetGamepads(&count), SDL_free);
  REQUIRE(devices != nullptr);
  std::vector<SDL_JoystickID> matches;
  for (int index = 0; index < count; ++index) {
    const auto id = devices.get()[index];
    const char* name = SDL_GetGamepadNameForID(id);
    if (name != nullptr && target == name) matches.push_back(id);
  }
  REQUIRE(matches.size() == 1);
  return matches.front();
}

void CheckRumbleCapability(SDL_JoystickID id) {
  SDL_Gamepad* gamepad = SDL_GetGamepadFromID(id);
  REQUIRE(gamepad != nullptr);
  REQUIRE(SDL_GetBooleanProperty(SDL_GetGamepadProperties(gamepad),
                                 SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false));
}

void RunLowHardwarePulse(VS::Input::InputSystem& input, ActuatorLease& lease) {
  lease.levels = {0, 64};
  SDL_ClearError();
  for (int tick = 0; tick < 20; ++tick) {
    SDL_PumpEvents();
    input.Update();
    REQUIRE(std::string(SDL_GetError()).empty());
    SDL_Delay(16);
  }
  lease.levels = {};
  input.Update();
  REQUIRE(std::string(SDL_GetError()).empty());
}

void RunApprovedHardwareCheck() {
  const char* requested = std::getenv("VS_NATIVE_RUMBLE_DEVICE");
  if (requested == nullptr || *requested == '\0') {
    SKIP("Set VS_NATIVE_RUMBLE_DEVICE to the exact approved gamepad name");
  }
  const std::string target(requested);
  GamepadSubsystem subsystem;
  REQUIRE(subsystem.ready);
  const SDL_JoystickID id = FindApprovedGamepad(target);
  VS::Input::InputSystem input;
  Connect(input, id);
  CheckRumbleCapability(id);
  ActuatorLease lease;
  RunLowHardwarePulse(input, lease);
}
}  // namespace

TEST_CASE("Manual selected gamepad receives a brief low rumble",
          "[.hardware-rumble]") {
  RunApprovedHardwareCheck();
}
