// Hand-maintained (tools/port/owned.csv).
// Live binding for the actor spawning rules (ActorSpawning.cpp).
#include "Actors/ActorSpawningLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <optional>
#include <span>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorSpawning.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/ActorTableLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/RoomEnemyMemoryLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/Ai/ActorCreationLive.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/CdDrive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Runtime.hpp"
#include "Items/EquipmentLive.hpp"
#include "State/GameClockLive.hpp"
extern "C" {
#include "Actors/ActorAllocation.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/CharacterInit.h"
#include "Actors/ModelInstance.h"
#include "Actors/RoomActorCache.h"
#include "Actors/Spawning.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/ActionAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hud.h"
#include "Battle/StatusEffects.h"
#include "Core/Diagnostics.h"
#include "DiscQueue.h"
#include "Field/Actors.h"
#include "Field/CameraLiveState.h"
#include "Field/Mpd.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Items/Categories.h"
#include "Items/EquipmentInit.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Legacy/Psx.h"
#include "State/Ashley.h"
}

namespace VS::Actors {
namespace {
using Content::AssetAddress;

constexpr int kAuxiliaryAllocation = 0;
constexpr int kEquippedAllocation = 1;
constexpr int kSecondaryBodyAllocation = 2;
// A room enemy whose second body takes the weapon model slot keeps an actor
// record there, not a weapon model (146C.c:6597).
constexpr std::size_t SecondaryBodyBytes() {
  return offsetof(Actors_ActorAllocation, modelSlots) +
         sizeof(Actors_ActorRecord);
}
// The ZND record byte holding the zone enemy's body model.
constexpr std::size_t kZoneEnemyModelByte = 2;
constexpr int kNibbleMask = 0xF;
constexpr int kNibbleSignBit = 0x8;
constexpr int kNibbleRange = 0x10;

static_assert(std::size(Battle_actors) == kBattleActorSlots);
static_assert(sizeof Battle_ActorStats{}.name == kActorNameBytes);
static_assert(sizeof Actors_CharacterInit{}.name == kActorNameBytes);
static_assert(std::size(Battle_ActorStats{}.limbs) == kActorLimbs);
static_assert(std::size(Actors_CharacterInit{}.limbs) == kActorLimbs);
static_assert(std::size(Battle_ActorStats{}.statusTimers) == kStatusTimers);
static_assert(std::size(Actors_usesSecondaryBodyModel) >
              std::numeric_limits<std::uint8_t>::max());
static_assert(std::size(Items_weaponCategoryUpperIds) >= kWeaponCategoryBounds);
static_assert(std::size(Audio_Seq_unusedInventory.weapons) ==
              kInventoryWeapons);
static_assert(std::size(Audio_Seq_unusedInventory.blades) == kInventoryBlades);
static_assert(std::size(Audio_Seq_unusedInventory.shields) ==
              kInventoryShields);
static_assert(std::size(Audio_Seq_unusedInventory.armor) == kInventoryArmor);
static_assert(std::size(State_ashleySnapshot.stats.limbs) == kActorLimbs);
static_assert(sizeof Field_MpdEnemySpawn{}.settings == kSpawnSettingsBytes);
static_assert(std::size(Actors_RoomActorRecord{}.limbHp) == kActorLimbs);

template <typename Id, typename T>
[[nodiscard]] Id IdOf(T* pointer) {
  // A handle names any actor or template, not only kNone.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<Id>(reinterpret_cast<std::uintptr_t>(pointer));
}

template <typename T, typename Id>
[[nodiscard]] T* PointerOf(Id id) {
  return reinterpret_cast<T*>(static_cast<std::uintptr_t>(id));
}

[[nodiscard]] AssetAddress Address(const void* pointer) {
  return IdOf<AssetAddress>(pointer);
}

[[nodiscard]] std::uint8_t Byte(char value) {
  return static_cast<std::uint8_t>(value);
}

// A four-bit `char` field takes the low nibble's bit pattern.
[[nodiscard]] char Nibble(std::uint8_t value) {
  const int nibble = value & kNibbleMask;
  return static_cast<char>(nibble >= kNibbleSignBit ? nibble - kNibbleRange
                                                    : nibble);
}

[[nodiscard]] Battle_Actor* SlotActor(std::size_t slot) {
  return slot < std::size(Battle_actors) ? Battle_actors[slot] : nullptr;
}

[[nodiscard]] Battle_ActorStats* SlotStats(std::size_t slot) {
  Battle_Actor* actor = SlotActor(slot);
  return actor != nullptr ? actor->stats : nullptr;
}

[[nodiscard]] Battle_ActorStats* CharacterStats() {
  return Battle_characterState != nullptr ? Battle_characterState->stats
                                          : nullptr;
}

[[nodiscard]] WeaponRange RangeOf(const Items_WeaponRange& range) {
  return {.range = Byte(range.range),
          .second_range = Byte(range.secondRange),
          .third_range = Byte(range.thirdRange),
          .attack_shape_angle = static_cast<std::uint8_t>(
              Byte(range.attackShapeAngle) & kAttackShapeAngleMask)};
}

// A zeroed block from the low end of the game heap, or null.
[[nodiscard]] void* ZeroedBlock(std::size_t bytes) {
  void* block = Core::Memory::AllocateLiveFromLowEnd(bytes);
  if (block != nullptr) {
    std::memset(block, 0, bytes);
  }
  return block;
}

void WriteName(std::span<const std::uint8_t, kActorNameBytes> name,
               Battle_ActorStats& stats) {
  for (std::size_t i = 0; i < kActorNameBytes; ++i) {
    stats.name[i] = static_cast<char>(name[i]);
  }
}

void WriteInitialLimbs(const InitialStats& stats, Battle_ActorStats& live) {
  for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
    const LimbInitialStats& source = stats.limbs.at(limb);
    Battle_LimbStats& target = live.limbs[limb];
    target.hp = static_cast<std::int16_t>(source.hp);
    target.maxHp = static_cast<std::int16_t>(source.hp);
    target.agilityDefenseBonus =
        static_cast<signed char>(source.agility_defense_bonus);
    target.anchorBone = static_cast<char>(source.anchor_bone);
    target.nameIndex = source.name_index;
    target.chainEvasion = static_cast<char>(source.chain_evasion);
    std::ranges::copy(source.types, std::begin(target.types));
    std::ranges::copy(source.affinities, std::begin(target.affinities));
    for (std::size_t action = 0; action < kLimbActions; ++action) {
      const LimbActionStats& from = stats.limb_actions.at(limb).at(action);
      Battle_ActionSlot& to = live.limbActions[limb][action];
      to.actionId = from.action_id;
      to.index = Nibble(from.index);
      to.effectModelId = Nibble(from.effect_model);
      to.opaque3 = 1;
    }
    for (std::size_t target_limb = 0; target_limb < kDpSplashTargets;
         ++target_limb) {
      live.dpSplash[limb][target_limb] =
          static_cast<char>(stats.dp_splash.at(limb).at(target_limb));
    }
  }
}

// The actor table, allocations, asset queue, zone and Ashley's state the
// spawn rules edit, for one operation.
class LiveActorSpawnHost final : public ActorSpawnHost {
 public:
  explicit LiveActorSpawnHost(const char* operation) : operation_(operation) {}

  void Refuse(SpawnRefusal refusal) override {
    WarnActorRefused(operation_, SpawnRefusalText(refusal));
  }
  void Diagnostic(int code) override { Core::IgnoreDiagnostic(code, 0); }
  [[nodiscard]] int RandomAveraged(int range) override {
    return Core::NextLiveRandomAveraged(range);
  }

  // --- The actor table and allocations
  [[nodiscard]] SpawnedActor ActorAt(std::size_t slot) const override {
    return IdOf<SpawnedActor>(SlotActor(slot));
  }

  [[nodiscard]] SpawnedActor Allocate(ActorShape shape) override {
    switch (shape) {
      case ActorShape::kAuxiliary: {
        auto* core = static_cast<Actors_ActorCore*>(
            ZeroedBlock(sizeof(Actors_ActorCore)));
        if (core == nullptr) {
          return SpawnedActor::kNone;
        }
        Battle_Actor& actor = core->actor.actor;
        actor.stats = &core->actor.stats;
        actor.model = &core->model;
        actor.allocationKind = kAuxiliaryAllocation;
        return IdOf<SpawnedActor>(&actor);
      }
      case ActorShape::kEquipped: {
        auto* allocation = static_cast<Actors_ActorAllocation*>(
            ZeroedBlock(sizeof(Actors_ActorAllocation)));
        if (allocation == nullptr) {
          return SpawnedActor::kNone;
        }
        Battle_Actor& actor = allocation->core.actor.actor;
        actor.stats = &allocation->core.actor.stats;
        actor.model = &allocation->core.model;
        actor.equipmentModels[0] = &allocation->modelSlots[0];
        actor.equipmentModels[1] = &allocation->modelSlots[1];
        actor.allocationKind = kEquippedAllocation;
        return IdOf<SpawnedActor>(&actor);
      }
      case ActorShape::kSecondaryBody: {
        auto* allocation = static_cast<Actors_ActorAllocation*>(
            ZeroedBlock(SecondaryBodyBytes()));
        if (allocation == nullptr) {
          return SpawnedActor::kNone;
        }
        Battle_Actor& actor = allocation->core.actor.actor;
        actor.stats = &allocation->core.actor.stats;
        actor.model = &allocation->core.model;
        actor.equipmentModels[0] = &allocation->modelSlots[0];
        actor.allocationKind = kSecondaryBodyAllocation;
        return IdOf<SpawnedActor>(&actor);
      }
    }
    return SpawnedActor::kNone;
  }

  void Install(std::size_t slot, SpawnedActor actor) override {
    if (slot < std::size(Battle_actors)) {
      Battle_actors[slot] = PointerOf<Battle_Actor>(actor);
    }
  }

  [[nodiscard]] AssetAddress ModelAddress(SpawnedActor actor) const override {
    const Battle_Actor* live = PointerOf<Battle_Actor>(actor);
    return live != nullptr ? Address(live->model) : AssetAddress::kNone;
  }

  [[nodiscard]] AssetAddress EquipmentAddress(
      SpawnedActor actor, EquipmentHand hand) const override {
    const Battle_Actor* live = PointerOf<Battle_Actor>(actor);
    if (live == nullptr) {
      return AssetAddress::kNone;
    }
    return Address(live->equipmentModels[static_cast<std::size_t>(hand)]);
  }

  void SetEquippedWeaponCategory(SpawnedActor actor,
                                 std::uint8_t category) override {
    if (Battle_Actor* live = PointerOf<Battle_Actor>(actor); live != nullptr) {
      live->equippedWeaponCategory = category;
    }
  }

  void WriteAuxiliaryIdentity(SpawnedActor actor,
                              const AuxiliaryIdentity& identity) override {
    Battle_Actor* live = PointerOf<Battle_Actor>(actor);
    if (live == nullptr) {
      return;
    }
    live->modelId = identity.model_id;
    live->kind = identity.kind;
    live->id = identity.id;
    live->next = nullptr;
    live->retainAfterDefeat = 0;
  }

  [[nodiscard]] std::array<std::uint8_t, kWeaponCategoryBounds>
  WeaponCategoryBounds() const override {
    std::array<std::uint8_t, kWeaponCategoryBounds> bounds{};
    for (std::size_t i = 0; i < bounds.size(); ++i) {
      bounds.at(i) = Byte(Items_weaponCategoryUpperIds[i]);
    }
    return bounds;
  }

  // --- The installed actor in `slot`
  void WriteDefaults(std::size_t slot, const ActorDefaults& defaults) override {
    Battle_Actor* actor = SlotActor(slot);
    if (actor == nullptr) {
      return;
    }
    actor->id = defaults.id;
    actor->modelId = defaults.model_id;
    actor->subType = 0;
    actor->bladeId = defaults.blade_id;
    actor->shieldId = defaults.shield_id;
    actor->weaponDrawn = defaults.disposition.weapon_drawn ? 1 : 0;
    actor->kind = defaults.disposition.kind;
    actor->interactionFlags = defaults.interaction_flags;
    actor->lastTriggerTile.word = 0;
    actor->facing = 0;
    actor->commandState = 0;
    actor->roomSpawnIndex = kNoRoomSpawn;
    actor->killCounted = 0;
    actor->defeated = 0;
    Battle_ActorStats* stats = actor->stats;
    if (stats == nullptr) {
      return;
    }
    stats->maxHp = kDefaultVitals;
    stats->currentHp = kDefaultVitals;
    stats->maxMp = kDefaultVitals;
    stats->currentMp = kDefaultVitals;
    stats->risk = 0;
    stats->actionRecoveryTimer = 0;
    stats->movementClass = static_cast<char>(defaults.gait.movement_class);
    stats->gaitClass = static_cast<char>(defaults.gait.gait_class);
    stats->walkSpeed = static_cast<char>(kDefaultWalkSpeed);
    stats->runSpeed = static_cast<char>(kDefaultRunSpeed);
    stats->weapon.recoveryDelay = static_cast<char>(kDefaultRecoveryDelay);
    stats->weapon.range.range = static_cast<char>(kDefaultWeaponRange[0]);
    stats->weapon.range.secondRange = static_cast<char>(kDefaultWeaponRange[1]);
    stats->weapon.range.thirdRange = static_cast<char>(kDefaultWeaponRange[2]);
    stats->cancelsHitResults = 0;
    stats->weapon.range.attackShapeAngle = 0;
    stats->actorId = kNoRoomSpawn;
  }

  void WriteTemplateIdentity(std::size_t slot,
                             const TemplateIdentity& identity) override {
    Battle_Actor* actor = SlotActor(slot);
    if (actor == nullptr) {
      return;
    }
    actor->id = identity.id;
    actor->modelId = identity.model_id;
    actor->subType = identity.sub_type;
    actor->bladeId = identity.blade_id;
    actor->shieldId = identity.shield_id;
    actor->weaponDrawn = identity.disposition.weapon_drawn ? 1 : 0;
    actor->kind = identity.disposition.kind;
    actor->retainAfterDefeat = identity.retain_after_defeat ? 1 : 0;
    actor->interactionFlags = identity.interaction_flags;
    actor->commandState = 0;
    actor->lastTriggerTile.word = 0;
  }

  void WriteInitialStats(std::size_t slot, const InitialStats& stats) override {
    Battle_ActorStats* live = SlotStats(slot);
    if (live == nullptr) {
      return;
    }
    live->actorId = stats.actor_id;
    WriteName(stats.name, *live);
    live->maxHp = stats.hp;
    live->currentHp = stats.hp;
    live->risk = 0;
    live->maxMp = stats.mp;
    live->currentMp = stats.mp;
    live->totalStrength = static_cast<u_short>(stats.strength);
    live->strength = static_cast<u_short>(stats.strength);
    live->totalIntelligence = static_cast<u_short>(stats.intelligence);
    live->intelligence = static_cast<u_short>(stats.intelligence);
    live->totalAgility = static_cast<u_short>(stats.agility);
    live->agility = static_cast<u_short>(stats.agility);
    live->movementClass = static_cast<char>(stats.gait.movement_class);
    live->gaitClass = static_cast<char>(stats.gait.gait_class);
    live->walkSpeed = static_cast<char>(stats.walk_speed);
    live->baseWalkSpeed = static_cast<char>(stats.walk_speed);
    live->runSpeed = static_cast<char>(stats.run_speed);
    live->baseRunSpeed = static_cast<char>(stats.run_speed);
    live->enemyClass = stats.enemy_class;
    live->reach = stats.reach;
    live->recoveryDelayBonus = stats.recovery_delay_bonus;
    live->mobilityLimb0 = stats.mobility_limbs[0];
    live->mobilityLimb1 = stats.mobility_limbs[1];
    live->defaultTargetLimb = stats.default_target_limb;
    live->cancelsHitResults = 0;
    live->isAnalyzed = 0;
    WriteInitialLimbs(stats, *live);
    live->statusImmunities = stats.status_immunities;
    live->statuses = stats.statuses;
    std::fill_n(std::begin(live->statusTimers), kInitialStatusTimers, 0);
    live->actionRecoveryTimer = 0;
  }

  [[nodiscard]] WeaponRange BuildTemplateEquipment(
      std::size_t slot, TemplateEquipment equipment) override {
    Battle_ActorStats* stats = SlotStats(slot);
    const auto* init = PointerOf<const Actors_CharacterInit>(equipment);
    if (stats == nullptr || init == nullptr) {
      return {};
    }
    Items::BuildLiveWeaponStatsFromParts(&stats->weapon, &init->weapon);
    Items::BuildLiveShieldStatsFromParts(&stats->shield, &init->shield);
    Items::BuildLiveAccessoryStatsFromPart(&stats->accessory, &init->accessory);
    for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
      Items::BuildLiveArmorStatsFromPart(&stats->limbs[limb].armor,
                                         &init->limbs[limb].armor);
    }
    return RangeOf(stats->weapon.range);
  }

  void WriteAttackReach(std::size_t slot, const AttackReach& reach) override {
    Battle_ActorStats* stats = SlotStats(slot);
    if (stats == nullptr) {
      return;
    }
    stats->currentRange = reach.range;
    stats->currentRangeY = reach.range_y;
    stats->currentRangeZ = reach.range_z;
    stats->currentAttackShapeAngle = reach.shape_angle;
    stats->currentAttackShape = kInitialAttackShape;
  }

  [[nodiscard]] SpawnedState State(std::size_t slot) const override {
    const Battle_Actor* actor = SlotActor(slot);
    if (actor == nullptr) {
      return {};
    }
    SpawnedState state{.kind = actor->kind, .weapon_drawn = actor->weaponDrawn};
    if (const Battle_ActorStats* stats = actor->stats; stats != nullptr) {
      state.walk_speed = Byte(stats->walkSpeed);
      state.run_speed = Byte(stats->runSpeed);
    }
    return state;
  }

  void WriteSpawnDetails(std::size_t slot,
                         const SpawnDetails& details) override {
    Battle_Actor* actor = SlotActor(slot);
    if (actor == nullptr) {
      return;
    }
    actor->facing = details.facing;
    actor->spawnByte26 = details.spawn_byte;
    actor->roomSpawnIndex = details.room_spawn_index;
    actor->killCounted = 0;
    actor->defeated = 0;
    actor->aiGroup = details.settings.ai_group;
    actor->reserved30Bit4 = details.settings.group_reserved;
    actor->aiLinkedGroup = details.settings.ai_linked_group;
    actor->reserved31Bit6 = details.settings.linked_reserved;
    actor->reserved34 = details.settings.extra;
    actor->templateWord2C = details.template_word;
    Battle_ActorStats* stats = actor->stats;
    if (stats == nullptr) {
      return;
    }
    stats->dropItem.id = details.drop.id;
    stats->dropItem.count = static_cast<char>(details.drop.count);
    stats->secondDropItem.id = details.second_drop.id;
    stats->secondDropItem.count = static_cast<char>(details.second_drop.count);
    stats->randomDropItem.id = details.random_drop.id;
    stats->randomDropItem.dropRate =
        static_cast<char>(details.random_drop.count);
  }

  void SetTemplateWord(std::size_t slot, std::int32_t word) override {
    if (Battle_Actor* actor = SlotActor(slot); actor != nullptr) {
      actor->templateWord2C = word;
    }
  }

  void RestoreRemembered(std::size_t slot,
                         const RememberedSpawn& remembered) override {
    Battle_ActorStats* stats = SlotStats(slot);
    if (stats == nullptr) {
      return;
    }
    stats->currentHp = remembered.current_hp;
    stats->maxHp = remembered.max_hp;
    stats->currentMp = remembered.current_mp;
    stats->maxMp = remembered.max_mp;
    stats->strength = static_cast<u_short>(remembered.strength);
    stats->totalStrength = static_cast<u_short>(remembered.strength);
    stats->intelligence = static_cast<u_short>(remembered.intelligence);
    stats->totalIntelligence = static_cast<u_short>(remembered.intelligence);
    stats->agility = static_cast<u_short>(remembered.agility);
    stats->totalAgility = static_cast<u_short>(remembered.agility);
    for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
      stats->limbs[limb].hp = remembered.limb_hp.at(limb);
    }
  }

  void InflictStatuses(std::size_t slot, std::int32_t statuses) override {
    if (Battle_ActorStats* stats = SlotStats(slot); stats != nullptr) {
      Battle_InflictActorStatuses(statuses, stats);
    }
  }

  void LinkAfterLeader(std::size_t slot) override {
    Battle_Actor* actor = SlotActor(slot);
    Battle_Actor* leader = Battle_actors[kAshleySlot];
    if (actor == nullptr || leader == nullptr || actor == leader) {
      return;
    }
    actor->next = leader->next;
    leader->next = actor;
  }

  void ClearNext(std::size_t slot) override {
    if (Battle_Actor* actor = SlotActor(slot); actor != nullptr) {
      actor->next = nullptr;
    }
  }

  [[nodiscard]] int FindBoneTag(std::size_t slot, int tag,
                                int kind) const override {
    return FindLiveActorBoneTag(static_cast<int>(slot), tag, kind);
  }

  [[nodiscard]] int BoneTag(std::size_t slot, int bone,
                            int kind) const override {
    return LiveActorBoneTag(static_cast<int>(slot), bone, kind);
  }

  [[nodiscard]] std::optional<EquippedModels> Equipped(
      std::size_t slot) const override {
    const Battle_ActorStats* stats = SlotStats(slot);
    if (stats == nullptr) {
      return std::nullopt;
    }
    return EquippedModels{.has_blade = stats->weapon.blade.id != 0,
                          .weapon_model = Byte(stats->weapon.wepId),
                          .blade_material = Byte(stats->weapon.blade.material),
                          .has_shield = stats->shield.base.id != 0,
                          .shield_model = Byte(stats->shield.wepId),
                          .shield_material = Byte(stats->shield.base.material)};
  }

  // --- Animation, display, AI and combat
  void DrawWeapon(std::size_t slot, int mode) override {
    SetLiveActorWeaponDrawn(static_cast<int>(slot), 1, mode);
  }
  void SetDisplayFlags(std::size_t slot, std::uint32_t flags) override {
    SetLiveActorDisplayFlags(static_cast<int>(slot), static_cast<int>(flags));
  }
  void PlayAnimation(std::size_t slot, int animation) override {
    static_cast<void>(
        PlayLiveActorAnimation(static_cast<int>(slot), animation, 0, 0));
  }
  void SetWalkRunSpeeds(std::size_t slot, int walk, int run) override {
    SetLiveActorWalkRunSpeeds(static_cast<int>(slot), walk, run);
  }
  void SetCombatPose(std::size_t slot, int pose) override {
    Battle_SetEnemyCombatPose(static_cast<int>(slot), pose);
  }
  [[nodiscard]] int CreateAiState(std::size_t slot, int flags) override {
    Battle_Actor* actor = SlotActor(slot);
    if (actor == nullptr) {
      return 0;
    }
    return Battle::Ai::CreateLiveActorAiState(actor, flags);
  }
  void InitializePlaceholder(std::size_t slot, int actor_id) override {
    InitializeLivePlaceholderActor(static_cast<int>(slot), actor_id);
  }
  void QueueBaseSequence(SpawnedActor actor, std::size_t slot) override {
    QueueLiveActorBaseSequenceLoad(PointerOf<Battle_Actor>(actor),
                                   static_cast<int>(slot));
  }
  void QueueWeaponSequence(SpawnedActor actor, std::size_t slot,
                           std::uint8_t category, int flags) override {
    QueueLiveActorWeaponSequenceLoad(PointerOf<char>(actor),
                                     static_cast<int>(slot), category, flags);
  }

  // --- The asset queue
  [[nodiscard]] int QueueAsset(
      const Content::ActorAssetRequest& request) override {
    return Content::QueueLiveActorAsset(request);
  }
  [[nodiscard]] bool AssetQueueIdle() const override {
    return Content::FindLiveFreeActorAssetRequest() == 0;
  }
  void UpdateAssetQueue() override {
    static_cast<void>(Content::UpdateLiveActorAssetQueue());
  }
  void PumpLoading() override {
    static_cast<void>(State::UpdateLivePlayTimeAndLoading(0));
  }
  void ReleaseWeaponModel(int slot) override {
    static_cast<void>(Content::ReleaseLiveWeaponModel(slot));
  }

  // --- The zone
  [[nodiscard]] bool UsesSecondaryBodyModel(
      std::uint8_t model_id) const override {
    return Actors_usesSecondaryBodyModel[model_id] != 0;
  }

  // Both the ZND records and the ZUD files have zndEnemyCount entries
  // (RoomLoading.cpp:143-151).
  [[nodiscard]] int ZoneEnemyCount() const override {
    if (Field_zoneContext.zndEnemies == nullptr ||
        Field_zoneContext.zudFiles == nullptr) {
      return 0;
    }
    return std::max(Field_zoneContext.zndEnemyCount, 0);
  }

  [[nodiscard]] std::uint8_t ZoneEnemyModel(int zud) const override {
    if (zud < 0 || zud >= ZoneEnemyCount()) {
      return 0;
    }
    return Field_zoneContext.zndEnemies[zud].recordBytes[kZoneEnemyModelByte];
  }

  [[nodiscard]] std::optional<RememberedSpawn> Remembered(
      std::uint8_t spawn_index) const override {
    if (!IsLiveRememberedRoom(Field_zoneContext.zndId,
                              Field_zoneContext.mapId)) {
      return std::nullopt;
    }
    Actors_RoomActorRecord record{};
    if (CopyLiveRememberedEnemy(spawn_index, &record) == nullptr) {
      return std::nullopt;
    }
    return DecodeRemembered(record);
  }

  // A block without a matching index, as after an interrupted read, never
  // matches a ZUD file, so the rules free it as the original did.
  [[nodiscard]] std::optional<int> CachedZud() const override {
    if (Field_zoneContext.zudData == nullptr) {
      return std::nullopt;
    }
    return Field_zoneContext.loadedZudIndexPlusOne - 1;
  }

  // The cached file, bounded by its file size and its heap payload.
  [[nodiscard]] ZudBlock CachedZudBlock() const override {
    const void* data = Field_zoneContext.zudData;
    const int zud = Field_zoneContext.loadedZudIndexPlusOne - 1;
    if (data == nullptr || zud < 0 || zud >= ZoneEnemyCount()) {
      return {};
    }
    const auto file_bytes = static_cast<std::size_t>(
        std::max(Field_zoneContext.zudFiles[zud].size, 0));
    const auto capacity = Core::Memory::LivePayloadCapacity(data);
    const std::size_t bytes =
        capacity.has_value() ? std::min(file_bytes, *capacity) : 0;
    return {.address = Address(data),
            .bytes = {static_cast<const std::byte*>(data), bytes}};
  }

  void FreeZud() override {
    if (Field_zoneContext.zudData != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(Field_zoneContext.zudData);
    }
    Field_zoneContext.zudData = nullptr;
    Field_zoneContext.loadedZudIndexPlusOne = 0;
  }

  [[nodiscard]] Content::DiscFileExtent ZudFile(int zud) const override {
    if (zud < 0 || zud >= ZoneEnemyCount()) {
      return {};
    }
    const Content_DiscFile& file = Field_zoneContext.zudFiles[zud];
    return {.sector = file.lba, .bytes = file.size};
  }

  [[nodiscard]] bool ZudReadPending() const override {
    return Field_zoneContext.zudDiscRequest != nullptr;
  }

  [[nodiscard]] bool AllocateZud(Content::DiscFileExtent file) override {
    if (file.bytes < 0) {
      return false;
    }
    Field_zoneContext.zudData = Core::Memory::AllocateLiveFromHighEnd(
        static_cast<std::size_t>(file.bytes));
    return Field_zoneContext.zudData != nullptr;
  }

  void BeginZudRead(Content::DiscFileExtent file) override {
    Content_DiscFile disc{};
    disc.lba = file.sector;
    disc.size = file.bytes;
    Content_DiscQueueSlot* request = Content::AllocateLiveDiscRequest(&disc);
    if (request != nullptr) {
      Content::EnqueueLivePriorityDiscRequest(request,
                                              Field_zoneContext.zudData);
    } else {
      request = Content::CompleteLiveRefusedDiscRequest(
          &disc, Field_zoneContext.zudData);
    }
    Field_zoneContext.zudDiscRequest = request;
  }

  [[nodiscard]] ZudReadState PollZudRead() const override {
    const Content_DiscQueueSlot* request = Field_zoneContext.zudDiscRequest;
    if (request == nullptr) {
      return ZudReadState::kFailed;
    }
    switch (static_cast<Content::DiscRequestState>(request->state)) {
      case Content::DiscRequestState::kLoaded:
        return ZudReadState::kLoaded;
      case Content::DiscRequestState::kError:
        return ZudReadState::kFailed;
      default:
        return ZudReadState::kPending;
    }
  }

  void FinishZudRead(int zud, bool loaded) override {
    Content::ReleaseLiveDiscRequest(Field_zoneContext.zudDiscRequest);
    Field_zoneContext.zudDiscRequest = nullptr;
    if (loaded) {
      Field_zoneContext.loadedZudIndexPlusOne = zud + 1;
    } else {
      FreeZud();
    }
  }

  // --- Ashley
  [[nodiscard]] AshleySnapshot ReadAshleySnapshot() const override {
    const State_AshleyStatsSnapshot& saved = State_ashleySnapshot.stats;
    AshleySnapshot snapshot{.pending = saved.isPending != 0,
                            .current_hp = saved.currentHp,
                            .max_hp = saved.maxHp,
                            .current_mp = saved.currentMp,
                            .max_mp = saved.maxMp,
                            .strength = saved.strength,
                            .total_strength = saved.totalStrength,
                            .intelligence = saved.intelligence,
                            .total_intelligence = saved.totalIntelligence,
                            .agility = saved.agility,
                            .total_agility = saved.totalAgility,
                            .risk = saved.risk,
                            .recovery_timer = saved.actorFlags,
                            .statuses = saved.statuses,
                            .weapon_drawn = saved.weaponDrawn,
                            .camera_pitch = saved.cameraPitch,
                            .camera_distance = saved.cameraDistance};
    for (std::size_t i = 0; i < kStatusTimers; ++i) {
      snapshot.status_timers.at(i) = Byte(saved.statusTimers[i]);
    }
    for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
      const State_AshleyLimbSnapshot& source = saved.limbs[limb];
      AshleyLimbSnapshot& target = snapshot.limbs.at(limb);
      target.hp = source.hp;
      target.max_hp = source.maxHp;
      std::ranges::copy(source.types, target.types.begin());
      std::ranges::copy(source.affinities, target.affinities.begin());
    }
    return snapshot;
  }

  [[nodiscard]] SpawnedActor Character() const override {
    return CharacterStats() != nullptr
               ? IdOf<SpawnedActor>(Battle_characterState)
               : SpawnedActor::kNone;
  }

  void SetCharacter(SpawnedActor actor) override {
    Battle_characterState = PointerOf<Battle_Actor>(actor);
  }

  void RestoreAshleyVitals(const AshleySnapshot& snapshot) override {
    Battle_ActorStats* stats = CharacterStats();
    if (stats == nullptr) {
      return;
    }
    stats->currentHp = snapshot.current_hp;
    stats->maxHp = snapshot.max_hp;
    stats->currentMp = snapshot.current_mp;
    stats->maxMp = snapshot.max_mp;
    stats->strength = static_cast<u_short>(snapshot.strength);
    stats->totalStrength = static_cast<u_short>(snapshot.total_strength);
    stats->intelligence = static_cast<u_short>(snapshot.intelligence);
    stats->totalIntelligence =
        static_cast<u_short>(snapshot.total_intelligence);
    stats->agility = static_cast<u_short>(snapshot.agility);
    stats->totalAgility = static_cast<u_short>(snapshot.total_agility);
    stats->risk = static_cast<u_short>(snapshot.risk);
    stats->actionRecoveryTimer = snapshot.recovery_timer;
  }

  void InflictCharacterStatuses(std::int32_t statuses) override {
    if (Battle_ActorStats* stats = CharacterStats(); stats != nullptr) {
      Battle_InflictActorStatuses(statuses, stats);
    }
  }

  void RestoreAshleyConditions(const AshleySnapshot& snapshot) override {
    Battle_ActorStats* stats = CharacterStats();
    if (stats == nullptr) {
      return;
    }
    std::ranges::copy(snapshot.status_timers, std::begin(stats->statusTimers));
    for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
      const AshleyLimbSnapshot& source = snapshot.limbs.at(limb);
      Battle_LimbStats& target = stats->limbs[limb];
      target.hp = static_cast<std::int16_t>(source.hp);
      target.maxHp = static_cast<std::int16_t>(source.max_hp);
      for (std::size_t i = 0; i < kLimbTypes; ++i) {
        target.types[i] = static_cast<std::int16_t>(source.types.at(i));
      }
      for (std::size_t i = 0; i < kLimbAffinities; ++i) {
        target.affinities[i] =
            static_cast<std::int16_t>(source.affinities.at(i));
      }
    }
  }

  void SetCharacterWeaponDrawn(std::int32_t weapon_drawn) override {
    if (Battle_characterState != nullptr) {
      Battle_characterState->weaponDrawn = weapon_drawn;
    }
  }

  void SetHudBattleMode(int mode) override { Battle_SetHudBattleMode(mode); }

  void ClearAshleySnapshotPending() override {
    State_ashleySnapshot.stats.isPending = 0;
  }

  void RestoreCamera(std::int32_t pitch, std::int32_t distance) override {
    Field_sphericalCamera.delta.pitch = pitch;
    Field_sphericalCamera.initialDistance = distance;
  }

  [[nodiscard]] AshleyInventory ReadAshleyInventory() const override {
    const Items_Inventory& live = Audio_Seq_unusedInventory;
    AshleyInventory inventory;
    for (std::size_t i = 0; i < kInventoryWeapons; ++i) {
      inventory.weapons.at(i) = {.equipped = live.weapons[i].isEquipped != 0,
                                 .blade = live.weapons[i].blade};
    }
    for (std::size_t i = 0; i < kInventoryBlades; ++i) {
      inventory.blades.at(i) = {.model = Byte(live.blades[i].wepId),
                                .material = live.blades[i].material};
    }
    for (std::size_t i = 0; i < kInventoryShields; ++i) {
      const Items_InventoryShield& shield = live.shields[i];
      inventory.shields.at(i) = {.equipped = shield.isEquipped != 0,
                                 .model = Byte(shield.base.wepId),
                                 .material = shield.base.material};
    }
    for (std::size_t i = 0; i < kInventoryArmor; ++i) {
      inventory.armor_limbs.at(i) = live.armor[i].limb;
    }
    return inventory;
  }

  [[nodiscard]] CharacterTemplate AshleyTemplate() const override {
    return DecodeCharacterTemplate(Actors_ashleyInitialLoadout);
  }

  [[nodiscard]] WeaponRange LoadAshleyWeapon(
      std::optional<std::size_t> weapon) override {
    Battle_ActorStats* stats = CharacterStats();
    if (stats == nullptr) {
      return {};
    }
    Items::LoadLiveInventoryWeaponStats(
        &stats->weapon, weapon && *weapon < kInventoryWeapons
                            ? &Audio_Seq_unusedInventory.weapons[*weapon]
                            : nullptr);
    return RangeOf(stats->weapon.range);
  }

  void LoadAshleyShield(std::optional<std::size_t> shield) override {
    if (Battle_ActorStats* stats = CharacterStats(); stats != nullptr) {
      Items::LoadLiveInventoryShieldStats(
          &stats->shield, shield && *shield < kInventoryShields
                              ? &Audio_Seq_unusedInventory.shields[*shield]
                              : nullptr);
    }
  }

  void LoadAshleyArmor(std::size_t limb,
                       std::optional<std::size_t> armor) override {
    Battle_ActorStats* stats = CharacterStats();
    if (stats == nullptr || limb >= kActorLimbs) {
      return;
    }
    Items::LoadLiveInventoryArmorStats(
        &stats->limbs[limb].armor,
        armor && *armor < kInventoryArmor
            ? &Audio_Seq_unusedInventory.armor[*armor]
            : nullptr);
  }

  void LoadAshleyAccessory(std::optional<std::size_t> armor) override {
    if (Battle_ActorStats* stats = CharacterStats(); stats != nullptr) {
      Items::LoadLiveInventoryAccessoryStats(
          &stats->accessory, armor && *armor < kInventoryArmor
                                 ? &Audio_Seq_unusedInventory.armor[*armor]
                                 : nullptr);
    }
  }

 private:
  const char* operation_;
};

LimbTemplate DecodeLimb(const Actors_LimbInit& limb) {
  LimbTemplate decoded{.hp = limb.hp,
                       .agility_defense_bonus = Byte(limb.agilityDefenseBonus),
                       .chain_evasion = Byte(limb.chainEvasion)};
  std::ranges::copy(limb.types, decoded.types.begin());
  std::ranges::copy(limb.affinities, decoded.affinities.begin());
  for (std::size_t action = 0; action < kLimbActions; ++action) {
    const Actors_LimbActionInit& source = limb.actions[action];
    decoded.actions.at(action) = {.action_id = source.actionId,
                                  .effect_model = Byte(source.opaque2),
                                  .index = Byte(source.index)};
  }
  for (std::size_t i = 0; i < kDpSplashTargets; ++i) {
    decoded.dp_splash.at(i) = Byte(limb.dpSplash[i]);
  }
  return decoded;
}
}  // namespace

CharacterTemplate DecodeCharacterTemplate(const Actors_CharacterInit& init) {
  CharacterTemplate decoded{
      .sub_type = init.subType,
      .model_id = Byte(init.characterKind),
      .reach = init.reach,
      .enemy_class = init.enemyClass,
      .hp = init.hp,
      .mp = init.mp,
      .strength = Byte(init.strength),
      .intelligence = Byte(init.intelligence),
      .agility = Byte(init.agility),
      .recovery_delay_bonus = Byte(init.opaque23),
      .material = Byte(init.material),
      .walk_speed = Byte(init.walkSpeed),
      .gait_class = Byte(init.opaque26),
      .run_speed = Byte(init.runSpeed),
      .status_immunities = init.opaque28,
      .template_word = init.opaque2C,
      .mobility_limbs = {Byte(init.opaque30), Byte(init.opaque31)},
      .default_target_limb = Byte(init.opaque32),
      .blade_model = static_cast<std::uint8_t>(init.weapon.blade.weaponModelId),
      .blade_category = Byte(init.weapon.blade.category),
      .weapon_material = Byte(init.weapon.material),
      .shield_model = static_cast<std::uint8_t>(init.shield.base.weaponModelId),
      .shield_material = Byte(init.shield.material),
      .zud_index = init.zudIndex,
      .equipment = IdOf<TemplateEquipment>(&init)};
  for (std::size_t i = 0; i < kActorNameBytes; ++i) {
    decoded.name.at(i) = Byte(init.name[i]);
  }
  for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
    decoded.limbs.at(limb) = DecodeLimb(init.limbs[limb]);
  }
  return decoded;
}

RoomSpawn DecodeRoomSpawn(const Field_MpdEnemySpawn& spawn) {
  RoomSpawn decoded{
      .index = spawn.index,
      .placement = {.tile_x = spawn.tileX,
                    .direction = spawn.direction,
                    .tile_z = spawn.tileZ,
                    .height = spawn.height},
      .drop = {.id = spawn.itemId, .count = spawn.itemCount},
      .second_drop = {.id = spawn.item2Id, .count = spawn.item2Count},
      .random_drop = {.id = spawn.item3Id, .count = spawn.item3Count},
      .facing = spawn.facing,
      .variant = spawn.variant,
      .combat_pose = spawn.reserved26};
  std::ranges::copy(spawn.settings, decoded.settings.begin());
  return decoded;
}

SpawnPlacement DecodePlacement(const Field_ActorPlacement& placement) {
  return {.tile_x = static_cast<std::uint8_t>(placement.tileX),
          .direction = static_cast<std::uint8_t>(placement.direction),
          .tile_z = static_cast<std::uint8_t>(placement.tileZ),
          .height = static_cast<std::uint8_t>(placement.height)};
}

RememberedSpawn DecodeRemembered(const Actors_RoomActorRecord& record) {
  RememberedSpawn decoded{.ai_flags = record.aiFlags,
                          .current_hp = record.currentHp,
                          .max_hp = record.maxHp,
                          .current_mp = record.currentMp,
                          .max_mp = record.maxMp,
                          .strength = record.strength,
                          .intelligence = record.intelligence,
                          .agility = record.agility,
                          .statuses = record.statuses,
                          .placement = {.tile_x = record.tileX,
                                        .direction = record.direction,
                                        .tile_z = record.tileZ,
                                        .height = record.height}};
  std::ranges::copy(record.limbHp, decoded.limb_hp.begin());
  return decoded;
}

// decomp: BATTLE.PRG func_800765B0 (src/BATTLE/BATTLE.PRG/146C.c:6064)
Battle_Actor* CreateLiveAuxiliaryActor(int index, int actor_id,
                                       Field_ActorPlacement* placement,
                                       int material, int /*unused*/) {
  constexpr const char* kOperation = "CreateAuxiliaryActor";
  if (placement == nullptr) {
    WarnActorRefused(kOperation, "no placement");
    return nullptr;
  }
  LiveActorSpawnHost host(kOperation);
  return PointerOf<Battle_Actor>(CreateAuxiliaryActor(
      host, index, actor_id, DecodePlacement(*placement), material));
}

// decomp: BATTLE.PRG func_800774FC (src/BATTLE/BATTLE.PRG/146C.c:6562)
Battle_Actor* CreateLiveActorWithEquipment(int actor_id, int model_id,
                                           int blade_model, int blade_material,
                                           int shield_model,
                                           int shield_material,
                                           Field_ActorPlacement* placement,
                                           int flags) {
  constexpr const char* kOperation = "CreateActorWithEquipment";
  if (placement == nullptr) {
    WarnActorRefused(kOperation, "no placement");
    return nullptr;
  }
  LiveActorSpawnHost host(kOperation);
  return PointerOf<Battle_Actor>(CreateActorWithEquipment(
      host, actor_id, model_id, blade_model, blade_material, shield_model,
      shield_material, DecodePlacement(*placement), flags));
}

// decomp: BATTLE.PRG func_800775C0 (src/BATTLE/BATTLE.PRG/146C.c:6578)
Battle_Actor* SpawnLiveRoomEnemy(int actor_id, Field_MpdEnemySpawn* spawn,
                                 Actors_CharacterInit* init, int flags) {
  constexpr const char* kOperation = "SpawnRoomEnemy";
  if (spawn == nullptr || init == nullptr) {
    WarnActorRefused(kOperation, "no spawn record or template");
    return nullptr;
  }
  LiveActorSpawnHost host(kOperation);
  return PointerOf<Battle_Actor>(
      SpawnRoomEnemy(host, actor_id, DecodeRoomSpawn(*spawn),
                     DecodeCharacterTemplate(*init), flags));
}

// decomp: BATTLE.PRG func_8007C8F8 (src/BATTLE/BATTLE.PRG/146C.c:8926)
int QueueLiveAshleyModelLoad(int model_id) {
  LiveActorSpawnHost host("QueueAshleyModelLoad");
  return QueueAshleyModelLoad(host, model_id);
}

// decomp: BATTLE.PRG func_8007C928 (src/BATTLE/BATTLE.PRG/146C.c:8936)
int QueueLiveScriptedActorSpawn(unsigned actor_id, int model_id,
                                Field_ActorPlacement* placement) {
  constexpr const char* kOperation = "QueueScriptedActorSpawn";
  Core_Debugf(
      "actors", "script spawn actor=%u model=%d existing=%p", actor_id,
      model_id,
      static_cast<void*>(actor_id < kSpawnActorSlots ? Battle_actors[actor_id]
                                                     : nullptr));
  if (placement == nullptr) {
    WarnActorRefused(kOperation, "no placement");
    return 0;
  }
  LiveActorSpawnHost host(kOperation);
  return QueueScriptedActorSpawn(host, actor_id, model_id,
                                 DecodePlacement(*placement));
}

// decomp: BATTLE.PRG func_8007CA20 (src/BATTLE/BATTLE.PRG/146C.c:8962)
void LoadLiveActorEquipmentModels(int actor_id, int blade_model,
                                  int shield_model) {
  LiveActorSpawnHost host("LoadActorEquipmentModels");
  LoadActorEquipmentModels(host, actor_id, blade_model, shield_model);
}

// decomp: BATTLE.PRG func_8007CAA4 (src/BATTLE/BATTLE.PRG/146C.c:8971)
void RefreshLiveActorEquipmentModels(int actor_id) {
  LiveActorSpawnHost host("RefreshActorEquipmentModels");
  RefreshActorEquipmentModels(host, actor_id);
}

// decomp: BATTLE.PRG func_8007D340 (src/BATTLE/BATTLE.PRG/146C.c:9256)
void RemoveLiveScriptedActor(int actor_id) {
  static_cast<void>(RemoveLiveActorAtSlot(static_cast<unsigned>(actor_id), 0));
}

// decomp: INITBTL.PRG func_800F9AD8 (src/BATTLE/INITBTL.PRG/18.c:310)
void RestoreLiveAshleyRuntimeState() {
  LiveActorSpawnHost host("RestoreAshleyRuntimeState");
  RestoreAshleyRuntimeState(host);
}

// decomp: INITBTL.PRG func_800F9CCC (src/BATTLE/INITBTL.PRG/18.c:361)
void SpawnLiveAshleyWithEquipment() {
  LiveActorSpawnHost host("SpawnAshleyWithEquipment");
  SpawnAshleyWithEquipment(host);
}
}  // namespace VS::Actors
