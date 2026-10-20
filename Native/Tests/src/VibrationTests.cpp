#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Input/LegacyPad.hpp"
#include "Input/Pad.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Input/Controller.h"
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
  std::array<Input_VibrationPatternSlot, 2> saved_slots =
      std::to_array(Input_vibrationPatternSlots);
  std::array<Input_ControllerPortState, 2> saved_ports =
      std::to_array(Input_controllerPorts);
  std::array<unsigned char, 11> pattern{5, 2, 1, 0, 0, 2, 128, 2, 128, 0, 0};

  PatternState() {
    std::fill_n(Input_vibrationPatternSlots, 2, Input_VibrationPatternSlot{});
    Input_ConfigureControllerActuators(0, nullptr);
  }

  ~PatternState() {
    std::copy(saved_slots.begin(), saved_slots.end(),
              Input_vibrationPatternSlots);
    std::copy(saved_ports.begin(), saved_ports.end(), Input_controllerPorts);
  }

  int Start(unsigned int slot) {
    return Input_StartVibrationPattern(reinterpret_cast<char*>(pattern.data()),
                                       slot, 0x80);
  }
};
}  // namespace

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
  Input_UpdateVibrationPatterns();
  fixture.input.Update();
  CheckLast(fixture.commands, 32896, 65535);
}

TEST_CASE("Both original vibration slots have independent complete envelopes",
          "[input][vibration][pattern]") {
  REQUIRE(std::size(Input_vibrationPatternSlots[0].channels) == 12);
  RumbleFixture fixture;
  PatternState pattern;
  pattern.pattern[2] = 0;
  REQUIRE(pattern.Start(1) == 1);
  REQUIRE(pattern.Start(2) == 2);
  Input_UpdateVibrationPatterns();
  fixture.input.Update();
  CheckLast(fixture.commands, 65535, 0);
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
