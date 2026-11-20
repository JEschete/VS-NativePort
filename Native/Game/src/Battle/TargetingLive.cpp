// Hand-maintained (tools/port/owned.csv).
// Binds the typed action targeting (Targeting.cpp) to the live action state,
// battle actors, room, targeting sphere, menus and pad input.
#include "Battle/TargetingLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/Location.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionPhasesLive.hpp"
#include "Battle/ActionPresentationLive.hpp"
#include "Battle/ActionPreviewLive.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionRecords.hpp"
#include "Battle/BattleTypes.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/Hit.hpp"
#include "Battle/Hud.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Battle/Sphere.hpp"
#include "Battle/SphereContainmentLive.hpp"
#include "Battle/SphereDrawLive.hpp"
#include "Battle/SphereGeometryLive.hpp"
#include "Battle/SphereLifecycleLive.hpp"
#include "Battle/SphereTargetLabelsLive.hpp"
#include "Battle/SphereTerrainLive.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusHelpersLive.hpp"
#include "Battle/TargetPages.hpp"
#include "Battle/TargetVolume.hpp"
#include "Battle/TargetingState.hpp"
#include "Battle/TextBoxes.hpp"
#include "Core/Log.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/Interactions.hpp"
#include "Field/MapLive.hpp"
#include "Field/Mpd.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Field/Room.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Input/Controller.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/LootLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/TextBox.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/Palette.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/SpritesLive.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::ActionRequestRecord;
using VS::Battle::ActionState;
using VS::Battle::TargetLabelLine;
using VS::Battle::TargetSphereLabelView;
using VS::Battle::TargetVolume;

namespace {
namespace battle = VS::Battle;
using battle::ActionTargetPreview;
using battle::ActorLimbTarget;
using battle::TargetingVolume;
using battle::TargetingVolumeSlot;
using battle::TargetPoint;

// The area shape byte keeps the shape in its low bits and the angle above.
constexpr int kAreaShapeMask = 7;
constexpr int kAreaAngleShift = 3;
// The text box the target list describes its targets in.
constexpr int kInformationTextBox = 7;
// The frame of the "no targets" message.
constexpr int kNoTargetsFrameShape = 1;
// Battle_CollectEnvironmentActionTargets collected moving platform points into
// a stack buffer of this many points.
constexpr std::size_t kPlatformPointLimit = 4;
// The sphere's area preview list (target_lists[1]).
constexpr int kAreaTargetList = 1;
// The halfwords of a point and the extent bytes of a volume.
constexpr std::size_t kPointHalfwords = 4;
constexpr std::size_t kVolumeExtentCount = 4;
constexpr std::size_t kSourcePlatformBytes = 184;

using HalfwordPair = std::array<std::int16_t, 2>;

static_assert(sizeof(VS::Field::MpdMovingPlatform) == kSourcePlatformBytes);
// The native action state holds every preview and hit target the typed
// collectors can produce; the PS1 0x2C10 allocation is not a limit here.
static_assert(std::tuple_size_v<decltype(ActionState::previews)> ==
              static_cast<std::size_t>(battle::kTargetPreviewCapacity));
static_assert(std::tuple_size_v<decltype(ActionRequestRecord::targets)> ==
              static_cast<std::size_t>(battle::kSelectionTargetCapacity));
// The sphere's area previews are read as label lines by its highlights.
static_assert(sizeof(VS::Battle::TargetPreview) == sizeof(TargetLabelLine));
static_assert(offsetof(VS::Battle::TargetPreview, actor_id) ==
              offsetof(TargetLabelLine, opaque9));
static_assert(offsetof(VS::Battle::TargetPreview, limb) ==
              offsetof(TargetLabelLine, opaque_a));
// A sphere spec is a targeting volume in the action state's byte order.
static_assert(sizeof(VS::Battle::TargetSphereSpec) == sizeof(TargetVolume));
// The label and text views share the sphere allocation.
static_assert(offsetof(VS::Battle::TargetSphereRecord, labels) ==
              offsetof(TargetSphereLabelView, opaque990));
static_assert(offsetof(TargetSphereLabelView, text_buffer) +
                  sizeof(TargetSphereLabelView::text_buffer) <=
              sizeof(VS::Battle::TargetSphereSetupView));
static_assert(sizeof(VS::Battle::TargetSphereRecord) <=
              sizeof(VS::Battle::TargetSphereSetupView));
// The sphere's highlight tint word is a colour code; its cursor word holds x
// and y.
static_assert(sizeof(VS::Render::ColorCode) ==
              sizeof(VS::Battle::TargetSphereRecord::highlight_tint));
static_assert(sizeof(HalfwordPair) ==
              sizeof(VS::Battle::TargetSphereRecord::cursor_xy));
static_assert(sizeof(HalfwordPair) == sizeof(VS::Battle::Actor::command_arg2));
static_assert(sizeof(SVECTOR) == sizeof(VS::Battle::HitTargetWord));

template <typename Array>
bool InRange(const Array& array, int index) {
  return index >= 0 && static_cast<std::size_t>(index) < std::size(array);
}

int Byte(char value) { return static_cast<std::uint8_t>(value); }

TargetPoint ToPoint(const SVECTOR& vector) {
  return {vector.vx, vector.vy, vector.vz, vector.pad};
}

SVECTOR ToSvector(TargetPoint point) {
  return {point.x, point.y, point.z, point.pad};
}

TargetPoint PositionOf(const TargetVolume& volume) {
  return {volume.position[0], volume.position[1], volume.position[2],
          volume.position[kPointHalfwords - 1]};
}

VS::Battle::Actor* LiveActor(int actor) {
  return InRange(VS::Battle::actors, actor) ? VS::Battle::actors[actor]
                                            : nullptr;
}

VS::Battle::TargetSphereRecord* LiveSphere() {
  return reinterpret_cast<VS::Battle::TargetSphereRecord*>(
      VS::Battle::targetSphere);
}

ActionTargetPreview ReadPreview(const VS::Battle::TargetPreview& raw) {
  return {.position = ToPoint(raw.position),
          .kind = raw.kind,
          .actor_id = raw.actor_id,
          .limb = raw.limb,
          .effect = static_cast<std::uint8_t>(raw.effect),
          .preview_amount = static_cast<std::uint16_t>(raw.preview_amount),
          .hit_chance = raw.hit_chance,
          .removed_statuses = static_cast<std::uint32_t>(raw.removed_statuses),
          .action_kind = static_cast<std::uint8_t>(raw.action_kind),
          .damage_type = static_cast<std::uint8_t>(raw.opaque15),
          .effect_variant = static_cast<std::uint8_t>(raw.opaque16),
          .enemy_class = static_cast<std::uint8_t>(raw.enemy_class)};
}

void WritePreview(VS::Battle::TargetPreview& raw,
                  const ActionTargetPreview& preview) {
  raw.position = ToSvector(preview.position);
  raw.kind = preview.kind;
  raw.actor_id = preview.actor_id;
  raw.limb = preview.limb;
  raw.effect = static_cast<char>(preview.effect);
  raw.preview_amount = static_cast<std::int16_t>(preview.preview_amount);
  raw.hit_chance = preview.hit_chance;
  raw.removed_statuses = static_cast<int>(preview.removed_statuses);
  raw.action_kind = static_cast<char>(preview.action_kind);
  raw.opaque15 = static_cast<char>(preview.damage_type);
  raw.opaque16 = static_cast<char>(preview.effect_variant);
  raw.enemy_class = static_cast<char>(preview.enemy_class);
}

// One targeting volume of the action state (rangeShape..rangeOrigin or
// areaShape..areaOrigin).
struct VolumeFields {
  std::int8_t& shape;
  std::int8_t& kind;
  std::int16_t& facing;
  std::int8_t& x;
  std::int8_t& y;
  std::int8_t& z;
  std::int8_t& angle;
  SVECTOR& origin;
};

VolumeFields FieldsOf(ActionState& state, TargetingVolumeSlot slot) {
  if (slot == TargetingVolumeSlot::kArea) {
    return {state.area_shape, state.area_kind,  state.area_facing,
            state.area_x,     state.area_y,     state.area_z,
            state.area_angle, state.area_origin};
  }
  return {state.range_shape, state.range_kind,  state.range_facing,
          state.range_x,     state.range_y,     state.range_z,
          state.range_angle, state.range_origin};
}

TargetingVolume ReadVolume(const VolumeFields& fields) {
  return {.shape = Byte(fields.shape),
          .kind = Byte(fields.kind),
          .facing = fields.facing,
          .x = Byte(fields.x),
          .y = Byte(fields.y),
          .z = Byte(fields.z),
          .angle = Byte(fields.angle),
          .origin = ToPoint(fields.origin)};
}

void WriteVolume(const VolumeFields& fields, const TargetingVolume& volume) {
  fields.shape = static_cast<char>(volume.shape);
  fields.kind = static_cast<char>(volume.kind);
  fields.facing = static_cast<std::int16_t>(volume.facing);
  fields.x = static_cast<char>(volume.x);
  fields.y = static_cast<char>(volume.y);
  fields.z = static_cast<char>(volume.z);
  fields.angle = static_cast<char>(volume.angle);
  fields.origin = ToSvector(volume.origin);
}

// The volume as VS::Battle::TargetVolume reads the action state's bytes.
TargetVolume RawVolume(const TargetingVolume& volume) {
  TargetVolume raw{};
  raw.kind = static_cast<std::uint8_t>(volume.shape);
  raw.opaque1 = static_cast<std::uint8_t>(volume.kind);
  raw.angle = static_cast<std::int16_t>(volume.facing);
  const std::array<std::uint8_t, kVolumeExtentCount> extents{
      static_cast<std::uint8_t>(volume.x), static_cast<std::uint8_t>(volume.y),
      static_cast<std::uint8_t>(volume.z),
      static_cast<std::uint8_t>(volume.angle)};
  std::ranges::copy(extents, std::begin(raw.scale.extents));
  const std::array<std::int16_t, kPointHalfwords> position{
      volume.origin.x, volume.origin.y, volume.origin.z, volume.origin.pad};
  std::ranges::copy(position, std::begin(raw.position));
  return raw;
}

VS::Battle::TargetSphereSpec SphereSpec(const TargetingVolume& volume) {
  return std::bit_cast<VS::Battle::TargetSphereSpec>(RawVolume(volume));
}

battle::RangeShape RangeOf(const VS::Battle::Ai::ActionRangeShape& shape) {
  return {static_cast<std::uint8_t>(shape.reserved0),
          static_cast<std::uint8_t>(shape.range_y),
          static_cast<std::uint8_t>(shape.range_z), shape.shape, shape.angle};
}

// The weapon range bytes at stats 0x38, which VS::Battle::Ai::ActionRangeShape
// views.
battle::RangeShape WeaponRangeOf(const VS::Battle::ActorStats& stats) {
  return {static_cast<int>(stats.current_range),
          static_cast<int>(stats.current_range_y),
          static_cast<int>(stats.current_range_z),
          static_cast<int>(stats.current_attack_shape),
          static_cast<int>(stats.current_attack_shape_angle)};
}

std::optional<battle::TargetingActionView> ActionView(int action_id) {
  if (!InRange(VS::Battle::actionDefinitions, action_id)) {
    VS::Core::LogError("battle.targeting",
                       "action {} is outside the action table", action_id);
    return std::nullopt;
  }
  const ActionDefinition& action = VS::Battle::actionDefinitions[action_id];
  return battle::TargetingActionView{
      .category = action.category,
      .effect = static_cast<int>(action.hits[0].effect_id),
      .returns_to_menu_on_cancel = action.returns_to_menu_on_cancel != 0,
      .range = {action.range_x, action.range_y, action.range_z, action.shape,
                action.angle},
      .area = {action.area_range_x, action.area_range_y, action.area_range_z,
               action.area_shape & kAreaShapeMask,
               action.area_shape >> kAreaAngleShift},
      .radial_area = action.ai_flags.radial_area != 0,
      .shaped_area = action.ai_flags.shaped_area != 0};
}

// decomp: BATTLE.PRG func_800E5698 (src/BATTLE/BATTLE.PRG/7CD68.c:476); a
// weapon-ranged action without the attacker's weapon range is refused as 0.
int ActionRangeSquared(int action_id,
                       std::optional<battle::RangeShape> weapon_range) {
  const std::optional<battle::TargetingActionView> action =
      ActionView(action_id);
  if (!action) {
    return 0;
  }
  if (action->range.x == battle::kWeaponRangeSentinel && !weapon_range) {
    VS::Core::LogError("battle.targeting",
                       "action {} range refused: no attacker weapon range",
                       action_id);
    return 0;
  }
  return battle::CalculateActionRangeSquared(
      *action, weapon_range.value_or(battle::RangeShape{}));
}

// The point the actor's two command words form (*(SVECTOR*)&commandArg1).
TargetPoint CommandPoint(const VS::Battle::Actor& actor) {
  const auto upper = std::bit_cast<HalfwordPair>(actor.command_arg2);
  return {static_cast<std::int16_t>(actor.command_arg1.halves[0]),
          static_cast<std::int16_t>(actor.command_arg1.halves[1]), upper[0],
          upper[1]};
}

battle::TargetingActor ActorView(const VS::Battle::Actor& actor,
                                 const VS::Battle::ActorStats& stats) {
  battle::TargetingActor view;
  view.kind = actor.kind;
  view.default_limb = static_cast<int>(stats.default_target_limb);
  view.enemy_class = static_cast<int>(stats.enemy_class);
  view.risk = stats.risk;
  view.weapon_range = {static_cast<int>(stats.current_range),
                       static_cast<int>(stats.current_range_y),
                       static_cast<int>(stats.current_range_z),
                       static_cast<int>(stats.current_attack_shape),
                       static_cast<int>(stats.current_attack_shape_angle)};
  view.command_actor = actor.command_arg1.halves[0];
  view.command_limb = actor.command_arg1.halves[1];
  view.command_point = CommandPoint(actor);
  view.command_target_byte = actor.command_arg1.bytes[0];
  view.command_limb_byte = actor.command_arg1.bytes[2];
  return view;
}

std::vector<battle::PlatformPoint> CollectPlatformPoints() {
  const auto bytes = VS::Field::roomData.header.moving_platforms_length;
  if (VS::Field::roomData.moving_platforms == nullptr) {
    if (bytes != 0) {
      VS::Core::LogError("battle.targeting",
                         "moving platform section is missing");
    }
    return {};
  }
  if (bytes % sizeof(VS::Field::MpdMovingPlatform) != 0) {
    VS::Core::LogError("battle.targeting",
                       "moving platform section is partial");
    return {};
  }
  const std::size_t capacity = bytes / sizeof(VS::Field::MpdMovingPlatform);
  if (capacity > std::size(VS::Field::platformBounceFrames)) {
    VS::Core::LogError("battle.targeting",
                       "{} moving platforms exceed the room", capacity);
    return {};
  }
  const auto contacts = VS::Field::CollectLiveMovingPlatformPositions();
  if (contacts.size() > capacity) {
    VS::Core::LogError("battle.targeting",
                       "moving platform collector returned {} of {}",
                       contacts.size(), capacity);
    return {};
  }
  std::size_t kept = contacts.size();
  if (kept > kPlatformPointLimit) {
    VS::Core::LogWarn("battle.targeting",
                      "{} moving platform points; the first {} are targets",
                      kept, kPlatformPointLimit);
    kept = kPlatformPointLimit;
  }
  std::vector<battle::PlatformPoint> points;
  points.reserve(kept);
  for (const VS::Field::PlatformContact& contact :
       std::span(contacts).first(kept)) {
    points.push_back({contact.x, contact.height, contact.z});
  }
  return points;
}

class LiveActionTargetingHost final : public battle::ActionTargetingHost {
 public:
  explicit LiveActionTargetingHost(ActionState& state) : state_(state) {}

  [[nodiscard]] battle::TargetingActionView Action(
      int action_id) const override {
    return ActionView(action_id).value_or(battle::TargetingActionView{});
  }

  [[nodiscard]] battle::TargetCategoryRules CategoryRules(
      int category) const override {
    if (!InRange(VS::Battle::categoryTargetActorMasks, category)) {
      VS::Core::LogError("battle.targeting", "target category {} is unknown",
                         category);
      return {};
    }
    return {.needs_target_selection =
                VS::Battle::categoryNeedsTargetSelection[category] != 0,
            .area_target = VS::Battle::categoryIsAreaTarget[category] != 0,
            .includes_acting_actor =
                VS::Battle::categoryIncludesActingActor[category] != 0,
            .targets_body_parts =
                VS::Battle::categoryTargetsBodyParts[category] != 0,
            .actor_mask = VS::Battle::categoryTargetActorMasks[category]};
  }

  [[nodiscard]] int SelectionActionId() const override {
    return state_.selection.action_id;
  }
  void SetSelectionActionId(int action_id) override {
    state_.selection.action_id = static_cast<std::uint16_t>(action_id);
  }
  void ResetSelection(int source_actor, int action_id) override {
    state_.selection = {};
    state_.selection.primary_target.targets_position = 0;
    state_.selection.primary_target.target.target_actor =
        static_cast<std::uint8_t>(source_actor);
    state_.selection.action_id = static_cast<std::uint16_t>(action_id);
  }
  [[nodiscard]] int PrimaryActor() const override {
    return state_.selection.primary_target.target.target_actor;
  }
  [[nodiscard]] bool PrimaryTargetsPosition() const override {
    return state_.selection.primary_target.targets_position != 0;
  }
  [[nodiscard]] int SelectionTargetCount() const override {
    return state_.selection.target_count;
  }
  void SetSelectionTargetCount(int count) override {
    state_.selection.target_count = static_cast<std::uint16_t>(count);
  }
  void SetSelectionActorTarget(int index, ActorLimbTarget target) override {
    VS::Battle::HitResult* hit = SelectionTarget(index);
    if (hit == nullptr) {
      return;
    }
    hit->targets_position = 0;
    hit->target.target_actor = static_cast<std::uint8_t>(target.actor);
    hit->target.target_limb = static_cast<std::int8_t>(target.limb);
  }
  void SetSelectionPositionTarget(int index, int mode,
                                  TargetPoint point) override {
    VS::Battle::HitResult* target = SelectionTarget(index);
    if (target == nullptr) {
      return;
    }
    target->targets_position = mode;
    target->target = std::bit_cast<VS::Battle::HitTargetWord>(ToSvector(point));
  }
  [[nodiscard]] TargetPoint FocusPosition() const override {
    return ToPoint(state_.selection.focus_position);
  }
  void SetFocusPosition(TargetPoint point) override {
    state_.selection.focus_position = ToSvector(point);
  }

  [[nodiscard]] bool IsPlayerAction() const override {
    return state_.is_player_action != 0;
  }
  void SetIsPlayerAction(bool player_action) override {
    state_.is_player_action = player_action ? 1 : 0;
  }
  void SetSourceActorId(int source) override {
    state_.source_actor_id = source;
  }
  [[nodiscard]] int SelectedPreview() const override {
    return state_.selected_preview;
  }
  void SetSelectedPreview(int index) override {
    state_.selected_preview = static_cast<std::uint8_t>(index);
  }
  void SetCameraPreview(int index) override {
    state_.camera_preview = static_cast<std::uint8_t>(index);
  }
  void ResetSelectionTimers() override {
    state_.selection_frames = 0;
    state_.chain_step = 0;
  }
  [[nodiscard]] TargetPoint AreaCenter() const override {
    return ToPoint(state_.area_center);
  }
  void SetAreaCenter(TargetPoint point) override {
    state_.area_center = ToSvector(point);
  }
  [[nodiscard]] TargetPoint SavedAreaCenter() const override {
    return ToPoint(state_.saved_area_center);
  }
  void SetSavedAreaCenter(TargetPoint point) override {
    state_.saved_area_center = ToSvector(point);
  }
  [[nodiscard]] TargetingVolume Volume(
      TargetingVolumeSlot slot) const override {
    return ReadVolume(FieldsOf(state_, slot));
  }
  void SetVolume(TargetingVolumeSlot slot,
                 const TargetingVolume& volume) override {
    WriteVolume(FieldsOf(state_, slot), volume);
  }
  [[nodiscard]] ActionTargetPreview Preview(int index) const override {
    if (!InRange(state_.previews, index)) {
      VS::Core::LogError("battle.targeting",
                         "preview {} is outside the previews", index);
      return {};
    }
    return ReadPreview(state_.previews[index]);
  }
  void SetPreview(int index, const ActionTargetPreview& preview) override {
    if (!InRange(state_.previews, index)) {
      VS::Core::LogError("battle.targeting",
                         "preview {} is outside the previews", index);
      return;
    }
    WritePreview(state_.previews[index], preview);
  }
  [[nodiscard]] int PreviewCount() const override {
    return state_.preview_count;
  }
  void SetPreviewCount(int count) override {
    state_.preview_count = static_cast<std::uint16_t>(count);
  }
  [[nodiscard]] bool IsPointInVolume(TargetingVolumeSlot slot,
                                     TargetPoint point) const override {
    TargetVolume volume = RawVolume(Volume(slot));
    std::array<std::uint16_t, kPointHalfwords> components{
        static_cast<std::uint16_t>(point.x),
        static_cast<std::uint16_t>(point.y),
        static_cast<std::uint16_t>(point.z),
        static_cast<std::uint16_t>(point.pad)};
    return VS::Battle::IsLivePointInTargetVolume(&volume, components) != 0;
  }
  [[nodiscard]] battle::ActionPreviewResult PreviewActionAgainstLimb(
      int action_id, int source_actor, int target_actor, int limb) override {
    ActionRequestRecord result{};
    VS::Battle::PreviewLiveActionAgainstLimb(action_id, source_actor,
                                             target_actor, limb, &result);
    const VS::Battle::HitResult& hit = result.targets[0];
    return {.action_id = result.action_id,
            .damage_type = result.damage_type,
            .effect_variant = result.effect_variant,
            .target_actor = hit.target.target_actor,
            .target_hp = hit.target.hp,
            .target_mp = hit.target.mp,
            .hit_threshold = hit.target.hit_threshold,
            .remove_statuses = static_cast<std::uint32_t>(hit.remove_statuses)};
  }

  [[nodiscard]] std::optional<battle::TargetingActor> Actor(
      int slot) const override {
    const VS::Battle::Actor* actor = LiveActor(slot);
    if (actor == nullptr || actor->stats == nullptr) {
      return std::nullopt;
    }
    return ActorView(*actor, *actor->stats);
  }
  [[nodiscard]] std::uint32_t ActorStateFlags(int slot) const override {
    return VS::Actors::LiveActorStateFlags(slot);
  }
  void SetActorDisplayFlags(int slot, std::uint32_t flags) override {
    VS::Actors::SetLiveActorDisplayFlags(slot, static_cast<int>(flags));
  }
  [[nodiscard]] std::optional<TargetPoint> ActorPlacement(
      int slot) const override {
    VS::Actors::ActorLocation location{};
    if (VS::Actors::LiveActorPlacement(slot, &location) < 0) {
      return std::nullopt;
    }
    return ToPoint(location.position);
  }
  [[nodiscard]] int FindBoneTag(int slot, int tag, int kind) const override {
    return VS::Actors::FindLiveActorBoneTag(slot, tag, kind);
  }
  [[nodiscard]] int BoneTag(int slot, int bone, int kind) const override {
    return VS::Actors::LiveActorBoneTag(slot, bone, kind);
  }
  [[nodiscard]] TargetPoint LowGroupBonePosition(
      int slot, int limb, TargetPoint current) const override {
    SVECTOR position = ToSvector(current);
    VS::Actors::LiveActorLowGroupBonePosition(slot, limb, &position, 0);
    return ToPoint(position);
  }
  [[nodiscard]] TargetPoint TaggedBonePosition(
      int slot, int tag, TargetPoint current) const override {
    SVECTOR position = ToSvector(current);
    VS::Actors::LiveActorTaggedBonePosition(slot, tag, &position, 0);
    return ToPoint(position);
  }
  [[nodiscard]] TargetPoint BonePosition(int slot, int bone,
                                         TargetPoint current) const override {
    SVECTOR position = ToSvector(current);
    VS::Actors::LiveActorBonePosition(slot, bone, &position, 0);
    return ToPoint(position);
  }
  void RestoreIdlePose(int slot) override {
    VS::Actors::RestoreLiveActorIdlePose(slot);
  }
  void FreezeActorAnimations() override {
    VS::Actors::FreezeLiveActorAnimations();
  }

  [[nodiscard]] std::vector<battle::EnvironmentTrigger> RoomTriggers()
      const override {
    const auto triggers = VS::Field::LiveActiveRoomTriggerInfo();
    std::vector<battle::EnvironmentTrigger> result;
    result.reserve(triggers.size());
    for (const auto& trigger : triggers) {
      result.push_back({.kind = trigger.kind,
                        .tile_x = trigger.tile_x,
                        .tile_z = trigger.tile_z});
    }
    return result;
  }
  [[nodiscard]] std::vector<battle::PlatformPoint> MovingPlatformPoints()
      const override {
    return CollectPlatformPoints();
  }
  [[nodiscard]] std::vector<battle::TargetChest> RoomChests() const override {
    int count = 0;
    const VS::Items::TreasureChest* chests =
        VS::Items::LiveRoomTreasureChests(&count);
    if (count <= 0) {
      return {};
    }
    if (chests == nullptr) {
      VS::Core::LogError("battle.targeting", "{} room chests without a table",
                         count);
      return {};
    }
    std::vector<battle::TargetChest> result;
    result.reserve(static_cast<std::size_t>(count));
    for (const VS::Items::TreasureChest& chest :
         std::span(chests, static_cast<std::size_t>(count))) {
      result.push_back({.tile_x = chest.tile_x,
                        .tile_z = chest.tile_z,
                        .opened_flag = chest.opened_flag,
                        .lock_level = chest.lock_level,
                        .is_open = chest.is_open != 0});
    }
    return result;
  }
  [[nodiscard]] int FloorHeightAt(int x, int z) const override {
    return VS::Field::LiveFloorHeightAt(x, z);
  }

  void InitSphere() override { VS::Battle::InitializeSphereEntry(); }
  void FreeSphere() override { VS::Battle::FreeSphereEntry(); }
  void ClearSphereTargets() override { VS::Battle::ClearSphereTargetsEntry(); }
  void ClearPrimarySphereVolume() override {
    VS::Battle::ClearPrimarySphereVolumeEntry();
  }
  void ClearSecondarySphereVolume() override {
    VS::Battle::ClearSecondarySphereVolumeEntry();
  }
  void ShowRangeVolume(bool with_area, int preview_count) override {
    if (!SphereReady("range volume")) {
      return;
    }
    VS::Battle::TargetSphereSpec range =
        SphereSpec(Volume(TargetingVolumeSlot::kRange));
    if (!with_area) {
      VS::Battle::SetSphereVolumesEntry(&range, nullptr, 0, nullptr);
      return;
    }
    VS::Battle::TargetSphereSpec area =
        SphereSpec(Volume(TargetingVolumeSlot::kArea));
    VS::Battle::SetSphereVolumesEntry(&range, &area,
                                      static_cast<std::uint8_t>(preview_count),
                                      state_.previews.data());
  }
  void ShowAreaVolume() override {
    if (!SphereReady("area volume")) {
      return;
    }
    VS::Battle::TargetSphereSpec area =
        SphereSpec(Volume(TargetingVolumeSlot::kArea));
    VS::Battle::SetSecondarySphereVolumeEntry(&area);
  }
  void ShowSphereTargets(int preview_count, int mode) override {
    battle::SetLiveSphereTargets(
        battle::CheckedTargetPreviews(state_.previews, preview_count), mode);
  }

  void SetFieldControlMode(int mode) override {
    VS::Field::controlMode = static_cast<std::uint32_t>(mode);
  }
  void SetActionFlowMode(int mode) override {
    VS::Battle::actionFlowMode = static_cast<std::uint32_t>(mode);
  }
  [[nodiscard]] bool WeaponDrawn() const override {
    if (VS::Battle::characterState == nullptr) {
      VS::Core::LogError("battle.targeting",
                         "no character state; weapon sheathed");
      return false;
    }
    return (VS::Battle::characterState->weapon_drawn & 1) != 0;
  }
  void SetHudBattleMode(int mode) override {
    VS::Battle::SetLiveHudBattleMode(mode);
  }
  void ResumeAfterActionSelection() override {
    VS::Menu::Framework::ResumeLiveAfterActionSelection();
  }
  void PauseFieldForMenu() override { VS::Field::LivePauseFieldForMenu(); }
  void ReleaseMenuRows() override {
    VS::Menu::Framework::ReleaseLiveMenuRows();
  }
  void BeginPostActionRecovery(int mode) override {
    VS::Battle::BeginLivePostActionRecovery(mode);
  }
  void CommitEnvironmentAction(int /*mode*/) override {
    // func_800719DC ignored the caller's menu mode and selected environment.
    VS::Battle::CommitLiveSelectedAction(0);
  }
  void StorePreferredCameraDistance() override {
    VS::Field::LiveStorePreferredActionCameraDistance();
  }
  void ResetRiskPenaltyTimer() override {
    VS::Battle::ResetLiveTargetingRiskPenaltyTimer();
  }
  void TransformRoomPalettes(
      const battle::RoomPaletteTransform& transform) override {
    VS::Render::TransformLiveRoomPalettes(
        transform.mode, transform.duration, transform.red,
        static_cast<std::int16_t>(transform.green),
        static_cast<std::int16_t>(transform.blue));
  }
  void SetMinimapVisible(bool visible) override {
    VS::Field::SetLiveMinimapVisible(visible ? 1 : 0);
  }
  void SetRoomAnimationsPaused(bool paused) override {
    VS::Field::LiveSetRoomAnimationsPaused(paused ? 1 : 0);
  }
  void SetMovingPlatformsPaused(bool paused) override {
    VS::Field::SetMovingPlatformsPausedEntry(paused ? 1 : 0);
  }
  void SetActorMarkersHidden(bool hidden) override {
    VS::Battle::SetLiveActorMarkersHidden(hidden ? 1 : 0);
  }
  void SetClutAnimationEnabled(bool enabled) override {
    VS::Render::SetLiveClutAnimationEnabled(enabled ? 1 : 0);
  }

 private:
  [[nodiscard]] VS::Battle::HitResult* SelectionTarget(int index) const {
    if (!InRange(state_.selection.targets, index)) {
      VS::Core::LogError("battle.targeting",
                         "hit target {} is outside the request", index);
      return nullptr;
    }
    return &state_.selection.targets[index];
  }

  [[nodiscard]] static bool SphereReady(std::string_view volume) {
    if (VS::Battle::targetSphere == nullptr) {
      VS::Core::LogError("battle.targeting", "{} refused: no targeting sphere",
                         volume);
      return false;
    }
    return true;
  }

  ActionState& state_;
};

// One column of the target list (D_800F4C70 and its group tables).
struct ColumnStorage {
  std::span<std::uint8_t, battle::kTargetColumnCapacity> entries;
  std::span<std::uint8_t, battle::kTargetColumnGroupCapacity> groups;
  std::span<std::uint8_t, battle::kTargetColumnGroupCapacity>
      group_first_targets;
  std::uint8_t& entry_count;
  std::uint8_t& group_count;
};

ColumnStorage LiveColumn(int column) {
  if (column == 0) {
    return {VS::Battle::targetColumnEntries[0], VS::Battle::firstColumnGroups,
            VS::Battle::firstColumnGroupFirstTargets,
            VS::Battle::firstColumnEntryCount,
            VS::Battle::firstColumnGroupCount};
  }
  return {VS::Battle::targetColumnEntries[1], VS::Battle::secondColumnGroups,
          VS::Battle::secondColumnGroupFirstTargets,
          VS::Battle::secondColumnEntryCount,
          VS::Battle::secondColumnGroupCount};
}

battle::TargetColumn ReadColumn(const ColumnStorage& storage) {
  battle::TargetColumn column;
  std::ranges::copy(storage.entries, column.entries.begin());
  std::ranges::copy(storage.groups, column.groups.begin());
  std::ranges::copy(storage.group_first_targets,
                    column.group_first_targets.begin());
  column.entry_count = storage.entry_count;
  column.group_count = storage.group_count;
  return column;
}

std::uint8_t CheckedCount(int count, int capacity, std::string_view what) {
  if (count < 0 || count > capacity) {
    VS::Core::LogError("battle.targeting", "{} count {} exceeds {}; clamped",
                       what, count, capacity);
  }
  return static_cast<std::uint8_t>(std::clamp(count, 0, capacity));
}

void WriteColumn(const ColumnStorage& storage,
                 const battle::TargetColumn& column) {
  std::ranges::copy(column.entries, storage.entries.begin());
  std::ranges::copy(column.groups, storage.groups.begin());
  std::ranges::copy(column.group_first_targets,
                    storage.group_first_targets.begin());
  storage.entry_count = CheckedCount(
      column.entry_count, battle::kTargetColumnCapacity, "column entry");
  storage.group_count = CheckedCount(
      column.group_count, battle::kTargetColumnGroupCapacity, "column group");
}

char* StatusString(int offset) {
  return reinterpret_cast<char*>(&VS::Battle::statusStrings[offset]);
}

// The string a target text names; an unknown source falls back to the empty
// status string.
char* LiveTargetText(battle::TargetText text) {
  using battle::TargetTextSource;
  switch (text.source) {
    case TargetTextSource::kStatusIndex:
      if (InRange(VS::Battle::statusStrings, text.value) &&
          InRange(VS::Battle::statusStrings,
                  VS::Battle::statusStrings[text.value])) {
        return StatusString(VS::Battle::statusStrings[text.value]);
      }
      break;
    case TargetTextSource::kStatusOffset:
      if (InRange(VS::Battle::statusStrings, text.value)) {
        return StatusString(text.value);
      }
      break;
    case TargetTextSource::kActorName:
      if (VS::Battle::Actor* actor = LiveActor(text.value);
          actor != nullptr && actor->stats != nullptr) {
        return actor->stats->name.data();
      }
      break;
    case TargetTextSource::kEmptyStatus:
      return VS::Battle::emptyStatusString.data();
    case TargetTextSource::kStatusLineTemplate:
      return VS::Battle::targetStatusLineTemplate.data();
  }
  VS::Core::LogError("battle.targeting",
                     "target text {} of source {} is unknown", text.value,
                     static_cast<int>(text.source));
  return VS::Battle::emptyStatusString.data();
}

class LiveTargetSelectionHost final : public battle::TargetSelectionHost {
 public:
  explicit LiveTargetSelectionHost(VS::Battle::TargetSphereRecord& sphere)
      : sphere_(sphere) {}

  [[nodiscard]] battle::SphereVolume Volume(int index) const override {
    if (!InRange(sphere_.volumes, index)) {
      VS::Core::LogError("battle.targeting", "sphere volume {} is unknown",
                         index);
      return {};
    }
    const TargetVolume& volume = sphere_.volumes[index];
    return {.kind = volume.kind,
            .range_kind = volume.opaque1,
            .angle = volume.angle,
            .extents = volume.scale.extents,
            .position = PositionOf(volume)};
  }
  void SetVolume(int index, const battle::SphereVolume& volume) override {
    if (!InRange(sphere_.volumes, index)) {
      VS::Core::LogError("battle.targeting", "sphere volume {} is unknown",
                         index);
      return;
    }
    TargetVolume& raw = sphere_.volumes[index];
    raw.kind = static_cast<std::uint8_t>(volume.kind);
    raw.opaque1 = static_cast<std::uint8_t>(volume.range_kind);
    raw.angle = static_cast<std::int16_t>(volume.angle);
    std::ranges::copy(volume.extents, std::begin(raw.scale.extents));
    const std::array<std::int16_t, kPointHalfwords> position{
        volume.position.x, volume.position.y, volume.position.z,
        volume.position.pad};
    std::ranges::copy(position, std::begin(raw.position));
  }
  [[nodiscard]] battle::SphereCursor Cursor() const override {
    const auto across = std::bit_cast<HalfwordPair>(sphere_.cursor_xy);
    return {across[0], across[1], sphere_.cursor_z, sphere_.input_locked};
  }
  void SetCursor(const battle::SphereCursor& cursor) override {
    sphere_.cursor_xy =
        std::bit_cast<int>(HalfwordPair{static_cast<std::int16_t>(cursor.x),
                                        static_cast<std::int16_t>(cursor.y)});
    sphere_.cursor_z = static_cast<std::int16_t>(cursor.z);
    sphere_.input_locked = static_cast<std::int16_t>(cursor.locked);
  }
  [[nodiscard]] int WireShape() const override { return sphere_.wire_shape; }
  [[nodiscard]] int CameraYaw() const override { return sphere_.camera_yaw; }
  [[nodiscard]] int TargetCount() const override {
    return Byte(sphere_.target_count);
  }
  [[nodiscard]] int SecondaryTargetCount() const override {
    return Byte(sphere_.sphere_mode);
  }
  [[nodiscard]] int SelectedTarget() const override {
    return Byte(sphere_.selected_target);
  }
  void SetSelectedTarget(int index) override {
    sphere_.selected_target = static_cast<char>(index);
  }
  [[nodiscard]] bool AllTargetsMode() const override {
    return sphere_.target_mode != 0;
  }
  [[nodiscard]] battle::HighlightedTarget Highlighted() const override {
    return {Byte(sphere_.highlighted_group),
            static_cast<std::int8_t>(sphere_.highlighted_pulse_slot)};
  }
  void SetHighlighted(battle::HighlightedTarget target) override {
    sphere_.highlighted_group = static_cast<char>(target.group);
    sphere_.highlighted_pulse_slot = static_cast<char>(target.pulse_slot);
  }
  void SetHighlightTint(int rgb) override { sphere_.highlight_tint = rgb; }
  [[nodiscard]] ActionTargetPreview SphereTarget(int list,
                                                 int index) const override {
    if (!InRange(sphere_.target_lists, list) ||
        sphere_.target_lists[list] == nullptr || index < 0 ||
        index >= battle::kTargetPreviewCapacity) {
      VS::Core::LogError("battle.targeting",
                         "sphere target {} of list {} is absent", index, list);
      return {};
    }
    const std::span targets(
        sphere_.target_lists[list],
        static_cast<std::size_t>(battle::kTargetPreviewCapacity));
    return ReadPreview(targets[static_cast<std::size_t>(index)]);
  }
  void BuildCameraBasis() override {
    static_cast<void>(VS::Battle::LiveBuildSphereCameraBasis());
  }
  [[nodiscard]] battle::SphereBasis BuildVolumeTransform() override {
    TargetVolume& volume = sphere_.volumes[0];
    const MATRIX* matrix = VS::Battle::BuildSphereVolumeTransformEntry(
        volume.scale.extents.data(), volume.angle);
    VS::Render::LoadLiveFrameViewMatrix();
    if (matrix == nullptr) {
      VS::Core::LogError("battle.targeting",
                         "sphere volume transform is missing");
      transform_.reset();
      return {};
    }
    transform_ = *matrix;
    return {matrix->m[0][0], matrix->m[0][2], matrix->m[2][0], matrix->m[2][2]};
  }
  void CacheTerrainRow(int row) override {
    SVECTOR position = ToSvector(PositionOf(sphere_.volumes[0]));
    VS::Battle::LiveCacheSphereTerrainRow(&position,
                                          static_cast<std::uint32_t>(row));
  }
  void UpdateTargetHighlights() override {
    const int count = Byte(sphere_.sphere_mode);
    VS::Battle::TargetPreview* list = sphere_.target_lists[kAreaTargetList];
    if (list == nullptr && count > 0) {
      VS::Core::LogError("battle.targeting", "{} area targets without a list",
                         count);
      return;
    }
    VS::Battle::UpdateSphereTargetHighlightsEntry(
        reinterpret_cast<TargetLabelLine*>(list), count);
  }
  void DrawTerrainIntersection(TargetPoint point, int scale) override {
    if (!transform_) {
      VS::Core::LogError("battle.targeting",
                         "terrain intersection refused: no volume transform");
      return;
    }
    VS::Battle::DrawLiveSphereTerrainIntersection(ToSvector(point), *transform_,
                                                  scale);
  }
  [[nodiscard]] int CollisionMapDimensions() const override {
    return VS::Field::LiveCollisionMapDimensions(0);
  }
  void DrawPositionCursor(TargetPoint point, bool lock_pitch) override {
    SVECTOR position = ToSvector(point);
    VS::Battle::LiveDrawSpherePositionCursor(&position, lock_pitch ? 1 : 0);
  }
  void ClearPrimarySphereVolume() override {
    VS::Battle::ClearPrimarySphereVolumeEntry();
  }
  void SetReuseSphereVolumes(bool reuse) override {
    VS::Battle::reuseSphereVolumes = reuse ? 1 : 0;
  }

  void DisablePalettePulse(int actor, int pulse_slot) override {
    VS::Actors::DisableLiveModelPalettePulse(actor, pulse_slot);
  }
  void EnablePalettePulse(int actor, int pulse_flags, int pulse_slot) override {
    VS::Actors::EnableLiveModelPalettePulse(actor, pulse_flags, pulse_slot);
  }
  void SetActorTint(int actor, bool tinted) override {
    if (!tinted) {
      VS::Actors::SetLiveActorTint(actor, nullptr);
      return;
    }
    const auto tint =
        std::bit_cast<VS::Render::ColorCode>(sphere_.highlight_tint);
    VS::Actors::SetLiveActorTint(actor, &tint);
  }
  [[nodiscard]] std::optional<int> ActorKind(int actor) const override {
    const VS::Battle::Actor* live = LiveActor(actor);
    if (live == nullptr) {
      return std::nullopt;
    }
    return live->kind;
  }

  [[nodiscard]] std::uint32_t ButtonsHeld() const override {
    return static_cast<std::uint32_t>(VS::Input::buttonsState);
  }
  [[nodiscard]] std::uint32_t ButtonsPressed() const override {
    return VS::Input::buttonsPressed;
  }
  [[nodiscard]] std::uint32_t ButtonsRepeated() const override {
    return static_cast<std::uint32_t>(VS::Input::buttonRepeat);
  }
  [[nodiscard]] battle::StickInput RightStick() const override {
    return {VS::Input::MapStickDeadZone(VS::Input::stickPosBuf.r_stick_x),
            VS::Input::MapStickDeadZone(VS::Input::stickPosBuf.r_stick_y)};
  }
  void PlayLeaveSound() override { VS::Menu::PlayLeaveSound(); }
  void PlaySelectSound() override { VS::Menu::PlaySelectSound(); }
  void PlayChangeSound() override { VS::Menu::PlayChangeSound(); }

  [[nodiscard]] int SelectionStep() const override {
    return VS::Battle::targetSelectionStep;
  }
  void SetSelectionStep(int step) override {
    VS::Battle::targetSelectionStep = static_cast<std::uint8_t>(step);
  }
  [[nodiscard]] battle::TargetColumns Columns() const override {
    battle::TargetColumns columns;
    columns.columns[0] = ReadColumn(LiveColumn(0));
    columns.columns[1] = ReadColumn(LiveColumn(1));
    columns.top_offset = VS::Battle::targetColumnTopOffset;
    return columns;
  }
  void SetColumns(const battle::TargetColumns& columns) override {
    WriteColumn(LiveColumn(0), columns.columns[0]);
    WriteColumn(LiveColumn(1), columns.columns[1]);
    VS::Battle::targetColumnTopOffset =
        static_cast<std::uint8_t>(columns.top_offset);
  }
  [[nodiscard]] battle::TargetSelectionLabel Label(int index) const override {
    if (!InRange(sphere_.labels, index)) {
      VS::Core::LogError("battle.targeting", "target label {} is unknown",
                         index);
      return {};
    }
    const VS::Battle::SphereTargetLabelRecord& label = sphere_.labels[index];
    return {.state = Byte(label.state),
            .depth = Byte(label.depth),
            .gradient = Byte(label.gradient),
            .ring_size = Byte(label.ring_size),
            .screen_x = label.screen_x,
            .offset_y = label.offset_y,
            .fade = Byte(label.fade),
            .world_position = ToPoint(label.world_position)};
  }
  void SetLabel(int index, const battle::TargetSelectionLabel& label) override {
    if (!InRange(sphere_.labels, index)) {
      VS::Core::LogError("battle.targeting", "target label {} is unknown",
                         index);
      return;
    }
    VS::Battle::SphereTargetLabelRecord& raw = sphere_.labels[index];
    raw.state = static_cast<char>(label.state);
    raw.depth = static_cast<char>(label.depth);
    raw.gradient = static_cast<char>(label.gradient);
    raw.ring_size = static_cast<char>(label.ring_size);
    raw.screen_x = static_cast<std::int16_t>(label.screen_x);
    raw.offset_y = static_cast<std::uint8_t>(label.offset_y);
    raw.fade = static_cast<char>(label.fade);
    raw.world_position = ToSvector(label.world_position);
  }
  void SetLabelText(int index, battle::TargetLabelText text) override {
    if (!InRange(sphere_.labels, index) ||
        !InRange(VS::Battle::limbNames, text.limb_name) ||
        VS::Battle::limbNames[text.limb_name] == nullptr) {
      VS::Core::LogError("battle.targeting", "label {} text {} is unknown",
                         index, text.limb_name);
      return;
    }
    const std::span name(VS::Battle::limbNames[text.limb_name], 2);
    sphere_.labels[index].text =
        text.skip_first_character ? &name[1] : &name[0];
  }
  [[nodiscard]] bool LabelsSettled() const override {
    return VS::Battle::AreTargetLabelsSettledEntry() != 0;
  }
  [[nodiscard]] bool HandlePageButtons() override {
    return VS::Battle::HandleTargetPageButtons() != 0;
  }
  [[nodiscard]] int MapPageIndex(int index) const override {
    return VS::Battle::MapTargetPageIndex(static_cast<std::uint8_t>(index));
  }

  void InitInformationBox() override {
    VS::Menu::InitLiveInformationTextBox(VS::Menu::InformationHeader::kNone);
  }
  void ShowNoTargetsMessage() override {
    const int offset = battle::doorStrings[0];
    if (!InRange(battle::doorStrings, offset)) {
      VS::Core::LogError("battle.targeting", "no-targets text {} is unknown",
                         offset);
      return;
    }
    VS::Menu::textBoxes[kInformationTextBox].flags.frame_shape =
        kNoTargetsFrameShape;
    VS::Menu::SetLiveTextBoxText(
        kInformationTextBox,
        reinterpret_cast<char*>(&battle::doorStrings[offset]));
  }
  void SetContextText(int slot, battle::TargetText text) override {
    if (!InRange(VS::Battle::stringContext.strings, slot)) {
      VS::Core::LogError("battle.targeting", "string slot {} is unknown", slot);
      return;
    }
    VS::Battle::stringContext.strings[slot] = LiveTargetText(text);
  }
  void SetContextInteger(int slot, int value) override {
    if (!InRange(VS::Battle::stringContext.integers, slot)) {
      VS::Core::LogError("battle.targeting", "integer slot {} is unknown",
                         slot);
      return;
    }
    VS::Battle::stringContext.integers[slot] = value;
  }
  void ShowTargetInfo() override {
    auto* view = reinterpret_cast<TargetSphereLabelView*>(&sphere_);
    VS::Render::FormatLiveEncodedText(view->text_buffer,
                                      VS::Battle::targetInfoTemplate);
    VS::Menu::SetLiveTextBoxText(kInformationTextBox, view->text_buffer.data());
  }
  void DismissInformationBox() override {
    VS::Menu::DismissLiveTextBox(kInformationTextBox);
  }

 private:
  VS::Battle::TargetSphereRecord& sphere_;
  // The volume transform built this frame (Battle_BuildSphereVolumeTransform
  // returns scratch memory).
  std::optional<MATRIX> transform_;
};

template <typename Operation>
void WithActionHost(std::string_view operation, Operation&& run) {
  if (VS::Battle::actionState == nullptr) {
    VS::Core::LogError("battle.targeting", "{} refused: no action state",
                       operation);
    return;
  }
  LiveActionTargetingHost host(*VS::Battle::actionState);
  std::forward<Operation>(run)(host);
}

VS::Battle::TargetSphereRecord* SphereFor(std::string_view operation) {
  VS::Battle::TargetSphereRecord* sphere = LiveSphere();
  if (sphere == nullptr) {
    VS::Core::LogError("battle.targeting", "{} refused: no targeting sphere",
                       operation);
  }
  return sphere;
}
}  // namespace

namespace VS::Battle {
void LiveReturnToBattleActionMenu() {
  WithActionHost("return to the action menu", [](ActionTargetingHost& host) {
    ReturnToBattleActionMenu(host);
  });
}

void LiveFilterAreaActionTargets() {
  WithActionHost("area target filter", [](ActionTargetingHost& host) {
    FilterAreaActionTargets(host);
  });
}

void LiveSetSingleActionTarget() {
  WithActionHost("single target", [](ActionTargetingHost& host) {
    SetSingleActionTarget(host);
  });
}

void LiveSetAllActionTargets() {
  WithActionHost("all targets",
                 [](ActionTargetingHost& host) { SetAllActionTargets(host); });
}

void LiveCollectEnvironmentActionTargets() {
  WithActionHost("environment targets", [](ActionTargetingHost& host) {
    CollectEnvironmentActionTargets(host);
  });
}

void LiveInitializeActionTargeting(int action_id) {
  WithActionHost("action targeting", [action_id](ActionTargetingHost& host) {
    InitializeActionTargeting(host, action_id);
  });
}

void LiveBeginActorActionSelection(int source_actor_slot,
                                   std::uint16_t action_id) {
  VS::Core::LogDebug("battle.targeting", "action selection: actor {} action {}",
                     source_actor_slot, action_id);
  WithActionHost("action selection", [&](ActionTargetingHost& host) {
    BeginActorActionSelection(host, source_actor_slot, action_id);
  });
}

int LiveUpdateSphereTargetPosition(SVECTOR* target_position) {
  if (target_position == nullptr) {
    VS::Core::LogError("battle.targeting", "sphere cursor refused: no target");
    return 0;
  }
  VS::Battle::TargetSphereRecord* sphere = SphereFor("sphere cursor");
  if (sphere == nullptr) {
    return 0;
  }
  LiveTargetSelectionHost host(*sphere);
  TargetPoint target = ToPoint(*target_position);
  const bool done = UpdateSphereTargetPosition(host, target);
  *target_position = ToSvector(target);
  return done ? 1 : 0;
}

int LiveUpdateTargetSelectionMenu(std::uint8_t* target) {
  if (target == nullptr) {
    // A null target resets the list; only the selection step changes.
    VS::Battle::targetSelectionStep = 0;
    return 0;
  }
  *target = static_cast<std::uint8_t>(kNoPreviewSelected);
  VS::Battle::TargetSphereRecord* sphere = SphereFor("target list");
  if (sphere == nullptr) {
    // Closes the list with no target, which the action flow takes as a
    // cancel.
    return 1;
  }
  LiveTargetSelectionHost host(*sphere);
  int chosen = kNoPreviewSelected;
  const bool closed = UpdateTargetSelectionMenu(host, chosen);
  *target = static_cast<std::uint8_t>(chosen);
  return closed ? 1 : 0;
}

int LiveCalculateActionAreaReach(std::intptr_t /*unused*/, int action_id,
                                 std::uint32_t maximum_reach) {
  const std::optional<TargetingActionView> action = ActionView(action_id);
  return action ? CalculateActionAreaReach(*action, maximum_reach) : 0;
}

int LiveCalculateActionRangeSquared(
    VS::Battle::Ai::ActionRangeView* actor_range_view, int action_id) {
  return ActionRangeSquared(
      action_id,
      actor_range_view != nullptr && actor_range_view->opaque_5c
          ? std::optional(RangeOf(actor_range_view->opaque_5c->opaque38))
          : std::nullopt);
}

int LiveCalculateActorActionRangeSquared(const VS::Battle::ActorStats* stats,
                                         int action_id) {
  return ActionRangeSquared(
      action_id,
      stats != nullptr ? std::optional(WeaponRangeOf(*stats)) : std::nullopt);
}

void LiveUpdateTargetSelectionHighlight(int mode) {
  VS::Battle::TargetSphereRecord* sphere = SphereFor("target highlight");
  if (sphere == nullptr) {
    return;
  }
  LiveTargetSelectionHost host(*sphere);
  UpdateTargetSelectionHighlight(host, mode);
}

int LiveCalculateRangeShapeExtent(
    const VS::Battle::Ai::ActionRangeShape* range_shape) {
  if (range_shape == nullptr) {
    VS::Core::LogError("battle.targeting",
                       "range extent refused: no range shape");
    return 0;
  }
  return CalculateRangeShapeExtent(RangeOf(*range_shape));
}
}  // namespace VS::Battle
