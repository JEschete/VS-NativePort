// Hand-maintained (tools/port/owned.csv).
// Live binding for the typed actor state rules (ActorFlags.cpp,
// BattleActorState.cpp, ActorLifecycle.cpp and ActorPlacement.cpp): the
// actor table, the battle actors and their stats, the gimmicks standing in
// empty actor slots and the weapon models.
#include "Actors/ActorStateLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Actors/ActorFlags.hpp"
#include "Actors/ActorLifecycle.hpp"
#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorPlacement.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorSpawning.hpp"
#include "Actors/ActorStateRecord.hpp"
#include "Actors/ActorTableLive.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/BattleActorState.hpp"
#include "Actors/Location.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/MotionRequestsLive.hpp"
#include "Actors/Movement.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/Shadow.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Actors/TextureAnimationRecords.hpp"
#include "Actors/WeaponModel.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/FlightTerrain.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Battle/Ai/RoomAiMap.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Core/Log.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/GimmickLookup.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Render/Palette.hpp"

using VS::Core::Gte::SVECTOR;

namespace VS::Actors {
namespace {
static_assert(std::size(VS::Actors::actorRecords) == kActorStateSlots);
static_assert(std::size(VS::Battle::actors) == kBattleActorSlots);
static_assert(std::tuple_size_v<decltype(VS::Battle::ActorStats::limbs)> ==
              kActorLimbs);
static_assert(sizeof(VS::Render::ColorCode) == sizeof(Render::ColorCode));

constexpr int kAllStatuses = 0x7FFFFFFF;
// The rider a freed moving platform takes.
constexpr int kNoPlatformRider = -1;
constexpr int kNoParentArgument = -1;
constexpr int kNoNearestActor = -1;

// A one-bit unsigned field's value.
std::uint8_t Bit(bool value) { return value ? 1 : 0; }
// A one-bit signed (char) field's value; its only set value is -1.
char SignedBit(bool value) { return value ? -1 : 0; }

Render::ColorCode ToColor(const VS::Render::ColorCode& live) {
  return {live.red, live.green, live.blue, live.code};
}

VS::Render::ColorCode ToLiveColor(const Render::ColorCode& color) {
  return {color.red, color.green, color.blue, color.code};
}

void LoadInstance(const VS::Actors::ModelInstance& live,
                  ActorStateRecord& record) {
  record.motion.current_tile.spare = live.reserved5f;
  record.skipped = live.is_skipped != 0;
  record.visible = live.is_visible != 0;
  record.frozen = live.is_frozen != 0;
  record.sound_enabled = live.sound_enabled != 0;
  record.attached = live.is_attached != 0;
  record.reaction_state = live.reaction_state;
  record.support_layer = live.support_layer;
  record.placement_layer = live.placement_layer;
  record.bone_count = live.bone_count;
  record.attach_parent = live.attach_parent;
  record.attach_bone = live.attach_bone;
  record.look_yaw = live.look_yaw;
  record.look_pitch = live.look_pitch;
  record.rotation_reserved = live.reserved2a;
  record.palette_pulse_kind = live.palette_pulse.kind;
}

void StoreInstance(const ActorStateRecord& record,
                   VS::Actors::ModelInstance& live) {
  live.reserved5f = record.motion.current_tile.spare;
  live.is_skipped = SignedBit(record.skipped);
  live.is_visible = SignedBit(record.visible);
  live.is_frozen = SignedBit(record.frozen);
  live.sound_enabled = Bit(record.sound_enabled);
  live.is_attached = Bit(record.attached);
  live.reaction_state = record.reaction_state;
  live.support_layer = record.support_layer;
  live.placement_layer = record.placement_layer;
  live.attach_parent = record.attach_parent;
  live.attach_bone = record.attach_bone;
  live.look_yaw = record.look_yaw;
  live.look_pitch = record.look_pitch;
  live.reserved2a = record.rotation_reserved;
  live.palette_pulse.kind = record.palette_pulse_kind;
}

CompanionStyle LoadCompanion(const VS::Actors::ActorRecord& live) {
  const VS::Actors::CompanionOffsetRecord& offset = live.companion_state;
  return {.offset = {.x = offset.offset_x,
                     .y = offset.offset_y,
                     .z = offset.offset_z,
                     .depth_bias = offset.depth_bias},
          .color = ToColor(live.companion_color)};
}

void StoreCompanion(const CompanionStyle& companion,
                    VS::Actors::ActorRecord& live) {
  live.companion_state = {.offset_x = companion.offset.x,
                          .offset_y = companion.offset.y,
                          .offset_z = companion.offset.z,
                          .depth_bias = companion.offset.depth_bias};
  live.companion_color = ToLiveColor(companion.color);
}

void LoadRecordFlags(const VS::Actors::ActorRecord& live,
                     ActorStateRecord& record) {
  record.attached_child_count = live.attached_child_count;
  record.animation_locked = live.animation_locked != 0;
  record.animation_event = live.animation_event_flag != 0;
  record.support_gimmick = live.support_gimmick;
  record.bypass_actor_collision = live.bypass_actor_collision != 0;
  record.is_swaying = live.is_swaying != 0;
  record.sway_returning = live.sway_returning != 0;
  record.holding_last_frame = live.pose.holding_last_frame != 0;
  record.pose_reached_end = live.pose.reached_end != 0;
  record.texture_timelines = {live.texture_animation_slots[0].timeline,
                              live.texture_animation_slots[1].timeline};
}

void StoreRecordFlags(const ActorStateRecord& record,
                      VS::Actors::ActorRecord& live) {
  live.attached_child_count = record.attached_child_count;
  live.animation_locked = Bit(record.animation_locked);
  live.is_swaying = Bit(record.is_swaying);
  live.sway_returning = Bit(record.sway_returning);
  live.texture_animation_slots[0].timeline = record.texture_timelines[0];
  live.texture_animation_slots[1].timeline = record.texture_timelines[1];
}

void LoadRecordFields(const VS::Actors::ActorRecord& live,
                      ActorStateRecord& record) {
  record.animation_id = live.animation_id;
  record.speed_quarters = live.speed_quarters;
  record.collision_height = live.collision_height;
  record.queued_animation = live.queued_animation;
  record.contact_kind = live.contact_kind;
  record.ledge_height = live.ledge_height;
  record.platform_offset_x = live.platform_offset_x;
  record.platform_offset_z = live.platform_offset_z;
  record.companion = LoadCompanion(live);
  record.platform_anchor = LiveMotionVector(live.platform_anchor);
  record.head_track_target = live.head_track_target;
  record.companion_slot = live.companion_slot;
  record.ledge_attempts = live.ledge_attempts;
  record.sway_angles = live.sway_angles[0];
  const VS::Actors::MovementRequest& request = live.turn_state;
  record.request_extras = {
      .reserved = {request.reserved1, request.reserved2, request.reserved3},
      .reserved_c = request.reserved0c,
      .step = LiveMotionVector(request.step)};
  record.reserved_1860 = live.opaque1860;
  record.shadow = LoadLiveShadowBlend(live.shadow_blend);
  record.shadow_extras = {.color = live.shadow_blend.color,
                          .reserved6 = live.shadow_blend.reserved6,
                          .reserved7 = live.shadow_blend.reserved7};
  record.shadow_expand_rate = live.shadow_radius_expand_rate;
  record.shadow_shrink_rate = live.shadow_radius_shrink_rate;
  record.saved_placement = std::bit_cast<PackedTile>(live.saved_placement);
  record.pending_target_x = live.pending_target_x;
  record.pending_target_z = live.pending_target_z;
  record.previous_target_x = live.previous_target_x;
  record.previous_target_z = live.previous_target_z;
}

void StoreRecordFields(const ActorStateRecord& record,
                       VS::Actors::ActorRecord& live) {
  live.speed_quarters = record.speed_quarters;
  live.queued_animation = record.queued_animation;
  live.contact_kind = record.contact_kind;
  live.ledge_height = record.ledge_height;
  live.platform_offset_x = record.platform_offset_x;
  live.platform_offset_z = record.platform_offset_z;
  StoreCompanion(record.companion, live);
  live.platform_anchor = LiveSvector(record.platform_anchor);
  live.head_track_target = record.head_track_target;
  live.companion_slot = record.companion_slot;
  live.ledge_attempts = record.ledge_attempts;
  live.sway_angles[0] = record.sway_angles;
  VS::Actors::MovementRequest& request = live.turn_state;
  request.reserved1 = record.request_extras.reserved[0];
  request.reserved2 = record.request_extras.reserved[1];
  request.reserved3 = record.request_extras.reserved[2];
  request.reserved0c = record.request_extras.reserved_c;
  request.step = LiveSvector(record.request_extras.step);
  live.opaque1860 = record.reserved_1860;
  StoreLiveShadowBlend(record.shadow, live.shadow_blend);
  live.shadow_blend.color = record.shadow_extras.color;
  live.shadow_blend.reserved6 = record.shadow_extras.reserved6;
  live.shadow_blend.reserved7 = record.shadow_extras.reserved7;
  live.pending_target_x = record.pending_target_x;
  live.pending_target_z = record.pending_target_z;
  live.previous_target_x = record.previous_target_x;
  live.previous_target_z = record.previous_target_z;
}

struct RecordTraits {
  using Live = VS::Actors::ActorRecord;
  using Typed = ActorStateRecord;
  static constexpr std::size_t kCapacity = kActorStateSlots;
  static constexpr const char* kWhat = "actor records";

  static Typed Load(const Live& live) {
    ActorStateRecord record;
    record.motion = LoadLiveActorMotion(live);
    LoadInstance(live.instance, record);
    LoadRecordFlags(live, record);
    LoadRecordFields(live, record);
    return record;
  }
  static void Store(const Typed& record, Live& live) {
    StoreLiveActorMotion(record.motion, live);
    StoreInstance(record, live.instance);
    StoreRecordFlags(record, live);
    StoreRecordFields(record, live);
  }
};

struct BattleActorTraits {
  using Live = VS::Battle::Actor;
  using Typed = BattleActorState;
  static constexpr std::size_t kCapacity = kBattleActorSlots;
  static constexpr const char* kWhat = "battle actors";

  static Typed Load(const Live& live) {
    return {.command_state = live.command_state,
            .model_id = live.model_id,
            .kind = live.kind,
            .retain_after_defeat = live.retain_after_defeat != 0};
  }
  static void Store(const Typed& actor, Live& live) {
    live.command_state = actor.command_state;
    live.kind = actor.kind;
  }
};

struct VitalsTraits {
  using Live = VS::Battle::ActorStats;
  using Typed = ActorVitals;
  static constexpr std::size_t kCapacity = kBattleActorSlots;
  static constexpr const char* kWhat = "battle actor stats";

  static Typed Load(const Live& live) {
    ActorVitals vitals{.current_hp = live.current_hp,
                       .max_hp = live.max_hp,
                       .current_mp = live.current_mp,
                       .max_mp = live.max_mp,
                       .risk = live.risk,
                       .action_recovery_timer = live.action_recovery_timer};
    for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
      vitals.limbs.at(limb) = {.hp = live.limbs[limb].hp,
                               .max_hp = live.limbs[limb].max_hp};
    }
    return vitals;
  }
  static void Store(const Typed& vitals, Live& live) {
    live.current_hp = vitals.current_hp;
    live.current_mp = vitals.current_mp;
    live.risk = vitals.risk;
    live.action_recovery_timer = vitals.action_recovery_timer;
    for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
      live.limbs[limb].hp = vitals.limbs.at(limb).hp;
    }
  }
};

// The live values one operation touches, loaded on first use and stored
// back when the operation ends. Reloads overwrite the entries in place, so
// the typed values the rules hold stay valid across host calls.
template <typename Traits>
class Registry {
 public:
  using Live = typename Traits::Live;
  using Typed = typename Traits::Typed;

  Registry() = default;
  Registry(const Registry&) = delete;
  Registry& operator=(const Registry&) = delete;
  Registry(Registry&&) = delete;
  Registry& operator=(Registry&&) = delete;
  ~Registry() { StoreAll(); }

  Typed& Attach(Live& live) {
    for (std::size_t index = 0; index < count_; ++index) {
      if (entries_.at(index).live == &live) return entries_.at(index).typed;
    }
    if (count_ == entries_.size()) {
      VS::Core::LogError("actors", "Actor state touched more than {} {}",
                         entries_.size(), Traits::kWhat);
      std::abort();
    }
    Entry& entry = entries_.at(count_++);
    entry.live = &live;
    entry.typed = Traits::Load(live);
    return entry.typed;
  }

  void StoreAll() const {
    for (std::size_t index = 0; index < count_; ++index) {
      Traits::Store(entries_.at(index).typed, *entries_.at(index).live);
    }
  }

  void ReloadAll() {
    for (std::size_t index = 0; index < count_; ++index) {
      entries_.at(index).typed = Traits::Load(*entries_.at(index).live);
    }
  }

 private:
  struct Entry {
    Live* live = nullptr;
    Typed typed{};
  };
  std::array<Entry, Traits::kCapacity> entries_{};
  std::size_t count_ = 0;
};

// Every legacy call that may read or write the tables runs inside a Synced
// scope, which stores the typed values first and reloads them afterwards.
template <typename Host>
class Synced {
 public:
  explicit Synced(Host& host) : host_(host) { host_.StoreAll(); }
  Synced(const Synced&) = delete;
  Synced& operator=(const Synced&) = delete;
  Synced(Synced&&) = delete;
  Synced& operator=(Synced&&) = delete;
  ~Synced() { host_.ReloadAll(); }

 private:
  Host& host_;
};

VS::Actors::WeaponModel* LiveWeaponModel(int index) {
  if (index < 0 || index >= kWeaponModelCount) return nullptr;
  return weaponModels[index];
}

class LiveActorStateHost final : public ActorStateHost {
 public:
  explicit LiveActorStateHost(const char* operation) : operation_(operation) {}

  void StoreAll() const { records_.StoreAll(); }
  void ReloadAll() { records_.ReloadAll(); }

  ActorStateRecord* Record(int id) override {
    VS::Actors::ActorRecord* record = LiveActorRecord(id, operation_);
    return record != nullptr ? &records_.Attach(*record) : nullptr;
  }
  std::optional<GimmickView> Gimmick(int id) override {
    const VS::Field::GimmickInstance* gimmick =
        VS::Field::FindGimmickForActorSlot(id);
    if (gimmick == nullptr) return std::nullopt;
    return GimmickView{
        .skipped = gimmick->is_skipped != 0,
        .visible = gimmick->is_visible != 0,
        .tile = {.x = static_cast<std::uint8_t>(gimmick->tile_x),
                 .layer = static_cast<std::uint8_t>(gimmick->tile_layer),
                 .z = static_cast<std::uint8_t>(gimmick->tile_z),
                 .spare = static_cast<std::uint8_t>(gimmick->reserved5f)},
        .position = {.x = gimmick->position_x,
                     .y = gimmick->position_y,
                     .z = gimmick->position_z,
                     .w = gimmick->reserved22},
        .facing = gimmick->rotation_y};
  }
  void SetGimmickDisplay(int id, bool skipped, bool visible) override {
    if (VS::Field::GimmickInstance* gimmick = KnownGimmick(id)) {
      gimmick->is_skipped = SignedBit(skipped);
      gimmick->is_visible = SignedBit(visible);
    }
  }
  void SetGimmickMode(int id, std::int16_t mode) override {
    if (VS::Field::GimmickInstance* gimmick = KnownGimmick(id)) {
      gimmick->mode = mode;
    }
  }
  void ClearGimmickSupport(int id) override {
    if (VS::Field::GimmickInstance* gimmick = KnownGimmick(id)) {
      gimmick->object_info.support_index = 0;
    }
  }
  void PlayGimmickSound(int id, int sound, int variant) override {
    const VS::Field::GimmickInstance* gimmick = KnownGimmick(id);
    if (gimmick == nullptr) return;
    const Synced synced(*this);
    Audio::LivePlayGimmickSoundPair(gimmick, sound, variant);
  }

  std::optional<bool> WeaponModelShown(int index) override {
    const VS::Actors::WeaponModel* model = LiveWeaponModel(index);
    if (model == nullptr) return std::nullopt;
    return model->shown != 0;
  }
  void SetWeaponModelShown(int index, bool shown) override {
    if (VS::Actors::WeaponModel* model = LiveWeaponModel(index)) {
      model->shown = Bit(shown);
    }
  }

  bool SharedBankHasTexturePages() override {
    return VS::Actors::sharedSequenceBankTexturePages
               [VS::Actors::sharedSequenceBankIndex] != 0;
  }
  void ResetHeadTrackingSpeed(int id) override {
    VS::Actors::ActorRecord* record = LiveActorRecord(id, operation_);
    if (record == nullptr) return;
    const Synced synced(*this);
    ResetLiveActorHeadTrackingSpeed(record);
  }
  void UploadTextureFrame(int id, int channel, int frame) override {
    VS::Actors::ActorRecord* record = LiveActorRecord(id, operation_);
    if (record == nullptr) return;
    const Synced synced(*this);
    UploadLiveActorTextureFrame(record, channel, frame);
  }
  void ClearTint(int id) override {
    const Synced synced(*this);
    static_cast<void>(SetLiveActorTint(id, nullptr));
  }
  void ClearSoundOverrides(int id) override {
    VS::Actors::ModelEffectSlot silent{};
    const Synced synced(*this);
    SetLiveActorSoundOverrides(id, &silent);
  }
  void PlayAnimation(int id, int animation, int blend_frames) override {
    const Synced synced(*this);
    static_cast<void>(PlayLiveActorAnimation(id, animation, 0, blend_frames));
  }
  bool HasAnimation(int id, int animation) override {
    VS::Actors::ActorRecord* record = LiveActorRecord(id, operation_);
    if (record == nullptr) return false;
    std::uint16_t* header = nullptr;
    const Synced synced(*this);
    static_cast<void>(FindLiveActorAnimationHeader(record, animation, &header));
    return header != nullptr;
  }
  void ReleaseMovingPlatform(int platform) override {
    const Synced synced(*this);
    static_cast<void>(
        VS::Field::SetMovingPlatformActorEntry(platform, kNoPlatformRider));
  }
  int SupportHeight(int x, int z) override {
    const Synced synced(*this);
    return Field::LiveActorSupportHeight(x, z);
  }
  int NavigationCeilingHeight() override {
    return Battle::Ai::RoomNavigationCeilingHeight(
        Battle::Ai::BattleRoomAiRuntime());
  }
  std::optional<Field::PlatformContact> MovingPlatformContact(
      int support_layer) override {
    const Synced synced(*this);
    return Field::LiveMovingPlatformPlacement(support_layer);
  }
  void SaveGroundedTransform(int id) override {
    VS::Actors::ActorRecord* record = LiveActorRecord(id, operation_);
    if (record == nullptr) return;
    const Synced synced(*this);
    SaveLiveActorGroundedTransform(record);
  }

 private:
  VS::Field::GimmickInstance* KnownGimmick(int id) const {
    VS::Field::GimmickInstance* gimmick =
        VS::Field::FindGimmickForActorSlot(id);
    if (gimmick == nullptr) {
      WarnActorRefused(operation_, "no gimmick stands in the actor slot");
    }
    return gimmick;
  }

  const char* operation_;
  Registry<RecordTraits> records_;
};

class LiveBattleActorHost final : public BattleActorHost {
 public:
  void StoreAll() const {
    actors_.StoreAll();
    vitals_.StoreAll();
  }
  void ReloadAll() {
    actors_.ReloadAll();
    vitals_.ReloadAll();
  }

  BattleActorState* Actor(int id) override {
    VS::Battle::Actor* actor = LiveActor(id);
    return actor != nullptr ? &actors_.Attach(*actor) : nullptr;
  }
  ActorVitals* Vitals(int id) override {
    const VS::Battle::Actor* actor = LiveActor(id);
    if (actor == nullptr || actor->stats == nullptr) return nullptr;
    return &vitals_.Attach(*actor->stats);
  }
  void ClearStatuses(int id) override {
    const VS::Battle::Actor* actor = LiveActor(id);
    if (actor == nullptr || actor->stats == nullptr) return;
    const Synced synced(*this);
    VS::Battle::RemoveLiveActorStatuses(kAllStatuses, actor->stats);
    // The original also cleared the write-only D_800F5160 (146C.c:8682).
  }
  void SetMarker(int id, int flags) override {
    const Synced synced(*this);
    VS::Battle::SetLiveActorMarker(id, flags);
  }
  bool UnlinkFromRoomList(int id) override {
    const Synced synced(*this);
    return UnlinkLiveActorFromRoomList(LiveActor(id)) != 0;
  }
  bool LinkAfterAshley(int id) override {
    const Synced synced(*this);
    return LinkLiveActorAfterAshley(LiveActor(id)) != 0;
  }
  void SetAnimationLocked(int id, bool locked) override {
    const Synced synced(*this);
    SetLiveActorAnimationLocked(id, locked ? 1 : 0);
  }
  void ClearAiLinksAndPlan(int id) override {
    const Synced synced(*this);
    Battle::Ai::ClearLiveActorAiLinksAndPlan(id);
  }
  void ResolveAiLinkedActors() override {
    const Synced synced(*this);
    Battle::Ai::ResolveLiveAiLinkedActors();
  }
  void RebuildActionPlans() override {
    const Synced synced(*this);
    static_cast<void>(Battle::Ai::RebuildLiveActorActionPlans(nullptr));
  }

 private:
  static VS::Battle::Actor* LiveActor(int id) {
    return IsBattleActorSlot(id) ? VS::Battle::actors[id] : nullptr;
  }

  Registry<BattleActorTraits> actors_;
  Registry<VitalsTraits> vitals_;
};

// The model switches compose the live state flags slot by slot.
class LiveModelToggleHost final : public ActorModelToggleHost {
 public:
  std::uint32_t StateFlags(std::size_t slot) const override {
    return LiveActorStateFlags(static_cast<int>(slot));
  }
  void SetDisplayFlags(std::size_t slot, std::uint32_t flags) override {
    SetLiveActorDisplayFlags(static_cast<int>(slot), std::bit_cast<int>(flags));
  }
  bool HasBattleActor(std::size_t slot) override {
    return slot < std::size(VS::Battle::actors) &&
           VS::Battle::actors[slot] != nullptr;
  }
};

bool IsModelSlot(int actor_id, const char* operation) {
  if (actor_id < 0) {
    WarnActorRefused(operation, "the actor id is negative");
    return false;
  }
  return true;
}

void WritePlacement(const ActorPlacementView& view,
                    VS::Actors::ActorLocation& placement) {
  placement.packed_tile.target.tile_x = view.tile.x;
  placement.packed_tile.target.tile_layer = view.tile.layer;
  placement.packed_tile.target.tile_z = view.tile.z;
  placement.packed_tile.target.facing = view.tile.spare;
  placement.position = LiveSvector(view.position);
}

int WriteLivePlacement(int actor_id, VS::Actors::ActorLocation* placement_out) {
  if (placement_out == nullptr) {
    WarnActorRefused("ActorPlacement", "no placement output");
    return -1;
  }
  const std::optional<ActorPlacementView> view =
      LiveActorPlacementView(actor_id);
  if (!view.has_value()) return -1;
  WritePlacement(*view, *placement_out);
  return 0;
}
}  // namespace

void EnableLiveActorModel(int actor_id) {
  if (!IsModelSlot(actor_id, "EnableActorModel")) return;
  LiveModelToggleHost host;
  EnableActorModel(host, static_cast<std::size_t>(actor_id));
}

void EnableAllLiveActorModels() {
  LiveModelToggleHost host;
  EnableAllActorModels(host);
}

void DisableLiveActorModel(int actor_id) {
  if (!IsModelSlot(actor_id, "DisableActorModel")) return;
  LiveModelToggleHost host;
  DisableActorModel(host, static_cast<std::size_t>(actor_id));
}

void DisableAllLiveActorModels() {
  LiveModelToggleHost host;
  DisableAllActorModels(host);
}

void SetLiveActorHostile(int actor_id, int hostile) {
  LiveBattleActorHost host;
  SetActorHostile(host, actor_id, hostile != 0);
}

int LiveActorHpPercentage(int actor_id) {
  LiveBattleActorHost host;
  return ActorHpPercentage(host, actor_id);
}

void RestoreLiveActorVitals(int actor_id) {
  LiveBattleActorHost host;
  RestoreActorVitals(host, actor_id);
}

int FindLiveActorByModelId(int model_id) {
  LiveBattleActorHost host;
  return FindActorByModelId(host, model_id);
}

VS::Battle::Actor* LiveActorAtSlot(int actor_id) {
  if (!IsBattleActorSlot(actor_id)) {
    WarnActorRefused("ActorAtSlot",
                     "the actor id is outside VS::Battle::actors");
    return nullptr;
  }
  return VS::Battle::actors[actor_id];
}

int CountLiveActorsByDisposition(int actor_kind) {
  LiveBattleActorHost host;
  return CountActorsByDisposition(host, actor_kind);
}

char IsLiveActorRetainedAfterDefeat(int actor_id) {
  LiveBattleActorHost host;
  return IsActorRetainedAfterDefeat(host, actor_id) ? 1 : 0;
}

int SetLiveActorLogicEnabled(int actor_id, int enabled) {
  LiveBattleActorHost host;
  SetActorLogicEnabled(host, actor_id, enabled != 0);
  return enabled;
}

int SetOtherLiveActorLogicEnabled(int enabled) {
  LiveBattleActorHost host;
  SetOtherActorLogicEnabled(host, enabled != 0);
  return enabled;
}

void SetLiveActorDetached(int actor_id, int detached) {
  LiveBattleActorHost host;
  SetActorDetached(host, actor_id, detached != 0);
}

void ResetLiveActorMotionState(int actor_id, int /*unused*/) {
  LiveActorStateHost host("ResetActorMotionState");
  ResetActorMotionState(host, actor_id);
}

void ResetAllLiveActorMovement() {
  LiveActorStateHost host("ResetAllActorMovement");
  ResetAllActorMovement(host);
}

void BeginLiveActorDeath(int actor_id, void* /*context*/, int animate) {
  LiveActorStateHost host("BeginActorDeath");
  BeginActorDeath(host, actor_id, animate != 0);
}

void SetLiveActorCompanion(int actor_id, int companion_slot,
                           const VS::Render::ColorCode* companion_state,
                           const VS::Render::ColorCode* companion_color) {
  // The record keeps the slot in a byte; the original tested the full value,
  // so a slot of 256 stored "no companion" yet still copied the style.
  if (companion_slot < 0 || companion_slot > UINT8_MAX) {
    WarnActorRefused("SetActorCompanion", "the companion slot is not a byte");
    return;
  }
  std::optional<CompanionStyle> style;
  if (companion_state != nullptr && companion_color != nullptr) {
    style = CompanionStyle{
        .offset = {.x = static_cast<std::int8_t>(companion_state->red),
                   .y = static_cast<std::int8_t>(companion_state->green),
                   .z = static_cast<std::int8_t>(companion_state->blue),
                   .depth_bias = companion_state->code},
        .color = ToColor(*companion_color)};
  }
  LiveActorStateHost host("SetActorCompanion");
  SetActorCompanion(host, actor_id, static_cast<std::uint8_t>(companion_slot),
                    style);
}

void AttachLiveActorToParentBone(int actor_id, int parent_actor_id,
                                 int parent_bone_index, int /*unused*/) {
  std::optional<AttachPoint> point;
  if (parent_actor_id != kNoParentArgument) {
    point = AttachPoint{.parent = static_cast<std::uint8_t>(parent_actor_id),
                        .bone = static_cast<std::uint8_t>(parent_bone_index)};
  }
  LiveActorStateHost host("AttachActorToParentBone");
  AttachActorToParentBone(host, actor_id, point);
}

int ReleaseLiveActorAttachmentLock(int actor_id, int /*unused*/) {
  LiveActorStateHost host("ReleaseActorAttachmentLock");
  return static_cast<int>(ReleaseActorAttachmentLock(host, actor_id));
}

void SetLiveActorDisplayFlags(int actor_id, int display_flags) {
  LiveActorStateHost host("SetActorDisplayFlags");
  SetActorDisplayFlags(host, actor_id,
                       std::bit_cast<std::uint32_t>(display_flags));
}

std::uint32_t LiveActorStateFlags(int actor_id) {
  LiveActorStateHost host("ActorStateFlags");
  return ActorStateFlags(host, actor_id);
}

std::optional<ActorPlacementView> LiveActorPlacementView(int actor_id) {
  LiveActorStateHost host("ActorPlacement");
  return ActorPlacement(host, actor_id);
}

int LiveActorPlacement(int actor_id, VS::Actors::ActorLocation* placement_out) {
  return WriteLivePlacement(actor_id, placement_out);
}

int LiveActorPlacement(int actor_id,
                       VS::Actors::ActorLocationQuery* placement_out) {
  return WriteLivePlacement(
      actor_id, placement_out != nullptr ? &placement_out->location : nullptr);
}

void PlaceLiveActorInRoom(int actor_id, const SpawnPlacement* placement,
                          int drop_height, int movement_class) {
  if (placement == nullptr) {
    WarnActorRefused("PlaceActorInRoom", "no placement");
    return;
  }
  // The placement's second and fourth bytes are the tile layer and facing.
  const PackedTile tile{.x = placement->tile_x,
                        .layer = placement->direction,
                        .z = placement->tile_z,
                        .spare = placement->height};
  LiveActorStateHost host("PlaceActorInRoom");
  PlaceActorInRoom(host, actor_id, tile, drop_height, movement_class);
}

int FindLiveActorAtFlightPosition(int actor_id, const SVECTOR* position) {
  if (position == nullptr) {
    WarnActorRefused("FindActorAtFlightPosition", "no position");
    return 0;
  }
  LiveActorStateHost host("FindActorAtFlightPosition");
  return FindActorAtFlightPosition(host, actor_id, LiveMotionVector(*position))
      .value_or(0);
}

void SeparateLiveActorFlightPlacement(int actor_id, SVECTOR* position) {
  if (position == nullptr) {
    WarnActorRefused("SeparateActorFlightPlacement", "no position");
    return;
  }
  MotionVector separated = LiveMotionVector(*position);
  {
    LiveActorStateHost host("SeparateActorFlightPlacement");
    SeparateActorFlightPlacement(host, actor_id, separated);
  }
  *position = LiveSvector(separated);
}

void LiveActorSavedPlacement(int actor_id, int movement_class,
                             VS::Actors::ActorLocationQuery* placement_out) {
  if (placement_out == nullptr) {
    WarnActorRefused("ActorSavedPlacement", "no placement output");
    return;
  }
  std::optional<PackedTile> tile;
  {
    LiveActorStateHost host("ActorSavedPlacement");
    tile = ActorSavedPlacement(host, actor_id, movement_class);
  }
  if (!tile.has_value()) return;
  VS::Actors::TileTarget& target = placement_out->location.packed_tile.target;
  target.tile_x = tile->x;
  target.tile_layer = tile->layer;
  target.tile_z = tile->z;
  target.facing = tile->spare;
}

int FindLiveNearestActorAhead(int actor_id, int horizontal_range,
                              int vertical_range) {
  LiveActorStateHost host("FindNearestActorAhead");
  return FindNearestActorAhead(
             host, actor_id,
             {.horizontal = horizontal_range, .vertical = vertical_range})
      .value_or(kNoNearestActor);
}
}  // namespace VS::Actors
