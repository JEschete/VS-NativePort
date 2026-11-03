// Hand-maintained (tools/port/owned.csv).
#include "Actors/ModelRendering.hpp"  // NOLINT(build/include): native C++ interface.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/ModelDrawing.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Actors {
namespace {
using Core::Math::Angles3;
using Core::Math::BuildEulerRotationMatrix;
using Core::Math::BuildEulerRotationMatrixQuadrant;
using Core::Math::Mat3;
using Core::Math::PremultiplyRotationMatrix;
using Core::Math::RotateTranslate;
using Core::Math::ScaleMatrixColumns;
using Core::Math::Transform3;
using Core::Math::Vec3;
using Core::Math::WrapSigned16;

// libgpu getTPage and getClut fields.
constexpr std::uint32_t kPageModeMask = 0x3;
constexpr std::uint32_t kPageColorModeShift = 7;
constexpr std::uint32_t kPageBlendModeShift = 5;
constexpr std::uint32_t kPageLowRowBit = 0x100;
constexpr std::uint32_t kPageLowRowShift = 4;
constexpr std::uint32_t kPageColumnMask = 0x3FF;
constexpr std::uint32_t kPageColumnShift = 6;
constexpr std::uint32_t kPageHighRowBit = 0x200;
constexpr std::uint32_t kPageHighRowShift = 2;
constexpr std::uint32_t kClutRowShift = 6;
constexpr std::uint32_t kClutColumnShift = 4;
constexpr std::uint32_t kClutColumnMask = 0x3F;

// Texture slot origins count 64-column pages and 256-row bands.
constexpr std::uint32_t kOriginRowShift = 8;
constexpr std::uint32_t kFourBitColor = 0;
constexpr std::uint32_t kEightBitColor = 1;
constexpr std::uint32_t kNoBlend = 0;
// Actor palettes are 16-color runs of column 768; slots below 12 have rows
// 228 and 246 onwards, the rest share rows 229 and 247.
constexpr std::uint32_t kActorPaletteColumn = 768;
constexpr std::size_t kOwnPaletteSlots = 12;
constexpr std::uint32_t kActorPaletteRow = 228;
constexpr std::uint32_t kActorAlternatePaletteRow = 246;
constexpr std::uint32_t kSharedPaletteRow = 229;
constexpr std::uint32_t kSharedAlternatePaletteRow = 247;
constexpr std::uint32_t kWeaponPageWidth = 64;
constexpr std::uint32_t kWeaponPaletteColumn = 928;
constexpr std::uint32_t kWeaponPairPaletteStride = 48;
constexpr std::uint32_t kWeaponPaletteRow = 228;
constexpr std::uint32_t kGimmickPageColumn = 64;
constexpr std::uint32_t kGimmickPageRow = 256;
constexpr std::uint32_t kGimmickPaletteColumn = 768;
constexpr std::uint32_t kGimmickPaletteWidth = 16;
constexpr std::uint32_t kGimmickPaletteRow = 237;
constexpr std::uint32_t kPulsedGimmickPaletteRow = 255;

// The root key over twice the Q12 instance scale.
constexpr std::int32_t kScaledRootDivisor = kUnitActorScale * 2;
constexpr std::uint32_t kAngleMask = 0xFFF;
// A gimmick is projected from 16 units below its center.
constexpr std::int32_t kGimmickLift = 16;
constexpr std::uint32_t kHeldObjectIds = 4;
constexpr std::uint32_t kFreeObjectIds = 2;

// Appearance states 4..8 other than 6 draw with the colour overrides; so do
// gimmick states from 12 and the weapon owner's state 15.
constexpr std::uint32_t kFirstOverrideState = 4;
constexpr std::uint32_t kOverrideStateEnd = 9;
constexpr std::uint32_t kPlainOverrideState = 6;
constexpr std::uint32_t kLateGimmickState = 12;
constexpr std::uint32_t kWeaponOverrideState = 15;
constexpr std::uint32_t kPulsedGimmickState = 6;

constexpr std::uint8_t kDetachedAttachBone = 0xFC;
constexpr std::uint16_t kDrawnCollisionRadius = 0x80;
constexpr std::uint8_t kFirstLinkedCompanion = 2;
// Gimmicks draw four ordering-table entries nearer than the actors.
constexpr std::int32_t kGimmickDepthEntries = 4;
// Gimmick mode 0xF7 steps its motion every pass.
constexpr std::int32_t kSelfDrivenGimmickMode = 0xF7;

[[nodiscard]] std::int32_t Halfword(std::int64_t value) noexcept {
  return WrapSigned16(static_cast<std::uint32_t>(value));
}

[[nodiscard]] std::int32_t MaskedAngle(std::int64_t value) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value) &
                                   kAngleMask);
}

[[nodiscard]] bool InOverrideRange(std::uint32_t state) noexcept {
  return state >= kFirstOverrideState && state < kOverrideStateEnd &&
         state != kPlainOverrideState;
}

[[nodiscard]] std::int32_t HalvedRootKey(std::int32_t key, bool unscaled,
                                         std::int32_t scale) noexcept {
  if (unscaled) return Halfword(Halfword(key) >> 1);
  return Halfword(Halfword(key) * Halfword(scale) / kScaledRootDivisor);
}

// Whether an earlier slot holds a model with the same id: only the first actor
// of a model scrolls its shared texture.
[[nodiscard]] bool EarlierSlotSharesModel(const RoomModelPassHost& host,
                                          std::size_t slot,
                                          std::uint16_t model_id) {
  for (std::size_t earlier = 0; earlier < slot; ++earlier) {
    const std::optional<RoomActorState> other = host.Actor(earlier);
    if (other && other->model_id == model_id) return true;
  }
  return false;
}

// The texture animation, palette pulse and appearance transition of `slot`.
// Nothing for an actor no longer in the table.
bool UpdateActorAppearance(RoomModelPassHost& host, std::size_t slot) {
  host.UpdateTextureAnimations(slot);
  std::optional<RoomActorState> actor = host.Actor(slot);
  if (!actor) return false;
  if (actor->pulsing) {
    host.UpdatePalettePulse(slot);
    actor = host.Actor(slot);
    if (!actor) return false;
  }
  if (actor->transitioning) host.UpdateAppearanceTransition(slot);
  return true;
}

// A frozen actor's sound transition and texture scrolling; the scroll carries
// `drawing_slot`, the frozen actor's own slot.
void UpdateFrozenTextures(RoomModelPassHost& host, std::size_t slot,
                          std::size_t drawing_slot) {
  host.UpdateSoundTransition(slot);
  const std::optional<RoomActorState> actor = host.Actor(slot);
  if (!actor || EarlierSlotSharesModel(host, drawing_slot, actor->model_id)) {
    return;
  }
  host.UpdateTextureScrolling(slot, drawing_slot);
}

[[nodiscard]] Vec3 Negated(Vec3 offset) noexcept {
  return Vec3{-offset.x, -offset.y, -offset.z};
}

// The companion draw: a linked companion's appearance first, then the actor
// redrawn with the companion's geometry, offset and depth bias.
void DrawCompanion(RoomModelPassHost& host, std::size_t slot) {
  std::optional<RoomActorState> actor = host.Actor(slot);
  if (!actor || actor->companion_slot == 0) return;
  const std::size_t companion = actor->companion_slot;
  if (companion >= kFirstLinkedCompanion && host.Actor(companion) &&
      UpdateActorAppearance(host, companion)) {
    actor = host.Actor(slot);
    if (!actor) return;
    if (actor->frozen) UpdateFrozenTextures(host, companion, slot);
  }
  actor = host.Actor(slot);
  if (!actor) return;
  host.OffsetOrderingTable(actor->depth_bias);
  host.MoveActor(slot, actor->companion_offset);
  actor = host.Actor(slot);
  if (!actor) return;
  host.UpdateMotionAndLookAt(slot, actor->companion_slot);
  actor = host.Actor(slot);
  if (!actor) return;
  host.DrawActor(slot, actor->companion_slot);
  actor = host.Actor(slot);
  if (!actor) return;
  host.OffsetOrderingTable(-static_cast<std::int32_t>(actor->depth_bias));
  host.MoveActor(slot, Negated(actor->companion_offset));
}

void DrawRoomActor(RoomModelPassHost& host, std::size_t slot) {
  std::optional<RoomActorState> actor = host.Actor(slot);
  if (!actor || !DrawsInRoomPass(*actor)) return;
  if (!UpdateActorAppearance(host, slot)) return;
  host.SetDrawn(slot, false);
  actor = host.Actor(slot);
  if (!actor) return;
  if (actor->frozen) {
    UpdateFrozenTextures(host, slot, slot);
    host.StepFrozenAnimation(slot);
  } else {
    host.PrepareRootTransform(slot);
  }
  actor = host.Actor(slot);
  if (!actor) return;
  if (actor->attached) {
    host.DrawAttached(slot);
  } else {
    host.DrawActor(slot, 0);
    DrawCompanion(host, slot);
  }
  actor = host.Actor(slot);
  if (actor && MarksActorDrawn(*actor)) host.SetDrawn(slot, true);
}

void DrawRoomWeapon(RoomModelPassHost& host, std::size_t index) {
  const std::optional<RoomWeaponState> weapon = host.Weapon(index);
  if (!weapon || !weapon->shown || weapon->attachment_bone == 0) return;
  const std::optional<RoomActorState> owner = host.Actor(weapon->owner);
  if (owner && OwnerCarriesWeapon(*owner)) host.DrawWeapon(index);
}

void DrawRoomGimmick(RoomModelPassHost& host, std::size_t index) {
  std::optional<RoomGimmickState> gimmick = host.Gimmick(index);
  if (!gimmick || !gimmick->has_bones) return;
  const PushedGimmick pushed = host.PushedGimmickState();
  const bool pushed_now = pushed.index >= 0 &&
                          static_cast<std::size_t>(pushed.index) == index &&
                          !pushed.update_complete;
  if (pushed_now || gimmick->mode == kSelfDrivenGimmickMode) {
    host.UpdateGimmickMotion(index);
  }
  host.CenterGimmick(index);
  gimmick = host.Gimmick(index);
  if (!gimmick) return;
  if (gimmick->pulsing) {
    host.UpdateGimmickPalettePulse(index);
    gimmick = host.Gimmick(index);
    if (!gimmick) return;
  }
  if (gimmick->motion_state != 0) {
    host.UpdateGimmickAppearanceTransition(index);
  }
  host.DrawGimmick(index);
}
}  // namespace

// decomp: libgpu getTPage/getClut, as the INITBTL.PRG and BATTLE.PRG callers
// below inline them.
std::uint16_t TexturePageId(std::uint32_t color_mode, std::uint32_t blend_mode,
                            VramPosition origin) noexcept {
  return static_cast<std::uint16_t>(
      ((color_mode & kPageModeMask) << kPageColorModeShift) |
      ((blend_mode & kPageModeMask) << kPageBlendModeShift) |
      ((origin.y & kPageLowRowBit) >> kPageLowRowShift) |
      ((origin.x & kPageColumnMask) >> kPageColumnShift) |
      ((origin.y & kPageHighRowBit) << kPageHighRowShift));
}

std::uint16_t ClutId(VramPosition origin) noexcept {
  return static_cast<std::uint16_t>(
      (origin.y << kClutRowShift) |
      ((origin.x >> kClutColumnShift) & kClutColumnMask));
}

// decomp: INITBTL.PRG func_800FA7B0 (src/BATTLE/INITBTL.PRG/FB0.c:19), the
// two actor texture slot loops.
TextureSlotIds ActorTextureSlotIds(std::size_t slot, std::int16_t origin_column,
                                   std::int16_t origin_page) noexcept {
  TextureSlotIds ids;
  ids.texture_page = TexturePageId(
      kFourBitColor, kNoBlend,
      VramPosition{static_cast<std::uint32_t>(origin_column)
                       << kPageColumnShift,
                   static_cast<std::uint32_t>(origin_page) << kOriginRowShift});
  if (slot < kOwnPaletteSlots) {
    const auto row = static_cast<std::uint32_t>(slot);
    ids.palette = ClutId({kActorPaletteColumn, kActorPaletteRow + row});
    ids.alternate_palette =
        ClutId({kActorPaletteColumn, kActorAlternatePaletteRow + row});
  } else {
    ids.palette = ClutId({kActorPaletteColumn, kSharedPaletteRow});
    ids.alternate_palette =
        ClutId({kActorPaletteColumn, kSharedAlternatePaletteRow});
  }
  return ids;
}

// decomp: INITBTL.PRG func_800FA7B0 (src/BATTLE/INITBTL.PRG/FB0.c:19), the
// weapon texture slot loop.
TextureSlotIds WeaponTextureSlotIds(std::size_t slot) noexcept {
  const auto pair = static_cast<std::uint32_t>(slot / 2);
  const auto side = static_cast<std::uint32_t>(slot & 1U);
  TextureSlotIds ids;
  ids.texture_page =
      TexturePageId(kEightBitColor, kNoBlend, {pair * kWeaponPageWidth, 0});
  ids.palette = ClutId({side * kWeaponPairPaletteStride + kWeaponPaletteColumn,
                        pair + kWeaponPaletteRow});
  return ids;
}

// decomp: BATTLE.PRG func_800AEAE8 (src/BATTLE/BATTLE.PRG/44F14.c:826).
Vec3 ActorRootOffset(Vec3 root_key, Vec3 instance_scale,
                     std::int32_t actor_scale) noexcept {
  const bool unscaled =
      instance_scale == Vec3{kUnitActorScale, kUnitActorScale, kUnitActorScale};
  const Vec3 halved{HalvedRootKey(root_key.x, unscaled, instance_scale.x),
                    HalvedRootKey(root_key.y, unscaled, instance_scale.y),
                    HalvedRootKey(root_key.z, unscaled, instance_scale.z)};
  const std::int32_t scale = Halfword(actor_scale);
  return Vec3{Halfword(halved.x * scale / kUnitActorScale),
              Halfword(halved.y * scale / kUnitActorScale),
              Halfword(halved.z * scale / kUnitActorScale)};
}

// decomp: BATTLE.PRG func_800B07DC (src/BATTLE/BATTLE.PRG/44F14.c:1695).
RootTransform TransformRootOffset(Angles3 rotation, Vec3 offset,
                                  Vec3 position) noexcept {
  const Mat3 matrix = BuildEulerRotationMatrix(rotation);
  const Vec3 translation = RotateTranslate(Transform3{matrix, Vec3{}}, offset);
  return RootTransform{
      Transform3{matrix, translation},
      Vec3{Halfword(static_cast<std::int64_t>(translation.x) + position.x),
           Halfword(static_cast<std::int64_t>(translation.y) + position.y),
           Halfword(static_cast<std::int64_t>(translation.z) + position.z)}};
}

// decomp: BATTLE.PRG func_800B1A68 (src/BATTLE/BATTLE.PRG/49268.c:48).
GimmickPlacement ClassifyGimmickPlacement(std::uint32_t object_id,
                                          std::uint32_t holder,
                                          std::int32_t mode) noexcept {
  if (object_id >= kHeldObjectIds) {
    return holder == kNoGimmickHolder ? GimmickPlacement::kFree
                                      : GimmickPlacement::kHeld;
  }
  if (object_id >= kFreeObjectIds) return GimmickPlacement::kFree;
  return mode != 0 ? GimmickPlacement::kScripted : GimmickPlacement::kPlaced;
}

std::optional<Vec3> ModelColumnScale(Vec3 scale) noexcept {
  if (scale == Vec3{kUnitModelScale, kUnitModelScale, kUnitModelScale}) {
    return std::nullopt;
  }
  return Vec3{scale.x * kUnitModelScale, scale.y * kUnitModelScale,
              scale.z * kUnitModelScale};
}

// decomp: BATTLE.PRG func_800B1A68 (src/BATTLE/BATTLE.PRG/49268.c:48). The
// original's GTE sequence: RTV0TR of the lifted center (or the anchors'
// midpoint) through the camera gives the translation; the rotation depends on
// the placement; every placement but kScripted is then turned by the camera.
GimmickTransform PlaceGimmick(GimmickPlacement placement,
                              const GimmickPose& pose, const Transform3& camera,
                              Vec3 center, Vec3 room_scale) noexcept {
  GimmickTransform result;
  result.position =
      Vec3{Halfword(center.x),
           Halfword(static_cast<std::int64_t>(center.y) + kGimmickLift),
           Halfword(center.z)};
  Mat3 rotation;
  switch (placement) {
    case GimmickPlacement::kHeld: {
      const auto& [first, second] = pose.anchors;
      result.position =
          Vec3{Halfword((static_cast<std::int64_t>(first.x) + second.x) / 2),
               Halfword((static_cast<std::int64_t>(first.y) + second.y) / 2),
               Halfword((static_cast<std::int64_t>(first.z) + second.z) / 2)};
      const Angles3 own{
          Halfword(static_cast<std::int64_t>(pose.rotation_offset.x) +
                   pose.rotation.x),
          Halfword(static_cast<std::int64_t>(pose.rotation_offset.y) +
                   pose.rotation.y),
          Halfword(static_cast<std::int64_t>(pose.rotation_offset.z) +
                   pose.rotation.z)};
      const Angles3 holder{Halfword(pose.holder_rotation.x),
                           Halfword(pose.holder_rotation.y),
                           Halfword(pose.holder_rotation.z)};
      rotation =
          PremultiplyRotationMatrix(BuildEulerRotationMatrixQuadrant(holder),
                                    BuildEulerRotationMatrixQuadrant(own));
      result.rotation = Vec3{holder.x, holder.y, holder.z};
      break;
    }
    case GimmickPlacement::kFree: {
      const Angles3 own{
          MaskedAngle(static_cast<std::int64_t>(pose.rotation_offset.x) +
                      pose.rotation.x),
          MaskedAngle(static_cast<std::int64_t>(pose.rotation_offset.y) +
                      pose.rotation.y),
          MaskedAngle(static_cast<std::int64_t>(pose.rotation_offset.z) +
                      pose.rotation.z)};
      rotation = BuildEulerRotationMatrixQuadrant(own);
      result.rotation = Vec3{own.x, own.y, own.z};
      break;
    }
    case GimmickPlacement::kScripted: {
      const Angles3 own{Halfword(pose.rotation.x), Halfword(pose.rotation.y),
                        Halfword(pose.rotation.z)};
      // libgte MulMatrix saturates each element (#284).
      rotation = Core::Math::MultiplyMatrixIntoLeft(
          PremultiplyRotationMatrix(camera.rotation,
                                    BuildEulerRotationMatrix(own)),
          pose.placement);
      result.rotation = Vec3{own.x, own.y, own.z};
      break;
    }
    case GimmickPlacement::kPlaced:
      rotation = pose.placement;
      break;
  }
  if (placement != GimmickPlacement::kScripted) {
    rotation = PremultiplyRotationMatrix(camera.rotation, rotation);
  }
  rotation = ScaleMatrixColumns(rotation, room_scale);
  result.scale = ModelColumnScale(pose.scale);
  if (result.scale) rotation = ScaleMatrixColumns(rotation, *result.scale);
  result.model = Transform3{rotation, RotateTranslate(camera, result.position)};
  return result;
}

// decomp: BATTLE.PRG func_800AEF94 (src/BATTLE/BATTLE.PRG/44F14.c:923), the
// gimmick center the pass leaves in the model workspace.
Vec3 GimmickCenter(Vec3 position, std::int32_t height) noexcept {
  return Vec3{
      Halfword(position.x),
      Halfword(static_cast<std::int64_t>(Halfword(position.y)) + height / 2),
      Halfword(position.z)};
}

std::uint16_t GimmickTexturePage() noexcept {
  return TexturePageId(kFourBitColor, kNoBlend,
                       {kGimmickPageColumn, kGimmickPageRow});
}

std::uint16_t GimmickPalette(std::uint32_t slot, bool pulsing,
                             std::uint32_t motion_state) noexcept {
  const bool pulsed = pulsing || motion_state >= kPulsedGimmickState;
  return ClutId({slot * kGimmickPaletteWidth + kGimmickPaletteColumn,
                 pulsed ? kPulsedGimmickPaletteRow : kGimmickPaletteRow});
}

bool GimmickDrawsWithOverrides(std::uint32_t motion_state) noexcept {
  return InOverrideRange(motion_state) || motion_state >= kLateGimmickState;
}

// decomp: BATTLE.PRG func_800B217C (src/BATTLE/BATTLE.PRG/49268.c:155).
WeaponFadeStep StepWeaponFade(WeaponFade fade) noexcept {
  if (fade.frames == 0) return WeaponFadeStep{fade, false};
  const std::int32_t change =
      (static_cast<std::int32_t>(fade.target) - fade.scale) / fade.frames;
  fade.scale = static_cast<std::uint8_t>(fade.scale + change);
  --fade.frames;
  return WeaponFadeStep{fade, fade.scale == 0 && fade.frames == 0};
}

Transform3 WeaponRootTransform(const Transform3& attachment,
                               std::uint8_t scale) noexcept {
  const std::optional<Vec3> columns =
      ModelColumnScale(Vec3{scale, scale, scale});
  if (!columns) return attachment;
  return Transform3{ScaleMatrixColumns(attachment.rotation, *columns),
                    attachment.translation};
}

// decomp: BATTLE.PRG func_800B217C (src/BATTLE/BATTLE.PRG/49268.c:155), the
// bone loop. The original turned each bone by the skeleton pose and moved it
// by the weapon's bone matrices; both hold the parent's transform once the
// parent precedes its child.
std::optional<WeaponBoneChain> BuildWeaponBoneChain(
    const Transform3& root, std::span<const WeaponBone> bones) noexcept {
  if (bones.size() > kWeaponBoneLimit) return std::nullopt;
  WeaponBoneChain world{};
  world[0] = root;
  for (std::size_t bone = 1; bone < bones.size(); ++bone) {
    const std::size_t parent = bones[bone].parent;
    if (parent >= bone) return std::nullopt;
    const Transform3& base = world[parent];
    const Vec3 tip{Core::Math::WrappingNegate(bones[parent].length), 0, 0};
    const Angles3& angles = bones[bone].rotation;
    const Angles3 own{MaskedAngle(angles.x), MaskedAngle(angles.y),
                      MaskedAngle(angles.z)};
    world[bone] =
        Transform3{PremultiplyRotationMatrix(
                       base.rotation, BuildEulerRotationMatrixQuadrant(own)),
                   RotateTranslate(base, tip)};
  }
  return world;
}

bool OwnerShowsWeapon(bool visible, std::int32_t height, bool leader,
                      bool event_playing) noexcept {
  if (!visible) return false;
  return height < kHiddenActorHeight || (!leader && event_playing);
}

bool WeaponDrawsWithOverrides(std::uint32_t action_state) noexcept {
  return InOverrideRange(action_state) || action_state == kWeaponOverrideState;
}

bool DrawsInRoomPass(const RoomActorState& actor) noexcept {
  return actor.has_bones && !actor.skipped &&
         actor.attach_bone != kDetachedAttachBone;
}

bool MarksActorDrawn(const RoomActorState& actor) noexcept {
  return actor.collision_radius >= kDrawnCollisionRadius &&
         actor.blend_frame != 0;
}

bool OwnerCarriesWeapon(const RoomActorState& owner) noexcept {
  return !owner.skipped && owner.visible && !owner.culled;
}

// decomp: BATTLE.PRG func_800AEF94 (src/BATTLE/BATTLE.PRG/44F14.c:923).
void UpdateAndDrawRoomModels(RoomModelPassHost& host) {
  host.PrepareCamera();
  for (std::size_t slot = 0; slot < kRoomActorSlots; ++slot) {
    DrawRoomActor(host, slot);
  }
  for (std::size_t index = 0; index < kRoomWeaponSlots; ++index) {
    DrawRoomWeapon(host, index);
  }
  host.OffsetOrderingTable(kGimmickDepthEntries);
  for (std::size_t index = 0; index < kRoomGimmickSlots; ++index) {
    DrawRoomGimmick(host, index);
  }
  host.ClearGimmickPushUpdate();
  host.OffsetOrderingTable(-kGimmickDepthEntries);
  host.CountPass();
}
}  // namespace VS::Actors
