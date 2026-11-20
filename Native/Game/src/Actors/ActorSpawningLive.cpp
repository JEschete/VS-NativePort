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

#include "Actors/ActorAllocation.hpp"
#include "Actors/ActorModelSlot.hpp"
#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorSpawning.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/ActorTableLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/CharacterInit.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/RoomActorCache.hpp"
#include "Actors/RoomEnemyMemoryLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Actors/Spawning.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/ActionAnimation.hpp"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCreationLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hud.hpp"
#include "Battle/HudStateLive.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/CdDrive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Runtime.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/Mpd.hpp"
#include "Field/RoomLoading.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Field/Zone.hpp"
#include "Items/Categories.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.hpp"
#include "State/Ashley.hpp"
#include "State/GameClockLive.hpp"

using VS::Battle::ActionSlot;
using VS::Battle::LimbStats;

namespace VS::Actors {
namespace {
using Content::AssetAddress;

constexpr int kAuxiliaryAllocation = 0;
constexpr int kEquippedAllocation = 1;
constexpr int kSecondaryBodyAllocation = 2;
// A room enemy whose second body takes the weapon model slot keeps an actor
// record there, not a weapon model (146C.c:6597).
constexpr std::size_t SecondaryBodyBytes() {
  return offsetof(VS::Actors::ActorAllocation, model_slots) +
         sizeof(VS::Actors::ActorRecord);
}
// The ZND record byte holding the zone enemy's body model.
constexpr std::size_t kZoneEnemyModelByte = 2;
constexpr int kNibbleMask = 0xF;
constexpr int kNibbleSignBit = 0x8;
constexpr int kNibbleRange = 0x10;

static_assert(std::size(VS::Battle::actors) == kBattleActorSlots);
static_assert(sizeof VS::Battle::ActorStats{}.name == kActorNameBytes);
static_assert(sizeof VS::Actors::CharacterInit{}.name == kActorNameBytes);
static_assert(std::size(VS::Battle::ActorStats{}.limbs) == kActorLimbs);
static_assert(std::size(VS::Actors::CharacterInit{}.limbs) == kActorLimbs);
static_assert(std::size(VS::Battle::ActorStats{}.status_timers) ==
              kStatusTimers);
static_assert(std::size(VS::Actors::usesSecondaryBodyModel) >
              std::numeric_limits<std::uint8_t>::max());
static_assert(std::size(VS::Items::weaponCategoryUpperIds) >=
              kWeaponCategoryBounds);
static_assert(std::size(VS::Items::inventory.weapons) == kInventoryWeapons);
static_assert(std::size(VS::Items::inventory.blades) == kInventoryBlades);
static_assert(std::size(VS::Items::inventory.shields) == kInventoryShields);
static_assert(std::size(VS::Items::inventory.armor) == kInventoryArmor);
static_assert(std::size(VS::State::ashleySnapshot.stats.limbs) == kActorLimbs);
static_assert(sizeof VS::Field::MpdEnemySpawn{}.settings ==
              kSpawnSettingsBytes);
static_assert(std::size(VS::Actors::RoomActorRecord{}.limb_hp) == kActorLimbs);

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

[[nodiscard]] VS::Battle::Actor* SlotActor(std::size_t slot) {
  return slot < std::size(VS::Battle::actors) ? VS::Battle::actors[slot]
                                              : nullptr;
}

[[nodiscard]] VS::Battle::ActorStats* SlotStats(std::size_t slot) {
  VS::Battle::Actor* actor = SlotActor(slot);
  return actor != nullptr ? actor->stats : nullptr;
}

[[nodiscard]] VS::Battle::ActorStats* CharacterStats() {
  return VS::Battle::characterState != nullptr
             ? VS::Battle::characterState->stats
             : nullptr;
}

[[nodiscard]] WeaponRange RangeOf(const VS::Items::WeaponRange& range) {
  return {.range = range.range,
          .second_range = range.second_range,
          .third_range = range.third_range,
          .attack_shape_angle = range.AttackShapeAngle()};
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
               VS::Battle::ActorStats& stats) {
  for (std::size_t i = 0; i < kActorNameBytes; ++i) {
    stats.name[i] = static_cast<char>(name[i]);
  }
}

void WriteInitialLimbs(const InitialStats& stats,
                       VS::Battle::ActorStats& live) {
  for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
    const LimbInitialStats& source = stats.limbs.at(limb);
    LimbStats& target = live.limbs[limb];
    target.hp = static_cast<std::int16_t>(source.hp);
    target.max_hp = static_cast<std::int16_t>(source.hp);
    target.agility_defense_bonus =
        static_cast<signed char>(source.agility_defense_bonus);
    target.anchor_bone = static_cast<char>(source.anchor_bone);
    target.name_index = source.name_index;
    target.chain_evasion = static_cast<char>(source.chain_evasion);
    std::ranges::copy(source.types, std::begin(target.types));
    std::ranges::copy(source.affinities, std::begin(target.affinities));
    for (std::size_t action = 0; action < kLimbActions; ++action) {
      const LimbActionStats& from = stats.limb_actions.at(limb).at(action);
      ActionSlot& to = live.limb_actions[limb][action];
      to.action_id = from.action_id;
      to.index = Nibble(from.index);
      to.effect_model_id = Nibble(from.effect_model);
      to.opaque3 = 1;
    }
    for (std::size_t target_limb = 0; target_limb < kDpSplashTargets;
         ++target_limb) {
      live.dp_splash[limb][target_limb] =
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
        auto* core = static_cast<VS::Actors::ActorCore*>(
            ZeroedBlock(sizeof(VS::Actors::ActorCore)));
        if (core == nullptr) {
          return SpawnedActor::kNone;
        }
        VS::Battle::Actor& actor = core->actor.actor;
        actor.stats = &core->actor.stats;
        actor.model = &core->model;
        actor.allocation_kind = kAuxiliaryAllocation;
        return IdOf<SpawnedActor>(&actor);
      }
      case ActorShape::kEquipped: {
        auto* allocation = static_cast<VS::Actors::ActorAllocation*>(
            ZeroedBlock(sizeof(VS::Actors::ActorAllocation)));
        if (allocation == nullptr) {
          return SpawnedActor::kNone;
        }
        VS::Battle::Actor& actor = allocation->core.actor.actor;
        actor.stats = &allocation->core.actor.stats;
        actor.model = &allocation->core.model;
        actor.equipment_models[0] = &allocation->model_slots[0];
        actor.equipment_models[1] = &allocation->model_slots[1];
        actor.allocation_kind = kEquippedAllocation;
        return IdOf<SpawnedActor>(&actor);
      }
      case ActorShape::kSecondaryBody: {
        auto* allocation = static_cast<VS::Actors::ActorAllocation*>(
            ZeroedBlock(SecondaryBodyBytes()));
        if (allocation == nullptr) {
          return SpawnedActor::kNone;
        }
        VS::Battle::Actor& actor = allocation->core.actor.actor;
        actor.stats = &allocation->core.actor.stats;
        actor.model = &allocation->core.model;
        actor.equipment_models[0] = &allocation->model_slots[0];
        actor.allocation_kind = kSecondaryBodyAllocation;
        return IdOf<SpawnedActor>(&actor);
      }
    }
    return SpawnedActor::kNone;
  }

  void Install(std::size_t slot, SpawnedActor actor) override {
    if (slot < std::size(VS::Battle::actors)) {
      VS::Battle::actors[slot] = PointerOf<VS::Battle::Actor>(actor);
    }
  }

  [[nodiscard]] AssetAddress ModelAddress(SpawnedActor actor) const override {
    const VS::Battle::Actor* live = PointerOf<VS::Battle::Actor>(actor);
    return live != nullptr ? Address(live->model) : AssetAddress::kNone;
  }

  [[nodiscard]] AssetAddress EquipmentAddress(
      SpawnedActor actor, EquipmentHand hand) const override {
    const VS::Battle::Actor* live = PointerOf<VS::Battle::Actor>(actor);
    if (live == nullptr) {
      return AssetAddress::kNone;
    }
    return Address(live->equipment_models[static_cast<std::size_t>(hand)]);
  }

  void SetEquippedWeaponCategory(SpawnedActor actor,
                                 std::uint8_t category) override {
    if (VS::Battle::Actor* live = PointerOf<VS::Battle::Actor>(actor);
        live != nullptr) {
      live->equipped_weapon_category = category;
    }
  }

  void WriteAuxiliaryIdentity(SpawnedActor actor,
                              const AuxiliaryIdentity& identity) override {
    VS::Battle::Actor* live = PointerOf<VS::Battle::Actor>(actor);
    if (live == nullptr) {
      return;
    }
    live->model_id = identity.model_id;
    live->kind = identity.kind;
    live->id = identity.id;
    live->next = nullptr;
    live->retain_after_defeat = 0;
  }

  [[nodiscard]] std::array<std::uint8_t, kWeaponCategoryBounds>
  WeaponCategoryBounds() const override {
    std::array<std::uint8_t, kWeaponCategoryBounds> bounds{};
    for (std::size_t i = 0; i < bounds.size(); ++i) {
      bounds.at(i) = VS::Items::weaponCategoryUpperIds[i];
    }
    return bounds;
  }

  // --- The installed actor in `slot`
  void WriteDefaults(std::size_t slot, const ActorDefaults& defaults) override {
    VS::Battle::Actor* actor = SlotActor(slot);
    if (actor == nullptr) {
      return;
    }
    actor->id = defaults.id;
    actor->model_id = defaults.model_id;
    actor->sub_type = 0;
    actor->blade_id = defaults.blade_id;
    actor->shield_id = defaults.shield_id;
    actor->weapon_drawn = defaults.disposition.weapon_drawn ? 1 : 0;
    actor->kind = defaults.disposition.kind;
    actor->interaction_flags = defaults.interaction_flags;
    actor->last_trigger_tile.word = 0;
    actor->facing = 0;
    actor->command_state = 0;
    actor->room_spawn_index = kNoRoomSpawn;
    actor->kill_counted = 0;
    actor->defeated = 0;
    VS::Battle::ActorStats* stats = actor->stats;
    if (stats == nullptr) {
      return;
    }
    stats->max_hp = kDefaultVitals;
    stats->current_hp = kDefaultVitals;
    stats->max_mp = kDefaultVitals;
    stats->current_mp = kDefaultVitals;
    stats->risk = 0;
    stats->action_recovery_timer = 0;
    stats->movement_class = static_cast<char>(defaults.gait.movement_class);
    stats->gait_class = static_cast<char>(defaults.gait.gait_class);
    stats->walk_speed = static_cast<char>(kDefaultWalkSpeed);
    stats->run_speed = static_cast<char>(kDefaultRunSpeed);
    stats->weapon.recovery_delay = kDefaultRecoveryDelay;
    stats->weapon.range.range = kDefaultWeaponRange[0];
    stats->weapon.range.second_range = kDefaultWeaponRange[1];
    stats->weapon.range.third_range = kDefaultWeaponRange[2];
    stats->cancels_hit_results = 0;
    stats->weapon.range.SetAttackShapeAngle(0);
    stats->actor_id = kNoRoomSpawn;
  }

  void WriteTemplateIdentity(std::size_t slot,
                             const TemplateIdentity& identity) override {
    VS::Battle::Actor* actor = SlotActor(slot);
    if (actor == nullptr) {
      return;
    }
    actor->id = identity.id;
    actor->model_id = identity.model_id;
    actor->sub_type = identity.sub_type;
    actor->blade_id = identity.blade_id;
    actor->shield_id = identity.shield_id;
    actor->weapon_drawn = identity.disposition.weapon_drawn ? 1 : 0;
    actor->kind = identity.disposition.kind;
    actor->retain_after_defeat = identity.retain_after_defeat ? 1 : 0;
    actor->interaction_flags = identity.interaction_flags;
    actor->command_state = 0;
    actor->last_trigger_tile.word = 0;
  }

  void WriteInitialStats(std::size_t slot, const InitialStats& stats) override {
    VS::Battle::ActorStats* live = SlotStats(slot);
    if (live == nullptr) {
      return;
    }
    live->actor_id = stats.actor_id;
    WriteName(stats.name, *live);
    live->max_hp = stats.hp;
    live->current_hp = stats.hp;
    live->risk = 0;
    live->max_mp = stats.mp;
    live->current_mp = stats.mp;
    live->total_strength = static_cast<std::uint16_t>(stats.strength);
    live->strength = static_cast<std::uint16_t>(stats.strength);
    live->total_intelligence = static_cast<std::uint16_t>(stats.intelligence);
    live->intelligence = static_cast<std::uint16_t>(stats.intelligence);
    live->total_agility = static_cast<std::uint16_t>(stats.agility);
    live->agility = static_cast<std::uint16_t>(stats.agility);
    live->movement_class = static_cast<char>(stats.gait.movement_class);
    live->gait_class = static_cast<char>(stats.gait.gait_class);
    live->walk_speed = static_cast<char>(stats.walk_speed);
    live->base_walk_speed = static_cast<char>(stats.walk_speed);
    live->run_speed = static_cast<char>(stats.run_speed);
    live->base_run_speed = static_cast<char>(stats.run_speed);
    live->enemy_class = stats.enemy_class;
    live->reach = stats.reach;
    live->recovery_delay_bonus = stats.recovery_delay_bonus;
    live->mobility_limb0 = stats.mobility_limbs[0];
    live->mobility_limb1 = stats.mobility_limbs[1];
    live->default_target_limb = stats.default_target_limb;
    live->cancels_hit_results = 0;
    live->is_analyzed = 0;
    WriteInitialLimbs(stats, *live);
    live->status_immunities = stats.status_immunities;
    live->statuses = stats.statuses;
    std::fill_n(std::begin(live->status_timers), kInitialStatusTimers, 0);
    live->action_recovery_timer = 0;
  }

  [[nodiscard]] WeaponRange BuildTemplateEquipment(
      std::size_t slot, TemplateEquipment equipment) override {
    VS::Battle::ActorStats* stats = SlotStats(slot);
    const auto* init = PointerOf<const VS::Actors::CharacterInit>(equipment);
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
    VS::Battle::ActorStats* stats = SlotStats(slot);
    if (stats == nullptr) {
      return;
    }
    stats->current_range = reach.range;
    stats->current_range_y = reach.range_y;
    stats->current_range_z = reach.range_z;
    stats->current_attack_shape_angle = reach.shape_angle;
    stats->current_attack_shape = kInitialAttackShape;
  }

  [[nodiscard]] SpawnedState State(std::size_t slot) const override {
    const VS::Battle::Actor* actor = SlotActor(slot);
    if (actor == nullptr) {
      return {};
    }
    SpawnedState state{.kind = actor->kind,
                       .weapon_drawn = actor->weapon_drawn};
    if (const VS::Battle::ActorStats* stats = actor->stats; stats != nullptr) {
      state.walk_speed = Byte(stats->walk_speed);
      state.run_speed = Byte(stats->run_speed);
    }
    return state;
  }

  void WriteSpawnDetails(std::size_t slot,
                         const SpawnDetails& details) override {
    VS::Battle::Actor* actor = SlotActor(slot);
    if (actor == nullptr) {
      return;
    }
    actor->facing = details.facing;
    actor->spawn_byte26 = details.spawn_byte;
    actor->room_spawn_index = details.room_spawn_index;
    actor->kill_counted = 0;
    actor->defeated = 0;
    actor->ai_group = details.settings.ai_group;
    actor->reserved30_bit4 = details.settings.group_reserved;
    actor->ai_linked_group = details.settings.ai_linked_group;
    actor->reserved31_bit6 = details.settings.linked_reserved;
    actor->reserved34 = details.settings.extra;
    actor->template_word2c = details.template_word;
    VS::Battle::ActorStats* stats = actor->stats;
    if (stats == nullptr) {
      return;
    }
    stats->drop_item.id = details.drop.id;
    stats->drop_item.count = details.drop.count;
    stats->second_drop_item.id = details.second_drop.id;
    stats->second_drop_item.count = details.second_drop.count;
    stats->random_drop_item.id = details.random_drop.id;
    stats->random_drop_item.drop_rate = details.random_drop.count;
  }

  void SetTemplateWord(std::size_t slot, std::int32_t word) override {
    if (VS::Battle::Actor* actor = SlotActor(slot); actor != nullptr) {
      actor->template_word2c = word;
    }
  }

  void RestoreRemembered(std::size_t slot,
                         const RememberedSpawn& remembered) override {
    VS::Battle::ActorStats* stats = SlotStats(slot);
    if (stats == nullptr) {
      return;
    }
    stats->current_hp = remembered.current_hp;
    stats->max_hp = remembered.max_hp;
    stats->current_mp = remembered.current_mp;
    stats->max_mp = remembered.max_mp;
    stats->strength = static_cast<std::uint16_t>(remembered.strength);
    stats->total_strength = static_cast<std::uint16_t>(remembered.strength);
    stats->intelligence = static_cast<std::uint16_t>(remembered.intelligence);
    stats->total_intelligence =
        static_cast<std::uint16_t>(remembered.intelligence);
    stats->agility = static_cast<std::uint16_t>(remembered.agility);
    stats->total_agility = static_cast<std::uint16_t>(remembered.agility);
    for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
      stats->limbs[limb].hp = remembered.limb_hp.at(limb);
    }
  }

  void InflictStatuses(std::size_t slot, std::int32_t statuses) override {
    if (VS::Battle::ActorStats* stats = SlotStats(slot); stats != nullptr) {
      VS::Battle::InflictLiveActorStatuses(statuses, stats);
    }
  }

  void LinkAfterLeader(std::size_t slot) override {
    VS::Battle::Actor* actor = SlotActor(slot);
    VS::Battle::Actor* leader = VS::Battle::actors[kAshleySlot];
    if (actor == nullptr || leader == nullptr || actor == leader) {
      return;
    }
    actor->next = leader->next;
    leader->next = actor;
  }

  void ClearNext(std::size_t slot) override {
    if (VS::Battle::Actor* actor = SlotActor(slot); actor != nullptr) {
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
    const VS::Battle::ActorStats* stats = SlotStats(slot);
    if (stats == nullptr) {
      return std::nullopt;
    }
    return EquippedModels{.has_blade = stats->weapon.blade.id != 0,
                          .weapon_model = stats->weapon.weapon_model_id,
                          .blade_material = stats->weapon.blade.material,
                          .has_shield = stats->shield.base.id != 0,
                          .shield_model = stats->shield.weapon_model_id,
                          .shield_material = stats->shield.base.material};
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
    Battle::SetLiveEnemyCombatPose(static_cast<int>(slot), pose);
  }
  [[nodiscard]] int CreateAiState(std::size_t slot, int flags) override {
    VS::Battle::Actor* actor = SlotActor(slot);
    if (actor == nullptr) {
      return 0;
    }
    return Battle::Ai::CreateLiveActorAiState(actor, flags);
  }
  void InitializePlaceholder(std::size_t slot, int actor_id) override {
    InitializeLivePlaceholderActor(static_cast<int>(slot), actor_id);
  }
  void QueueBaseSequence(SpawnedActor actor, std::size_t slot) override {
    QueueLiveActorBaseSequenceLoad(PointerOf<VS::Battle::Actor>(actor),
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
    return VS::Actors::usesSecondaryBodyModel[model_id] != 0;
  }

  // Both the ZND records and the ZUD files have zone_enemy_count entries
  // (RoomLoading.cpp:143-151).
  [[nodiscard]] int ZoneEnemyCount() const override {
    if (VS::Field::zoneContext.zone_enemies == nullptr ||
        VS::Field::zoneContext.zud_files == nullptr) {
      return 0;
    }
    return std::max(VS::Field::zoneContext.zone_enemy_count, 0);
  }

  [[nodiscard]] std::uint8_t ZoneEnemyModel(int zud) const override {
    if (zud < 0 || zud >= ZoneEnemyCount()) {
      return 0;
    }
    return VS::Field::zoneContext.zone_enemies[zud].bytes[kZoneEnemyModelByte];
  }

  [[nodiscard]] std::optional<RememberedSpawn> Remembered(
      std::uint8_t spawn_index) const override {
    if (!IsLiveRememberedRoom(VS::Field::zoneContext.zone_id,
                              VS::Field::zoneContext.room_id)) {
      return std::nullopt;
    }
    VS::Actors::RoomActorRecord record{};
    if (CopyLiveRememberedEnemy(spawn_index, &record) == nullptr) {
      return std::nullopt;
    }
    return DecodeRemembered(record);
  }

  // A block without a matching index, as after an interrupted read, never
  // matches a ZUD file, so the rules free it as the original did.
  [[nodiscard]] std::optional<int> CachedZud() const override {
    if (VS::Field::zoneContext.zud_data == nullptr) {
      return std::nullopt;
    }
    return VS::Field::zoneContext.loaded_zud_index_plus_one - 1;
  }

  // The cached file, bounded by its file size and its heap payload.
  [[nodiscard]] ZudBlock CachedZudBlock() const override {
    const void* data = VS::Field::zoneContext.zud_data;
    const int zud = VS::Field::zoneContext.loaded_zud_index_plus_one - 1;
    if (data == nullptr || zud < 0 || zud >= ZoneEnemyCount()) {
      return {};
    }
    const auto file_bytes = static_cast<std::size_t>(
        std::max(VS::Field::zoneContext.zud_files[zud].size, 0));
    const auto capacity = Core::Memory::LivePayloadCapacity(data);
    const std::size_t bytes =
        capacity.has_value() ? std::min(file_bytes, *capacity) : 0;
    return {.address = Address(data),
            .bytes = {static_cast<const std::byte*>(data), bytes}};
  }

  void FreeZud() override {
    if (VS::Field::zoneContext.zud_data != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(VS::Field::zoneContext.zud_data);
    }
    VS::Field::zoneContext.zud_data = nullptr;
    VS::Field::zoneContext.loaded_zud_index_plus_one = 0;
  }

  [[nodiscard]] Content::DiscFileExtent ZudFile(int zud) const override {
    if (zud < 0 || zud >= ZoneEnemyCount()) {
      return {};
    }
    const VS::Content::DiscFile& file = VS::Field::zoneContext.zud_files[zud];
    return {.sector = file.lba, .bytes = file.size};
  }

  [[nodiscard]] bool ZudReadPending() const override {
    return VS::Field::zoneContext.zud_disc_request != nullptr;
  }

  [[nodiscard]] bool AllocateZud(Content::DiscFileExtent file) override {
    if (file.bytes < 0) {
      return false;
    }
    VS::Field::zoneContext.zud_data = Core::Memory::AllocateLiveFromHighEnd(
        static_cast<std::size_t>(file.bytes));
    return VS::Field::zoneContext.zud_data != nullptr;
  }

  void BeginZudRead(Content::DiscFileExtent file) override {
    VS::Content::DiscFile disc{};
    disc.lba = file.sector;
    disc.size = file.bytes;
    VS::Content::DiscQueueSlot* request =
        Content::AllocateLiveDiscRequest(&disc);
    if (request != nullptr) {
      Content::EnqueueLivePriorityDiscRequest(request,
                                              VS::Field::zoneContext.zud_data);
    } else {
      request = Content::CompleteLiveRefusedDiscRequest(
          &disc, VS::Field::zoneContext.zud_data);
    }
    VS::Field::zoneContext.zud_disc_request = request;
  }

  [[nodiscard]] ZudReadState PollZudRead() const override {
    const VS::Content::DiscQueueSlot* request =
        VS::Field::zoneContext.zud_disc_request;
    if (request == nullptr) {
      return ZudReadState::kFailed;
    }
    switch (request->state) {
      case Content::DiscRequestState::kLoaded:
        return ZudReadState::kLoaded;
      case Content::DiscRequestState::kError:
        return ZudReadState::kFailed;
      default:
        return ZudReadState::kPending;
    }
  }

  void FinishZudRead(int zud, bool loaded) override {
    Content::ReleaseLiveDiscRequest(VS::Field::zoneContext.zud_disc_request);
    VS::Field::zoneContext.zud_disc_request = nullptr;
    if (loaded) {
      VS::Field::zoneContext.loaded_zud_index_plus_one = zud + 1;
    } else {
      FreeZud();
    }
  }

  // --- Ashley
  [[nodiscard]] AshleySnapshot ReadAshleySnapshot() const override {
    const VS::State::AshleyStatsSnapshot& saved =
        VS::State::ashleySnapshot.stats;
    AshleySnapshot snapshot{.pending = saved.is_pending != 0,
                            .current_hp = saved.current_hp,
                            .max_hp = saved.max_hp,
                            .current_mp = saved.current_mp,
                            .max_mp = saved.max_mp,
                            .strength = saved.strength,
                            .total_strength = saved.total_strength,
                            .intelligence = saved.intelligence,
                            .total_intelligence = saved.total_intelligence,
                            .agility = saved.agility,
                            .total_agility = saved.total_agility,
                            .risk = saved.risk,
                            .recovery_timer = saved.action_recovery_timer,
                            .statuses = saved.statuses,
                            .weapon_drawn = saved.weapon_drawn,
                            .camera_pitch = saved.camera_pitch,
                            .camera_distance = saved.camera_distance};
    for (std::size_t i = 0; i < kStatusTimers; ++i) {
      snapshot.status_timers.at(i) = saved.status_timers[i];
    }
    for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
      const VS::State::AshleyLimbSnapshot& source = saved.limbs[limb];
      AshleyLimbSnapshot& target = snapshot.limbs.at(limb);
      target.hp = source.hp;
      target.max_hp = source.max_hp;
      std::ranges::copy(source.types, target.types.begin());
      std::ranges::copy(source.affinities, target.affinities.begin());
    }
    return snapshot;
  }

  [[nodiscard]] SpawnedActor Character() const override {
    return CharacterStats() != nullptr
               ? IdOf<SpawnedActor>(VS::Battle::characterState)
               : SpawnedActor::kNone;
  }

  void SetCharacter(SpawnedActor actor) override {
    VS::Battle::characterState = PointerOf<VS::Battle::Actor>(actor);
  }

  void RestoreAshleyVitals(const AshleySnapshot& snapshot) override {
    VS::Battle::ActorStats* stats = CharacterStats();
    if (stats == nullptr) {
      return;
    }
    stats->current_hp = snapshot.current_hp;
    stats->max_hp = snapshot.max_hp;
    stats->current_mp = snapshot.current_mp;
    stats->max_mp = snapshot.max_mp;
    stats->strength = static_cast<std::uint16_t>(snapshot.strength);
    stats->total_strength = static_cast<std::uint16_t>(snapshot.total_strength);
    stats->intelligence = static_cast<std::uint16_t>(snapshot.intelligence);
    stats->total_intelligence =
        static_cast<std::uint16_t>(snapshot.total_intelligence);
    stats->agility = static_cast<std::uint16_t>(snapshot.agility);
    stats->total_agility = static_cast<std::uint16_t>(snapshot.total_agility);
    stats->risk = static_cast<std::uint16_t>(snapshot.risk);
    stats->action_recovery_timer = snapshot.recovery_timer;
  }

  void InflictCharacterStatuses(std::int32_t statuses) override {
    if (VS::Battle::ActorStats* stats = CharacterStats(); stats != nullptr) {
      VS::Battle::InflictLiveActorStatuses(statuses, stats);
    }
  }

  void RestoreAshleyConditions(const AshleySnapshot& snapshot) override {
    VS::Battle::ActorStats* stats = CharacterStats();
    if (stats == nullptr) {
      return;
    }
    std::ranges::copy(snapshot.status_timers, std::begin(stats->status_timers));
    for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
      const AshleyLimbSnapshot& source = snapshot.limbs.at(limb);
      LimbStats& target = stats->limbs[limb];
      target.hp = static_cast<std::int16_t>(source.hp);
      target.max_hp = static_cast<std::int16_t>(source.max_hp);
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
    if (VS::Battle::characterState != nullptr) {
      VS::Battle::characterState->weapon_drawn = weapon_drawn;
    }
  }

  void SetHudBattleMode(int mode) override {
    VS::Battle::SetLiveHudBattleMode(mode);
  }

  void ClearAshleySnapshotPending() override {
    VS::State::ashleySnapshot.stats.is_pending = 0;
  }

  void RestoreCamera(std::int32_t pitch, std::int32_t distance) override {
    VS::Field::sphericalCamera.motion.target_yaw = pitch;
    VS::Field::sphericalCamera.preferred_distance = distance;
  }

  [[nodiscard]] AshleyInventory ReadAshleyInventory() const override {
    const VS::Items::InventoryContents& live = VS::Items::inventory;
    AshleyInventory inventory;
    for (std::size_t i = 0; i < kInventoryWeapons; ++i) {
      inventory.weapons.at(i) = {.equipped = live.weapons[i].is_equipped != 0,
                                 .blade = live.weapons[i].blade};
    }
    for (std::size_t i = 0; i < kInventoryBlades; ++i) {
      inventory.blades.at(i) = {.model = live.blades[i].weapon_model_id,
                                .material = live.blades[i].material};
    }
    for (std::size_t i = 0; i < kInventoryShields; ++i) {
      const VS::Items::InventoryShield& shield = live.shields[i];
      inventory.shields.at(i) = {.equipped = shield.is_equipped != 0,
                                 .model = shield.base.weapon_model_id,
                                 .material = shield.base.material};
    }
    for (std::size_t i = 0; i < kInventoryArmor; ++i) {
      inventory.armor_limbs.at(i) = live.armor[i].limb;
    }
    return inventory;
  }

  [[nodiscard]] CharacterTemplate AshleyTemplate() const override {
    return DecodeCharacterTemplate(VS::Actors::ashleyInitialLoadout);
  }

  [[nodiscard]] WeaponRange LoadAshleyWeapon(
      std::optional<std::size_t> weapon) override {
    VS::Battle::ActorStats* stats = CharacterStats();
    if (stats == nullptr) {
      return {};
    }
    Items::LoadLiveInventoryWeaponStats(
        &stats->weapon, weapon && *weapon < kInventoryWeapons
                            ? &VS::Items::inventory.weapons[*weapon]
                            : nullptr);
    return RangeOf(stats->weapon.range);
  }

  void LoadAshleyShield(std::optional<std::size_t> shield) override {
    if (VS::Battle::ActorStats* stats = CharacterStats(); stats != nullptr) {
      Items::LoadLiveInventoryShieldStats(
          &stats->shield, shield && *shield < kInventoryShields
                              ? &VS::Items::inventory.shields[*shield]
                              : nullptr);
    }
  }

  void LoadAshleyArmor(std::size_t limb,
                       std::optional<std::size_t> armor) override {
    VS::Battle::ActorStats* stats = CharacterStats();
    if (stats == nullptr || limb >= kActorLimbs) {
      return;
    }
    Items::LoadLiveInventoryArmorStats(&stats->limbs[limb].armor,
                                       armor && *armor < kInventoryArmor
                                           ? &VS::Items::inventory.armor[*armor]
                                           : nullptr);
  }

  void LoadAshleyAccessory(std::optional<std::size_t> armor) override {
    if (VS::Battle::ActorStats* stats = CharacterStats(); stats != nullptr) {
      Items::LoadLiveInventoryAccessoryStats(
          &stats->accessory, armor && *armor < kInventoryArmor
                                 ? &VS::Items::inventory.armor[*armor]
                                 : nullptr);
    }
  }

 private:
  const char* operation_;
};

LimbTemplate DecodeLimb(const VS::Actors::LimbInit& limb) {
  LimbTemplate decoded{.hp = limb.hp,
                       .agility_defense_bonus = limb.agility_defense_bonus,
                       .chain_evasion = limb.chain_evasion};
  std::ranges::copy(limb.types, decoded.types.begin());
  std::ranges::copy(limb.affinities, decoded.affinities.begin());
  for (std::size_t action = 0; action < kLimbActions; ++action) {
    const VS::Actors::LimbActionInit& source = limb.actions[action];
    decoded.actions.at(action) = {.action_id = source.action_id,
                                  .effect_model = source.effect_model,
                                  .index = source.index};
  }
  decoded.dp_splash = limb.dp_splash;
  return decoded;
}
}  // namespace

CharacterTemplate DecodeCharacterTemplate(
    const VS::Actors::CharacterInit& init) {
  CharacterTemplate decoded{
      .sub_type = init.sub_type,
      .model_id = init.model_id,
      .reach = init.reach,
      .enemy_class = init.enemy_class,
      .hp = init.hp,
      .mp = init.mp,
      .strength = init.strength,
      .intelligence = init.intelligence,
      .agility = init.agility,
      .recovery_delay_bonus = init.recovery_delay_bonus,
      .material = init.material,
      .walk_speed = init.walk_speed,
      .gait_class = init.gait_class,
      .run_speed = init.run_speed,
      .status_immunities = init.status_immunities,
      .template_word = init.template_word,
      .mobility_limbs = init.mobility_limbs,
      .default_target_limb = init.default_target_limb,
      .blade_model =
          static_cast<std::uint8_t>(init.weapon.blade.weapon_model_id),
      .blade_category = init.weapon.blade.category,
      .weapon_material = init.weapon.material,
      .shield_model =
          static_cast<std::uint8_t>(init.shield.base.weapon_model_id),
      .shield_material = init.shield.material,
      .zud_index = init.zud_index,
      .equipment = IdOf<TemplateEquipment>(&init)};
  for (std::size_t i = 0; i < kActorNameBytes; ++i) {
    decoded.name.at(i) = Byte(init.name[i]);
  }
  for (std::size_t limb = 0; limb < kActorLimbs; ++limb) {
    decoded.limbs.at(limb) = DecodeLimb(init.limbs[limb]);
  }
  return decoded;
}

RoomSpawn DecodeRoomSpawn(const VS::Field::MpdEnemySpawn& spawn) {
  RoomSpawn decoded{
      .index = spawn.index,
      .placement = {.tile_x = spawn.tile_x,
                    .direction = spawn.direction,
                    .tile_z = spawn.tile_z,
                    .height = spawn.height},
      .drop = {.id = spawn.item_id, .count = spawn.item_count},
      .second_drop = {.id = spawn.item2_id, .count = spawn.item2_count},
      .random_drop = {.id = spawn.item3_id, .count = spawn.item3_count},
      .facing = spawn.facing,
      .variant = spawn.variant,
      .combat_pose = spawn.reserved26};
  std::ranges::copy(spawn.settings, decoded.settings.begin());
  return decoded;
}

RememberedSpawn DecodeRemembered(const VS::Actors::RoomActorRecord& record) {
  RememberedSpawn decoded{.ai_flags = record.ai_flags,
                          .current_hp = record.current_hp,
                          .max_hp = record.max_hp,
                          .current_mp = record.current_mp,
                          .max_mp = record.max_mp,
                          .strength = record.strength,
                          .intelligence = record.intelligence,
                          .agility = record.agility,
                          .statuses = record.statuses,
                          .placement = {.tile_x = record.tile_x,
                                        .direction = record.direction,
                                        .tile_z = record.tile_z,
                                        .height = record.height}};
  std::ranges::copy(record.limb_hp, decoded.limb_hp.begin());
  return decoded;
}

// decomp: BATTLE.PRG func_800765B0 (src/BATTLE/BATTLE.PRG/146C.c:6064)
VS::Battle::Actor* CreateLiveAuxiliaryActor(int index, int actor_id,
                                            const SpawnPlacement* placement,
                                            int material, int /*unused*/) {
  constexpr const char* kOperation = "CreateAuxiliaryActor";
  if (placement == nullptr) {
    WarnActorRefused(kOperation, "no placement");
    return nullptr;
  }
  LiveActorSpawnHost host(kOperation);
  return PointerOf<VS::Battle::Actor>(
      CreateAuxiliaryActor(host, index, actor_id, *placement, material));
}

// decomp: BATTLE.PRG func_800774FC (src/BATTLE/BATTLE.PRG/146C.c:6562)
VS::Battle::Actor* CreateLiveActorWithEquipment(
    int actor_id, int model_id, int blade_model, int blade_material,
    int shield_model, int shield_material, const SpawnPlacement* placement,
    int flags) {
  constexpr const char* kOperation = "CreateActorWithEquipment";
  if (placement == nullptr) {
    WarnActorRefused(kOperation, "no placement");
    return nullptr;
  }
  LiveActorSpawnHost host(kOperation);
  return PointerOf<VS::Battle::Actor>(CreateActorWithEquipment(
      host, actor_id, model_id, blade_model, blade_material, shield_model,
      shield_material, *placement, flags));
}

// decomp: BATTLE.PRG func_800775C0 (src/BATTLE/BATTLE.PRG/146C.c:6578)
VS::Battle::Actor* SpawnLiveRoomEnemy(int actor_id,
                                      VS::Field::MpdEnemySpawn* spawn,
                                      VS::Actors::CharacterInit* init,
                                      int flags) {
  constexpr const char* kOperation = "SpawnRoomEnemy";
  if (spawn == nullptr || init == nullptr) {
    WarnActorRefused(kOperation, "no spawn record or template");
    return nullptr;
  }
  LiveActorSpawnHost host(kOperation);
  return PointerOf<VS::Battle::Actor>(
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
                                const SpawnPlacement* placement) {
  constexpr const char* kOperation = "QueueScriptedActorSpawn";
  VS::Core::Debugf("actors", "script spawn actor=%u model=%d existing=%p",
                   actor_id, model_id,
                   static_cast<void*>(actor_id < kSpawnActorSlots
                                          ? VS::Battle::actors[actor_id]
                                          : nullptr));
  if (placement == nullptr) {
    WarnActorRefused(kOperation, "no placement");
    return 0;
  }
  LiveActorSpawnHost host(kOperation);
  return QueueScriptedActorSpawn(host, actor_id, model_id, *placement);
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
