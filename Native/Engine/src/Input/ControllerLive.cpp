// Hand-maintained (tools/port/owned.csv).
// Live binding for the controller service (Controller.cpp): the legacy port,
// button, stick and settings globals, libpad, and the typed vibration slots.
#include "Input/ControllerLive.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>

#include "Input/Controller.hpp"
#include "Legacy/Input/LibPad.h"

extern "C" {
#include "Boot/GameLoop.h"
#include "Boot/Lifecycle.h"
#include "Core/Diagnostics.h"
#include "Input/ControllerLiveState.h"
#include "Input/Vibration.h"
#include "State/Timers.h"
}

// decomp: SLUS_010.40 _portInfo, vs_main_buttonsReleased,
// _buttonHeldFrameCount, vs_main_stickPosBuf, vs_main_buttonRepeat,
// vs_main_buttonsPreviousState, vs_main_buttonsPressed, vs_main_buttonsState
// and vs_main_settings (main.c), zero like the PS1 BSS.
extern "C" {
Input_ControllerPortState
    Input_controllerPorts[VS::Input::kControllerPortCount]{};
int Input_releasedButtons = 0;
u_int Input_buttonHeldFrames[VS::Input::kButtonCount]{};
Input_StickPosition Input_stickPosBuf{};
int Input_buttonRepeat = 0;
int Input_previousButtons = 0;
union Input_PressedButtonsView Input_buttonsPressed{};
int Input_buttonsState = 0;
State_Settings Input_settings{};
}

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
static_assert(sizeof(Input_ControllerPortState) == kLegacyPortBytes);
static_assert(offsetof(Input_ControllerPortState, controllerType) == 0);
static_assert(offsetof(Input_ControllerPortState, extendedType) == 1);
static_assert(offsetof(Input_ControllerPortState, buttonBits) == 2);
static_assert(offsetof(Input_ControllerPortState, rightStickX) ==
              kLegacyRightStickX);
static_assert(offsetof(Input_ControllerPortState, rightStickY) ==
              kLegacyRightStickY);
static_assert(offsetof(Input_ControllerPortState, leftStickX) ==
              kLegacyLeftStickX);
static_assert(offsetof(Input_ControllerPortState, leftStickY) ==
              kLegacyLeftStickY);
static_assert(offsetof(Input_ControllerPortState, modeLock) == kLegacyModeLock);
static_assert(offsetof(Input_ControllerPortState, actuatorLevels) ==
              kLegacyActuators);
static_assert(offsetof(Input_ControllerPortState, connected) ==
              kLegacyConnected);
static_assert(std::size(Input_ControllerPortState{}.actuatorLevels) ==
              kMotorCount);
static_assert(std::size(Input_controllerPorts) == kControllerPortCount);
static_assert(std::size(Input_buttonHeldFrames) == kButtonCount);
static_assert(sizeof(Input_legacyPadResponseBytes[0]) == kPadPacketBytes);

constexpr std::size_t kLegacyStickBytes = 0x10;
constexpr std::size_t kLegacyStickRightX = 8;
constexpr std::size_t kLegacyStickRightY = 0xC;
static_assert(sizeof(Input_StickPosition) == kLegacyStickBytes);
static_assert(offsetof(Input_StickPosition, lStickX) == 0);
static_assert(offsetof(Input_StickPosition, rStickX) == kLegacyStickRightX);
static_assert(offsetof(Input_StickPosition, rStickY) == kLegacyStickRightY);

constexpr std::size_t kSettingsMonoSound = 10;
constexpr std::size_t kSettingsVibrationOn = 11;
static_assert(offsetof(State_Settings, monoSound) == kSettingsMonoSound);
static_assert(offsetof(State_Settings, vibrationOn) == kSettingsVibrationOn);

// libpad port IDs: port index << 4.
constexpr int kPortIdShift = 4;
// main.c:139 D_8004A504: the preset directory and its one pattern.
constexpr std::size_t kVibrationPresetBytes = 24;
// Patterns are a few steps long; a stream not ended within this many bytes
// is refused rather than followed through memory.
constexpr std::size_t kMaxPatternBytes = 4096;

std::uint64_t refused_requests = 0;
VibrationState live_vibration;

// Reports the first occurrence and then every power of two.
bool Reportable(std::uint64_t count) noexcept {
  return (count & (count - 1)) == 0;
}

void Refused(const char* operation, const char* reason, std::int64_t value) {
  ++refused_requests;
  if (Reportable(refused_requests)) {
    Core_Warnf("input",
               "%s: %s (%" PRId64 "); nothing changed (refusal %" PRIu64 ")",
               operation, reason, value, refused_requests);
  }
}

std::uint8_t Byte(char value) noexcept {
  return static_cast<std::uint8_t>(value);
}

char LegacyByte(std::uint8_t value) noexcept {
  return static_cast<char>(value);
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
  const Input_ControllerPortState& legacy =
      std::span(Input_controllerPorts)[PortIndex(port)];
  return {
      .kind = static_cast<ControllerKind>(Byte(legacy.controllerType)),
      .extended_id = Byte(legacy.extendedType),
      .button_bits = Byte(legacy.buttonBits),
      .left_stick = {.x = Byte(legacy.rightStickX),
                     .y = Byte(legacy.rightStickY)},
      .right_stick = {.x = Byte(legacy.leftStickX),
                      .y = Byte(legacy.leftStickY)},
      .mode_lock = Byte(legacy.modeLock),
      .motor_commands = {Byte(legacy.actuatorLevels[0]),
                         Byte(legacy.actuatorLevels[1])},
      .connected = legacy.connected != 0,
  };
}

void StorePort(ControllerPort port, const ControllerPortState& state) noexcept {
  Input_ControllerPortState& legacy =
      std::span(Input_controllerPorts)[PortIndex(port)];
  legacy.controllerType = LegacyByte(static_cast<std::uint8_t>(state.kind));
  legacy.extendedType = LegacyByte(state.extended_id);
  legacy.buttonBits = LegacyByte(state.button_bits);
  legacy.rightStickX = LegacyByte(state.left_stick.x);
  legacy.rightStickY = LegacyByte(state.left_stick.y);
  legacy.leftStickX = LegacyByte(state.right_stick.x);
  legacy.leftStickY = LegacyByte(state.right_stick.y);
  legacy.modeLock = LegacyByte(state.mode_lock);
  legacy.actuatorLevels[0] = LegacyByte(state.motor_commands[0]);
  legacy.actuatorLevels[1] = LegacyByte(state.motor_commands[1]);
  legacy.connected = static_cast<char>(state.connected ? 1 : 0);
}

ControllerPorts LoadPorts() noexcept {
  return {LoadPort(ControllerPort::kFirst), LoadPort(ControllerPort::kSecond)};
}

void StorePorts(const ControllerPorts& ports) noexcept {
  StorePort(ControllerPort::kFirst, ports[0]);
  StorePort(ControllerPort::kSecond, ports[1]);
}

PadPacket ToPacket(LegacyPadPacket bytes) noexcept {
  PadPacket packet{};
  std::ranges::transform(bytes, packet.begin(), Byte);
  return packet;
}

// libpad over the legacy port records: the registered motor commands are
// the record's actuator bytes, which the shim reads every frame.
class LibPadHost final : public ControllerHost {
 public:
  std::int32_t QueryMode(ControllerPort port, PadInfoQuery query) override {
    return PadInfoMode(PortId(port), static_cast<int>(query), 0);
  }

  void SetMainMode(ControllerPort port, std::int32_t mode_offset,
                   std::uint8_t lock) override {
    static_cast<void>(PadSetMainMode(PortId(port), mode_offset, lock));
  }

  PadConnection ConnectionState(ControllerPort port) override {
    return static_cast<PadConnection>(PadGetState(PortId(port)));
  }

  void RegisterMotorCommands(ControllerPort port) override {
    char* const bytes =
        std::span(Input_controllerPorts)[PortIndex(port)].actuatorLevels;
    PadSetAct(PortId(port), reinterpret_cast<unsigned char*>(bytes),
              static_cast<int>(kMotorCount));
  }

  bool AlignMotors(ControllerPort port) override {
    return PadSetActAlign(PortId(port), reinterpret_cast<unsigned char*>(
                                            Input_legacyActuatorAlignment)) !=
           0;
  }

  PadPacket ReceivedPacket(ControllerPort port) override {
    return ToPacket(std::span(Input_legacyPadResponseBytes)[PortIndex(port)]);
  }
};

// The bytes a playing pattern reads: both (frames, level) streams through
// their terminating zero-frame step (main.c:8721-8745). Empty when a stream
// does not end within kMaxPatternBytes.
std::span<const std::byte> PatternExtent(const char* pattern) noexcept {
  const std::span window(reinterpret_cast<const std::uint8_t*>(pattern),
                         kMaxPatternBytes);
  const std::array<std::size_t, kMotorCount> starts{1, window[0]};
  std::size_t end = 0;
  for (std::size_t cursor : starts) {
    while (cursor + 1 < window.size() && window[cursor] != 0) {
      cursor += 2;
    }
    if (cursor + 1 >= window.size()) {
      return {};
    }
    end = std::max(end, cursor + 2);
  }
  return std::as_bytes(window.first(end));
}

ButtonState LoadButtons() noexcept {
  ButtonState buttons;
  buttons.previous = static_cast<std::uint32_t>(Input_previousButtons);
  std::ranges::copy(Input_buttonHeldFrames, buttons.held_ticks.begin());
  return buttons;
}

void StoreControllerState(const ControllerState& state) noexcept {
  StorePorts(state.ports);
  const ButtonState& buttons = state.buttons;
  Input_buttonsState = static_cast<int>(buttons.held);
  Input_buttonsPressed.allButtons = buttons.pressed;
  Input_releasedButtons = static_cast<int>(buttons.released);
  Input_previousButtons = static_cast<int>(buttons.previous);
  Input_buttonRepeat = static_cast<int>(buttons.repeat);
  std::ranges::copy(buttons.held_ticks, std::begin(Input_buttonHeldFrames));
  // The legacy r fields are the physical left stick (ControllerLiveState.h).
  Input_stickPosBuf.rStickX = state.sticks.left.x;
  Input_stickPosBuf.rStickY = state.sticks.left.y;
  Input_stickPosBuf.lStickX = state.sticks.right.x;
  Input_stickPosBuf.lStickY = state.sticks.right.y;
}

ControllerFrame PollLiveControllers() {
  ControllerState state{
      .ports = LoadPorts(), .buttons = LoadButtons(), .sticks = {}};
  ControllerSettings settings{
      .mono_sound = Input_settings.monoSound != 0,
      .vibration_on = Input_settings.vibrationOn != 0,
  };
  const ControllerFrameOptions options{
      .tick_speed = static_cast<std::uint32_t>(State_tickSpeed),
      .soft_reset_enabled = Boot_isSoftResetEnabled != 0,
  };
  LibPadHost host;
  const ControllerFrame frame =
      UpdateControllers(state, settings, host, options);
  StoreControllerState(state);
  if (frame == ControllerFrame::kSoftReset) {
    Input_settings.monoSound = static_cast<char>(settings.mono_sound ? 1 : 0);
    Input_settings.vibrationOn =
        static_cast<char>(settings.vibration_on ? 1 : 0);
  }
  return frame;
}
}  // namespace

VibrationState& LiveVibrationState() noexcept { return live_vibration; }

std::uint64_t RefusedLiveControllerRequestCount() noexcept {
  return refused_requests;
}

void InvalidateLiveControllerConnections() {
  ControllerPorts ports = LoadPorts();
  InvalidateControllerConnections(ports);
  StorePorts(ports);
}

void UnlockLiveAnalogModeSwitch() {
  ControllerPorts ports = LoadPorts();
  LibPadHost host;
  UnlockAnalogModeSwitch(ports, host);
  StorePorts(ports);
}

void ConfigureLiveControllerActuators(int port_id, LegacyPadPacket /*packet*/) {
  const std::optional<ControllerPort> port = PortFromId(port_id);
  if (!port) {
    Refused("ConfigureLiveControllerActuators", "unknown port ID", port_id);
    return;
  }
  ControllerPortState state = LoadPort(*port);
  LibPadHost host;
  ConfigureControllerActuators(state, *port, host);
  StorePort(*port, state);
}

int ReadLiveControllerPort(int port_id, LegacyPadPacket packet) {
  const std::optional<ControllerPort> port = PortFromId(port_id);
  if (!port) {
    Refused("ReadLiveControllerPort", "unknown port ID", port_id);
    return 0;
  }
  ControllerPortState state = LoadPort(*port);
  LibPadHost host;
  const std::uint32_t buttons =
      ReadControllerPort(state, *port, ToPacket(packet), host);
  StorePort(*port, state);
  return static_cast<int>(buttons);
}

void UpdateLiveControllerConnection(int port_id, LegacyPadPacket /*packet*/) {
  const std::optional<ControllerPort> port = PortFromId(port_id);
  if (!port) {
    Refused("UpdateLiveControllerConnection", "unknown port ID", port_id);
    return;
  }
  ControllerPortState state = LoadPort(*port);
  LibPadHost host;
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

int StartLiveVibrationPattern(const char* pattern, std::uint32_t request,
                              int owner) {
  if (pattern == nullptr) {
    Refused("StartLiveVibrationPattern", "null pattern for slot request",
            request);
    return 0;
  }
  const std::span<const std::byte> extent = PatternExtent(pattern);
  if (extent.empty()) {
    Refused("StartLiveVibrationPattern", "unterminated pattern, slot request",
            request);
    return 0;
  }
  return static_cast<int>(
      StartVibrationPattern(live_vibration, extent, request, owner));
}

int StopLiveVibrationPatterns(std::uint32_t selector, int owner) {
  return static_cast<int>(
      StopVibrationPatterns(live_vibration, selector, owner));
}

void ResetLiveVibrationPatterns() {
  ControllerPorts ports = LoadPorts();
  ResetVibrationPatterns(live_vibration, ports);
  StorePorts(ports);
}

void UpdateLiveVibrationPatterns() {
  ControllerPortState first = LoadPort(ControllerPort::kFirst);
  const std::uint32_t outside = UpdateVibrationPatterns(
      live_vibration, first, Input_settings.vibrationOn != 0);
  StorePort(ControllerPort::kFirst, first);
  if (outside != 0) {
    Refused("UpdateLiveVibrationPatterns", "pattern reads outside the pattern",
            outside);
  }
}

void PlayLiveVibrationPreset(int index) {
  const auto presets =
      std::as_bytes(std::span(Input_vibrationPresets, kVibrationPresetBytes));
  const auto started = PlayVibrationPreset(live_vibration, presets, index);
  if (!started && started.error() != VibrationPresetError::kNotListed) {
    Refused("PlayLiveVibrationPreset",
            VibrationPresetErrorName(started.error()), index);
  }
}

// main.c:8872-8876: a soft reset restarts at the title and does not return.
int UpdateLiveControllers() {
  if (PollLiveControllers() == ControllerFrame::kSoftReset) {
    Boot_ResetToTitle();
  }
  return 1;
}
}  // namespace VS::Input
