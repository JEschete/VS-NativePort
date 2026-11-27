// Hand-maintained (tools/port/owned.csv).
// Actor spawning rules (ActorSpawning.hpp).
#include "Actors/ActorSpawning.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/ActorRoster.hpp"
#include "Actors/BattleActorState.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/CdDrive.hpp"

namespace VS::Actors {
namespace {
using Content::ActorAssetKind;
using Content::ActorAssetRequest;
using Content::AdvanceAddress;

constexpr unsigned kByteBits = 8;
constexpr int kSpawnFlagKindMask = 0x3;
constexpr int kSpawnFlagDrawn = 1;
constexpr int kSpawnFlagFollower = 2;
constexpr int kSpawnFlagFollowerAlt = 3;
constexpr int kInteractionFlagsMask = 0x1C;
constexpr int kRetainAfterDefeatFlag = 0x80;
constexpr int kTextureOverrideFlag = 0x80;
constexpr unsigned kOverrideSubKindShift = 8;
constexpr unsigned kOverrideTextureShift = 12;
constexpr int kOverrideNibble = 0xF;
// A refused model load, like a full queue.
constexpr int kRefusedModelLoad = -1;
constexpr std::uint8_t kSurfaceMask = 1;
// The surface byte's high nibble carries a gimmick's model variant
// (146C.c:5921).
constexpr unsigned kVariantShift = 4;
constexpr unsigned kFacingQuadrantShift = 6;
constexpr std::uint8_t kRecoveryDelayBonusMask = 0x3F;
constexpr std::uint8_t kLimbActionFieldMask = 0xF;
constexpr std::uint8_t kScriptedSpawnFlags = 6;
constexpr int kNoDrawMode = 0;
constexpr int kModelsOnlyDrawMode = 2;
constexpr int kIdleAnimation = 0;
constexpr int kNeutralCombatPose = 0;
constexpr int kBattleHudMode = 1;
constexpr int kBoneTagLimbKind = 2;
constexpr int kBoneTagNameKind = 0;
constexpr int kSpeedScaleNumerator = 9;
constexpr int kSpeedScaleDenominator = 10;
constexpr int kAiGroupBits = 4;
constexpr int kAiGroupReservedBits = 5;
constexpr int kAiLinkedGroupBits = 5;
constexpr unsigned kAiGroupReservedShift = kAiGroupBits;
constexpr unsigned kAiLinkedGroupShift = kAiGroupBits + kAiGroupReservedBits;
constexpr std::size_t kSpawnSettingsWordBytes = 4;
constexpr std::uint8_t kFirstBladeIndex = 1;

constexpr std::uint32_t BitMask(int bits) { return (1U << bits) - 1U; }

[[nodiscard]] std::optional<std::size_t> CheckedSlot(ActorSpawnHost& host,
                                                     std::int64_t actor_id) {
  if (actor_id < 0 ||
      actor_id >= static_cast<std::int64_t>(kBattleActorSlots)) {
    host.Refuse(SpawnRefusal::kActorSlot);
    return std::nullopt;
  }
  return static_cast<std::size_t>(actor_id);
}

[[nodiscard]] bool InstalledActor(ActorSpawnHost& host, std::size_t slot) {
  if (slot >= kBattleActorSlots) {
    host.Refuse(SpawnRefusal::kActorSlot);
    return false;
  }
  if (host.ActorAt(slot) == SpawnedActor::kNone) {
    host.Refuse(SpawnRefusal::kNoActor);
    return false;
  }
  return true;
}

// The listed kinds join the list after Ashley; refuse before anything is
// allocated when they cannot.
[[nodiscard]] bool LeaderLinkAllowed(ActorSpawnHost& host, std::size_t slot,
                                     int flags, bool leader_ends_chain) {
  if ((DispositionFor(slot, flags).kind & kListedActorKinds) == 0) {
    return true;
  }
  if (slot == kAshleySlot) {
    if (leader_ends_chain) {
      return true;
    }
    host.Refuse(SpawnRefusal::kSelfLeader);
    return false;
  }
  if (host.ActorAt(kAshleySlot) == SpawnedActor::kNone) {
    host.Refuse(SpawnRefusal::kNoLeader);
    return false;
  }
  return true;
}

void LinkListed(ActorSpawnHost& host, std::size_t slot, std::uint16_t kind) {
  if ((kind & kListedActorKinds) == 0) {
    return;
  }
  if (slot == kAshleySlot) {
    host.ClearNext(slot);
  } else {
    host.LinkAfterLeader(slot);
  }
}

// The asset queue is drained a frame at a time (146C.c:6111).
void DrainAssetQueue(ActorSpawnHost& host) {
  while (!host.AssetQueueIdle()) {
    host.UpdateAssetQueue();
    host.PumpLoading();
  }
}

class OwnedSpawnAssets {
 public:
  OwnedSpawnAssets(ActorSpawnHost& host, SpawnedActor actor, std::size_t slot)
      : host_(host), actor_(actor), slot_(slot) {}

  [[nodiscard]] bool Submit(ActorRecordPart part, ActorAssetRequest& request) {
    request.target_slot =
        static_cast<std::int8_t>(ActorPartRecordSlot(slot_, part));
    if (tickets_.size() == count_) return false;
    const auto ticket = host_.SubmitOwnedAsset(request, actor_);
    if (!ticket) return false;
    tickets_.at(count_++) = *ticket;
    return true;
  }

  [[nodiscard]] bool SubmitSequence(ActorAssetRequest& request) {
    return Submit(ActorRecordPart::kBody, request);
  }

  [[nodiscard]] bool Finish() {
    if (count_ == 0) return true;
    DrainAssetQueue(host_);
    bool loaded = true;
    bool pending = false;
    for (std::size_t i = 0; i < count_; ++i) {
      const auto outcome = host_.AssetCompletion(tickets_.at(i)).outcome;
      loaded = outcome == Content::ActorAssetOutcome::kLoaded && loaded;
      pending = outcome == Content::ActorAssetOutcome::kPending || pending;
    }
    if (!pending) Forget();
    return loaded;
  }

  [[nodiscard]] bool CancelOrFinish() {
    if (host_.CancelOwnedAssets(actor_)) {
      Forget();
      return true;
    }
    static_cast<void>(Finish());
    return count_ == 0;
  }

 private:
  void Forget() {
    for (std::size_t i = 0; i < count_; ++i) {
      host_.ForgetAsset(tickets_.at(i));
    }
    count_ = 0;
  }

  static constexpr std::size_t kCapacity = 5;
  ActorSpawnHost& host_;
  SpawnedActor actor_;
  std::size_t slot_;
  std::array<Content::ActorAssetTicket, kCapacity> tickets_{};
  std::size_t count_ = 0;
};

[[nodiscard]] std::int16_t RollStat(ActorSpawnHost& host, int value,
                                    bool randomize) {
  if (randomize) {
    value += host.RandomAveraged(kStatRollRange) - kStatRollBias;
  }
  return ClampInitialStat(value);
}

[[nodiscard]] std::uint32_t SettledDisplayFlags(std::int32_t weapon_drawn) {
  return (weapon_drawn & 1) != 0 ? kWeaponDrawnDisplayFlags
                                 : kSheathedDisplayFlags;
}

void ApplyInitialStats(ActorSpawnHost& host, std::size_t slot,
                       const CharacterTemplate& character, bool randomize) {
  InitialStats stats;
  stats.actor_id = static_cast<std::uint8_t>(slot);
  stats.name = character.name;
  stats.hp = RollStat(host, character.hp, randomize);
  // A template without MP keeps none and draws no roll.
  stats.mp = character.mp != 0 ? RollStat(host, character.mp, randomize)
                               : std::int16_t{0};
  stats.strength = RollStat(host, character.strength, randomize);
  stats.intelligence = RollStat(host, character.intelligence, randomize);
  stats.agility = RollStat(host, character.agility, randomize);
  stats.gait = {.movement_class = character.material,
                .gait_class = character.gait_class};
  stats.walk_speed = ScaledSpeed(character.walk_speed);
  stats.run_speed = ScaledSpeed(character.run_speed);
  stats.enemy_class = character.enemy_class;
  stats.reach = static_cast<std::uint8_t>(character.reach & kReachMask);
  stats.recovery_delay_bonus = static_cast<std::uint8_t>(
      character.recovery_delay_bonus & kRecoveryDelayBonusMask);
  stats.mobility_limbs = character.mobility_limbs;
  stats.default_target_limb = character.default_target_limb;

  const WeaponRange range =
      host.BuildTemplateEquipment(slot, character.equipment);

  for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
    const LimbTemplate& source = character.limbs.at(limb);
    LimbInitialStats& target = stats.limbs.at(limb);
    target.hp = source.hp;
    target.agility_defense_bonus = source.agility_defense_bonus;
    // A limb without a tagged bone anchors to bone 0 and has no name.
    const int bone =
        host.FindBoneTag(slot, static_cast<int>(limb), kBoneTagLimbKind);
    if (bone >= 0) {
      target.anchor_bone = static_cast<std::uint8_t>(bone);
      target.name_index =
          static_cast<std::uint8_t>(host.BoneTag(slot, bone, kBoneTagNameKind));
    }
    target.chain_evasion = source.chain_evasion;
    std::ranges::copy(source.types, target.types.begin());
    std::copy_n(source.affinities.begin(), kInitialLimbAffinities,
                target.affinities.begin());
    for (std::size_t action = 0; action < kLimbActions; ++action) {
      const LimbActionTemplate& from = source.actions.at(action);
      stats.limb_actions.at(limb).at(action) = {
          .action_id = from.action_id,
          .index = static_cast<std::uint8_t>(from.index & kLimbActionFieldMask),
          .effect_model = static_cast<std::uint8_t>(from.effect_model &
                                                    kLimbActionFieldMask)};
    }
    stats.dp_splash.at(limb) = source.dp_splash;
  }
  stats.status_immunities = character.status_immunities;
  stats.statuses = InitialStatuses(character.status_immunities);

  host.WriteInitialStats(slot, stats);
  host.WriteAttackReach(slot, AttackReachFor(range, stats.reach));
}

void ApplyDefaults(ActorSpawnHost& host, std::size_t slot, int model_id,
                   int blade_id, int shield_id, int flags) {
  const SpawnDisposition disposition = DispositionFor(slot, flags);
  host.WriteDefaults(slot, {.id = static_cast<std::int32_t>(slot),
                            .model_id = static_cast<std::uint16_t>(model_id),
                            .blade_id = static_cast<std::uint8_t>(blade_id),
                            .shield_id = static_cast<std::uint8_t>(shield_id),
                            .disposition = disposition,
                            .interaction_flags = static_cast<std::uint8_t>(
                                flags & kInteractionFlagsMask),
                            .gait = DefaultMovementGait(model_id)});
  if (disposition.weapon_drawn) {
    host.DrawWeapon(slot, kNoDrawMode);
  }
}

void ApplyTemplate(ActorSpawnHost& host, std::size_t slot,
                   const CharacterTemplate& character, int blade_id,
                   int shield_id, int flags, bool randomize) {
  const SpawnDisposition disposition = DispositionFor(slot, flags);
  host.WriteTemplateIdentity(
      slot, {.id = static_cast<std::int32_t>(slot),
             .model_id = character.model_id,
             .sub_type = character.sub_type,
             .blade_id = static_cast<std::uint8_t>(blade_id),
             .shield_id = static_cast<std::uint8_t>(shield_id),
             .disposition = disposition,
             .retain_after_defeat = (flags & kRetainAfterDefeatFlag) != 0,
             .interaction_flags =
                 static_cast<std::uint8_t>(flags & kInteractionFlagsMask)});
  if (disposition.weapon_drawn) {
    host.DrawWeapon(slot, kNoDrawMode);
  }
  ApplyInitialStats(host, slot, character, randomize);
}

// Native (#48): the request names no memory; the owning actor provides its
// model memory when the record binds.
[[nodiscard]] ActorAssetRequest ModelLoadRequest(int index, int model_id,
                                                 SpawnPlacement placement,
                                                 int flags) {
  ActorAssetRequest request{};
  request.kind = static_cast<std::uint8_t>(ActorAssetKind::kActorModelLoad);
  request.target_slot = static_cast<std::int8_t>(index);
  request.model_id = static_cast<std::int16_t>(model_id);
  request.placement = PackPlacement(placement);
  if ((flags & kTextureOverrideFlag) != 0) {
    request.sub_kind = static_cast<std::uint8_t>(
        (static_cast<unsigned>(flags) >> kOverrideSubKindShift) &
        kOverrideNibble);
    request.texture_slot = static_cast<std::uint8_t>(
        (static_cast<unsigned>(flags) >> kOverrideTextureShift) &
        kOverrideNibble);
  } else {
    request.sub_kind = kDefaultSubKind;
  }
  request.variant = 0;
  request.material = kModelLoadMaterial;
  return request;
}

// Native (#48): the actor owns its body load, as it owns its equipment loads.
[[nodiscard]] int QueueModelLoad(ActorSpawnHost& host, SpawnedActor actor,
                                 int index, int model_id,
                                 SpawnPlacement placement, int flags) {
  return host.QueueOwnedAsset(
      ModelLoadRequest(index, model_id, placement, flags), actor);
}

void QueueEquipment(ActorSpawnHost& host, SpawnedActor actor, int actor_id,
                    int model_id, EquipmentHand hand, int material) {
  if (model_id == kUnchangedEquipment) {
    return;
  }
  const std::int64_t slot = (static_cast<std::int64_t>(actor_id) * 2) +
                            static_cast<std::int64_t>(hand);
  if (slot < 0 || slot >= kEquipmentModelSlots) {
    host.Refuse(SpawnRefusal::kEquipmentSlot);
    return;
  }
  host.ReleaseWeaponModel(static_cast<int>(slot));
  if (model_id == 0) {
    return;
  }
  ActorAssetRequest request{};
  request.kind = static_cast<std::uint8_t>(ActorAssetKind::kWeaponModelLoad);
  request.target_slot = static_cast<std::int8_t>(slot);
  request.model_id = static_cast<std::int16_t>(model_id);
  request.sub_kind = static_cast<std::uint8_t>(actor_id);
  request.texture_slot =
      hand == EquipmentHand::kWeapon ? kWeaponTextureSlot : kShieldTextureSlot;
  request.material = static_cast<std::uint8_t>(material);
  // Native (#48): scripts load equipment for published actors, so the actor
  // owns the load and removal cancels it before the equipment memory goes.
  // The actor provides that memory when the record binds.
  static_cast<void>(host.QueueOwnedAsset(request, actor));
}

[[nodiscard]] SpawnedActor AllocateEquipped(
    ActorSpawnHost& host, std::size_t slot, int model_id, int blade_model,
    int blade_material, int shield_model, int shield_material,
    SpawnPlacement placement, int flags) {
  const SpawnedActor actor = host.Allocate(ActorShape::kEquipped, slot);
  if (actor == SpawnedActor::kNone) {
    host.Refuse(SpawnRefusal::kAllocation);
    return SpawnedActor::kNone;
  }
  const std::uint8_t category =
      EquippedWeaponCategory(blade_model, host.WeaponCategoryBounds());
  host.SetEquippedWeaponCategory(actor, category);
  const int actor_id = static_cast<int>(slot);
  static_cast<void>(
      QueueModelLoad(host, actor, actor_id, model_id, placement, flags));
  if (model_id != kBareModel) {
    host.QueueBaseSequence(actor, slot);
    QueueEquipment(host, actor, actor_id, blade_model, EquipmentHand::kWeapon,
                   blade_material);
    QueueEquipment(host, actor, actor_id, shield_model, EquipmentHand::kShield,
                   shield_material);
    host.QueueWeaponSequence(actor, slot, category, flags);
  }
  return actor;
}

void Register(ActorSpawnHost& host, SpawnedActor actor, std::size_t slot,
              int model_id, int blade_id, int shield_id, int flags) {
  host.PlayAnimation(slot, kIdleAnimation);
  host.SetDisplayFlags(slot, kLoadingDisplayFlags);
  host.Install(slot, actor);
  ApplyDefaults(host, slot, model_id, blade_id, shield_id, flags);
  host.SetWalkRunSpeeds(slot, kRegisteredWalkSpeed, kRegisteredRunSpeed);
  host.SetCombatPose(slot, kNeutralCombatPose);
  const SpawnedState state = host.State(slot);
  LinkListed(host, slot, state.kind);
  host.SetDisplayFlags(slot, SettledDisplayFlags(state.weapon_drawn));
  host.SetTemplateWord(slot, 0);
  static_cast<void>(host.CreateAiState(slot, kFreshAiFlags));
}

// VS::Field::zoneContext's one-file ZUD cache (146C.c:6604-6627): the cached
// file when it is `zud`, else a read of `zud` in its place.
[[nodiscard]] std::optional<ZudBlock> LoadZud(ActorSpawnHost& host, int zud) {
  if (const std::optional<int> cached = host.CachedZud()) {
    if (*cached == zud) {
      return host.CachedZudBlock();
    }
    host.FreeZud();
  }
  const Content::DiscFileExtent file = host.ZudFile(zud);
  if (host.ZudReadPending()) {
    host.Diagnostic(kZudReadPendingDiagnostic);
  }
  if (!host.AllocateZud(file)) {
    host.Refuse(SpawnRefusal::kZudAllocation);
    return std::nullopt;
  }
  host.BeginZudRead(file);
  ZudReadState state = host.PollZudRead();
  while (state == ZudReadState::kPending) {
    host.PumpLoading();
    state = host.PollZudRead();
  }
  const bool loaded = state == ZudReadState::kLoaded;
  host.FinishZudRead(zud, loaded);
  if (!loaded) {
    host.Refuse(SpawnRefusal::kZudRead);
    return std::nullopt;
  }
  return host.CachedZudBlock();
}

[[nodiscard]] std::int32_t ReadWord(std::span<const std::byte> bytes,
                                    std::size_t offset) {
  std::uint32_t word = 0;
  for (std::size_t i = 0; i < sizeof word; ++i) {
    word |= static_cast<std::uint32_t>(bytes[offset + i]) << (kByteBits * i);
  }
  return static_cast<std::int32_t>(word);
}

[[nodiscard]] Content::AssetAddress SectionAddress(const ZudBlock& block,
                                                   ZudSection section) {
  return AdvanceAddress(block.address, section.offset);
}
}  // namespace

const char* SpawnRefusalText(SpawnRefusal refusal) {
  switch (refusal) {
    case SpawnRefusal::kActorSlot:
      return "actor id outside the actor table";
    case SpawnRefusal::kNoActor:
      return "no actor in the slot";
    case SpawnRefusal::kEquipmentSlot:
      return "equipment model slot outside the weapon model table";
    case SpawnRefusal::kAllocation:
      return "actor allocation refused";
    case SpawnRefusal::kAssetQueue:
      return "actor asset request submission or completion failed";
    case SpawnRefusal::kZudIndex:
      return "ZUD index outside the zone's enemy files";
    case SpawnRefusal::kZudAllocation:
      return "ZUD allocation refused";
    case SpawnRefusal::kZudRead:
      return "ZUD read failed";
    case SpawnRefusal::kZudSection:
      return "ZUD section outside the file";
    case SpawnRefusal::kNoLeader:
      return "listed actor without Ashley's actor to follow";
    case SpawnRefusal::kSelfLeader:
      return "room enemy in Ashley's slot would follow itself";
    case SpawnRefusal::kAshleyBlade:
      return "Ashley's weapon names no blade in the inventory";
    case SpawnRefusal::kAshleyActor:
      return "Ashley's actor could not be created";
    case SpawnRefusal::kNoCharacter:
      return "no character actor to restore";
  }
  return "unknown refusal";
}

SpawnSettings DecodeSpawnSettings(
    std::span<const std::uint8_t, kSpawnSettingsBytes> bytes) {
  std::uint32_t group_word = 0;
  std::uint32_t extra_word = 0;
  for (std::size_t i = 0; i < kSpawnSettingsWordBytes; ++i) {
    group_word |= static_cast<std::uint32_t>(bytes[i]) << (kByteBits * i);
    extra_word |= static_cast<std::uint32_t>(bytes[kSpawnSettingsWordBytes + i])
                  << (kByteBits * i);
  }
  return {
      .ai_group = static_cast<std::uint8_t>(group_word & BitMask(kAiGroupBits)),
      .group_reserved =
          static_cast<std::uint8_t>((group_word >> kAiGroupReservedShift) &
                                    BitMask(kAiGroupReservedBits)),
      .ai_linked_group = static_cast<std::uint8_t>(
          (group_word >> kAiLinkedGroupShift) & BitMask(kAiLinkedGroupBits)),
      .linked_reserved =
          group_word >> (kAiLinkedGroupShift + kAiLinkedGroupBits),
      .extra = static_cast<std::int32_t>(extra_word)};
}

std::optional<ZudLayout> ParseZudHeader(std::span<const std::byte> file) {
  if (file.size() < kZudHeaderBytes) {
    return std::nullopt;
  }
  // ZudHeader (146C.c:128): two id words, then offset and size pairs.
  constexpr std::size_t kShape = 0x08;
  constexpr std::size_t kWeapon = 0x10;
  constexpr std::size_t kShield = 0x18;
  constexpr std::size_t kCommonAnimation = 0x20;
  constexpr std::size_t kBattleAnimation = 0x28;
  constexpr std::size_t kSize = 4;
  const auto section = [file](std::size_t at) {
    return ZudSection{.offset = ReadWord(file, at),
                      .size = ReadWord(file, at + kSize)};
  };
  return ZudLayout{.shape = section(kShape),
                   .weapon = section(kWeapon),
                   .shield = section(kShield),
                   .common_animation = section(kCommonAnimation),
                   .battle_animation = section(kBattleAnimation)};
}

bool ZudSectionFits(ZudSection section, std::size_t file_bytes) {
  if (section.offset < 0 || section.size < 0) {
    return false;
  }
  const auto offset = static_cast<std::size_t>(section.offset);
  const auto size = static_cast<std::size_t>(section.size);
  return offset <= file_bytes && size <= file_bytes - offset;
}

SpawnDisposition DispositionFor(std::size_t slot, int flags) {
  if (slot == kAshleySlot) {
    return {.kind = kLeaderActorKind};
  }
  if (slot == 1) {
    return {.kind = kNeutralActorKind};
  }
  switch (flags & kSpawnFlagKindMask) {
    case kSpawnFlagDrawn:
      return {.kind = static_cast<std::uint16_t>(kHostileActorKind),
              .weapon_drawn = true};
    case kSpawnFlagFollower:
    case kSpawnFlagFollowerAlt:
      return {.kind = static_cast<std::uint16_t>(kPeacefulActorKind)};
    default:
      return {.kind = kNeutralActorKind};
  }
}

MovementGait DefaultMovementGait(int model_id) {
  // func_80076D50 (146C.c:6341): two models move on their own classes.
  constexpr int kWideStrideModel = 41;
  constexpr int kHeavyStrideModel = 48;
  constexpr MovementGait kWideStride{.movement_class = 16, .gait_class = 32};
  constexpr MovementGait kHeavyStride{.movement_class = 2, .gait_class = 8};
  constexpr MovementGait kDefaultStride{.movement_class = 2, .gait_class = 4};
  if (model_id == kWideStrideModel) {
    return kWideStride;
  }
  if (model_id == kHeavyStrideModel) {
    return kHeavyStride;
  }
  return kDefaultStride;
}

std::uint8_t EquippedWeaponCategory(
    int blade_model,
    std::span<const std::uint8_t, kWeaponCategoryBounds> bounds) {
  if (blade_model == 0) {
    return kUnarmedWeaponCategory;
  }
  for (std::size_t category = 1; category < kWeaponCategoryBounds; ++category) {
    if (bounds[category] >= blade_model) {
      return static_cast<std::uint8_t>(category);
    }
  }
  return static_cast<std::uint8_t>(kWeaponCategoryBounds);
}

std::int16_t ClampInitialStat(int value) {
  return static_cast<std::int16_t>(std::clamp(value, 0, kMaxInitialStat));
}

std::uint8_t ScaledSpeed(std::uint8_t speed) {
  return static_cast<std::uint8_t>((speed * kSpeedScaleNumerator) /
                                   kSpeedScaleDenominator);
}

std::int32_t InitialStatuses(std::int32_t immunities) {
  constexpr std::uint32_t kSignBit = 0x80000000U;
  return immunities < 0 ? static_cast<std::int32_t>(kSignBit) : 0;
}

AttackReach AttackReachFor(WeaponRange range, std::uint8_t reach) {
  const int bonus = reach & kReachMask;
  return {.range = static_cast<std::uint8_t>(range.range + bonus),
          .range_y = static_cast<std::uint8_t>(range.range_y + bonus),
          .range_z = static_cast<std::uint8_t>(range.range_z + bonus),
          .shape_angle = static_cast<std::uint8_t>(range.attack_shape_angle &
                                                   kAttackShapeAngleMask)};
}

std::uint8_t AuxiliaryTextureSlot(int actor_id) {
  // The original rounded the division by 256 toward zero.
  constexpr int kTextureSlotDivisor = 256;
  return static_cast<std::uint8_t>(actor_id / kTextureSlotDivisor);
}

SpawnPlacement RoomEnemyPlacement(
    const RoomSpawn& spawn, const std::optional<RememberedSpawn>& remembered) {
  if (remembered) {
    return {.tile_x = remembered->placement.tile_x,
            .surface = static_cast<std::uint8_t>(remembered->placement.surface &
                                                 kSurfaceMask),
            .tile_z = remembered->placement.tile_z,
            .facing = remembered->placement.facing};
  }
  return {.tile_x = spawn.placement.tile_x,
          .surface =
              static_cast<std::uint8_t>(spawn.placement.surface & kSurfaceMask),
          .tile_z = spawn.placement.tile_z,
          .facing = static_cast<std::uint8_t>(spawn.placement.facing
                                              << kFacingQuadrantShift)};
}

std::optional<AshleyEquipment> ChooseAshleyEquipment(
    const AshleyInventory& inventory) {
  AshleyEquipment equipment;
  for (std::size_t i = 0; i < kInventoryWeapons; ++i) {
    if (inventory.weapons.at(i).equipped) {
      equipment.weapon = i;
    }
  }
  if (equipment.weapon) {
    const std::uint8_t blade = inventory.weapons.at(*equipment.weapon).blade;
    if (blade < kFirstBladeIndex || blade > kInventoryBlades) {
      return std::nullopt;
    }
    const InventoryModelView& part = inventory.blades.at(blade - 1U);
    equipment.blade_model = part.model;
    equipment.blade_material = part.material;
  }
  for (std::size_t i = 0; i < kInventoryShields; ++i) {
    const InventoryModelView& shield = inventory.shields.at(i);
    if (shield.equipped) {
      equipment.shield = i;
      equipment.shield_model = shield.model;
      equipment.shield_material = shield.material;
    }
  }
  return equipment;
}

// decomp: BATTLE.PRG func_800765B0 (src/BATTLE/BATTLE.PRG/146C.c:5877)
SpawnedActor CreateAuxiliaryActor(ActorSpawnHost& host, int index, int actor_id,
                                  SpawnPlacement placement, int material) {
  if (index == kAnyAuxiliarySlot) {
    std::optional<int> free;
    for (std::size_t slot = kFirstAuxiliarySlot; slot < kSpawnActorSlots;
         ++slot) {
      if (host.ActorAt(slot) == SpawnedActor::kNone) {
        free = static_cast<int>(slot);
        break;
      }
    }
    if (!free) {
      host.Diagnostic(kNoAuxiliarySlotDiagnostic);
      return SpawnedActor::kNone;
    }
    index = *free;
  }
  const std::optional<std::size_t> slot = CheckedSlot(host, index);
  if (!slot || host.ActorAt(*slot) != SpawnedActor::kNone) {
    return SpawnedActor::kNone;
  }
  const SpawnedActor actor = host.Allocate(ActorShape::kAuxiliary, *slot);
  if (actor == SpawnedActor::kNone) {
    host.Refuse(SpawnRefusal::kAllocation);
    return SpawnedActor::kNone;
  }
  host.WriteAuxiliaryIdentity(actor, {.id = index});

  SpawnPlacement queued = placement;
  queued.surface &= kSurfaceMask;
  ActorAssetRequest request{};
  request.kind = static_cast<std::uint8_t>(ActorAssetKind::kGimmick);
  request.placement = PackPlacement(queued);
  request.sub_kind = static_cast<std::uint8_t>(actor_id);
  request.texture_slot = AuxiliaryTextureSlot(actor_id);
  request.material = static_cast<std::uint8_t>(material);
  request.variant =
      static_cast<std::uint8_t>(placement.surface >> kVariantShift);
  OwnedSpawnAssets assets(host, actor, *slot);
  if (!assets.Submit(ActorRecordPart::kGimmick, request) || !assets.Finish()) {
    host.Refuse(SpawnRefusal::kAssetQueue);
    if (assets.CancelOrFinish()) host.DiscardUninstalled(actor);
    return SpawnedActor::kNone;
  }

  host.Install(*slot, actor);
  if (host.ActorAt(*slot) != actor) {
    host.Refuse(SpawnRefusal::kAllocation);
    host.DiscardUninstalled(actor);
    return SpawnedActor::kNone;
  }
  host.InitializePlaceholder(*slot, actor_id);
  static_cast<void>(host.CreateAiState(*slot, kFreshAiFlags));
  host.SetDisplayFlags(*slot, kWeaponDrawnDisplayFlags);
  return actor;
}

// decomp: BATTLE.PRG func_80076784 (src/BATTLE/BATTLE.PRG/146C.c:5941)
void ApplyActorInitialStats(ActorSpawnHost& host, std::size_t slot,
                            const CharacterTemplate& character,
                            bool randomize) {
  if (InstalledActor(host, slot)) {
    ApplyInitialStats(host, slot, character, randomize);
  }
}

// decomp: BATTLE.PRG func_80076D50 (src/BATTLE/BATTLE.PRG/146C.c:6104)
void InitializeActorDefaults(ActorSpawnHost& host, std::size_t slot,
                             int model_id, int blade_id, int shield_id,
                             int flags) {
  if (InstalledActor(host, slot)) {
    ApplyDefaults(host, slot, model_id, blade_id, shield_id, flags);
  }
}

std::optional<ActorRecordPart> ActorRecordPartAt(
    ActorShape shape, std::size_t slot, Content::ActorRecordTable table,
    std::size_t record) {
  static constexpr std::array kEquippedParts{ActorRecordPart::kBody,
                                             ActorRecordPart::kWeapon,
                                             ActorRecordPart::kShield};
  static constexpr std::array kSecondaryBodyParts{ActorRecordPart::kBody,
                                                  ActorRecordPart::kSecondBody};
  static constexpr std::array kAuxiliaryParts{ActorRecordPart::kGimmick};
  std::span<const ActorRecordPart> parts;
  switch (shape) {
    case ActorShape::kEquipped:
      parts = kEquippedParts;
      break;
    case ActorShape::kSecondaryBody:
      parts = kSecondaryBodyParts;
      break;
    case ActorShape::kAuxiliary:
      parts = kAuxiliaryParts;
      break;
  }
  for (const ActorRecordPart part : parts) {
    if (ActorPartTable(part) == table &&
        ActorPartRecordSlot(slot, part) == record) {
      return part;
    }
  }
  return std::nullopt;
}

std::optional<ActorRecordPart> OwnedActorRecordPart(
    const ActorInstanceTable& handles, SpawnedActor actor,
    Content::ActorRecordTable table, int record) {
  const std::optional<std::size_t> slot = handles.Slot(actor);
  const std::optional<ActorDefinition> definition = handles.Definition(actor);
  if (!slot || !definition || record < 0) {
    return std::nullopt;
  }
  return ActorRecordPartAt(definition->shape, *slot, table,
                           static_cast<std::size_t>(record));
}

// decomp: BATTLE.PRG func_80076F24 (src/BATTLE/BATTLE.PRG/146C.c:6179)
void InitializeActorFromTemplate(ActorSpawnHost& host, std::size_t slot,
                                 const CharacterTemplate& character,
                                 int blade_id, int shield_id, int flags,
                                 bool randomize) {
  if (InstalledActor(host, slot)) {
    ApplyTemplate(host, slot, character, blade_id, shield_id, flags, randomize);
  }
}

// decomp: BATTLE.PRG func_80077078 (src/BATTLE/BATTLE.PRG/146C.c:6234)
int QueueActorModelLoad(ActorSpawnHost& host, SpawnedActor actor, int index,
                        int model_id, SpawnPlacement placement, int flags) {
  if (actor == SpawnedActor::kNone) {
    host.Refuse(SpawnRefusal::kNoActor);
    return kRefusedModelLoad;
  }
  return QueueModelLoad(host, actor, index, model_id, placement, flags);
}

// decomp: BATTLE.PRG setActorWeaponModel (src/BATTLE/BATTLE.PRG/146C.c:6270;
// was func_80077130)
void QueueEquipmentModelLoad(ActorSpawnHost& host, SpawnedActor actor,
                             int actor_id, int model_id, EquipmentHand hand,
                             int material) {
  if (actor == SpawnedActor::kNone) {
    host.Refuse(SpawnRefusal::kNoActor);
    return;
  }
  QueueEquipment(host, actor, actor_id, model_id, hand, material);
}

// decomp: BATTLE.PRG func_80077240 (src/BATTLE/BATTLE.PRG/146C.c:6307)
SpawnedActor AllocateActorWithEquipment(ActorSpawnHost& host, int actor_id,
                                        int model_id, int blade_model,
                                        int blade_material, int shield_model,
                                        int shield_material,
                                        SpawnPlacement placement, int flags) {
  const std::optional<std::size_t> slot = CheckedSlot(host, actor_id);
  if (!slot || host.ActorAt(*slot) != SpawnedActor::kNone ||
      *slot >= kEquippedActorSlots) {
    return SpawnedActor::kNone;
  }
  return AllocateEquipped(host, *slot, model_id, blade_model, blade_material,
                          shield_model, shield_material, placement, flags);
}

// decomp: BATTLE.PRG func_800773BC (src/BATTLE/BATTLE.PRG/146C.c:6352)
void RegisterLoadedActor(ActorSpawnHost& host, SpawnedActor actor,
                         std::size_t slot, int model_id, int blade_id,
                         int shield_id, int flags) {
  if (slot >= kBattleActorSlots) {
    host.Refuse(SpawnRefusal::kActorSlot);
    return;
  }
  if (actor == SpawnedActor::kNone) {
    host.Refuse(SpawnRefusal::kNoActor);
    return;
  }
  if (!LeaderLinkAllowed(host, slot, flags, true)) {
    return;
  }
  Register(host, actor, slot, model_id, blade_id, shield_id, flags);
}

// decomp: BATTLE.PRG func_800774FC (src/BATTLE/BATTLE.PRG/146C.c:6377)
// The original allocated through func_80077240 and linked Ashley's list while
// registering; the leader link is checked before AllocateActorWithEquipment
// so a refused link allocates nothing.
SpawnedActor CreateActorWithEquipment(ActorSpawnHost& host, int actor_id,
                                      int model_id, int blade_model,
                                      int blade_material, int shield_model,
                                      int shield_material,
                                      SpawnPlacement placement, int flags) {
  const std::optional<std::size_t> slot = CheckedSlot(host, actor_id);
  if (!slot || host.ActorAt(*slot) != SpawnedActor::kNone ||
      *slot >= kEquippedActorSlots) {
    return SpawnedActor::kNone;
  }
  if (!LeaderLinkAllowed(host, *slot, flags, true)) {
    return SpawnedActor::kNone;
  }
  const SpawnedActor actor = AllocateActorWithEquipment(
      host, actor_id, model_id, blade_model, blade_material, shield_model,
      shield_material, placement, flags);
  if (actor == SpawnedActor::kNone) {
    return SpawnedActor::kNone;
  }
  DrainAssetQueue(host);
  Register(host, actor, *slot, model_id, blade_model, shield_model, flags);
  return actor;
}

namespace {

// The parts of a room enemy's ZUD file its spawn queues.
struct RoomEnemyParts {
  bool second_body = false;
  bool bare = false;
  bool weapon = false;
  bool shield = false;
  bool battle_animation = false;
  bool common_animation = false;
};

[[nodiscard]] bool RoomEnemySectionsFit(const ZudLayout& layout,
                                        const RoomEnemyParts& parts,
                                        std::size_t file_bytes) {
  return ZudSectionFits(layout.shape, file_bytes) &&
         (!parts.weapon || ZudSectionFits(layout.weapon, file_bytes)) &&
         (!parts.shield || ZudSectionFits(layout.shield, file_bytes)) &&
         (!parts.battle_animation ||
          ZudSectionFits(layout.battle_animation, file_bytes)) &&
         (!parts.common_animation ||
          ZudSectionFits(layout.common_animation, file_bytes));
}

// The second body, equipment and animation requests after the body's, each
// carrying the previous request's fields as the original's stack record did.
[[nodiscard]] bool QueueRoomEnemyParts(
    OwnedSpawnAssets& assets, ActorAssetRequest& request, std::size_t slot,
    const ZudBlock& block, const ZudLayout& layout,
    const CharacterTemplate& character, const RoomEnemyParts& parts,
    std::uint8_t body_model, std::uint8_t category) {
  const auto slot_byte = static_cast<std::uint8_t>(slot);
  if (parts.second_body) {
    request.kind = static_cast<std::uint8_t>(ActorAssetKind::kActorModelLoad);
    request.model_id = static_cast<std::int16_t>(body_model + 1);
    request.sub_kind = slot_byte;
    request.texture_slot = kSecondBodyTextureSlot;
    request.variant = 0;
    if (!assets.Submit(ActorRecordPart::kSecondBody, request)) return false;
  } else {
    if (parts.weapon) {
      request.kind =
          static_cast<std::uint8_t>(ActorAssetKind::kWeaponModelParse);
      request.data = SectionAddress(block, layout.weapon);
      request.model_id = character.blade_model;
      request.sub_kind = slot_byte;
      request.texture_slot = kWeaponTextureSlot;
      request.material = character.weapon_material;
      if (!assets.Submit(ActorRecordPart::kWeapon, request)) return false;
    }
    if (parts.shield) {
      request.kind =
          static_cast<std::uint8_t>(ActorAssetKind::kWeaponModelParse);
      request.data = SectionAddress(block, layout.shield);
      request.model_id = character.shield_model;
      request.sub_kind = slot_byte;
      request.texture_slot = kShieldTextureSlot;
      request.material = character.shield_material;
      if (!assets.Submit(ActorRecordPart::kShield, request)) return false;
    }
  }
  if (parts.battle_animation) {
    request.kind = static_cast<std::uint8_t>(ActorAssetKind::kSequenceParse);
    request.data = SectionAddress(block, layout.battle_animation);
    request.sub_kind = 0;
    request.model_id = category;
    if (!assets.SubmitSequence(request)) return false;
  }
  if (parts.common_animation) {
    request.kind = static_cast<std::uint8_t>(ActorAssetKind::kSequenceParse);
    request.data = SectionAddress(block, layout.common_animation);
    request.sub_kind = 0;
    request.model_id = 0;
    if (!assets.SubmitSequence(request)) return false;
  }
  return true;
}

void ReleaseRoomEnemyAssets(ActorSpawnHost& host, std::size_t slot,
                            const RoomEnemyParts& parts) {
  const auto release_record = [&](ActorRecordPart part) {
    const std::size_t record = ActorPartRecordSlot(slot, part);
    if (record < Content::kActorRecordSlots) {
      host.ReleaseActorModel(static_cast<int>(record));
    }
  };
  const auto release_equipment = [&](ActorRecordPart part) {
    const std::size_t record = ActorPartRecordSlot(slot, part);
    if (record < Content::kWeaponModelSlots) {
      host.ReleaseWeaponModel(static_cast<int>(record));
    }
  };
  release_record(ActorRecordPart::kBody);
  if (parts.second_body) {
    release_record(ActorRecordPart::kSecondBody);
    return;
  }
  if (parts.weapon) release_equipment(ActorRecordPart::kWeapon);
  if (parts.shield) release_equipment(ActorRecordPart::kShield);
}

void DiscardRoomEnemy(ActorSpawnHost& host, SpawnedActor actor,
                      std::size_t slot, const RoomEnemyParts& parts) {
  ReleaseRoomEnemyAssets(host, slot, parts);
  host.DiscardUninstalled(actor);
}

// Installs the loaded enemy, applies its template and restores what its room
// remembered of it.
[[nodiscard]] bool SettleRoomEnemy(
    ActorSpawnHost& host, std::size_t slot, SpawnedActor actor,
    const RoomSpawn& spawn, const CharacterTemplate& character,
    const std::optional<RememberedSpawn>& remembered, int flags) {
  host.Install(slot, actor);
  if (host.ActorAt(slot) != actor) {
    return false;
  }
  ApplyTemplate(host, slot, character, character.blade_model,
                character.shield_model, flags, true);
  const SpawnedState state = host.State(slot);
  host.SetDisplayFlags(slot, SettledDisplayFlags(state.weapon_drawn));
  host.PlayAnimation(slot, kIdleAnimation);
  host.SetWalkRunSpeeds(slot, state.walk_speed << kSpeedFractionBits,
                        state.run_speed << kSpeedFractionBits);
  if ((state.kind & kListedActorKinds) != 0) {
    host.LinkAfterLeader(slot);
  }
  host.WriteSpawnDetails(slot, {.facing = spawn.facing,
                                .spawn_byte = spawn.combat_pose,
                                .room_spawn_index = spawn.index,
                                .drop = spawn.drop,
                                .second_drop = spawn.second_drop,
                                .random_drop = spawn.random_drop,
                                .settings = DecodeSpawnSettings(spawn.settings),
                                .ai_settings = character.ai_settings});

  int ai_state = 0;
  if (remembered) {
    host.RestoreRemembered(slot, *remembered);
    host.InflictStatuses(slot, remembered->statuses);
    ai_state = host.CreateAiState(slot, remembered->ai_flags);
  } else {
    ai_state = host.CreateAiState(slot, kFreshAiFlags);
  }
  host.SetCombatPose(slot,
                     ai_state == 0 ? spawn.combat_pose : kNeutralCombatPose);
  return true;
}
}  // namespace

// decomp: BATTLE.PRG func_800775C0 (src/BATTLE/BATTLE.PRG/146C.c:6578)
SpawnedActor SpawnRoomEnemy(ActorSpawnHost& host, int actor_id,
                            const RoomSpawn& spawn,
                            const CharacterTemplate& character, int flags) {
  const std::optional<std::size_t> checked = CheckedSlot(host, actor_id);
  if (!checked || host.ActorAt(*checked) != SpawnedActor::kNone) {
    return SpawnedActor::kNone;
  }
  const std::size_t slot = *checked;
  if (!LeaderLinkAllowed(host, slot, flags, false)) {
    return SpawnedActor::kNone;
  }
  const int zud = character.zud_index;
  if (zud < 0 || zud >= host.ZoneEnemyCount()) {
    host.Refuse(SpawnRefusal::kZudIndex);
    return SpawnedActor::kNone;
  }
  RoomEnemyParts parts;
  parts.second_body = host.UsesSecondaryBodyModel(character.model_id);
  parts.bare = character.model_id == kBareModel;

  const std::optional<ZudBlock> block = LoadZud(host, zud);
  if (!block) {
    return SpawnedActor::kNone;
  }
  const std::optional<ZudLayout> layout = ParseZudHeader(block->bytes);
  if (!layout) {
    host.Refuse(SpawnRefusal::kZudSection);
    return SpawnedActor::kNone;
  }
  parts.battle_animation =
      !parts.bare &&
      (slot < 2 || (flags & kSpawnFlagKindMask) == kSpawnFlagDrawn) &&
      layout->battle_animation.size != 0;
  parts.common_animation = !parts.bare && layout->common_animation.size != 0;
  parts.weapon =
      !parts.bare && !parts.second_body && character.blade_model != 0;
  parts.shield =
      !parts.bare && !parts.second_body && character.shield_model != 0;
  if (!RoomEnemySectionsFit(*layout, parts, block->bytes.size())) {
    host.Refuse(SpawnRefusal::kZudSection);
    return SpawnedActor::kNone;
  }

  const SpawnedActor actor = host.Allocate(
      parts.second_body ? ActorShape::kSecondaryBody : ActorShape::kEquipped,
      slot);
  if (actor == SpawnedActor::kNone) {
    host.Refuse(SpawnRefusal::kAllocation);
    return SpawnedActor::kNone;
  }
  const std::uint8_t category = character.blade_model != 0
                                    ? character.blade_category
                                    : kUnarmedWeaponCategory;
  host.SetEquippedWeaponCategory(actor, category);
  const std::optional<RememberedSpawn> remembered =
      host.Remembered(spawn.index);
  const std::uint8_t body_model = host.ZoneEnemyModel(zud);

  // One request, its fields carried from each queued request to the next as
  // the original's stack record was.
  ActorAssetRequest request{};
  request.kind = static_cast<std::uint8_t>(ActorAssetKind::kActorModelParse);
  request.data = SectionAddress(*block, layout->shape);
  request.model_id = body_model;
  request.placement = PackPlacement(RoomEnemyPlacement(spawn, remembered));
  request.sub_kind = kDefaultSubKind;
  request.variant = static_cast<std::uint8_t>(spawn.variant & 1U);
  request.material = character.material;
  OwnedSpawnAssets assets(host, actor, slot);
  if (!assets.Submit(ActorRecordPart::kBody, request) ||
      (!parts.bare &&
       !QueueRoomEnemyParts(assets, request, slot, *block, *layout, character,
                            parts, body_model, category))) {
    host.Refuse(SpawnRefusal::kAssetQueue);
    if (!assets.CancelOrFinish()) return SpawnedActor::kNone;
    DiscardRoomEnemy(host, actor, slot, parts);
    return SpawnedActor::kNone;
  }
  if (!assets.Finish()) {
    host.Refuse(SpawnRefusal::kAssetQueue);
    if (!assets.CancelOrFinish()) return SpawnedActor::kNone;
    DiscardRoomEnemy(host, actor, slot, parts);
    return SpawnedActor::kNone;
  }
  if (!SettleRoomEnemy(host, slot, actor, spawn, character, remembered,
                       flags)) {
    DiscardRoomEnemy(host, actor, slot, parts);
    return SpawnedActor::kNone;
  }
  return actor;
}

// decomp: BATTLE.PRG func_8007C8F8 (src/BATTLE/BATTLE.PRG/146C.c:8881)
// Native (#48): an installed Ashley owns the load into her record; without
// her it stays unowned, as the original queued it.
int QueueAshleyModelLoad(ActorSpawnHost& host, int model_id) {
  ActorAssetRequest request{};
  request.model_id = static_cast<std::int16_t>(model_id);
  request.kind = static_cast<std::uint8_t>(ActorAssetKind::kActorModelLoad);
  request.target_slot = static_cast<std::int8_t>(kAshleySlot);
  const SpawnedActor ashley = host.ActorAt(kAshleySlot);
  if (ashley == SpawnedActor::kNone) {
    return host.QueueAsset(request) + 1;
  }
  return host.QueueOwnedAsset(request, ashley) + 1;
}

// decomp: BATTLE.PRG spawnModelActor (src/BATTLE/BATTLE.PRG/146C.c:8891; was
// func_8007C928)
int QueueScriptedActorSpawn(ActorSpawnHost& host, std::uint32_t actor_id,
                            int model_id, SpawnPlacement placement) {
  if (actor_id >= kSpawnActorSlots) {
    return 0;
  }
  const std::size_t slot = actor_id;
  if (host.ActorAt(slot) != SpawnedActor::kNone) {
    return 0;
  }
  const SpawnedActor actor = host.Allocate(ActorShape::kEquipped, slot);
  if (actor == SpawnedActor::kNone) {
    host.Refuse(SpawnRefusal::kAllocation);
    return 0;
  }
  // Native (#48): the actor is published while its model loads, so a queued
  // request could outlive it and bind its record into the freed actor. The
  // actor owns the request, and removal cancels it (RemoveActorAtSlot).
  const int request = host.QueueOwnedAsset(
      ModelLoadRequest(static_cast<int>(slot), model_id, placement, 0), actor);
  if (request < 0) {
    host.DiscardUninstalled(actor);
    host.Refuse(SpawnRefusal::kAssetQueue);
    return 0;
  }
  host.Install(slot, actor);
  ApplyDefaults(host, slot, model_id, 0, 0, kScriptedSpawnFlags);
  static_cast<void>(host.CreateAiState(slot, kFreshAiFlags));
  return request + 1;
}

// decomp: BATTLE.PRG setActorWeaponModels (src/BATTLE/BATTLE.PRG/146C.c:8920;
// was func_8007CA20)
void LoadActorEquipmentModels(ActorSpawnHost& host, int actor_id,
                              int blade_model, int shield_model) {
  const std::optional<std::size_t> slot = CheckedSlot(host, actor_id);
  if (!slot) {
    return;
  }
  const SpawnedActor actor = host.ActorAt(*slot);
  if (actor == SpawnedActor::kNone) {
    return;
  }
  QueueEquipment(host, actor, actor_id, blade_model, EquipmentHand::kWeapon, 0);
  QueueEquipment(host, actor, actor_id, shield_model, EquipmentHand::kShield,
                 0);
}

// decomp: BATTLE.PRG restoreActorWeaponModels
// (src/BATTLE/BATTLE.PRG/146C.c:8933; was func_8007CAA4)
void RefreshActorEquipmentModels(ActorSpawnHost& host, int actor_id) {
  const std::optional<std::size_t> slot = CheckedSlot(host, actor_id);
  if (!slot) {
    return;
  }
  const SpawnedActor actor = host.ActorAt(*slot);
  if (actor == SpawnedActor::kNone) {
    return;
  }
  const std::optional<EquippedModels> models = host.Equipped(*slot);
  if (!models) {
    return;
  }
  if (models->has_blade) {
    QueueEquipment(host, actor, actor_id, models->weapon_model,
                   EquipmentHand::kWeapon, models->blade_material);
  } else {
    QueueEquipment(host, actor, actor_id, 0, EquipmentHand::kWeapon, 0);
  }
  if (models->has_shield) {
    QueueEquipment(host, actor, actor_id, models->shield_model,
                   EquipmentHand::kShield, models->shield_material);
  } else {
    // The original's blade form (#302): the weapon slot is released again.
    QueueEquipment(host, actor, actor_id, 0, EquipmentHand::kWeapon, 0);
  }
}

// decomp: INITBTL.PRG func_800F9AD8 (src/BATTLE/INITBTL.PRG/18.c:323)
void RestoreAshleyRuntimeState(ActorSpawnHost& host) {
  if (host.Character() == SpawnedActor::kNone) {
    host.Refuse(SpawnRefusal::kNoCharacter);
    return;
  }
  const AshleySnapshot snapshot = host.ReadAshleySnapshot();
  host.RestoreAshleyVitals(snapshot);
  host.InflictCharacterStatuses(snapshot.statuses);
  host.RestoreAshleyConditions(snapshot);
  host.SetCharacterWeaponDrawn(snapshot.weapon_drawn);
  if ((snapshot.weapon_drawn & 1) != 0) {
    host.DrawWeapon(kAshleySlot, kModelsOnlyDrawMode);
    host.SetHudBattleMode(kBattleHudMode);
  }
  host.ClearAshleySnapshotPending();
  host.RestoreCamera(snapshot.camera_pitch, snapshot.camera_distance);
}

// decomp: INITBTL.PRG func_800F9CCC (src/BATTLE/INITBTL.PRG/18.c:374)
void SpawnAshleyWithEquipment(ActorSpawnHost& host) {
  const AshleyInventory inventory = host.ReadAshleyInventory();
  const std::optional<AshleyEquipment> equipment =
      ChooseAshleyEquipment(inventory);
  if (!equipment) {
    host.Refuse(SpawnRefusal::kAshleyBlade);
    return;
  }
  const SpawnedActor actor = CreateActorWithEquipment(
      host, kAshleySlot, 0, equipment->blade_model, equipment->blade_material,
      equipment->shield_model, equipment->shield_material, kAshleyPlacement, 0);
  if (actor == SpawnedActor::kNone) {
    host.Refuse(SpawnRefusal::kAshleyActor);
    return;
  }
  host.SetCharacter(actor);
  const CharacterTemplate character = host.AshleyTemplate();
  ApplyTemplate(host, kAshleySlot, character, kAshleyTemplateBlade, 0,
                kAshleyTemplateFlags, false);
  const WeaponRange range = host.LoadAshleyWeapon(equipment->weapon);
  host.WriteAttackReach(kAshleySlot, AttackReachFor(range, character.reach));
  host.LoadAshleyShield(equipment->shield);
  for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
    host.LoadAshleyArmor(limb, std::nullopt);
    for (std::size_t armor = 0; armor < kInventoryArmor; ++armor) {
      if (inventory.armor_limbs.at(armor) == limb + 1) {
        host.LoadAshleyArmor(limb, armor);
      }
    }
  }
  host.LoadAshleyAccessory(std::nullopt);
  for (std::size_t armor = 0; armor < kInventoryArmor; ++armor) {
    if (inventory.armor_limbs.at(armor) == kAccessoryLimb) {
      host.LoadAshleyAccessory(armor);
    }
  }
  if (host.ReadAshleySnapshot().pending) {
    RestoreAshleyRuntimeState(host);
  }
  DrainAssetQueue(host);
}
}  // namespace VS::Actors
