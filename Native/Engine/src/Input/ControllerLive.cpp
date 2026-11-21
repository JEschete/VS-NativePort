// Hand-maintained (tools/port/owned.csv).
// Live binding for the controller service (Controller.cpp): the legacy port,
// button, stick and settings globals, the submitted pad samples and motor
// links that replace libpad, and the typed vibration slots.
#include "Input/ControllerLive.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>

#include "Boot/GameLoop.hpp"
#include "Boot/SessionLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Input/Controller.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Input/Vibration.hpp"
#include "State/Timers.hpp"

// decomp: SLUS_010.40 _portInfo, vs_main_buttonsReleased,
// _buttonHeldFrameCount, vs_main_stickPosBuf, vs_main_buttonRepeat,
// vs_main_buttonsPreviousState, vs_main_buttonsPressed, vs_main_buttonsState
// and vs_main_settings (main.c), zero like the PS1 BSS.
VS::Input::LegacyControllerPort
    VS::Input::controllerPorts[VS::Input::kControllerPortCount]{};
int VS::Input::releasedButtons = 0;
std::uint32_t VS::Input::buttonHeldFrames[VS::Input::kButtonCount]{};
VS::Input::StickPosition VS::Input::stickPosBuf{};
int VS::Input::buttonRepeat = 0;
int VS::Input::previousButtons = 0;
std::uint32_t VS::Input::buttonsPressed = 0;
int VS::Input::buttonsState = 0;
VS::Input::Settings VS::Input::settings{};

namespace VS::Input {
namespace {
constexpr std::size_t kLegacyPortBytes = 0xB;
constexpr std::size_t kLegacyRightStickX = 3;
constexpr std::size_t kLegacyRightStickY = 4;
constexpr std::size_t kLegacyLeftStickX = 5;
constexpr std::size_t kLegacyLeftStickY = 6;
constexpr std::size_t kLegacyModeLock = 7;
constexpr std::size_t kLegacyActuators = 8;
constexpr std::size_t kLegacyConnected = 0xA;
static_assert(sizeof(LegacyControllerPort) == kLegacyPortBytes);
static_assert(offsetof(LegacyControllerPort, controller_type) == 0);
static_assert(offsetof(LegacyControllerPort, extended_type) == 1);
static_assert(offsetof(LegacyControllerPort, button_bits) == 2);
static_assert(offsetof(LegacyControllerPort, right_stick_x) ==
              kLegacyRightStickX);
static_assert(offsetof(LegacyControllerPort, right_stick_y) ==
              kLegacyRightStickY);
static_assert(offsetof(LegacyControllerPort, left_stick_x) ==
              kLegacyLeftStickX);
static_assert(offsetof(LegacyControllerPort, left_stick_y) ==
              kLegacyLeftStickY);
static_assert(offsetof(LegacyControllerPort, mode_lock) == kLegacyModeLock);
static_assert(offsetof(LegacyControllerPort, actuator_levels) ==
              kLegacyActuators);
static_assert(offsetof(LegacyControllerPort, connected) == kLegacyConnected);
static_assert(
    std::tuple_size_v<decltype(LegacyControllerPort::actuator_levels)> ==
    kMotorCount);
static_assert(std::size(controllerPorts) == kControllerPortCount);
static_assert(std::size(buttonHeldFrames) == kButtonCount);

constexpr std::size_t kLegacyStickBytes = 0x10;
constexpr std::size_t kLegacyStickRightX = 8;
constexpr std::size_t kLegacyStickRightY = 0xC;
static_assert(sizeof(StickPosition) == kLegacyStickBytes);
static_assert(offsetof(StickPosition, l_stick_x) == 0);
static_assert(offsetof(StickPosition, r_stick_x) == kLegacyStickRightX);
static_assert(offsetof(StickPosition, r_stick_y) == kLegacyStickRightY);

constexpr std::size_t kSettingsMonoSound = 10;
constexpr std::size_t kSettingsVibrationOn = 11;
static_assert(offsetof(VS::Input::Settings, mono_sound) == kSettingsMonoSound);
static_assert(offsetof(VS::Input::Settings, vibration_on) ==
              kSettingsVibrationOn);

// libpad port IDs: port index << 4.
constexpr int kPortIdShift = 4;
// Patterns are a few steps long; a stream not ended within this many bytes
// is refused rather than followed through memory.
constexpr std::size_t kMaxPatternBytes = 4096;

std::uint64_t refusedRequests = 0;
VibrationState liveVibration;

// A port's link to its pad: the submitted report, and whether the game
// registered its motor commands (PadSetAct) and the pad accepted their
// alignment (PadSetActAlign).
struct PadLink {
  PadSample sample;
  bool motors_registered = false;
  bool motors_aligned = false;
};
std::array<PadLink, kControllerPortCount> padLinks;

// libpad PadInfoMode answers for a DualShock (psx-spx "Controllers"): its
// current ID is 7 in analog mode and 4 in digital mode, and its extended ID 7
// says it can switch modes and has motors.
constexpr std::int32_t kAnalogPadId = 7;
constexpr std::int32_t kDigitalPadId = 4;
constexpr std::int32_t kDualShockExtendedId = 7;
constexpr std::int32_t kAnalogModeOffset = 1;

// Reports the first occurrence and then every power of two.
bool Reportable(std::uint64_t count) noexcept {
  return (count & (count - 1)) == 0;
}

void Refused(const char* operation, const char* reason, std::int64_t value) {
  ++refusedRequests;
  if (Reportable(refusedRequests)) {
    VS::Core::Warnf(
        "input", "%s: %s (%" PRId64 "); nothing changed (refusal %" PRIu64 ")",
        operation, reason, value, refusedRequests);
  }
}

int PortId(ControllerPort port) noexcept {
  return static_cast<int>(port) << kPortIdShift;
}

std::size_t PortIndex(ControllerPort port) noexcept {
  return static_cast<std::size_t>(port);
}

std::optional<ControllerPort> PortFromId(int port_id) noexcept {
  for (const ControllerPort port :
       {ControllerPort::kFirst, ControllerPort::kSecond}) {
    if (PortId(port) == port_id) {
      return port;
    }
  }
  return std::nullopt;
}

ControllerPortState LoadPort(ControllerPort port) noexcept {
  const LegacyControllerPort& legacy =
      std::span(controllerPorts)[PortIndex(port)];
  return {
      .kind = static_cast<ControllerKind>(legacy.controller_type),
      .extended_id = legacy.extended_type,
      .button_bits = legacy.button_bits,
      .left_stick = {.x = legacy.right_stick_x, .y = legacy.right_stick_y},
      .right_stick = {.x = legacy.left_stick_x, .y = legacy.left_stick_y},
      .mode_lock = legacy.mode_lock,
      .motor_commands = legacy.actuator_levels,
      .connected = legacy.connected != 0,
  };
}

void StorePort(ControllerPort port, const ControllerPortState& state) noexcept {
  LegacyControllerPort& legacy = std::span(controllerPorts)[PortIndex(port)];
  legacy.controller_type = static_cast<std::uint8_t>(state.kind);
  legacy.extended_type = state.extended_id;
  legacy.button_bits = state.button_bits;
  legacy.right_stick_x = state.left_stick.x;
  legacy.right_stick_y = state.left_stick.y;
  legacy.left_stick_x = state.right_stick.x;
  legacy.left_stick_y = state.right_stick.y;
  legacy.mode_lock = state.mode_lock;
  legacy.actuator_levels = state.motor_commands;
  legacy.connected = state.connected ? 1 : 0;
}

ControllerPorts LoadPorts() noexcept {
  return {LoadPort(ControllerPort::kFirst), LoadPort(ControllerPort::kSecond)};
}

void StorePorts(const ControllerPorts& ports) noexcept {
  StorePort(ControllerPort::kFirst, ports[0]);
  StorePort(ControllerPort::kSecond, ports[1]);
}

PadLink& LinkFor(ControllerPort port) noexcept {
  return std::span(padLinks)[PortIndex(port)];
}

// The submitted pads and the motor links in place of libpad. The registered
// motor commands are the port record's actuator bytes, read when the host
// loop asks for the rumble (LiveRumble).
class LivePadHost final : public ControllerHost {
 public:
  std::int32_t QueryMode(ControllerPort port, PadInfoQuery query) override {
    const PadSample& sample = LinkFor(port).sample;
    if (!sample.connected) {
      return 0;
    }
    switch (query) {
      case PadInfoQuery::kCurrentId:
        return sample.analog ? kAnalogPadId : kDigitalPadId;
      case PadInfoQuery::kCurrentExtendedId:
        return kDualShockExtendedId;
      case PadInfoQuery::kCurrentModeOffset:
        return sample.analog ? kAnalogModeOffset : 0;
    }
    return 0;
  }

  // The submitted pad keeps the mode its device reports.
  void SetMainMode(ControllerPort /*port*/, std::int32_t /*mode_offset*/,
                   std::uint8_t /*lock*/) override {}

  PadConnection ConnectionState(ControllerPort port) override {
    return LinkFor(port).sample.connected ? PadConnection::kStable
                                          : PadConnection::kDisconnected;
  }

  void RegisterMotorCommands(ControllerPort port) override {
    LinkFor(port).motors_registered = true;
  }

  // _actParams (main.c) assigns command byte 0 to the small motor and byte 1
  // to the large one, the alignment a DualShock accepts.
  bool AlignMotors(ControllerPort port) override {
    LinkFor(port).motors_aligned = true;
    return true;
  }

  PadSample Sample(ControllerPort port) override {
    return LinkFor(port).sample;
  }
};

// The bytes a playing pattern reads: both (frames, level) streams through
// their terminating zero-frame step (main.c:8721-8745).
std::span<const std::byte> PatternExtent(
    std::span<const std::byte> window) noexcept {
  if (window.empty()) {
    return {};
  }
  const std::array<std::size_t, kMotorCount> starts{
      1, std::to_integer<std::size_t>(window.front())};
  std::size_t end = 0;
  for (std::size_t cursor : starts) {
    while (cursor < window.size() && window.size() - cursor >= 2 &&
           window[cursor] != std::byte{0}) {
      cursor += 2;
    }
    if (cursor >= window.size() || window.size() - cursor < 2) {
      return {};
    }
    end = std::max(end, cursor + 2);
  }
  return window.first(end);
}

std::span<const std::byte> ReadVibrationPattern(
    std::span<const std::byte> pattern, std::uint32_t request) {
  if (pattern.size() > std::numeric_limits<std::uint32_t>::max()) {
    Refused("StartLiveVibrationPattern", "pattern exceeds cursor range",
            request);
    return {};
  }
  const std::span<const std::byte> extent = PatternExtent(pattern);
  if (extent.empty()) {
    Refused("StartLiveVibrationPattern", "unterminated pattern, slot request",
            request);
  }
  return extent;
}

ButtonState LoadButtons() noexcept {
  ButtonState buttons;
  buttons.previous = static_cast<std::uint32_t>(previousButtons);
  std::ranges::copy(buttonHeldFrames, buttons.held_ticks.begin());
  return buttons;
}

void StoreControllerState(const ControllerState& state) noexcept {
  StorePorts(state.ports);
  const ButtonState& buttons = state.buttons;
  buttonsState = static_cast<int>(buttons.held);
  buttonsPressed = buttons.pressed;
  releasedButtons = static_cast<int>(buttons.released);
  previousButtons = static_cast<int>(buttons.previous);
  buttonRepeat = static_cast<int>(buttons.repeat);
  std::ranges::copy(buttons.held_ticks, std::begin(buttonHeldFrames));
  // The legacy r fields are the physical left stick (ControllerLiveState.hpp).
  stickPosBuf.r_stick_x = state.sticks.left.x;
  stickPosBuf.r_stick_y = state.sticks.left.y;
  stickPosBuf.l_stick_x = state.sticks.right.x;
  stickPosBuf.l_stick_y = state.sticks.right.y;
}

ControllerFrame PollLiveControllers() {
  ControllerState state{
      .ports = LoadPorts(), .buttons = LoadButtons(), .sticks = {}};
  ControllerSettings settings{
      .mono_sound = VS::Input::settings.mono_sound != 0,
      .vibration_on = VS::Input::settings.vibration_on != 0,
  };
  const ControllerFrameOptions options{
      .tick_speed = static_cast<std::uint32_t>(VS::State::tickSpeed),
      .soft_reset_enabled = VS::Boot::isSoftResetEnabled != 0,
  };
  LivePadHost host;
  const ControllerFrame frame =
      UpdateControllers(state, settings, host, options);
  StoreControllerState(state);
  if (frame == ControllerFrame::kSoftReset) {
    VS::Input::settings.mono_sound =
        static_cast<char>(settings.mono_sound ? 1 : 0);
    VS::Input::settings.vibration_on =
        static_cast<char>(settings.vibration_on ? 1 : 0);
  }
  return frame;
}
}  // namespace

void SubmitLivePad(ControllerPort port, const PadState& pad, bool connected) {
  LinkFor(port).sample = {
      .connected = connected,
      .analog = connected && pad.analog,
      .buttons = static_cast<std::uint16_t>(connected ? pad.held : 0U),
      .left_stick = {.x = pad.sticks.lx, .y = pad.sticks.ly},
      .right_stick = {.x = pad.sticks.rx, .y = pad.sticks.ry},
  };
}

void ResetLivePadLinks() { padLinks = {}; }

// The levels the libpad shim derived from the registered command bytes: the
// large motor's byte scaled to the full range, the small motor on or off
// (main.c:8698 vs_main_setVibrateParams writes them).
Rumble LiveRumble(ControllerPort port) {
  const PadLink& link = LinkFor(port);
  if (!link.sample.connected || !link.sample.analog ||
      !link.motors_registered || !link.motors_aligned ||
      VS::Input::settings.vibration_on == 0) {
    return {};
  }
  constexpr unsigned kByteToLevel = 257;
  constexpr std::uint8_t kSmallMotorOn = 1;
  const auto& commands =
      std::span(controllerPorts)[PortIndex(port)].actuator_levels;
  const std::uint8_t large = commands[static_cast<std::size_t>(Motor::kLarge)];
  const std::uint8_t small = commands[static_cast<std::size_t>(Motor::kSmall)];
  return {
      .low_frequency = static_cast<std::uint16_t>(large * kByteToLevel),
      .high_frequency = (small & kSmallMotorOn) != 0
                            ? std::numeric_limits<std::uint16_t>::max()
                            : std::uint16_t{0},
  };
}

VibrationState& LiveVibrationState() noexcept { return liveVibration; }

std::uint64_t RefusedLiveControllerRequestCount() noexcept {
  return refusedRequests;
}

void InvalidateLiveControllerConnections() {
  ControllerPorts ports = LoadPorts();
  InvalidateControllerConnections(ports);
  StorePorts(ports);
}

void UnlockLiveAnalogModeSwitch() {
  ControllerPorts ports = LoadPorts();
  LivePadHost host;
  UnlockAnalogModeSwitch(ports, host);
  StorePorts(ports);
}

void ConfigureLiveControllerActuators(int port_id) {
  const std::optional<ControllerPort> port = PortFromId(port_id);
  if (!port) {
    Refused("ConfigureLiveControllerActuators", "unknown port ID", port_id);
    return;
  }
  ControllerPortState state = LoadPort(*port);
  LivePadHost host;
  ConfigureControllerActuators(state, *port, host);
  StorePort(*port, state);
}

int ReadLiveControllerPort(int port_id) {
  const std::optional<ControllerPort> port = PortFromId(port_id);
  if (!port) {
    Refused("ReadLiveControllerPort", "unknown port ID", port_id);
    return 0;
  }
  ControllerPortState state = LoadPort(*port);
  LivePadHost host;
  const std::uint32_t buttons =
      ReadControllerPort(state, *port, host.Sample(*port), host);
  StorePort(*port, state);
  return static_cast<int>(buttons);
}

void UpdateLiveControllerConnection(int port_id) {
  const std::optional<ControllerPort> port = PortFromId(port_id);
  if (!port) {
    Refused("UpdateLiveControllerConnection", "unknown port ID", port_id);
    return;
  }
  ControllerPortState state = LoadPort(*port);
  LivePadHost host;
  UpdateControllerConnection(state, *port, host);
  StorePort(*port, state);
}

void SetLiveMotorCommand(int port_index, int motor, std::uint8_t command) {
  if (port_index < 0 ||
      static_cast<std::size_t>(port_index) >= kControllerPortCount) {
    Refused("SetLiveMotorCommand", "unknown port index", port_index);
    return;
  }
  if (motor < 0 || static_cast<std::size_t>(motor) >= kMotorCount) {
    Refused("SetLiveMotorCommand", "unknown motor", motor);
    return;
  }
  const auto port = static_cast<ControllerPort>(port_index);
  ControllerPortState state = LoadPort(port);
  SetMotorCommand(state, static_cast<Motor>(motor), command);
  StorePort(port, state);
}

int StartLiveVibrationPattern(std::span<const std::byte> pattern,
                              std::uint32_t request, int owner) {
  const std::span<const std::byte> extent =
      ReadVibrationPattern(pattern, request);
  if (extent.empty()) {
    return 0;
  }
  return static_cast<int>(
      StartVibrationPattern(liveVibration, extent, request, owner));
}

int StartLiveOwnedVibrationPattern(std::span<const std::byte> pattern,
                                   std::uint32_t request, int owner) {
  const std::span<const std::byte> extent =
      ReadVibrationPattern(pattern, request);
  if (extent.empty()) {
    return 0;
  }
  try {
    const auto storage = std::make_shared<const VibrationPattern::Storage>(
        extent.begin(), extent.end());
    return static_cast<int>(
        StartOwnedVibrationPattern(liveVibration, storage, request, owner));
  } catch (const std::bad_alloc&) {
    Refused("StartLiveOwnedVibrationPattern",
            "pattern allocation failed, slot request", request);
    return 0;
  }
}

int StartLiveVibrationPattern(const char* pattern, std::uint32_t request,
                              int owner) {
  if (pattern == nullptr) {
    Refused("StartLiveVibrationPattern", "null pattern for slot request",
            request);
    return 0;
  }
  return StartLiveVibrationPattern(
      {reinterpret_cast<const std::byte*>(pattern), kMaxPatternBytes}, request,
      owner);
}

int StopLiveVibrationPatterns(std::uint32_t selector, int owner) {
  return static_cast<int>(
      StopVibrationPatterns(liveVibration, selector, owner));
}

void ResetLiveVibrationPatterns() {
  ControllerPorts ports = LoadPorts();
  ResetVibrationPatterns(liveVibration, ports);
  StorePorts(ports);
}

void UpdateLiveVibrationPatterns() {
  ControllerPortState first = LoadPort(ControllerPort::kFirst);
  const std::uint32_t outside = UpdateVibrationPatterns(
      liveVibration, first, VS::Input::settings.vibration_on != 0);
  StorePort(ControllerPort::kFirst, first);
  if (outside != 0) {
    Refused("UpdateLiveVibrationPatterns", "pattern reads outside the pattern",
            outside);
  }
}

void PlayLiveVibrationPreset(int index) {
  const auto presets = std::as_bytes(std::span(kVibrationPresets));
  const auto started = PlayVibrationPreset(liveVibration, presets, index);
  if (!started && started.error() != VibrationPresetError::kNotListed) {
    Refused("PlayLiveVibrationPreset",
            VibrationPresetErrorName(started.error()), index);
  }
}

// main.c:8872-8876: a soft reset restarts at the title and does not return.
int UpdateLiveControllers() {
  if (PollLiveControllers() == ControllerFrame::kSoftReset) {
    VS::Boot::ResetToTitleLive();
  }
  return 1;
}
}  // namespace VS::Input
