#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

// The room model pass and the model transforms it builds (BATTLE.PRG
// 44F14.c:826-1143 and 1695, 49268.c:48-245) and the texture slot setup
// (INITBTL.PRG FB0.c:19): actor root offsets, gimmick placements, weapon bone
// chains and fades, and the rules that choose texture pages, palettes and
// override draws. The transforms take and return plain values; the pass reaches
// the actor, weapon and gimmick tables, the model workspace and the ordering
// table only through RoomModelPassHost. ModelRenderingLive binds them.
namespace VS::Actors {
// Q12 one: an actor whose three instance scales equal it is unscaled.
inline constexpr std::int32_t kUnitActorScale = 4096;
// The unit scale of gimmick and weapon models (64 is 1.0).
inline constexpr std::int32_t kUnitModelScale = 64;
inline constexpr std::size_t kRoomActorSlots = 17;
inline constexpr std::size_t kRoomWeaponSlots = 20;
inline constexpr std::size_t kRoomGimmickSlots = 16;
inline constexpr std::size_t kWeaponBoneLimit = 4;
// The texture slots the setup initializes, of each table.
inline constexpr std::size_t kInitializedActorTextureSlots = 24;
inline constexpr std::size_t kWeaponTextureSlots = 20;

// A VRAM position in 16-bit columns and rows.
struct VramPosition {
  std::uint32_t x = 0;
  std::uint32_t y = 0;
};

// libgpu getTPage: the texture page word for a color mode and blend mode at
// `origin`. getClut: the palette word for the 16-color run at `origin`.
[[nodiscard]] std::uint16_t TexturePageId(std::uint32_t color_mode,
                                          std::uint32_t blend_mode,
                                          VramPosition origin) noexcept;
[[nodiscard]] std::uint16_t ClutId(VramPosition origin) noexcept;

struct TextureSlotIds {
  std::uint16_t texture_page = 0;
  std::uint16_t palette = 0;
  std::uint16_t alternate_palette = 0;

  friend bool operator==(const TextureSlotIds&,
                         const TextureSlotIds&) = default;
};

// Actor slot `slot` draws from the 64-column page at its origin column and
// page; the first twelve slots have their own palette rows.
[[nodiscard]] TextureSlotIds ActorTextureSlotIds(
    std::size_t slot, std::int16_t origin_column,
    std::int16_t origin_page) noexcept;
// Weapon slots pair up on one 8-bit page per actor; the two weapons of a pair
// use palette runs 48 columns apart. They have no alternate palette.
[[nodiscard]] TextureSlotIds WeaponTextureSlotIds(std::size_t slot) noexcept;

// The actor root offset before rotation: the root key halved (or scaled by
// the instance scales over 8192), then scaled by the actor's Q12 scale, each
// step truncated to a signed halfword.
[[nodiscard]] Core::Math::Vec3 ActorRootOffset(
    Core::Math::Vec3 root_key, Core::Math::Vec3 instance_scale,
    std::int32_t actor_scale) noexcept;

struct RootTransform {
  // The actor rotation with the rotated offset as its translation.
  Core::Math::Transform3 matrix;
  // The rotated offset moved to the actor's position, in signed halfwords.
  Core::Math::Vec3 root_offset;
};

// Rotates the root offset by the actor's x, facing and z angles (battle Euler
// order) and moves it to `position`.
[[nodiscard]] RootTransform TransformRootOffset(
    Core::Math::Angles3 rotation, Core::Math::Vec3 offset,
    Core::Math::Vec3 position) noexcept;

// How a gimmick model is oriented, from its object id and parent.
enum class GimmickPlacement : std::uint8_t {
  // Carried between two tagged bones of its holder actor, turned with it.
  kHeld,
  // Turned by its own rotation plus its rotation offset.
  kFree,
  // Turned by its rotation and then by its placement matrix.
  kScripted,
  // Oriented by its placement matrix alone.
  kPlaced,
};

inline constexpr std::uint32_t kNoGimmickHolder = 0xFF;

[[nodiscard]] GimmickPlacement ClassifyGimmickPlacement(
    std::uint32_t object_id, std::uint32_t holder, std::int32_t mode) noexcept;

struct GimmickPose {
  Core::Math::Angles3 rotation;
  Core::Math::Angles3 rotation_offset;
  // The placement matrix's rotation (matrices[4]).
  Core::Math::Mat3 placement;
  Core::Math::Vec3 scale{kUnitModelScale, kUnitModelScale, kUnitModelScale};
  // kHeld: the holder's two tagged bone positions and its rotation.
  std::array<Core::Math::Vec3, 2> anchors{};
  Core::Math::Angles3 holder_rotation;
};

struct GimmickTransform {
  // The model matrix, in view space.
  Core::Math::Transform3 model;
  // The point the translation was taken from: 16 above the center, or the
  // midpoint of the holder's anchors.
  Core::Math::Vec3 position;
  // The rotation the placement leaves in the model workspace (none for
  // kPlaced).
  std::optional<Core::Math::Vec3> rotation;
  // The column scale, when the gimmick is not at unit scale.
  std::optional<Core::Math::Vec3> scale;
};

// Places a gimmick whose center is `center` through `camera`, then applies
// the room scale and the gimmick's own scale. kScripted composes its
// placement matrix with the saturating product (libgte MulMatrix, #284).
[[nodiscard]] GimmickTransform PlaceGimmick(
    GimmickPlacement placement, const GimmickPose& pose,
    const Core::Math::Transform3& camera, Core::Math::Vec3 center,
    Core::Math::Vec3 room_scale) noexcept;

// The column scale of a gimmick or weapon model at `scale` (64 per unit), or
// nothing at unit scale, where the model matrix keeps the room scale alone.
[[nodiscard]] std::optional<Core::Math::Vec3> ModelColumnScale(
    Core::Math::Vec3 scale) noexcept;

// The center of a gimmick standing at `position` with a model of `height`.
[[nodiscard]] Core::Math::Vec3 GimmickCenter(Core::Math::Vec3 position,
                                             std::int32_t height) noexcept;
[[nodiscard]] std::uint16_t GimmickTexturePage() noexcept;
// Pulsing or late-state gimmicks draw from the pulsed palette row.
[[nodiscard]] std::uint16_t GimmickPalette(std::uint32_t slot, bool pulsing,
                                           std::uint32_t motion_state) noexcept;
[[nodiscard]] bool GimmickDrawsWithOverrides(
    std::uint32_t motion_state) noexcept;

// reserved11..13 of a weapon model: its scale (64 is 1.0), the scale it fades
// to and the frames left.
struct WeaponFade {
  std::uint8_t scale = kUnitModelScale;
  std::uint8_t target = 0;
  std::uint8_t frames = 0;

  friend bool operator==(const WeaponFade&, const WeaponFade&) = default;
};

struct WeaponFadeStep {
  WeaponFade fade;
  // The fade shrank the weapon away: it stops being drawn.
  bool hidden = false;
};

// One frame of the fade: an even share of the remaining change.
[[nodiscard]] WeaponFadeStep StepWeaponFade(WeaponFade fade) noexcept;
// The weapon's root: its attachment bone scaled by the fade scale.
[[nodiscard]] Core::Math::Transform3 WeaponRootTransform(
    const Core::Math::Transform3& attachment, std::uint8_t scale) noexcept;

struct WeaponBone {
  // The SHP bone's parent index and length.
  std::uint32_t parent = 0;
  std::int32_t length = 0;
  // The weapon's pose angles for the bone.
  Core::Math::Angles3 rotation;
};

// Each bone's transform: bone 0 is `root`, each later bone sits at the tip of
// its parent turned by its own angles. Nothing for more than kWeaponBoneLimit
// bones or a parent that does not precede its child.
[[nodiscard]] std::optional<
    std::array<Core::Math::Transform3, kWeaponBoneLimit>>
BuildWeaponBoneChain(const Core::Math::Transform3& root,
                     std::span<const WeaponBone> bones) noexcept;

// An owner high above the floor shows its weapon only while an event plays,
// and never for the leader (as the owner's own model).
[[nodiscard]] bool OwnerShowsWeapon(bool visible, std::int32_t height,
                                    bool leader, bool event_playing) noexcept;
[[nodiscard]] bool WeaponDrawsWithOverrides(
    std::uint32_t action_state) noexcept;

// What the room pass reads of one actor record.
struct RoomActorState {
  bool has_bones = false;
  bool skipped = false;
  bool visible = false;
  // The last projection failed (isPreviewModel).
  bool culled = false;
  bool frozen = false;
  bool attached = false;
  bool pulsing = false;
  bool transitioning = false;
  std::uint8_t attach_bone = 0;
  std::uint8_t companion_slot = 0;
  std::uint16_t model_id = 0;
  std::uint16_t collision_radius = 0;
  std::uint8_t blend_frame = 0;
  // The companion draw's position offset and ordering-table bias.
  Core::Math::Vec3 companion_offset{};
  std::uint8_t depth_bias = 0;
};

struct RoomWeaponState {
  // reserved08Bit4: the weapon is drawn in the room.
  bool shown = false;
  std::uint8_t owner = 0;
  std::uint8_t attachment_bone = 0;
};

struct RoomGimmickState {
  bool has_bones = false;
  bool pulsing = false;
  std::uint8_t motion_state = 0;
  std::int32_t mode = 0;
};

struct PushedGimmick {
  std::int32_t index = 0;
  // Set once an actor update has handled the push this frame.
  bool update_complete = false;
};

// An actor detached from the room pass (attach bone 0xFC) is not drawn.
[[nodiscard]] bool DrawsInRoomPass(const RoomActorState& actor) noexcept;
[[nodiscard]] bool MarksActorDrawn(const RoomActorState& actor) noexcept;
[[nodiscard]] bool OwnerCarriesWeapon(const RoomActorState& owner) noexcept;

class RoomModelPassHost {
 public:
  virtual ~RoomModelPassHost() = default;
  virtual void PrepareCamera() = 0;
  // The actor in `slot`, or nothing for an empty slot.
  [[nodiscard]] virtual std::optional<RoomActorState> Actor(
      std::size_t slot) const = 0;
  virtual void UpdateTextureAnimations(std::size_t slot) = 0;
  virtual void UpdatePalettePulse(std::size_t slot) = 0;
  virtual void UpdateAppearanceTransition(std::size_t slot) = 0;
  virtual void SetDrawn(std::size_t slot, bool drawn) = 0;
  virtual void UpdateSoundTransition(std::size_t slot) = 0;
  // `carried_index` is the drawing actor's index, which the scroll carries.
  virtual void UpdateTextureScrolling(std::size_t slot,
                                      std::size_t carried_index) = 0;
  // The frozen actor's route, animation blend at the frame's tick speed and
  // step-length reset.
  virtual void StepFrozenAnimation(std::size_t slot) = 0;
  virtual void PrepareRootTransform(std::size_t slot) = 0;
  // Draws an attached actor from its parent's bone.
  virtual void DrawAttached(std::size_t slot) = 0;
  virtual void DrawActor(std::size_t slot, std::uint8_t companion) = 0;
  virtual void MoveActor(std::size_t slot, Core::Math::Vec3 offset) = 0;
  virtual void OffsetOrderingTable(std::int32_t entries) = 0;
  virtual void UpdateMotionAndLookAt(std::size_t slot,
                                     std::uint8_t companion) = 0;
  [[nodiscard]] virtual std::optional<RoomWeaponState> Weapon(
      std::size_t index) const = 0;
  // Draws the weapon from its owner's attachment bone.
  virtual void DrawWeapon(std::size_t index) = 0;
  [[nodiscard]] virtual std::optional<RoomGimmickState> Gimmick(
      std::size_t index) const = 0;
  [[nodiscard]] virtual PushedGimmick PushedGimmickState() const = 0;
  virtual void UpdateGimmickMotion(std::size_t index) = 0;
  // Centers the model workspace on the gimmick.
  virtual void CenterGimmick(std::size_t index) = 0;
  virtual void UpdateGimmickPalettePulse(std::size_t index) = 0;
  virtual void UpdateGimmickAppearanceTransition(std::size_t index) = 0;
  virtual void DrawGimmick(std::size_t index) = 0;
  // Clears the flag that an actor update handled the pushed gimmick.
  virtual void ClearGimmickPushUpdate() = 0;
  virtual void CountPass() = 0;
};

// One frame of the room's actors, their weapons and the gimmicks, in the
// original order (func_800AEF94). Each decision reads the record's state as
// it is then, as the original re-read its fields; an actor that leaves the
// table during its own update is not processed further.
void UpdateAndDrawRoomModels(RoomModelPassHost& host);
}  // namespace VS::Actors
