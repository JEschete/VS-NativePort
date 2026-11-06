#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// MPD room effects (2842C.c:550-1456): geometry group flags, texture frame,
// scroll, image and swing animations over the room's texture effect records,
// and the effect scripts that move geometry groups and play the room's
// sounds. Every record is decoded from and stored back to the section bytes
// the host supplies; reads and writes stay inside those spans and anything
// that would leave them is refused with a "field.effects" warning.
namespace VS::Field {
// D_800F1DD8: texture effects take the first 64 slots, scripts the rest.
inline constexpr std::size_t kRoomEffectSlotCount = 100;
inline constexpr std::size_t kRoomEffectScriptSlotBase = 64;
// sizeof(Field_RoomEffectObject): the heap block CreateRoomEffectObjects
// reserves for each script object.
inline constexpr std::size_t kRoomEffectObjectRecordBytes = 0x34;
// sizeof(Field_RoomEffectCommand): the header and the widest operands.
inline constexpr std::size_t kRoomEffectCommandBytes = 28;
inline constexpr std::size_t kRoomEffectCommandHeaderBytes = 4;
inline constexpr std::size_t kRoomEffectUvCorners = 4;

// D_800E85CC: the texture effect handler for each record kind. Kinds 3 to 5
// hold func_80092540, which stops the effect.
enum class RoomTextureEffectKind : std::uint8_t {
  kFrames = 0,  // func_80091C6C
  kImages = 1,  // func_80091FE8
  kScroll = 2,  // func_80091E10
  kSwing = 6,   // func_800923F8
};
inline constexpr std::uint8_t kRoomTextureEffectKindCount = 7;

// D_800E85E8: the effect script command handlers, by opcode.
enum class RoomEffectOpcode : std::int16_t {
  kRadialVelocity = 0,  // func_80092C68
  kVelocity = 1,        // func_80092E14
  kSkip = 2,            // func_80092F6C
  kHideObject = 3,      // func_80092F74
  kShowGroup = 4,       // func_80092F98
  kAdvanceMotion = 5,   // func_80092FBC
  kPlaySound = 6,       // func_8009352C
  kStopSound = 7,       // func_800935B8
  kSetTransform = 8,    // func_80092E7C
  kStartEffect = 9,     // func_800935F8
  kHideGroup = 10,      // func_8009361C
  kJump = 11,           // func_800936F0: the size field is the signed jump
  kStopScript = 12,     // func_80093640
};
inline constexpr std::int16_t kRoomEffectOpcodeCount = 13;

// A handler's verdict: advance past the command, or keep the object waiting
// on it until the next update (a nonzero return in the original).
enum class RoomEffectCommandResult : std::uint8_t { kAdvance, kWait };

// func_80091C6C_t: kind 0, 24 bytes. The UV corners are offsets from the
// current frame's cell.
struct TextureFrameEffect {
  std::int32_t geometry_offset = 0;
  std::uint8_t delay = 0;
  std::uint8_t base_u = 0;
  std::uint8_t base_v = 0;
  std::uint8_t frame_count = 0;
  std::uint8_t columns = 0;
  std::uint8_t frame_width = 0;
  std::uint8_t frame_height = 0;
  std::uint8_t tick = 0;
  std::uint8_t frame = 0;
  std::array<std::uint8_t, kRoomEffectUvCorners> u{};
  std::array<std::uint8_t, kRoomEffectUvCorners> v{};

  friend bool operator==(const TextureFrameEffect&,
                         const TextureFrameEffect&) = default;
};

// func_80091E10_t: kind 2, 28 bytes in a 32-byte section stride. The window
// scrolls inside the base rectangle and wraps at its edges.
struct TextureScrollEffect {
  std::int32_t geometry_offset = 0;
  std::uint8_t delay = 0;
  std::uint8_t base_u = 0;
  std::uint8_t base_v = 0;
  std::uint8_t width = 0;
  std::uint8_t height = 0;
  std::uint8_t current_u = 0;
  std::uint8_t current_v = 0;
  std::uint8_t window_width = 0;
  std::uint8_t window_height = 0;
  std::int8_t step_u = 0;
  std::int8_t step_v = 0;
  std::uint8_t tick = 0;
  std::array<std::uint8_t, kRoomEffectUvCorners> u{};
  std::array<std::uint8_t, kRoomEffectUvCorners> v{};

  friend bool operator==(const TextureScrollEffect&,
                         const TextureScrollEffect&) = default;
};

// A VRAM rectangle (libgpu RECT).
struct RoomImageRect {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t w = 0;
  std::int16_t h = 0;

  friend bool operator==(const RoomImageRect&, const RoomImageRect&) = default;
};

// func_80091FE8_t: kind 1, 20 bytes. Copies animation cells (or a scrolling
// band) of a VRAM sheet over the room texture and its half-size copy.
struct TextureImageEffect {
  std::uint8_t tick = 0;
  std::uint8_t frame = 0;
  std::uint8_t frame_count = 0;
  std::uint8_t columns = 0;
  std::uint8_t delay = 0;
  std::int32_t scroll_step = 0;
  RoomImageRect rect;

  friend bool operator==(const TextureImageEffect&,
                         const TextureImageEffect&) = default;
};

// D_800F1DD8_t2: kind 6, 36 bytes. A geometry group swinging on one axis
// toward a rest angle.
struct SwingEffect {
  std::int32_t geometry_offset = 0;
  std::uint8_t needs_reset = 0;
  std::uint8_t axis = 0;
  std::int32_t angular_velocity = 0;
  std::int32_t acceleration = 0;
  std::int32_t damping_shift = 0;
  std::int32_t angle = 0;
  std::int32_t rest_angle = 0;
  std::int32_t start_angle = 0;
  std::int32_t initial_velocity = 0;

  friend bool operator==(const SwingEffect&, const SwingEffect&) = default;
};

struct RoomEffectPosition {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;

  friend bool operator==(const RoomEffectPosition&,
                         const RoomEffectPosition&) = default;
};

struct RoomEffectVector {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;

  friend bool operator==(const RoomEffectVector&,
                         const RoomEffectVector&) = default;
};

// func_80092F74_t: one geometry group an effect script moves. The original's
// spare vector lanes carry the motion timer, the bounce count and the two
// damping factors.
struct RoomEffectObject {
  // The geometry group index, or -1 when the object moves nothing.
  std::int16_t object_id = -1;
  // The group record's byte offset in the room geometry.
  std::size_t group_offset = 0;
  // The next command's byte offset in the effect scripts, or none when the
  // object's script has ended.
  std::optional<std::size_t> cursor;
  // Sixteen times the group position.
  RoomEffectPosition position;
  std::int32_t elapsed = 0;
  RoomEffectVector rotation;
  std::int16_t bounces = 0;
  RoomEffectVector velocity;
  std::int16_t velocity_damping = 0;
  RoomEffectVector angular_velocity;
  std::int16_t angular_damping = 0;

  friend bool operator==(const RoomEffectObject&,
                         const RoomEffectObject&) = default;
};

// One decoded command: the header and as many operand bytes as its opcode
// reads; the rest stay zero. Accessors take the original byte offsets.
struct RoomEffectCommand {
  std::int16_t size = 0;
  RoomEffectOpcode opcode = RoomEffectOpcode::kSkip;
  std::array<std::uint8_t, kRoomEffectCommandBytes> bytes{};

  [[nodiscard]] std::uint16_t Half(std::size_t offset) const;
  [[nodiscard]] std::int16_t SignedHalf(std::size_t offset) const;
  [[nodiscard]] std::int32_t Word(std::size_t offset) const;
};

// The heap block a script's objects hold in the original, as the host names
// it.
enum class RoomEffectStorageId : std::uintptr_t { kNone = 0 };

// D_800F1DD8_t: one texture effect or effect script slot.
struct RoomEffectInstance {
  std::uint8_t active = 0;
  // The texture effect kind, or the script's object count.
  std::uint8_t kind_or_object_count = 0;
  std::uint8_t update_enabled = 0;
  // The record's byte offset in the texture effects or effect scripts.
  std::size_t data_offset = 0;
  RoomEffectStorageId storage = RoomEffectStorageId::kNone;
  // Advances whenever the objects are rebuilt or released.
  std::uint32_t generation = 0;
  std::vector<RoomEffectObject> objects;
};

// D_800F1DD8, D_800F1D00 and D_800F2258..D_800F225B.
struct RoomEffectState {
  std::array<RoomEffectInstance, kRoomEffectSlotCount> instances{};
  std::size_t texture_effect_count = 0;
  std::size_t script_count = 0;
  // The looping room sound, or -1 for none (zero until the first room, as
  // in the original's BSS).
  std::int16_t sound_id = 0;
  bool sound_restored = false;
};

// Everything the room effects need from the loaded room and the rest of the
// game.
class RoomEffectHost {
 public:
  RoomEffectHost() = default;
  RoomEffectHost(const RoomEffectHost&) = delete;
  RoomEffectHost& operator=(const RoomEffectHost&) = delete;
  RoomEffectHost(RoomEffectHost&&) = delete;
  RoomEffectHost& operator=(RoomEffectHost&&) = delete;
  virtual ~RoomEffectHost() = default;

  // The room's MPD sections; empty when the room has none.
  [[nodiscard]] virtual std::span<std::byte> Geometry() = 0;
  [[nodiscard]] virtual std::span<std::byte> TextureEffects() = 0;
  [[nodiscard]] virtual std::span<const std::byte> EffectScripts() = 0;
  [[nodiscard]] virtual std::span<const std::byte> EntryPointSection() = 0;

  // The room's sound bank and its sounds.
  [[nodiscard]] virtual bool HasSoundBank() = 0;
  virtual void PlayCenteredSound(std::int32_t sound_index) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void PlaySound(std::int32_t sound_index, std::int32_t pan,
                         std::int32_t volume) = 0;
  virtual void StopSound(std::int32_t sound_index) = 0;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  virtual void FadeSound(std::int32_t sound_index, std::int32_t volume,
                         std::int32_t fade_ticks) = 0;

  // Copies a VRAM rectangle to (x, y).
  virtual void MoveImage(const RoomImageRect& source, std::int32_t x,
                         std::int32_t y) = 0;

  // The heap block the original allocates for a script's objects; none when
  // the heap refuses it.
  [[nodiscard]] virtual std::optional<RoomEffectStorageId> ReserveObjectStorage(
      std::size_t bytes) = 0;
  virtual void ReleaseObjectStorage(RoomEffectStorageId storage) = 0;

  // The collision floor query's code: a signed 15-bit height below a flag.
  [[nodiscard]] virtual std::uint16_t FloorHeightCode(int world_x,
                                                      int world_z) = 0;
  // Scripts run every command to its end at once (the skip state).
  [[nodiscard]] virtual bool FastForwarding() = 0;
  // Runs the code after the section E header through a known binding.
  [[nodiscard]] virtual bool RunRoomEntryPoint(
      std::span<const std::byte> code) = 0;
};

// Geometry groups. The group index is checked against the room's group
// count; a group record outside the geometry span is refused.
[[nodiscard]] std::optional<std::size_t> FindRoomGeometryGroup(
    std::span<const std::byte> geometry, int group_index);
bool ShowRoomGeometryGroup(std::span<std::byte> geometry, int group_index);
bool HideRoomGeometryGroup(std::span<std::byte> geometry, int group_index);
bool SwapRoomGeometryGroupFlags(std::span<std::byte> geometry,
                                int first_group_index, int second_group_index);
bool AddRoomGeometryDepthBias(std::span<std::byte> geometry, int group_index,
                              int depth_bias);
bool DisableRoomGeometryGroup(std::span<std::byte> geometry, int group_index);
void SwapRoomGeometryRedGreen(std::span<std::byte> geometry, int group_index);

// Texture effect handlers: false stops the effect.
bool AdvanceRoomTextureFrames(TextureFrameEffect& effect,
                              std::span<std::byte> geometry);
bool ScrollRoomTextureCoordinates(TextureScrollEffect& effect,
                                  std::span<std::byte> geometry);
bool AnimateRoomTextureImages(TextureImageEffect& effect, RoomEffectHost& host);
bool UpdateRoomRotationEffect(SwingEffect& effect,
                              std::span<std::byte> geometry);
// The empty texture and command handler: it always reports done.
[[nodiscard]] bool SkipRoomEffectCommand();

// Room set-up, the per-frame update and the slot controls.
void UpdateRoomEffects(RoomEffectState& state, RoomEffectHost& host);
void InitializeRoomEffects(RoomEffectState& state, RoomEffectHost& host);
void CreateRoomEffectObjects(RoomEffectInstance& instance,
                             RoomEffectHost& host);
void FinishRoomEffectSetup();
bool StartRoomEffect(RoomEffectState& state, RoomEffectHost& host,
                     int effect_index);
bool ReleaseRoomEffects(RoomEffectState& state, RoomEffectHost& host);
bool ToggleRoomEffectUpdates(RoomEffectState& state, int effect_index);
[[nodiscard]] std::uint8_t IsRoomEffectActive(const RoomEffectState& state,
                                              int effect_index);
void FinishRoomDoorRotation(RoomEffectState& state, RoomEffectHost& host,
                            int effect_index);
void FinishRoomTiltRotation(const RoomEffectState& state, RoomEffectHost& host,
                            int effect_index);

// The effect script interpreter: runs each object of one script slot until
// it waits or ends; false once every object has ended.
bool ExecuteRoomEffectCommands(RoomEffectState& state, RoomEffectHost& host,
                               std::size_t instance_index);
RoomEffectCommandResult SetRoomEffectRadialVelocity(
    RoomEffectObject& object, const RoomEffectCommand& command);
RoomEffectCommandResult SetRoomEffectVelocity(RoomEffectObject& object,
                                              const RoomEffectCommand& command);
RoomEffectCommandResult SetRoomEffectTransform(RoomEffectObject& object,
                                               const RoomEffectCommand& command,
                                               std::span<std::byte> geometry);
void WriteRoomEffectTransform(const RoomEffectObject& object,
                              std::span<std::byte> geometry);
RoomEffectCommandResult HideRoomEffectObject(const RoomEffectObject& object,
                                             std::span<std::byte> geometry);
RoomEffectCommandResult ExecuteRoomEffectShowGroup(
    const RoomEffectCommand& command, std::span<std::byte> geometry);
RoomEffectCommandResult AdvanceRoomEffectMotion(
    RoomEffectObject& object, const RoomEffectCommand& command,
    RoomEffectHost& host);
bool StepRoomEffectGravity(RoomEffectObject& object, RoomEffectHost& host);
bool StepRoomEffectDampedMotion(RoomEffectObject& object,
                                std::span<std::byte> geometry);
RoomEffectCommandResult ExecuteRoomEffectPlaySound(
    const RoomEffectCommand& command, RoomEffectState& state,
    RoomEffectHost& host);
RoomEffectCommandResult ExecuteRoomEffectStopSound(
    const RoomEffectCommand& command, RoomEffectState& state,
    RoomEffectHost& host);
RoomEffectCommandResult ExecuteRoomEffectStartEffect(
    const RoomEffectCommand& command, RoomEffectState& state,
    RoomEffectHost& host);
RoomEffectCommandResult ExecuteRoomEffectHideGroup(
    const RoomEffectCommand& command, std::span<std::byte> geometry);
RoomEffectCommandResult ExecuteRoomEffectStopScript(
    const RoomEffectCommand& command, RoomEffectState& state,
    RoomEffectHost& host);

// The looping room sound.
void DuckRoomEffectSound(const RoomEffectState& state, RoomEffectHost& host,
                         bool ducked);
void RestoreRoomEffectSoundVolume(RoomEffectState& state, RoomEffectHost& host);
void RestartRoomEffectSound(const RoomEffectState& state, RoomEffectHost& host);

// 2EA3C.s:1770: the code after the section E header, run only through a
// binding the host recognizes.
bool CallRoomEntryPoint(RoomEffectHost& host);
}  // namespace VS::Field
