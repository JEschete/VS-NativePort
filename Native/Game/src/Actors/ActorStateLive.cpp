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
#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorStateRecord.hpp"
#include "Actors/ActorTableLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/BattleActorState.hpp"
#include "Actors/MotionRequestsLive.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/FlightTerrain.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Battle/Ai/RoomAiMap.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Core/Log.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/PlatformPlacementLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/SkeletonLiveState.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Recovery.h"
#include "Battle/StatusEffects.h"
#include "Field/Actors.h"
#include "Field/GimmickLookup.h"
#include "Field/Gimmicks.h"
#include "Field/PlatformPlacementLive.h"
#include "Field/Platforms.h"
#include "Render/Palette.h"
}

namespace VS::Actors {
namespace {
static_assert(std::size(Actors_actorRecords) == kActorStateSlots);
static_assert(std::size(Battle_actors) == kBattleActorSlots);
static_assert(std::extent_v<decltype(Battle_ActorStats::limbs)> == kActorLimbs);
static_assert(sizeof(Render_ColorCode) == sizeof(Render::ColorCode));

constexpr int kAllStatuses = 0x7FFFFFFF;
// The rider a freed moving platform takes.
constexpr int kNoPlatformRider = -1;
constexpr int kNoParentArgument = -1;
constexpr int kNoNearestActor = -1;

// A one-bit unsigned field's value.
u_char Bit(bool value) { return value ? 1 : 0; }
// A one-bit signed (char) field's value; its only set value is -1.
char SignedBit(bool value) { return value ? -1 : 0; }

Render::ColorCode ToColor(const Render_ColorCode& live) {
  return {live.red, live.green, live.blue, live.code};
}

Render_ColorCode ToLiveColor(const Render::ColorCode& color) {
  return {color.red, color.green, color.blue, color.code};
}

void LoadInstance(const Actors_ModelInstance& live, ActorStateRecord& record) {
  record.motion.current_tile.spare = live.reserved5F;
  record.skipped = live.isSkipped != 0;
  record.visible = live.isVisible != 0;
  record.frozen = live.isFrozen != 0;
  record.sound_enabled = live.soundEnabled != 0;
  record.attached = live.isAttached != 0;
  record.reaction_state = live.reactionState;
  record.support_layer = live.supportLayer;
  record.placement_layer = live.placementLayer;
  record.bone_count = live.boneCount;
  record.attach_parent = live.attachParent;
  record.attach_bone = live.attachBone;
  record.look_yaw = live.lookYaw;
  record.look_pitch = live.lookPitch;
  record.rotation_reserved = live.reserved2A;
  record.palette_pulse_kind = live.palettePulse.kind;
}

void StoreInstance(const ActorStateRecord& record, Actors_ModelInstance& live) {
  live.reserved5F = record.motion.current_tile.spare;
  live.isSkipped = SignedBit(record.skipped);
  live.isVisible = SignedBit(record.visible);
  live.isFrozen = SignedBit(record.frozen);
  live.soundEnabled = Bit(record.sound_enabled);
  live.isAttached = Bit(record.attached);
  live.reactionState = record.reaction_state;
  live.supportLayer = record.support_layer;
  live.placementLayer = record.placement_layer;
  live.attachParent = record.attach_parent;
  live.attachBone = record.attach_bone;
  live.lookYaw = record.look_yaw;
  live.lookPitch = record.look_pitch;
  live.reserved2A = record.rotation_reserved;
  live.palettePulse.kind = record.palette_pulse_kind;
}

CompanionStyle LoadCompanion(const Actors_ActorRecord& live) {
  const Actors_CompanionOffset& offset = live.companionState;
  return {.offset = {.x = offset.offsetX,
                     .y = offset.offsetY,
                     .z = offset.offsetZ,
                     .depth_bias = offset.depthBias},
          .color = ToColor(live.companionColor)};
}

void StoreCompanion(const CompanionStyle& companion, Actors_ActorRecord& live) {
  live.companionState = {.offsetX = companion.offset.x,
                         .offsetY = companion.offset.y,
                         .offsetZ = companion.offset.z,
                         .depthBias = companion.offset.depth_bias};
  live.companionColor = ToLiveColor(companion.color);
}

void LoadRecordFlags(const Actors_ActorRecord& live, ActorStateRecord& record) {
  record.attached_child_count = live.attachedChildCount;
  record.animation_locked = live.animationLocked != 0;
  record.animation_event = live.animationEventFlag != 0;
  record.support_gimmick = live.supportGimmick;
  record.bypass_actor_collision = live.bypassActorCollision != 0;
  record.palette_fade_active = live.paletteFadeActive != 0;
  record.palette_fade_reverse = live.paletteFadeReverse != 0;
  record.holding_last_frame = live.pose.holdingLastFrame != 0;
  record.pose_reached_end = live.pose.reachedEnd != 0;
  record.texture_timelines = {live.textureAnimationSlots[0].timeline,
                              live.textureAnimationSlots[1].timeline};
}

void StoreRecordFlags(const ActorStateRecord& record,
                      Actors_ActorRecord& live) {
  live.attachedChildCount = record.attached_child_count;
  live.animationLocked = Bit(record.animation_locked);
  live.paletteFadeActive = Bit(record.palette_fade_active);
  live.paletteFadeReverse = Bit(record.palette_fade_reverse);
  live.textureAnimationSlots[0].timeline = record.texture_timelines[0];
  live.textureAnimationSlots[1].timeline = record.texture_timelines[1];
}

void LoadRecordFields(const Actors_ActorRecord& live,
                      ActorStateRecord& record) {
  record.animation_id = live.animationId;
  record.collision_state = live.collisionState;
  record.collision_height = live.collisionHeight;
  record.queued_animation = live.queuedAnimation;
  record.contact_kind = live.contactKind;
  record.ledge_height = live.ledgeHeight;
  record.platform_offset_x = live.platformOffsetX;
  record.platform_offset_z = live.platformOffsetZ;
  record.companion = LoadCompanion(live);
  record.platform_anchor = LiveMotionVector(live.platformAnchor);
  record.head_track_target = live.headTrackTarget;
  record.companion_slot = live.companionSlot;
  record.reserved_17ff = live.opaque17FF;
  record.palette_fade_state = live.paletteFadeState[0];
  const Actors_MovementRequest& request = live.turnState;
  record.request_extras = {
      .reserved = {request.reserved1, request.reserved2, request.reserved3},
      .reserved_c = request.reservedC,
      .step = LiveMotionVector(request.step)};
  record.reserved_1860 = live.opaque1860;
  record.shadow = LoadLiveShadowBlend(live.shadowBlend);
  record.shadow_extras = {.reserved0 = live.shadowBlend.reserved0,
                          .reserved6 = live.shadowBlend.reserved6,
                          .reserved7 = live.shadowBlend.reserved7};
  record.shadow_expand_rate = live.shadowRadiusExpandRate;
  record.shadow_shrink_rate = live.shadowRadiusShrinkRate;
  record.saved_placement = std::bit_cast<PackedTile>(live.savedPlacement);
  record.pending_target_x = live.pendingTargetX;
  record.pending_target_z = live.pendingTargetZ;
  record.previous_target_x = live.previousTargetX;
  record.previous_target_z = live.previousTargetZ;
}

void StoreRecordFields(const ActorStateRecord& record,
                       Actors_ActorRecord& live) {
  live.collisionState = record.collision_state;
  live.queuedAnimation = record.queued_animation;
  live.contactKind = record.contact_kind;
  live.ledgeHeight = record.ledge_height;
  live.platformOffsetX = record.platform_offset_x;
  live.platformOffsetZ = record.platform_offset_z;
  StoreCompanion(record.companion, live);
  live.platformAnchor = LiveSvector(record.platform_anchor);
  live.headTrackTarget = record.head_track_target;
  live.companionSlot = record.companion_slot;
  live.opaque17FF = record.reserved_17ff;
  live.paletteFadeState[0] = record.palette_fade_state;
  Actors_MovementRequest& request = live.turnState;
  request.reserved1 = record.request_extras.reserved[0];
  request.reserved2 = record.request_extras.reserved[1];
  request.reserved3 = record.request_extras.reserved[2];
  request.reservedC = record.request_extras.reserved_c;
  request.step = LiveSvector(record.request_extras.step);
  live.opaque1860 = record.reserved_1860;
  StoreLiveShadowBlend(record.shadow, live.shadowBlend);
  live.shadowBlend.reserved0 = record.shadow_extras.reserved0;
  live.shadowBlend.reserved6 = record.shadow_extras.reserved6;
  live.shadowBlend.reserved7 = record.shadow_extras.reserved7;
  live.pendingTargetX = record.pending_target_x;
  live.pendingTargetZ = record.pending_target_z;
  live.previousTargetX = record.previous_target_x;
  live.previousTargetZ = record.previous_target_z;
}

struct RecordTraits {
  using Live = Actors_ActorRecord;
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
  using Live = Battle_Actor;
  using Typed = BattleActorState;
  static constexpr std::size_t kCapacity = kBattleActorSlots;
  static constexpr const char* kWhat = "battle actors";

  static Typed Load(const Live& live) {
    return {.command_state = live.commandState,
            .model_id = live.modelId,
            .kind = live.kind,
            .retain_after_defeat = live.retainAfterDefeat != 0};
  }
  static void Store(const Typed& actor, Live& live) {
    live.commandState = actor.command_state;
    live.kind = actor.kind;
  }
};

struct VitalsTraits {
  using Live = Battle_ActorStats;
  using Typed = ActorVitals;
  static constexpr std::size_t kCapacity = kBattleActorSlots;
  static constexpr const char* kWhat = "battle actor stats";

  static Typed Load(const Live& live) {
    ActorVitals vitals{.current_hp = live.currentHp,
                       .max_hp = live.maxHp,
                       .current_mp = live.currentMp,
                       .max_mp = live.maxMp,
                       .risk = live.risk,
                       .action_recovery_timer = live.actionRecoveryTimer};
    for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
      vitals.limbs.at(limb) = {.hp = live.limbs[limb].hp,
                               .max_hp = live.limbs[limb].maxHp};
    }
    return vitals;
  }
  static void Store(const Typed& vitals, Live& live) {
    live.currentHp = vitals.current_hp;
    live.currentMp = vitals.current_mp;
    live.risk = vitals.risk;
    live.actionRecoveryTimer = vitals.action_recovery_timer;
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
      VS_LOG_ERROR("actors", "Actor state touched more than {} {}",
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

Actors_WeaponModel* LiveWeaponModel(int index) {
  if (index < 0 || index >= kActorsWeaponModelCount) return nullptr;
  return Actors_weaponModels[index];
}

class LiveActorStateHost final : public ActorStateHost {
 public:
  explicit LiveActorStateHost(const char* operation) : operation_(operation) {}

  void StoreAll() const { records_.StoreAll(); }
  void ReloadAll() { records_.ReloadAll(); }

  ActorStateRecord* Record(int id) override {
    Actors_ActorRecord* record = LiveActorRecord(id, operation_);
    return record != nullptr ? &records_.Attach(*record) : nullptr;
  }
  std::optional<GimmickView> Gimmick(int id) override {
    const Field_GimmickInstance* gimmick = Field_FindGimmickForActorSlot(id);
    if (gimmick == nullptr) return std::nullopt;
    return GimmickView{
        .skipped = gimmick->isSkipped != 0,
        .visible = gimmick->isVisible != 0,
        .tile = {.x = static_cast<std::uint8_t>(gimmick->tileX),
                 .layer = static_cast<std::uint8_t>(gimmick->tileLayer),
                 .z = static_cast<std::uint8_t>(gimmick->tileZ),
                 .spare = static_cast<std::uint8_t>(gimmick->reserved5F)},
        .position = {.x = gimmick->positionX,
                     .y = gimmick->positionY,
                     .z = gimmick->positionZ,
                     .w = gimmick->reserved22},
        .facing = gimmick->rotationY};
  }
  void SetGimmickDisplay(int id, bool skipped, bool visible) override {
    if (Field_GimmickInstance* gimmick = KnownGimmick(id)) {
      gimmick->isSkipped = SignedBit(skipped);
      gimmick->isVisible = SignedBit(visible);
    }
  }
  void SetGimmickMode(int id, std::int16_t mode) override {
    if (Field_GimmickInstance* gimmick = KnownGimmick(id)) {
      gimmick->mode = mode;
    }
  }
  void ClearGimmickSupport(int id) override {
    if (Field_GimmickInstance* gimmick = KnownGimmick(id)) {
      gimmick->objectInfo.supportIndex = 0;
    }
  }
  void PlayGimmickSound(int id, int sound, int variant) override {
    const Field_GimmickInstance* gimmick = KnownGimmick(id);
    if (gimmick == nullptr) return;
    const Synced synced(*this);
    Audio::LivePlayGimmickSoundPair(gimmick, sound, variant);
  }

  std::optional<bool> WeaponModelShown(int index) override {
    const Actors_WeaponModel* model = LiveWeaponModel(index);
    if (model == nullptr) return std::nullopt;
    return model->reserved08Bit4 != 0;
  }
  void SetWeaponModelShown(int index, bool shown) override {
    if (Actors_WeaponModel* model = LiveWeaponModel(index)) {
      model->reserved08Bit4 = Bit(shown);
    }
  }

  bool SharedBankHasTexturePages() override {
    return Actors_sharedSequenceBankTexturePages
               [Actors_sharedSequenceBankIndex] != 0;
  }
  void ResetHeadTrackingSpeed(int id) override {
    Actors_ActorRecord* record = LiveActorRecord(id, operation_);
    if (record == nullptr) return;
    const Synced synced(*this);
    ResetLiveActorHeadTrackingSpeed(record);
  }
  void UploadTextureFrame(int id, int channel, int frame) override {
    Actors_ActorRecord* record = LiveActorRecord(id, operation_);
    if (record == nullptr) return;
    const Synced synced(*this);
    UploadLiveActorTextureFrame(record, channel, frame);
  }
  void ClearTint(int id) override {
    const Synced synced(*this);
    static_cast<void>(SetLiveActorTint(id, nullptr));
  }
  void ClearSoundOverrides(int id) override {
    Actors_ModelEffectSlot silent{};
    const Synced synced(*this);
    SetLiveActorSoundOverrides(id, &silent);
  }
  void PlayAnimation(int id, int animation, int blend_frames) override {
    const Synced synced(*this);
    static_cast<void>(PlayLiveActorAnimation(id, animation, 0, blend_frames));
  }
  bool HasAnimation(int id, int animation) override {
    Actors_ActorRecord* record = LiveActorRecord(id, operation_);
    if (record == nullptr) return false;
    u_short* header = nullptr;
    const Synced synced(*this);
    static_cast<void>(FindLiveActorAnimationHeader(record, animation, &header));
    return header != nullptr;
  }
  void ReleaseMovingPlatform(int platform) override {
    const Synced synced(*this);
    static_cast<void>(
        Field_SetMovingPlatformActorEntry(platform, kNoPlatformRider));
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
    std::optional<Field_PlatformContactPoint> contact;
    {
      const Synced synced(*this);
      contact = Field::LiveMovingPlatformPlacement(support_layer);
    }
    if (!contact.has_value()) return std::nullopt;
    return Field::PlatformContact{.x = contact->x,
                                  .height = contact->height,
                                  .z = contact->z,
                                  .platform_index = contact->platformIndex};
  }
  void SaveGroundedTransform(int id) override {
    Actors_ActorRecord* record = LiveActorRecord(id, operation_);
    if (record == nullptr) return;
    const Synced synced(*this);
    SaveLiveActorGroundedTransform(record);
  }

 private:
  Field_GimmickInstance* KnownGimmick(int id) const {
    Field_GimmickInstance* gimmick = Field_FindGimmickForActorSlot(id);
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
    Battle_Actor* actor = LiveActor(id);
    return actor != nullptr ? &actors_.Attach(*actor) : nullptr;
  }
  ActorVitals* Vitals(int id) override {
    const Battle_Actor* actor = LiveActor(id);
    if (actor == nullptr || actor->stats == nullptr) return nullptr;
    return &vitals_.Attach(*actor->stats);
  }
  void ClearStatuses(int id) override {
    const Battle_Actor* actor = LiveActor(id);
    if (actor == nullptr || actor->stats == nullptr) return;
    const Synced synced(*this);
    VS::Battle::RemoveLiveActorStatuses(kAllStatuses, actor->stats);
    Battle_recoveryResetWord = 0;
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
  static Battle_Actor* LiveActor(int id) {
    return IsBattleActorSlot(id) ? Battle_actors[id] : nullptr;
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
    return slot < std::size(Battle_actors) && Battle_actors[slot] != nullptr;
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
                    Actors_ActorLocation& placement) {
  placement.packedTile.target.tileX = view.tile.x;
  placement.packedTile.target.tileLayer = view.tile.layer;
  placement.packedTile.target.tileZ = view.tile.z;
  placement.packedTile.target.facing = view.tile.spare;
  placement.position = LiveSvector(view.position);
}

int WriteLivePlacement(int actor_id, Actors_ActorLocation* placement_out) {
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

Battle_Actor* LiveActorAtSlot(int actor_id) {
  if (!IsBattleActorSlot(actor_id)) {
    WarnActorRefused("ActorAtSlot", "the actor id is outside Battle_actors");
    return nullptr;
  }
  return Battle_actors[actor_id];
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
                           const Render_ColorCode* companion_state,
                           const Render_ColorCode* companion_color) {
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

int LiveActorPlacement(int actor_id, Actors_ActorLocation* placement_out) {
  return WriteLivePlacement(actor_id, placement_out);
}

int LiveActorPlacement(int actor_id, Actors_ActorLocationQuery* placement_out) {
  return WriteLivePlacement(
      actor_id, placement_out != nullptr ? &placement_out->location : nullptr);
}

void PlaceLiveActorInRoom(int actor_id, const Field_ActorPlacement* placement,
                          int drop_height, int movement_class) {
  if (placement == nullptr) {
    WarnActorRefused("PlaceActorInRoom", "no placement");
    return;
  }
  // The placement's second and fourth bytes are the tile layer and facing.
  const PackedTile tile{
      .x = static_cast<std::uint8_t>(placement->tileX),
      .layer = static_cast<std::uint8_t>(placement->direction),
      .z = static_cast<std::uint8_t>(placement->tileZ),
      .spare = static_cast<std::uint8_t>(placement->height)};
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
                             Actors_ActorLocationQuery* placement_out) {
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
  Actors_TileTarget& target = placement_out->location.packedTile.target;
  target.tileX = tile->x;
  target.tileLayer = tile->layer;
  target.tileZ = tile->z;
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
