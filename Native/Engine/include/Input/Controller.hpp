#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// The SLUS_010.40 controller service (main.c:8454-8870) and the battle
// stick dead zone (BATTLE 5BE50.c:82): pad packets, connection and analog
// mode, button edges and repeat, and the two-slot vibration pattern player.
// The pad hardware is reached only through ControllerHost.
namespace VS::Input {

enum class ControllerPort : std::uint8_t { kFirst, kSecond };
inline constexpr std::size_t kControllerPortCount = 2;

// libpad PadInfoMode terms.
enum class PadInfoQuery : std::int32_t {
  kCurrentId = 1,
  kCurrentExtendedId = 2,
  kCurrentModeOffset = 3,
};

// libpad PadGetState results the service distinguishes.
enum class PadConnection : std::int32_t {
  kDisconnected = 0,
  kFindPad = 1,
  kFindCtp1 = 2,
  kStable = 6,
};

// Actuator 0 is the on/off motor, actuator 1 the variable one.
enum class Motor : std::uint8_t { kSmall, kLarge };
inline constexpr std::size_t kMotorCount = 2;

// The low byte of the libpad controller ID.
enum class ControllerKind : std::uint8_t {
  kNone = 0,
  kMouse = 1,
  kNegcon = 2,
  kKonamiGun = 3,
  kDigital = 4,
  kAnalogStick = 5,
  kNamcoGun = 6,
  kAnalogPad = 7,
};

inline constexpr std::uint8_t kStickCenter = 128;

struct AnalogStick {
  std::uint8_t x = kStickCenter;
  std::uint8_t y = kStickCenter;
};

// One controller port (main.h:19 portInfo_t). `left_stick` is the physical
// left stick (packet bytes 6-7), `right_stick` the right one (bytes 4-5).
struct ControllerPortState {
  ControllerKind kind = ControllerKind::kNone;
  std::uint8_t extended_id = 0;
  std::uint8_t button_bits = 0;
  AnalogStick left_stick;
  AnalogStick right_stick;
  std::uint8_t mode_lock = 0;
  std::array<std::uint8_t, kMotorCount> motor_commands{};
  bool connected = false;
};

using ControllerPorts = std::array<ControllerPortState, kControllerPortCount>;

// A libpad receive buffer: [0] 0 = data valid, [2..3] active-low buttons,
// [4..7] right X, right Y, left X, left Y.
inline constexpr std::size_t kPadPacketBytes = 34;
using PadPacket = std::array<std::uint8_t, kPadPacketBytes>;

// One held-tick counter per bit of the button word.
inline constexpr std::size_t kButtonCount = 32;
static_assert(kButtonCount == std::numeric_limits<std::uint32_t>::digits);

// Player 1 in bits 0-15 and player 2 in bits 16-31 (libetc PADxxx order).
struct ButtonState {
  std::uint32_t held = 0;
  std::uint32_t previous = 0;
  std::uint32_t pressed = 0;
  std::uint32_t released = 0;
  // Pressed, plus every fourth tick once a button is held for 20 ticks.
  std::uint32_t repeat = 0;
  std::array<std::uint32_t, kButtonCount> held_ticks{};
};

struct StickPositions {
  AnalogStick left;
  AnalogStick right;
};

struct ControllerState {
  ControllerPorts ports{};
  ButtonState buttons;
  StickPositions sticks;
};

struct ControllerSettings {
  bool mono_sound = false;
  bool vibration_on = true;
};

struct ControllerFrameOptions {
  std::uint32_t tick_speed = 1;
  bool soft_reset_enabled = false;
};

enum class ControllerFrame : std::uint8_t { kContinue, kSoftReset };

// The pad hardware: libpad on the PS1, LibPad.cpp in the live game.
class ControllerHost {
 public:
  ControllerHost() = default;
  ControllerHost(const ControllerHost&) = delete;
  ControllerHost& operator=(const ControllerHost&) = delete;
  ControllerHost(ControllerHost&&) = delete;
  ControllerHost& operator=(ControllerHost&&) = delete;
  virtual ~ControllerHost() = default;

  virtual std::int32_t QueryMode(ControllerPort port, PadInfoQuery query) = 0;
  virtual void SetMainMode(ControllerPort port, std::int32_t mode_offset,
                           std::uint8_t lock) = 0;
  virtual PadConnection ConnectionState(ControllerPort port) = 0;
  // The host drives the port's motors from its motor commands from now on.
  virtual void RegisterMotorCommands(ControllerPort port) = 0;
  // Whether the pad accepted the actuator alignment.
  virtual bool AlignMotors(ControllerPort port) = 0;
  virtual PadPacket ReceivedPacket(ControllerPort port) = 0;
};

// One vibration bytecode stream: (frames, level) pairs ending at frames 0.
struct VibrationEnvelope {
  bool active = false;
  std::int32_t level = 0;
  std::int32_t step = 0;
  std::int32_t target = 0;
  std::uint32_t cursor = 0;
  std::uint32_t remaining = 0;
};

// Owned copies follow slot snapshots; borrowed patterns retain the original
// caller-managed lifetime contract.
class VibrationPattern {
 public:
  using Storage = std::vector<std::byte>;
  using Owner = std::shared_ptr<const Storage>;

  VibrationPattern() = default;
  explicit VibrationPattern(std::span<const std::byte> bytes) noexcept
      : borrowed_(bytes) {}
  explicit VibrationPattern(Owner owner) noexcept : owned_(std::move(owner)) {
    if (owned_) {
      owned_view_ = *owned_;
    }
  }

  [[nodiscard]] std::span<const std::byte> Bytes() const noexcept {
    return owned_ ? owned_view_ : borrowed_;
  }
  [[nodiscard]] std::optional<VibrationPattern> RetainView(
      std::span<const std::byte> bytes) const noexcept;

 private:
  std::span<const std::byte> borrowed_{};
  Owner owned_;
  std::span<const std::byte> owned_view_{};
};

// Byte 0 is the offset of the large motor's stream; the small motor's stream
// starts at byte 1.
struct VibrationSlot {
  VibrationPattern pattern;
  bool active = false;
  std::uint16_t owner = 0;
  std::array<VibrationEnvelope, kMotorCount> envelopes{};
};

inline constexpr std::size_t kVibrationSlotCount = 2;

// main.c:19-25 padAct_t[2]: the motor mix and two pattern slots.
struct VibrationState {
  std::array<VibrationSlot, kVibrationSlotCount> slots{};
  std::array<std::int32_t, kMotorCount> motor_levels{};
};

// Slot IDs are one-based; 0 means none.
inline constexpr std::uint32_t kFreeVibrationSlot = 0xFF;
inline constexpr std::int32_t kPresetVibrationOwner = 0x80;

enum class VibrationPresetError : std::uint8_t {
  kNotListed,
  kNegativeIndex,
  kDirectoryOutsideData,
  kPatternOutsideData,
};

[[nodiscard]] const char* VibrationPresetErrorName(
    VibrationPresetError error) noexcept;

[[nodiscard]] std::int32_t MapStickDeadZone(std::int32_t axis_byte) noexcept;

void InvalidateControllerConnections(ControllerPorts& ports) noexcept;
void UnlockAnalogModeSwitch(ControllerPorts& ports, ControllerHost& host);
void ConfigureControllerActuators(ControllerPortState& state,
                                  ControllerPort port, ControllerHost& host);
std::uint32_t ReadControllerPort(ControllerPortState& state,
                                 ControllerPort port, const PadPacket& packet,
                                 ControllerHost& host);
void UpdateControllerConnection(ControllerPortState& state, ControllerPort port,
                                ControllerHost& host);
void SetMotorCommand(ControllerPortState& state, Motor motor,
                     std::uint8_t command) noexcept;
[[nodiscard]] std::uint8_t MotorCommand(const ControllerPortState& state,
                                        Motor motor) noexcept;

// Views into slot-owned storage retain that owner; other inputs must remain
// readable until the slot is cleared or replaced, including while paused.
std::uint32_t StartVibrationPattern(VibrationState& vibration,
                                    std::span<const std::byte> pattern,
                                    std::uint32_t request,
                                    std::int32_t owner) noexcept;
std::uint32_t StartOwnedVibrationPattern(VibrationState& vibration,
                                         VibrationPattern::Owner pattern,
                                         std::uint32_t request,
                                         std::int32_t owner) noexcept;
// Slot functions refuse IDs other than 1 and 2 and return 0 for them.
std::uint32_t ClearVibrationSlot(VibrationState& vibration,
                                 std::uint32_t slot_id) noexcept;
// A slot ID, or 0 for every slot (only those of `owner` when it is nonzero).
std::uint32_t StopVibrationPatterns(VibrationState& vibration,
                                    std::uint32_t selector,
                                    std::int32_t owner) noexcept;
std::uint32_t PauseVibrationSlot(VibrationState& vibration,
                                 std::uint32_t slot_id) noexcept;
std::uint32_t PauseVibrationPatterns(VibrationState& vibration,
                                     std::uint32_t selector) noexcept;
std::uint32_t ResumeVibrationSlot(VibrationState& vibration,
                                  std::uint32_t slot_id) noexcept;
std::uint32_t ResumeVibrationPatterns(VibrationState& vibration,
                                      std::uint32_t selector) noexcept;
void ResetVibrationPatterns(VibrationState& vibration,
                            ControllerPorts& ports) noexcept;
[[nodiscard]] std::int32_t RoundMotorIntensityTowardZero(
    std::int32_t intensity) noexcept;
// Advances every playing slot one tick and, when vibration is on, writes the
// mix to the first port's motor commands. Returns how many pattern reads fell
// outside the pattern; each read as 0, which ends the stream.
std::uint32_t UpdateVibrationPatterns(VibrationState& vibration,
                                      ControllerPortState& first_port,
                                      bool vibration_on) noexcept;
// `presets`: a count byte and three spare bytes, then one little-endian
// pattern offset per preset. Starts the preset in a free slot.
std::expected<std::uint32_t, VibrationPresetError> PlayVibrationPreset(
    VibrationState& vibration, std::span<const std::byte> presets,
    std::int32_t index) noexcept;

// Polls both ports and derives the button edges, repeat and stick positions.
// A soft reset (Start+Select+L1+L2+R1+R2) restores the sound and vibration
// defaults in `settings` and is returned for the caller to perform.
ControllerFrame UpdateControllers(ControllerState& state,
                                  ControllerSettings& settings,
                                  ControllerHost& host,
                                  const ControllerFrameOptions& options);
}  // namespace VS::Input
