// Hand-maintained (tools/port/owned.csv).
// The SLUS_010.40 controller service (main.c:8454-8870) and the battle stick
// dead zone (BATTLE 5BE50.c:82) over typed state (Controller.hpp).
#include "Input/Controller.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <utility>

namespace VS::Input {
namespace {
constexpr std::int32_t kDeadZoneLow = 64;
constexpr std::int32_t kDeadZoneHigh = 192;

constexpr std::size_t kPacketStatus = 0;
constexpr std::size_t kPacketButtonsHigh = 2;
constexpr std::size_t kPacketButtonsLow = 3;
constexpr std::size_t kPacketRightX = 4;
constexpr std::size_t kPacketRightY = 5;
constexpr std::size_t kPacketLeftX = 6;
constexpr std::size_t kPacketLeftY = 7;
constexpr unsigned kByteBits = std::numeric_limits<std::uint8_t>::digits;

// main.c:8464 and 8476: analog mode, switchable by the player.
constexpr std::int32_t kAnalogModeOffset = 1;
constexpr std::uint8_t kModeSwitchUnlocked = 2;
// main.c:8479: a pad without actuator support keeps this small-motor byte.
constexpr std::uint8_t kDigitalSmallMotorCommand = 0x40;

constexpr std::uint32_t kPortButtonMask = 0xFFFF;
constexpr unsigned kSecondPortShift = 16;

constexpr std::uint32_t kButtonL2 = 1U << 0U;
constexpr std::uint32_t kButtonR2 = 1U << 1U;
constexpr std::uint32_t kButtonL1 = 1U << 2U;
constexpr std::uint32_t kButtonR1 = 1U << 3U;
constexpr std::uint32_t kButtonSelect = 1U << 8U;
constexpr std::uint32_t kButtonR3 = 1U << 10U;
constexpr std::uint32_t kButtonStart = 1U << 11U;
constexpr std::uint32_t kButtonUp = 1U << 12U;
constexpr std::uint32_t kButtonRight = 1U << 13U;
constexpr std::uint32_t kButtonDown = 1U << 14U;
constexpr std::uint32_t kButtonLeft = 1U << 15U;
constexpr std::uint32_t SoftResetButtons() noexcept {
  return kButtonStart | kButtonSelect | kButtonR1 | kButtonR2 | kButtonL1 |
         kButtonL2;
}
constexpr std::uint32_t kSoftResetButtons = SoftResetButtons();

// main.c:8813-8839: the left stick drives the directions and a deflected
// right stick reports R3.
constexpr std::uint8_t kDirectionLow = 16;
constexpr std::uint8_t kDirectionHigh = 241;
constexpr std::uint8_t kDeflectionLow = 32;
constexpr std::uint8_t kDeflectionHigh = 225;

constexpr std::uint32_t kRepeatDelayTicks = 20;
constexpr std::uint32_t kRepeatIntervalMask = 3;

constexpr std::size_t kSmallMotorStreamStart = 1;
constexpr std::size_t kVibrationStepBytes = 2;
constexpr std::size_t kNextStepLevel = 3;
constexpr unsigned kLevelShift = 8;
constexpr std::int32_t kMaxMotorLevel = 0xFF00;
constexpr std::int32_t kTowardZeroBias = 0xFF;

constexpr std::size_t kPresetCountByte = 0;
constexpr std::size_t kPresetDirectoryStart = 4;
constexpr std::size_t kPresetEntryBytes = 4;

constexpr bool IsSlotId(std::uint32_t slot_id) noexcept {
  return slot_id >= 1 && slot_id <= kVibrationSlotCount;
}

VibrationSlot& SlotFor(VibrationState& vibration,
                       std::uint32_t slot_id) noexcept {
  return vibration.slots[slot_id - 1];
}

ControllerKind QueryKind(ControllerPort port, ControllerHost& host) {
  return static_cast<ControllerKind>(static_cast<std::uint8_t>(
      host.QueryMode(port, PadInfoQuery::kCurrentId)));
}

// main.c:8503-8509: these kinds report no pad buttons.
constexpr bool ReportsButtons(ControllerKind kind) noexcept {
  switch (kind) {
    case ControllerKind::kMouse:
    case ControllerKind::kNegcon:
    case ControllerKind::kKonamiGun:
    case ControllerKind::kNamcoGun:
      return false;
    default:
      return true;
  }
}

constexpr bool ReportsSticks(ControllerKind kind) noexcept {
  return kind == ControllerKind::kAnalogStick ||
         kind == ControllerKind::kAnalogPad;
}

constexpr bool Deflected(std::uint8_t axis) noexcept {
  return axis < kDeflectionLow || axis >= kDeflectionHigh;
}

// main.c:8800-8847 (port 1 only): stick positions and the buttons the
// sticks press.
std::uint32_t SampleSticks(StickPositions& sticks,
                           const ControllerPortState& first) noexcept {
  if (first.kind != ControllerKind::kAnalogPad) {
    sticks = {};
    return 0;
  }
  sticks.left = first.left_stick;
  sticks.right = first.right_stick;
  std::uint32_t buttons = 0;
  if (first.left_stick.x < kDirectionLow) {
    buttons |= kButtonLeft;
  }
  if (first.left_stick.x >= kDirectionHigh) {
    buttons |= kButtonRight;
  }
  if (first.left_stick.y < kDirectionLow) {
    buttons |= kButtonUp;
  }
  if (first.left_stick.y >= kDirectionHigh) {
    buttons |= kButtonDown;
  }
  if (Deflected(first.right_stick.x) || Deflected(first.right_stick.y)) {
    buttons |= kButtonR3;
  }
  return buttons;
}

// main.c:8849-8868.
void UpdateButtonEdges(ButtonState& buttons, std::uint32_t held,
                       std::uint32_t tick_speed) noexcept {
  buttons.held = held;
  buttons.pressed = ~buttons.previous & held;
  buttons.released = buttons.previous & ~held;
  buttons.previous = held;
  buttons.repeat = buttons.pressed;
  for (std::size_t bit = 0; bit < kButtonCount; ++bit) {
    std::uint32_t& ticks = buttons.held_ticks[bit];
    const std::uint32_t mask = 1U << bit;
    ticks = (held & mask) != 0 ? ticks + tick_speed : 0;
    if (ticks >= kRepeatDelayTicks &&
        ((ticks - kRepeatDelayTicks) & kRepeatIntervalMask) == 0) {
      buttons.repeat |= mask;
    }
  }
}

// Reads a pattern byte, counting reads outside the pattern as 0.
class PatternReader {
 public:
  explicit PatternReader(std::span<const std::byte> pattern) noexcept
      : pattern_(pattern) {}

  std::uint32_t At(std::size_t offset) noexcept {
    if (offset >= pattern_.size()) {
      ++outside_;
      return 0;
    }
    return std::to_integer<std::uint32_t>(pattern_[offset]);
  }

  std::int32_t LevelAt(std::size_t offset) noexcept {
    return static_cast<std::int32_t>(At(offset) << kLevelShift);
  }

  [[nodiscard]] std::uint32_t OutsideReads() const noexcept { return outside_; }

 private:
  std::span<const std::byte> pattern_;
  std::uint32_t outside_ = 0;
};

// main.c:8721-8745: loads the next (frames, level) step when the current one
// has run out, then moves one tick towards its level.
std::uint32_t AdvanceEnvelope(VibrationEnvelope& envelope,
                              std::span<const std::byte> pattern,
                              bool interpolate) noexcept {
  PatternReader reader(pattern);
  if (envelope.remaining == 0) {
    const std::size_t cursor = envelope.cursor;
    envelope.remaining = reader.At(cursor);
    envelope.level = reader.LevelAt(cursor + 1);
    envelope.target = envelope.level;
    if (envelope.remaining == 0) {
      envelope.level = 0;
      envelope.active = false;
    } else {
      envelope.target = reader.LevelAt(cursor + kNextStepLevel);
      envelope.step = interpolate
                          ? (envelope.target - envelope.level) /
                                static_cast<std::int32_t>(envelope.remaining)
                          : 0;
      envelope.active = true;
      envelope.cursor += kVibrationStepBytes;
    }
  }
  if (envelope.remaining != 0) {
    envelope.level += envelope.step;
    --envelope.remaining;
  } else {
    envelope.level = envelope.target;
  }
  return reader.OutsideReads();
}

std::uint8_t CommandForLevel(std::int32_t level) noexcept {
  return static_cast<std::uint8_t>(RoundMotorIntensityTowardZero(level) >>
                                   kLevelShift);
}

std::uint32_t SetSlotPlaying(VibrationState& vibration, std::uint32_t slot_id,
                             bool playing) noexcept {
  if (!IsSlotId(slot_id)) {
    return 0;
  }
  SlotFor(vibration, slot_id).active = playing;
  return slot_id;
}

std::uint32_t SetPatternsPlaying(VibrationState& vibration,
                                 std::uint32_t selector,
                                 bool playing) noexcept {
  if (IsSlotId(selector)) {
    return SetSlotPlaying(vibration, selector, playing);
  }
  if (selector != 0) {
    return 0;
  }
  for (VibrationSlot& slot : vibration.slots) {
    slot.active = playing;
  }
  return 1;
}

std::uint32_t StartPattern(VibrationState& vibration, VibrationPattern pattern,
                           std::uint32_t request, std::int32_t owner) noexcept {
  const std::span<const std::byte> bytes = pattern.Bytes();
  if (bytes.empty()) {
    return 0;
  }
  std::uint32_t slot_id = 0;
  if (request == kFreeVibrationSlot) {
    const auto idle =
        std::ranges::find(vibration.slots, false, &VibrationSlot::active);
    if (idle != vibration.slots.end()) {
      slot_id = static_cast<std::uint32_t>(idle - vibration.slots.begin()) + 1;
    }
  } else if (IsSlotId(request)) {
    slot_id = request;
  }
  if (slot_id == 0) {
    return 0;
  }
  VibrationSlot& slot = SlotFor(vibration, slot_id);
  slot.pattern = std::move(pattern);
  auto& [small_motor, large_motor] = slot.envelopes;
  small_motor.cursor = kSmallMotorStreamStart;
  large_motor.cursor = std::to_integer<std::uint32_t>(bytes.front());
  for (VibrationEnvelope& envelope : slot.envelopes) {
    envelope.remaining = 0;
    envelope.level = 0;
  }
  slot.active = true;
  slot.owner = static_cast<std::uint16_t>(owner);
  return slot_id;
}
}  // namespace

std::optional<VibrationPattern> VibrationPattern::RetainView(
    std::span<const std::byte> bytes) const noexcept {
  if (!owned_) {
    return std::nullopt;
  }
  const auto base = reinterpret_cast<std::uintptr_t>(owned_->data());
  const auto address = reinterpret_cast<std::uintptr_t>(bytes.data());
  if (address < base || address - base > owned_->size() ||
      bytes.size() > owned_->size() - (address - base)) {
    return std::nullopt;
  }
  VibrationPattern view(owned_);
  view.owned_view_ = bytes;
  return view;
}

const char* VibrationPresetErrorName(VibrationPresetError error) noexcept {
  switch (error) {
    case VibrationPresetError::kNotListed:
      return "not listed";
    case VibrationPresetError::kNegativeIndex:
      return "negative index";
    case VibrationPresetError::kDirectoryOutsideData:
      return "directory entry outside the preset data";
    case VibrationPresetError::kPatternOutsideData:
      return "pattern offset outside the preset data";
  }
  return "unknown";
}

// decomp: BATTLE.PRG vs_battle_mapStickDeadZone
// (src/BATTLE/BATTLE.PRG/5BE50.c:82)
std::int32_t MapStickDeadZone(std::int32_t axis_byte) noexcept {
  if (axis_byte < kDeadZoneLow) {
    return axis_byte - kDeadZoneLow;
  }
  if (axis_byte >= kDeadZoneHigh) {
    return axis_byte - kDeadZoneHigh;
  }
  return 0;
}

// decomp: SLUS_010.40 vs_main_padDisconnectAll
// (src/SLUS_010.40/main.c:8454)
void InvalidateControllerConnections(ControllerPorts& ports) noexcept {
  for (ControllerPortState& port : ports) {
    port.connected = false;
  }
}

// decomp: SLUS_010.40 _unlockPadModeSwitch (src/SLUS_010.40/main.c:8460)
void UnlockAnalogModeSwitch(ControllerPorts& ports, ControllerHost& host) {
  InvalidateControllerConnections(ports);
  if (host.QueryMode(ControllerPort::kFirst,
                     PadInfoQuery::kCurrentExtendedId) != 0) {
    host.SetMainMode(ControllerPort::kFirst, kAnalogModeOffset,
                     kModeSwitchUnlocked);
  }
}

// decomp: SLUS_010.40 _padResetDefaults (src/SLUS_010.40/main.c:8468)
void ConfigureControllerActuators(ControllerPortState& state,
                                  ControllerPort port, ControllerHost& host) {
  state.extended_id = static_cast<std::uint8_t>(
      host.QueryMode(port, PadInfoQuery::kCurrentExtendedId));
  if (state.extended_id != 0) {
    state.motor_commands = {};
    state.mode_lock = kModeSwitchUnlocked;
    const std::int32_t mode_offset =
        host.QueryMode(port, PadInfoQuery::kCurrentModeOffset);
    host.SetMainMode(port, mode_offset, state.mode_lock);
  } else {
    state.motor_commands = {kDigitalSmallMotorCommand, 0};
  }
  host.RegisterMotorCommands(port);
  static_cast<void>(host.AlignMotors(port));
}

// decomp: SLUS_010.40 vs_main_updatePadState (src/SLUS_010.40/main.c:8486)
std::uint32_t ReadControllerPort(ControllerPortState& state,
                                 ControllerPort port, const PadPacket& packet,
                                 ControllerHost& host) {
  if (packet[kPacketStatus] != 0) {
    state.left_stick = {};
    state.right_stick = {};
    return 0;
  }
  state.kind = QueryKind(port, host);
  std::uint32_t buttons = 0;
  if (ReportsButtons(state.kind)) {
    buttons = ~(
        (static_cast<std::uint32_t>(packet[kPacketButtonsHigh]) << kByteBits) |
        packet[kPacketButtonsLow]);
  }
  state.kind = QueryKind(port, host);
  if (ReportsSticks(state.kind)) {
    state.left_stick = {.x = packet[kPacketLeftX], .y = packet[kPacketLeftY]};
    state.right_stick = {.x = packet[kPacketRightX],
                         .y = packet[kPacketRightY]};
  } else {
    state.left_stick = {};
    state.right_stick = {};
  }
  state.button_bits = static_cast<std::uint8_t>(buttons);
  return buttons;
}

// decomp: SLUS_010.40 vs_main_padConnect (src/SLUS_010.40/main.c:8531)
void UpdateControllerConnection(ControllerPortState& state, ControllerPort port,
                                ControllerHost& host) {
  const PadConnection connection = host.ConnectionState(port);
  if (connection == PadConnection::kFindPad) {
    state.connected = false;
  }
  if (state.connected) {
    return;
  }
  host.RegisterMotorCommands(port);
  if (connection == PadConnection::kFindCtp1 ||
      (connection == PadConnection::kStable && host.AlignMotors(port))) {
    ConfigureControllerActuators(state, port, host);
    state.connected = true;
  }
}

// decomp: SLUS_010.40 _padSetActData (src/SLUS_010.40/main.c:8551)
void SetMotorCommand(ControllerPortState& state, Motor motor,
                     std::uint8_t command) noexcept {
  switch (motor) {
    case Motor::kSmall:
      state.motor_commands[0] =
          state.extended_id != 0 ? command : kDigitalSmallMotorCommand;
      return;
    case Motor::kLarge:
      state.motor_commands[1] = command;
      return;
  }
}

// decomp: SLUS_010.40 _padGetActData (src/SLUS_010.40/main.c:8562)
std::uint8_t MotorCommand(const ControllerPortState& state,
                          Motor motor) noexcept {
  switch (motor) {
    case Motor::kSmall:
      return state.motor_commands[0];
    case Motor::kLarge:
      return state.motor_commands[1];
  }
  return 0;
}

// decomp: SLUS_010.40 func_800433B4 (src/SLUS_010.40/main.c:8564)
std::uint32_t StartVibrationPattern(VibrationState& vibration,
                                    std::span<const std::byte> pattern,
                                    std::uint32_t request,
                                    std::int32_t owner) noexcept {
  for (const VibrationSlot& slot : vibration.slots) {
    if (auto retained = slot.pattern.RetainView(pattern)) {
      return StartPattern(vibration, std::move(*retained), request, owner);
    }
  }
  return StartPattern(vibration, VibrationPattern(pattern), request, owner);
}

std::uint32_t StartOwnedVibrationPattern(VibrationState& vibration,
                                         VibrationPattern::Owner pattern,
                                         std::uint32_t request,
                                         std::int32_t owner) noexcept {
  return StartPattern(vibration, VibrationPattern(std::move(pattern)), request,
                      owner);
}

// decomp: SLUS_010.40 func_80043470 (src/SLUS_010.40/main.c:8596)
std::uint32_t ClearVibrationSlot(VibrationState& vibration,
                                 std::uint32_t slot_id) noexcept {
  if (!IsSlotId(slot_id)) {
    return 0;
  }
  VibrationSlot& slot = SlotFor(vibration, slot_id);
  for (VibrationEnvelope& envelope : slot.envelopes) {
    envelope.level = 0;
  }
  slot.pattern = {};
  slot.active = false;
  return slot_id;
}

// decomp: SLUS_010.40 func_800434A4 (src/SLUS_010.40/main.c:8607)
std::uint32_t StopVibrationPatterns(VibrationState& vibration,
                                    std::uint32_t selector,
                                    std::int32_t owner) noexcept {
  if (IsSlotId(selector)) {
    return ClearVibrationSlot(vibration, selector);
  }
  if (selector != 0) {
    return 0;
  }
  for (std::uint32_t slot_id = 1; slot_id <= kVibrationSlotCount; ++slot_id) {
    const VibrationSlot& slot = SlotFor(vibration, slot_id);
    if (owner == 0 || static_cast<std::int32_t>(slot.owner) == owner) {
      ClearVibrationSlot(vibration, slot_id);
    }
  }
  return 1;
}

// decomp: SLUS_010.40 func_80043554 (src/SLUS_010.40/main.c:8630)
std::uint32_t PauseVibrationSlot(VibrationState& vibration,
                                 std::uint32_t slot_id) noexcept {
  return SetSlotPlaying(vibration, slot_id, false);
}

// decomp: SLUS_010.40 func_8004357C (src/SLUS_010.40/main.c:8637)
std::uint32_t PauseVibrationPatterns(VibrationState& vibration,
                                     std::uint32_t selector) noexcept {
  return SetPatternsPlaying(vibration, selector, false);
}

// decomp: SLUS_010.40 func_800435DC (src/SLUS_010.40/main.c:8656)
std::uint32_t ResumeVibrationSlot(VibrationState& vibration,
                                  std::uint32_t slot_id) noexcept {
  return SetSlotPlaying(vibration, slot_id, true);
}

// decomp: SLUS_010.40 func_80043608 (src/SLUS_010.40/main.c:8663)
std::uint32_t ResumeVibrationPatterns(VibrationState& vibration,
                                      std::uint32_t selector) noexcept {
  return SetPatternsPlaying(vibration, selector, true);
}

// decomp: SLUS_010.40 vs_main_resetPadAct (src/SLUS_010.40/main.c:8682)
void ResetVibrationPatterns(VibrationState& vibration,
                            ControllerPorts& ports) noexcept {
  InvalidateControllerConnections(ports);
  for (VibrationSlot& slot : vibration.slots) {
    slot.pattern = {};
    for (VibrationEnvelope& envelope : slot.envelopes) {
      envelope.level = 0;
    }
    slot.active = false;
  }
}

// decomp: SLUS_010.40 _abs2 (src/SLUS_010.40/main.c:8696)
std::int32_t RoundMotorIntensityTowardZero(std::int32_t intensity) noexcept {
  return intensity < 0 ? intensity + kTowardZeroBias : intensity;
}

// decomp: SLUS_010.40 vs_main_setVibrateParams (src/SLUS_010.40/main.c:8698)
std::uint32_t UpdateVibrationPatterns(VibrationState& vibration,
                                      ControllerPortState& first_port,
                                      bool vibration_on) noexcept {
  vibration.motor_levels = {};
  std::uint32_t outside = 0;
  for (VibrationSlot& slot : vibration.slots) {
    if (!slot.active) {
      continue;
    }
    const std::span<const std::byte> pattern = slot.pattern.Bytes();
    if (pattern.empty()) {
      slot.active = false;
      continue;
    }
    for (std::size_t motor = 0; motor < kMotorCount; ++motor) {
      VibrationEnvelope& envelope = slot.envelopes[motor];
      outside += AdvanceEnvelope(envelope, pattern, motor != 0);
      std::int32_t& level = vibration.motor_levels[motor];
      level = std::clamp(level + envelope.level, 0, kMaxMotorLevel);
    }
    slot.active = slot.envelopes[0].active || slot.envelopes[1].active;
    if (!slot.active) {
      slot.pattern = {};
    }
  }
  if (vibration_on) {
    SetMotorCommand(first_port, Motor::kSmall,
                    CommandForLevel(vibration.motor_levels[0]));
    SetMotorCommand(first_port, Motor::kLarge,
                    CommandForLevel(vibration.motor_levels[1]));
  }
  return outside;
}

// decomp: SLUS_010.40 func_800438C8 (src/SLUS_010.40/main.c:8769)
std::expected<std::uint32_t, VibrationPresetError> PlayVibrationPreset(
    VibrationState& vibration, std::span<const std::byte> presets,
    std::int32_t index) noexcept {
  if (presets.empty()) {
    return std::unexpected(VibrationPresetError::kDirectoryOutsideData);
  }
  if (index >= std::to_integer<std::int32_t>(presets[kPresetCountByte])) {
    return std::unexpected(VibrationPresetError::kNotListed);
  }
  if (index < 0) {
    return std::unexpected(VibrationPresetError::kNegativeIndex);
  }
  const std::size_t entry =
      kPresetDirectoryStart +
      (static_cast<std::size_t>(index) * kPresetEntryBytes);
  if (entry + kPresetEntryBytes > presets.size()) {
    return std::unexpected(VibrationPresetError::kDirectoryOutsideData);
  }
  std::uint32_t offset = 0;
  for (std::size_t byte = 0; byte < kPresetEntryBytes; ++byte) {
    offset |= std::to_integer<std::uint32_t>(presets[entry + byte])
              << (byte * kByteBits);
  }
  if (offset >= presets.size()) {
    return std::unexpected(VibrationPresetError::kPatternOutsideData);
  }
  return StartVibrationPattern(vibration, presets.subspan(offset),
                               kFreeVibrationSlot, kPresetVibrationOwner);
}

// decomp: SLUS_010.40 vs_main_processPadState (src/SLUS_010.40/main.c:8789)
ControllerFrame UpdateControllers(ControllerState& state,
                                  ControllerSettings& settings,
                                  ControllerHost& host,
                                  const ControllerFrameOptions& options) {
  auto& [first, second] = state.ports;
  const std::uint32_t first_buttons =
      ReadControllerPort(first, ControllerPort::kFirst,
                         host.ReceivedPacket(ControllerPort::kFirst), host) &
      kPortButtonMask;
  const std::uint32_t second_buttons =
      ReadControllerPort(second, ControllerPort::kSecond,
                         host.ReceivedPacket(ControllerPort::kSecond), host)
      << kSecondPortShift;
  UpdateControllerConnection(first, ControllerPort::kFirst, host);
  UpdateControllerConnection(second, ControllerPort::kSecond, host);
  const std::uint32_t held =
      first_buttons | second_buttons | SampleSticks(state.sticks, first);
  UpdateButtonEdges(state.buttons, held, options.tick_speed);
  if (options.soft_reset_enabled &&
      (state.buttons.previous & kSoftResetButtons) == kSoftResetButtons &&
      (state.buttons.pressed & kSoftResetButtons) != 0) {
    settings.mono_sound = false;
    settings.vibration_on = true;
    return ControllerFrame::kSoftReset;
  }
  return ControllerFrame::kContinue;
}
}  // namespace VS::Input
