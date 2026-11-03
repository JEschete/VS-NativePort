#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Content/ModuleData.h"
#include "Input/Controller.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/LegacyPad.hpp"
#include "Input/Pad.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Boot/GameLoop.h"
#include "Input/ControllerLiveState.h"
#include "Input/Controllers.h"
#include "State/Timers.h"
}

namespace {
namespace Input = VS::Input;
using Input::ControllerKind;
using Input::ControllerPort;
using Input::Motor;
using Input::PadConnection;
using Input::PadInfoQuery;
using Calls = std::vector<std::string>;
using MotorCommands = std::array<std::uint8_t, Input::kMotorCount>;

std::size_t Index(ControllerPort port) {
  return static_cast<std::size_t>(port);
}

// Records every libpad request; each port answers from its fields.
class FakeHost final : public Input::ControllerHost {
 public:
  struct Port {
    ControllerKind kind = ControllerKind::kDigital;
    std::int32_t extended_id = 0;
    std::int32_t mode_offset = 0;
    PadConnection connection = PadConnection::kStable;
    bool aligns = true;
    Input::PadPacket packet{};
  };

  std::array<Port, Input::kControllerPortCount> ports{};
  Calls calls;

  std::int32_t QueryMode(ControllerPort port, PadInfoQuery query) override {
    Record("query", port, std::to_string(static_cast<std::int32_t>(query)));
    const Port& answers = ports.at(Index(port));
    switch (query) {
      case PadInfoQuery::kCurrentId:
        return static_cast<std::int32_t>(answers.kind);
      case PadInfoQuery::kCurrentExtendedId:
        return answers.extended_id;
      case PadInfoQuery::kCurrentModeOffset:
        return answers.mode_offset;
    }
    return 0;
  }

  void SetMainMode(ControllerPort port, std::int32_t mode_offset,
                   std::uint8_t lock) override {
    Record("main-mode", port,
           std::to_string(mode_offset) + " " + std::to_string(lock));
  }

  PadConnection ConnectionState(ControllerPort port) override {
    Record("state", port);
    return ports.at(Index(port)).connection;
  }

  void RegisterMotorCommands(ControllerPort port) override {
    Record("register", port);
  }

  bool AlignMotors(ControllerPort port) override {
    Record("align", port);
    return ports.at(Index(port)).aligns;
  }

  Input::PadPacket ReceivedPacket(ControllerPort port) override {
    Record("packet", port);
    return ports.at(Index(port)).packet;
  }

 private:
  void Record(const char* request, ControllerPort port,
              const std::string& detail = {}) {
    std::string call = std::string(request) + " " + std::to_string(Index(port));
    if (!detail.empty()) {
      call += " " + detail;
    }
    calls.push_back(call);
  }
};

// A valid libpad packet: active-low buttons, then the right stick (bytes
// 4-5) and the left stick (bytes 6-7).
Input::PadPacket Packet(std::uint32_t held, Input::AnalogStick left = {},
                        Input::AnalogStick right = {}) {
  const std::uint32_t inverted = ~held;
  Input::PadPacket packet{};
  packet[2] = static_cast<std::uint8_t>(inverted >> 8U);
  packet[3] = static_cast<std::uint8_t>(inverted);
  packet[4] = right.x;
  packet[5] = right.y;
  packet[6] = left.x;
  packet[7] = left.y;
  return packet;
}

bool ReportsButtons(ControllerKind kind) {
  constexpr std::array kSilent{ControllerKind::kMouse, ControllerKind::kNegcon,
                               ControllerKind::kKonamiGun,
                               ControllerKind::kNamcoGun};
  return std::ranges::find(kSilent, kind) == kSilent.end();
}

void CheckCentred(const Input::AnalogStick& stick) {
  CHECK(stick.x == Input::kStickCenter);
  CHECK(stick.y == Input::kStickCenter);
}

constexpr std::uint32_t kButtonL2 = 1U << 0U;
constexpr std::uint32_t kButtonR2 = 1U << 1U;
constexpr std::uint32_t kButtonL1 = 1U << 2U;
constexpr std::uint32_t kButtonR1 = 1U << 3U;
constexpr std::uint32_t kButtonCross = 1U << 6U;
constexpr std::uint32_t kButtonSelect = 1U << 8U;
constexpr std::uint32_t kButtonR3 = 1U << 10U;
constexpr std::uint32_t kButtonStart = 1U << 11U;
constexpr std::uint32_t kButtonUp = 1U << 12U;
constexpr std::uint32_t kButtonRight = 1U << 13U;
constexpr std::uint32_t kButtonDown = 1U << 14U;
constexpr std::uint32_t kButtonLeft = 1U << 15U;
constexpr std::uint32_t SoftResetButtons() {
  return kButtonStart | kButtonSelect | kButtonR1 | kButtonR2 | kButtonL1 |
         kButtonL2;
}
constexpr std::uint32_t kSoftReset = SoftResetButtons();

// Both motor streams (bytes 1 and 3) end at their first step.
constexpr std::array<std::uint8_t, 5> kStepPattern{3, 0, 0, 0, 0};

// One frame of UpdateControllers with only the first port connected.
struct Frames {
  FakeHost host;
  Input::ControllerState state;
  Input::ControllerSettings settings{.mono_sound = true, .vibration_on = false};
  Input::ControllerFrameOptions options;

  Frames() {
    host.ports[1].packet[0] = 0xFF;
    for (Input::ControllerPortState& port : state.ports) {
      port.connected = true;
    }
  }

  Input::ControllerFrame Run(std::uint32_t held, Input::AnalogStick left = {},
                             Input::AnalogStick right = {}) {
    host.ports[0].packet = Packet(held, left, right);
    return Input::UpdateControllers(state, settings, host, options);
  }
};

std::span<const std::byte> Bytes(std::span<const std::uint8_t> values) {
  return std::as_bytes(values);
}

// Starts the step pattern in slot 1 for owner 0x80 and slot 2 for 0x10.
void StartBothSlots(Input::VibrationState& vibration) {
  Input::StartVibrationPattern(vibration, Bytes(kStepPattern), 1, 0x80);
  Input::StartVibrationPattern(vibration, Bytes(kStepPattern), 2, 0x10);
}

// Saves and restores every legacy global the Live binding writes.
struct LiveControllerGlobals {
  std::array<Input_ControllerPortState, 2> ports =
      std::to_array(Input_controllerPorts);
  std::array<std::array<char, Input::kPadPacketBytes>, 2> packets{
      std::to_array(Input_legacyPadResponseBytes[0]),
      std::to_array(Input_legacyPadResponseBytes[1])};
  std::array<u_int, Input::kButtonCount> held_frames =
      std::to_array(Input_buttonHeldFrames);
  Input::VibrationState vibration = Input::LiveVibrationState();
  int released = Input_releasedButtons;
  Input_StickPosition sticks = Input_stickPosBuf;
  int repeat = Input_buttonRepeat;
  int previous = Input_previousButtons;
  union Input_PressedButtonsView pressed = Input_buttonsPressed;
  int held = Input_buttonsState;
  State_Settings settings = Input_settings;
  int tick_speed = State_tickSpeed;
  int soft_reset = Boot_isSoftResetEnabled;

  LiveControllerGlobals(const LiveControllerGlobals&) = delete;
  LiveControllerGlobals& operator=(const LiveControllerGlobals&) = delete;
  LiveControllerGlobals(LiveControllerGlobals&&) = delete;
  LiveControllerGlobals& operator=(LiveControllerGlobals&&) = delete;
  LiveControllerGlobals() = default;

  ~LiveControllerGlobals() {
    PadInitDirect(nullptr, nullptr);
    std::ranges::copy(ports, std::begin(Input_controllerPorts));
    std::ranges::copy(packets[0], std::begin(Input_legacyPadResponseBytes[0]));
    std::ranges::copy(packets[1], std::begin(Input_legacyPadResponseBytes[1]));
    std::ranges::copy(held_frames, std::begin(Input_buttonHeldFrames));
    Input::LiveVibrationState() = vibration;
    Input_releasedButtons = released;
    Input_stickPosBuf = sticks;
    Input_buttonRepeat = repeat;
    Input_previousButtons = previous;
    Input_buttonsPressed = pressed;
    Input_buttonsState = held;
    Input_settings = settings;
    State_tickSpeed = tick_speed;
    Boot_isSoftResetEnabled = soft_reset;
  }
};

// Runs live frames at normal speed from a cleared button history.
void PrepareLiveControllers() {
  State_tickSpeed = 1;
  Boot_isSoftResetEnabled = 0;
  Input_previousButtons = 0;
  std::ranges::fill(Input_buttonHeldFrames, 0U);
  PadInitDirect(
      reinterpret_cast<unsigned char*>(Input_legacyPadResponseBytes[0]),
      reinterpret_cast<unsigned char*>(Input_legacyPadResponseBytes[1]));
}

// Feeds an analog first pad with its left stick at (200, 100) and no second.
void FeedLivePad(std::uint32_t held) {
  VS::Input::PadState pad;
  pad.analog = true;
  pad.held = held;
  pad.sticks.lx = 200;
  pad.sticks.ly = 100;
  pad.sticks.rx = 128;
  pad.sticks.ry = 128;
  VS::Legacy::FeedPad(0, pad, true);
  VS::Legacy::FeedPad(1, {}, false);
}
}  // namespace

TEST_CASE("The battle stick dead zone keeps its original thresholds",
          "[input-controller][input]") {
  constexpr std::array<std::int32_t, 7> kAxes{0, 63, 64, 128, 191, 192, 255};
  constexpr std::array<std::int32_t, 7> kOffsets{-64, -1, 0, 0, 0, 0, 63};
  for (std::size_t index = 0; index < kAxes.size(); ++index) {
    CAPTURE(kAxes[index]);
    CHECK(Input::MapStickDeadZone(kAxes[index]) == kOffsets[index]);
  }
}

TEST_CASE("Invalidating controller connections keeps the rest of each port",
          "[input-controller][input]") {
  Input::ControllerPorts ports{};
  for (Input::ControllerPortState& port : ports) {
    port.connected = true;
    port.mode_lock = 2;
    port.motor_commands = {1, 2};
  }
  Input::InvalidateControllerConnections(ports);
  for (const Input::ControllerPortState& port : ports) {
    CHECK_FALSE(port.connected);
    CHECK(port.mode_lock == 2);
    CHECK(port.motor_commands == MotorCommands{1, 2});
  }
}

TEST_CASE("Unlocking the analog mode switch needs an extended first pad",
          "[input-controller][input]") {
  FakeHost host;
  Input::ControllerPorts ports{};
  ports[1].connected = true;
  Input::UnlockAnalogModeSwitch(ports, host);
  CHECK_FALSE(ports[1].connected);
  CHECK(host.calls == Calls{"query 0 2"});

  host.calls.clear();
  host.ports[0].extended_id = 7;
  Input::UnlockAnalogModeSwitch(ports, host);
  CHECK(host.calls == Calls{"query 0 2", "main-mode 0 1 2"});
}

TEST_CASE("Configuring actuators unlocks extended pads and clears motors",
          "[input-controller][input]") {
  FakeHost host;
  host.ports[1].extended_id = 7;
  host.ports[1].mode_offset = 1;
  Input::ControllerPortState state;
  state.motor_commands = {9, 9};
  Input::ConfigureControllerActuators(state, ControllerPort::kSecond, host);
  CHECK(state.extended_id == 7);
  CHECK(state.mode_lock == 2);
  CHECK(state.motor_commands == MotorCommands{0, 0});
  CHECK(host.calls == Calls{"query 1 2", "query 1 3", "main-mode 1 1 2",
                            "register 1", "align 1"});
}

TEST_CASE("Configuring actuators keeps the digital small motor byte",
          "[input-controller][input]") {
  FakeHost host;
  // main.c:8471 stores the extended ID in a byte, so 0x100 reads as none.
  host.ports[0].extended_id = GENERATE(0, 0x100);
  host.ports[0].aligns = false;
  Input::ControllerPortState state;
  state.mode_lock = 5;
  state.motor_commands = {9, 9};
  Input::ConfigureControllerActuators(state, ControllerPort::kFirst, host);
  CHECK(state.extended_id == 0);
  CHECK(state.mode_lock == 5);
  CHECK(state.motor_commands == MotorCommands{0x40, 0});
  CHECK(host.calls == Calls{"query 0 2", "register 0", "align 0"});
}

TEST_CASE("An invalid packet centres the sticks and reports no buttons",
          "[input-controller][input]") {
  FakeHost host;
  Input::ControllerPortState state;
  state.kind = ControllerKind::kAnalogPad;
  state.button_bits = 0x12;
  state.left_stick = {.x = 1, .y = 2};
  state.right_stick = {.x = 3, .y = 4};
  Input::PadPacket packet = Packet(kButtonCross);
  packet[0] = 0xFF;
  CHECK(Input::ReadControllerPort(state, ControllerPort::kSecond, packet,
                                  host) == 0);
  CheckCentred(state.left_stick);
  CheckCentred(state.right_stick);
  CHECK(state.kind == ControllerKind::kAnalogPad);
  CHECK(state.button_bits == 0x12);
  CHECK(host.calls.empty());
}

TEST_CASE("Reading a port reports active-low buttons for pad kinds",
          "[input-controller][input]") {
  const auto kind = static_cast<ControllerKind>(GENERATE(range(0, 8)));
  CAPTURE(static_cast<int>(kind));
  FakeHost host;
  host.ports[1].kind = kind;
  Input::ControllerPortState state;
  const std::uint32_t buttons = Input::ReadControllerPort(
      state, ControllerPort::kSecond, Packet(kButtonLeft | 0x41), host);
  // main.c:8510: the inverted word keeps its upper sixteen bits set.
  const std::uint32_t expected = ReportsButtons(kind) ? 0xFFFF8041U : 0U;
  CHECK(buttons == expected);
  CHECK(state.button_bits == (expected & 0xFFU));
  CHECK(state.kind == kind);
  CHECK(host.calls == Calls{"query 1 1", "query 1 1"});
}

TEST_CASE("Analog kinds report both sticks from their packet bytes",
          "[input-controller][input]") {
  const auto kind = static_cast<ControllerKind>(GENERATE(4, 5, 7));
  CAPTURE(static_cast<int>(kind));
  FakeHost host;
  host.ports[0].kind = kind;
  Input::ControllerPortState state;
  state.left_stick = {.x = 1, .y = 1};
  state.right_stick = {.x = 1, .y = 1};
  Input::ReadControllerPort(state, ControllerPort::kFirst,
                            Packet(0, {.x = 10, .y = 20}, {.x = 30, .y = 40}),
                            host);
  if (kind == ControllerKind::kDigital) {
    CheckCentred(state.left_stick);
    CheckCentred(state.right_stick);
    return;
  }
  CHECK(state.left_stick.x == 10);
  CHECK(state.left_stick.y == 20);
  CHECK(state.right_stick.x == 30);
  CHECK(state.right_stick.y == 40);
}

TEST_CASE("Controller connection follows the libpad state machine",
          "[input-controller][input]") {
  struct Case {
    bool connected;
    PadConnection connection;
    bool aligns;
    bool connected_after;
    Calls calls;
  };
  const std::array<Case, 6> cases{{
      {true, PadConnection::kStable, true, true, {"state 0"}},
      {true, PadConnection::kFindPad, true, false, {"state 0", "register 0"}},
      {false,
       PadConnection::kDisconnected,
       true,
       false,
       {"state 0", "register 0"}},
      {false,
       PadConnection::kFindCtp1,
       true,
       true,
       {"state 0", "register 0", "query 0 2", "register 0", "align 0"}},
      {false,
       PadConnection::kStable,
       false,
       false,
       {"state 0", "register 0", "align 0"}},
      {false,
       PadConnection::kStable,
       true,
       true,
       {"state 0", "register 0", "align 0", "query 0 2", "register 0",
        "align 0"}},
  }};
  for (std::size_t index = 0; index < cases.size(); ++index) {
    CAPTURE(index);
    const Case& example = cases.at(index);
    FakeHost host;
    host.ports[0].connection = example.connection;
    host.ports[0].aligns = example.aligns;
    Input::ControllerPortState state;
    state.connected = example.connected;
    Input::UpdateControllerConnection(state, ControllerPort::kFirst, host);
    CHECK(state.connected == example.connected_after);
    CHECK(host.calls == example.calls);
  }
}

TEST_CASE("Motor commands keep the digital small motor byte",
          "[input-controller][input]") {
  Input::ControllerPortState state;
  Input::SetMotorCommand(state, Motor::kSmall, 1);
  CHECK(Input::MotorCommand(state, Motor::kSmall) == 0x40);
  Input::SetMotorCommand(state, Motor::kLarge, 0xC0);
  CHECK(Input::MotorCommand(state, Motor::kLarge) == 0xC0);
  state.extended_id = 7;
  Input::SetMotorCommand(state, Motor::kSmall, 1);
  CHECK(Input::MotorCommand(state, Motor::kSmall) == 1);
  CHECK(state.motor_commands == MotorCommands{1, 0xC0});
}

TEST_CASE("Starting a pattern resets the first idle slot's envelopes",
          "[input-controller][input]") {
  const auto pattern = Bytes(kStepPattern);
  Input::VibrationState vibration;
  vibration.slots[0].envelopes[0] = {.active = true,
                                     .level = 5,
                                     .step = 1,
                                     .target = 6,
                                     .cursor = 9,
                                     .remaining = 4};
  CHECK(Input::StartVibrationPattern(vibration, pattern, 0xFF, 0x12345) == 1);
  const Input::VibrationSlot& first = vibration.slots[0];
  CHECK(first.active);
  CHECK(first.owner == 0x2345);
  CHECK(first.pattern.data() == pattern.data());
  CHECK(first.envelopes[0].cursor == 1);
  CHECK(first.envelopes[1].cursor == 3);
  CHECK(first.envelopes[0].remaining == 0);
  CHECK(first.envelopes[0].level == 0);
}

TEST_CASE("Starting a pattern takes a free slot or the requested one",
          "[input-controller][input]") {
  const auto pattern = Bytes(kStepPattern);
  Input::VibrationState vibration;
  CHECK(Input::StartVibrationPattern(vibration, pattern, 0xFF, 0) == 1);
  CHECK(Input::StartVibrationPattern(vibration, pattern, 0xFF, 0) == 2);
  CHECK(Input::StartVibrationPattern(vibration, pattern, 0xFF, 0) == 0);
  CHECK(Input::StartVibrationPattern(vibration, pattern, 2, 0x80) == 2);
  CHECK(vibration.slots[1].owner == 0x80);
  CHECK(Input::StartVibrationPattern(vibration, pattern, 0, 0) == 0);
  CHECK(Input::StartVibrationPattern(vibration, pattern, 3, 0) == 0);
  CHECK(Input::StartVibrationPattern(vibration, {}, 1, 0) == 0);
}

TEST_CASE("Stopping a vibration slot clears only that slot",
          "[input-controller][input]") {
  Input::VibrationState vibration;
  StartBothSlots(vibration);
  vibration.slots[0].envelopes[1].level = 7;
  CHECK(Input::StopVibrationPatterns(vibration, 1, 0x10) == 1);
  CHECK_FALSE(vibration.slots[0].active);
  CHECK(vibration.slots[0].pattern.empty());
  CHECK(vibration.slots[0].envelopes[1].level == 0);
  CHECK(vibration.slots[1].active);
  CHECK(Input::StopVibrationPatterns(vibration, 3, 0) == 0);
  CHECK(vibration.slots[1].active);
}

TEST_CASE("Stopping every pattern clears the owner's slots or all of them",
          "[input-controller][input]") {
  Input::VibrationState vibration;
  StartBothSlots(vibration);
  CHECK(Input::StopVibrationPatterns(vibration, 0, 0x10) == 1);
  CHECK(vibration.slots[0].active);
  CHECK_FALSE(vibration.slots[1].active);
  StartBothSlots(vibration);
  CHECK(Input::StopVibrationPatterns(vibration, 0, 0) == 1);
  CHECK_FALSE(vibration.slots[0].active);
  CHECK_FALSE(vibration.slots[1].active);
}

TEST_CASE("Clearing pausing and resuming refuse unknown vibration slots",
          "[input-controller][input]") {
  const std::uint32_t slot_id = GENERATE(0U, 3U, 0xFFU, 0xFFFFFFFFU);
  CAPTURE(slot_id);
  Input::VibrationState vibration;
  vibration.slots[0].active = true;
  const Input::VibrationState before = vibration;
  CHECK(Input::ClearVibrationSlot(vibration, slot_id) == 0);
  CHECK(Input::PauseVibrationSlot(vibration, slot_id) == 0);
  CHECK(Input::ResumeVibrationSlot(vibration, slot_id) == 0);
  CHECK(vibration.slots[0].active == before.slots[0].active);
  CHECK(vibration.slots[1].active == before.slots[1].active);
}

TEST_CASE("Pausing and resuming one slot keeps its pattern",
          "[input-controller][input]") {
  const auto pattern = Bytes(kStepPattern);
  Input::VibrationState vibration;
  Input::StartVibrationPattern(vibration, pattern, 2, 0);
  CHECK(Input::PauseVibrationSlot(vibration, 2) == 2);
  CHECK_FALSE(vibration.slots[1].active);
  CHECK(vibration.slots[1].pattern.data() == pattern.data());
  CHECK(Input::ResumeVibrationSlot(vibration, 2) == 2);
  CHECK(vibration.slots[1].active);
}

TEST_CASE("Pausing and resuming every pattern take a slot or zero",
          "[input-controller][input]") {
  Input::VibrationState vibration;
  StartBothSlots(vibration);
  CHECK(Input::PauseVibrationPatterns(vibration, 4) == 0);
  CHECK(Input::PauseVibrationPatterns(vibration, 0) == 1);
  CHECK_FALSE(vibration.slots[0].active);
  CHECK_FALSE(vibration.slots[1].active);
  CHECK(Input::ResumeVibrationPatterns(vibration, 1) == 1);
  CHECK(vibration.slots[0].active);
  CHECK_FALSE(vibration.slots[1].active);
  CHECK(Input::ResumeVibrationPatterns(vibration, 0) == 1);
  CHECK(vibration.slots[1].active);
}

TEST_CASE("Resetting vibration disconnects the ports and clears the slots",
          "[input-controller][input]") {
  Input::VibrationState vibration;
  Input::StartVibrationPattern(vibration, Bytes(kStepPattern), 1, 0);
  vibration.slots[0].envelopes[0].level = 3;
  vibration.slots[1].envelopes[1].level = 4;
  Input::ControllerPorts ports{};
  ports[0].connected = true;
  ports[1].connected = true;
  Input::ResetVibrationPatterns(vibration, ports);
  CHECK_FALSE(ports[0].connected);
  CHECK_FALSE(ports[1].connected);
  for (const Input::VibrationSlot& slot : vibration.slots) {
    CHECK_FALSE(slot.active);
    CHECK(slot.pattern.empty());
    CHECK(slot.envelopes[0].level == 0);
    CHECK(slot.envelopes[1].level == 0);
  }
}

TEST_CASE("Motor intensity rounds toward zero before its byte is taken",
          "[input-controller][input]") {
  CHECK(Input::RoundMotorIntensityTowardZero(0) == 0);
  CHECK(Input::RoundMotorIntensityTowardZero(0x1234) == 0x1234);
  CHECK(Input::RoundMotorIntensityTowardZero(-1) == 0xFE);
  CHECK(Input::RoundMotorIntensityTowardZero(-0x100) == -1);
  CHECK((Input::RoundMotorIntensityTowardZero(-0x1FF) >> 8) == -1);
}

TEST_CASE("Vibration envelopes step the small motor and ramp the large one",
          "[input-controller][input]") {
  // Small stream at byte 1: two frames at 1. Large stream at byte 5: four
  // frames from 2 towards 0, then two frames at 0.
  constexpr std::array<std::uint8_t, 11> kPattern{5, 2, 1, 0, 0, 4,
                                                  2, 2, 0, 0, 0};
  Input::VibrationState vibration;
  Input::StartVibrationPattern(vibration, Bytes(kPattern), 1, 0);
  Input::ControllerPortState port;
  port.extended_id = 7;
  constexpr std::array<std::array<std::int32_t, 2>, 7> kLevels{
      {{256, 384}, {256, 256}, {0, 128}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}};
  constexpr std::array<MotorCommands, 7> kCommands{
      {{1, 1}, {1, 1}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}};
  constexpr std::array<bool, 7> kPlaying{true, true, true, true,
                                         true, true, false};
  for (std::size_t tick = 0; tick < kLevels.size(); ++tick) {
    CAPTURE(tick);
    CHECK(Input::UpdateVibrationPatterns(vibration, port, true) == 0);
    CHECK(vibration.motor_levels == kLevels.at(tick));
    CHECK(port.motor_commands == kCommands.at(tick));
    CHECK(vibration.slots[0].active == kPlaying.at(tick));
  }
  CHECK(vibration.slots[0].pattern.empty());
}

TEST_CASE("The vibration mix clamps and leaves motors alone when disabled",
          "[input-controller][input]") {
  constexpr std::array<std::uint8_t, 7> kPattern{3, 0, 0, 9, 0xFF, 0, 0};
  Input::VibrationState vibration;
  Input::StartVibrationPattern(vibration, Bytes(kPattern), 1, 0);
  Input::StartVibrationPattern(vibration, Bytes(kPattern), 2, 0);
  Input::ControllerPortState port;
  port.extended_id = 7;
  port.motor_commands = {5, 6};
  CHECK(Input::UpdateVibrationPatterns(vibration, port, false) == 0);
  CHECK(vibration.motor_levels == std::array<std::int32_t, 2>{0, 0xFF00});
  CHECK(port.motor_commands == MotorCommands{5, 6});
  CHECK(Input::UpdateVibrationPatterns(vibration, port, true) == 0);
  CHECK(port.motor_commands == MotorCommands{0, 0xFF});
}

TEST_CASE("Pattern reads outside the pattern count and read as zero",
          "[input-controller][input]") {
  // The large stream starts past the end and the small one has no level.
  constexpr std::array<std::uint8_t, 2> kPattern{3, 4};
  Input::VibrationState vibration;
  Input::StartVibrationPattern(vibration, Bytes(kPattern), 1, 0);
  Input::ControllerPortState port;
  CHECK(Input::UpdateVibrationPatterns(vibration, port, true) == 4);
  const Input::VibrationSlot& slot = vibration.slots[0];
  CHECK(slot.envelopes[0].active);
  CHECK(slot.envelopes[0].remaining == 3);
  CHECK_FALSE(slot.envelopes[1].active);
  CHECK(vibration.motor_levels == std::array<std::int32_t, 2>{0, 0});
  // The ended stream reads its terminating step again on every tick.
  CHECK(Input::UpdateVibrationPatterns(vibration, port, true) == 2);
}

TEST_CASE("Paused and patternless slots do not drive the motors",
          "[input-controller][input]") {
  constexpr std::array<std::uint8_t, 5> kPattern{3, 0, 0, 2, 0x80};
  Input::VibrationState vibration;
  Input::StartVibrationPattern(vibration, Bytes(kPattern), 1, 0);
  Input::PauseVibrationSlot(vibration, 1);
  vibration.slots[1].active = true;
  Input::ControllerPortState port;
  Input::UpdateVibrationPatterns(vibration, port, true);
  CHECK(vibration.motor_levels == std::array<std::int32_t, 2>{0, 0});
  CHECK_FALSE(vibration.slots[1].active);
  CHECK(vibration.slots[0].envelopes[1].remaining == 0);
  CHECK(port.motor_commands == MotorCommands{0x40, 0});
}

TEST_CASE("Presets start in a free slot owned by the preset player",
          "[input-controller][input]") {
  // Two listed presets: the first at byte 12, the second past the end.
  constexpr std::array<std::uint8_t, 17> kPresets{2, 0, 0, 0, 12, 0, 0, 0, 200,
                                                  0, 0, 0, 3, 0,  0, 2, 0};
  const auto presets = Bytes(kPresets);
  Input::VibrationState vibration;
  const auto first = Input::PlayVibrationPreset(vibration, presets, 0);
  REQUIRE(first.has_value());
  CHECK(*first == 1);
  CHECK(vibration.slots[0].pattern.data() == presets.subspan(12).data());
  CHECK(vibration.slots[0].owner == Input::kPresetVibrationOwner);
  CHECK(Input::PlayVibrationPreset(vibration, presets, 0).value_or(0) == 2);
  CHECK(Input::PlayVibrationPreset(vibration, presets, 0).value_or(9) == 0);
  CHECK(Input::PlayVibrationPreset(vibration, presets, 1).error() ==
        Input::VibrationPresetError::kPatternOutsideData);
  CHECK(Input::PlayVibrationPreset(vibration, presets, 2).error() ==
        Input::VibrationPresetError::kNotListed);
  CHECK(Input::PlayVibrationPreset(vibration, presets, -1).error() ==
        Input::VibrationPresetError::kNegativeIndex);
}

TEST_CASE("Preset directories outside their data are refused",
          "[input-controller][input]") {
  constexpr std::array<std::uint8_t, 8> kPresets{5, 0, 0, 0, 0, 0, 0, 0};
  Input::VibrationState vibration;
  CHECK(Input::PlayVibrationPreset(vibration, Bytes(kPresets), 1).error() ==
        Input::VibrationPresetError::kDirectoryOutsideData);
  CHECK(Input::PlayVibrationPreset(vibration, {}, 0).error() ==
        Input::VibrationPresetError::kDirectoryOutsideData);
  CHECK_FALSE(vibration.slots[0].active);
  for (const auto error : {Input::VibrationPresetError::kNotListed,
                           Input::VibrationPresetError::kNegativeIndex,
                           Input::VibrationPresetError::kDirectoryOutsideData,
                           Input::VibrationPresetError::kPatternOutsideData}) {
    CHECK(std::string(Input::VibrationPresetErrorName(error)).size() > 0);
  }
}

TEST_CASE("Controller updates combine both ports into held and pressed buttons",
          "[input-controller][input]") {
  Frames frames;
  frames.host.ports[1].packet = Packet(1U);
  CHECK(frames.Run(kButtonCross) == Input::ControllerFrame::kContinue);
  const Input::ButtonState& buttons = frames.state.buttons;
  CHECK(buttons.held == (kButtonCross | 1U << 16U));
  CHECK(buttons.pressed == buttons.held);
  CHECK(buttons.repeat == buttons.held);
  CHECK(buttons.released == 0);
  frames.Run(kButtonCross);
  CHECK(buttons.pressed == 0);
  CHECK(buttons.held_ticks[6] == 2);
  CHECK(buttons.held_ticks[16] == 2);
  CHECK(frames.host.calls.front() == "packet 0");
}

TEST_CASE("Controller updates report buttons released since the last frame",
          "[input-controller][input]") {
  Frames frames;
  frames.host.ports[1].packet = Packet(1U);
  frames.Run(kButtonCross);
  frames.host.ports[1].packet[0] = 0xFF;
  frames.Run(0);
  const Input::ButtonState& buttons = frames.state.buttons;
  CHECK(buttons.released == (kButtonCross | 1U << 16U));
  CHECK(buttons.held == 0);
  CHECK(buttons.held_ticks[6] == 0);
  CHECK(buttons.held_ticks[16] == 0);
  CHECK(buttons.previous == 0);
}

TEST_CASE("Held buttons repeat every fourth tick after twenty ticks",
          "[input-controller][input]") {
  const std::uint32_t tick_speed = GENERATE(1U, 2U, 3U);
  CAPTURE(tick_speed);
  Frames frames;
  frames.options.tick_speed = tick_speed;
  std::vector<std::uint32_t> repeats;
  for (std::uint32_t frame = 1; frame <= 40 / tick_speed; ++frame) {
    frames.Run(kButtonCross);
    if ((frames.state.buttons.repeat & kButtonCross) != 0) {
      repeats.push_back(frame * tick_speed);
    }
  }
  const std::array<std::vector<std::uint32_t>, 3> expected{{
      {1, 20, 24, 28, 32, 36, 40},
      {2, 20, 24, 28, 32, 36, 40},
      {3, 24, 36},
  }};
  CHECK(repeats == expected.at(tick_speed - 1));
}

TEST_CASE("The first analog pad's sticks press directions and R3",
          "[input-controller][input]") {
  Frames frames;
  frames.host.ports[0].kind = ControllerKind::kAnalogPad;
  frames.Run(0, {.x = 16, .y = 240}, {.x = 32, .y = 224});
  CHECK(frames.state.buttons.held == 0);
  CHECK(frames.state.sticks.left.x == 16);
  CHECK(frames.state.sticks.right.y == 224);
  frames.Run(0, {.x = 15, .y = 241}, {.x = 32, .y = 225});
  CHECK(frames.state.buttons.held == (kButtonLeft | kButtonDown | kButtonR3));
  frames.Run(0, {.x = 241, .y = 15}, {.x = 31, .y = 128});
  CHECK(frames.state.buttons.held == (kButtonRight | kButtonUp | kButtonR3));

  frames.host.ports[0].kind = ControllerKind::kAnalogStick;
  frames.Run(0, {.x = 0, .y = 0}, {.x = 0, .y = 0});
  CHECK(frames.state.buttons.held == 0);
  CheckCentred(frames.state.sticks.left);
  CheckCentred(frames.state.sticks.right);
  CHECK(frames.state.ports[0].left_stick.x == 0);
}

TEST_CASE("A soft reset needs the whole combination completed this frame",
          "[input-controller][input]") {
  Frames frames;
  CHECK(frames.Run(kSoftReset) == Input::ControllerFrame::kContinue);
  frames.options.soft_reset_enabled = true;
  CHECK(frames.Run(kSoftReset) == Input::ControllerFrame::kContinue);
  CHECK(frames.Run(kSoftReset & ~kButtonStart) ==
        Input::ControllerFrame::kContinue);
  CHECK(frames.settings.mono_sound);
  CHECK(frames.Run(kSoftReset) == Input::ControllerFrame::kSoftReset);
  CHECK_FALSE(frames.settings.mono_sound);
  CHECK(frames.settings.vibration_on);
  CHECK(frames.state.buttons.held == kSoftReset);
  frames.settings = {.mono_sound = true, .vibration_on = false};
  CHECK(frames.Run(kSoftReset | kButtonCross) ==
        Input::ControllerFrame::kContinue);
}

TEST_CASE("Retired controller entries leave no module address bindings",
          "[input-controller][input]") {
  // No module image stores these addresses, so generated callers reach the
  // typed service only through OWNED_CALL_BINDINGS (ControllerLive.hpp).
  const unsigned address =
      GENERATE(0x80042FDCU, 0x80042FF0U, 0x80043034U, 0x800430F4U, 0x80043250U,
               0x80043388U, 0x800433B4U, 0x80043470U, 0x800434A4U, 0x80043554U,
               0x8004357CU, 0x800435DCU, 0x80043608U, 0x80043668U, 0x800436B4U,
               0x800438C8U, 0x80043940U, 0x80050118U, 0x80050190U, 0x80055C78U,
               0x80055C90U, 0x8005DFC0U, 0x8005DFDCU, 0x8005E1C0U, 0x8005E1D0U,
               0x8005E238U, 0x80060020U);
  CAPTURE(address);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
        nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800C4708U) == nullptr);
}

TEST_CASE("Live controller requests refuse unknown ports motors and patterns",
          "[input-controller][input]") {
  const LiveControllerGlobals saved;
  const std::uint64_t refusals = Input::RefusedLiveControllerRequestCount();
  Input::SetLiveMotorCommand(2, 0, 0x11);
  Input::SetLiveMotorCommand(0, 2, 0x11);
  Input::SetLiveMotorCommand(-1, 0, 0x11);
  CHECK(Input::ReadLiveControllerPort(0x20, Input_legacyPadResponseBytes[0]) ==
        0);
  CHECK(Input::StartLiveVibrationPattern(nullptr, 1, 0) == 0);
  // A stream with no zero-frame step within the bound is not followed.
  const std::vector<char> unterminated(4096, 1);
  CHECK(Input::StartLiveVibrationPattern(unterminated.data(), 1, 0) == 0);
  CHECK(Input::RefusedLiveControllerRequestCount() == refusals + 6);
  CHECK_FALSE(Input::LiveVibrationState().slots[0].active);

  Input::SetLiveMotorCommand(1, 1, 0x77);
  CHECK(static_cast<unsigned char>(
            Input_controllerPorts[1].actuatorLevels[1]) == 0x77);
  CHECK(Input::RefusedLiveControllerRequestCount() == refusals + 6);
}

TEST_CASE("Live controller updates publish the legacy button state",
          "[input-controller][input]") {
  const LiveControllerGlobals saved;
  PrepareLiveControllers();
  FeedLivePad(kButtonCross);
  CHECK(Input::UpdateLiveControllers() == 1);
  CHECK(Input_buttonsState == static_cast<int>(kButtonCross));
  CHECK(Input_buttonsPressed.allButtons == kButtonCross);
  CHECK(Input_buttonRepeat == static_cast<int>(kButtonCross));
  CHECK(Input_previousButtons == static_cast<int>(kButtonCross));
  CHECK(Input_buttonHeldFrames[6] == 1U);

  FeedLivePad(0);
  CHECK(Input::UpdateLiveControllers() == 1);
  CHECK(Input_releasedButtons == static_cast<int>(kButtonCross));
  CHECK(Input_buttonHeldFrames[6] == 0U);
}

TEST_CASE("Live controller updates publish the legacy stick and port state",
          "[input-controller][input]") {
  const LiveControllerGlobals saved;
  PrepareLiveControllers();
  FeedLivePad(0);
  CHECK(Input::UpdateLiveControllers() == 1);
  // The legacy r fields hold the physical left stick (main.c:8803).
  CHECK(Input_stickPosBuf.rStickX == 200);
  CHECK(Input_stickPosBuf.rStickY == 100);
  CHECK(Input_stickPosBuf.lStickX == 128);
  CHECK(Input_controllerPorts[0].controllerType == 7);
  CHECK(Input_controllerPorts[0].connected == 1);
  CHECK(static_cast<unsigned char>(Input_controllerPorts[0].rightStickX) ==
        200);
}
