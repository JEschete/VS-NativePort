// Hand-maintained (tools/port/owned.csv).
// The battle action, actor and diagnostic helpers the Items Live bindings
// share (ItemRecordsLive.hpp).
#include "Items/ItemRecordsLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>

#include "Core/RandomLive.hpp"
#include "Items/ItemRecords.hpp"

namespace VS::Items::Live {
void WarnRefusal(const char* operation, ItemsStatus status) {
  if (status == ItemsStatus::kOk) return;
  const auto reason = Describe(status);
  Core_Warnf("items", "%s refused: %.*s", operation,
             static_cast<int>(reason.size()), reason.data());
}

void WarnNullRecord(const char* operation) {
  Core_Warnf("items", "%s refused: null record", operation);
}

ItemAction ToItemAction(const Battle_ActionDefinition& action) {
  ItemAction typed{};
  typed.type = static_cast<std::uint8_t>(action.type);
  typed.category = static_cast<std::uint8_t>(action.category);
  typed.first_hit_affinity = static_cast<std::uint8_t>(action.hits[0].affinity);
  for (std::size_t i = 0; i < kActionHitCount; ++i) {
    typed.hit_stat_factors[i] =
        static_cast<std::uint8_t>(action.hits[i].statFactor);
  }
  return typed;
}

Battle_Actor* BattleActor(std::int32_t actor_id) {
  if (actor_id < 0 ||
      static_cast<std::size_t>(actor_id) >= std::size(Battle_actors)) {
    return nullptr;
  }
  return Battle_actors[actor_id];
}

DefenderEquipment ToDefender(const Battle_ActorStats& stats) {
  DefenderEquipment defender{};
  defender.shield = Typed(stats.shield);
  for (std::size_t i = 0; i < kLimbCount; ++i) {
    auto& limb = defender.limbs[i];
    limb.max_hp = stats.limbs[i].maxHp;
    limb.name_index = stats.limbs[i].nameIndex;
    limb.armor = Typed(stats.limbs[i].armor);
  }
  if (const auto* actor = BattleActor(stats.actorId); actor != nullptr) {
    defender.weapon_drawn = (actor->weaponDrawn & 1) != 0;
  }
  defender.shows_notices = stats.actorId == 0;
  return defender;
}

void CommitDefender(Battle_ActorStats& stats,
                    const DefenderEquipment& defender) {
  Commit(stats.shield, defender.shield);
  for (std::size_t i = 0; i < kLimbCount; ++i) {
    Commit(stats.limbs[i].armor, defender.limbs[i].armor);
  }
}

Battle_ActorStats* AshleyStats(const char* operation) {
  if (Battle_characterState == nullptr ||
      Battle_characterState->stats == nullptr) {
    WarnRefusal(operation, ItemsStatus::kActorOutOfRange);
    return nullptr;
  }
  return Battle_characterState->stats;
}

AshleyWeaponState ToAshleyWeapon(const Battle_ActorStats& stats,
                                 const Battle_Actor& actor) {
  return AshleyWeaponState{
      .weapon = Typed(stats.weapon),
      .reach = static_cast<std::uint8_t>(stats.reach),
      .equipped_weapon_category = actor.equippedWeaponCategory,
      .current_range = static_cast<std::uint8_t>(stats.currentRange),
      .current_range_y = static_cast<std::uint8_t>(stats.currentRangeY),
      .current_range_z = static_cast<std::uint8_t>(stats.currentRangeZ),
      .current_attack_shape =
          static_cast<std::uint8_t>(stats.currentAttackShape),
      .current_attack_shape_angle =
          static_cast<std::uint8_t>(stats.currentAttackShapeAngle),
  };
}

void CommitAshleyWeapon(Battle_ActorStats& stats, Battle_Actor& actor,
                        const AshleyWeaponState& ashley) {
  Commit(stats.weapon, ashley.weapon);
  actor.equippedWeaponCategory = ashley.equipped_weapon_category;
  stats.currentRange = ashley.current_range;
  stats.currentRangeY = ashley.current_range_y;
  stats.currentRangeZ = ashley.current_range_z;
  stats.currentAttackShape = ashley.current_attack_shape;
  stats.currentAttackShapeAngle = ashley.current_attack_shape_angle;
}

std::int32_t LiveItemRandom::NextBounded(std::int32_t bound) {
  return VS::Core::NextLiveRandomBounded(bound);
}

std::int32_t LiveItemRandom::NextAveraged(std::int32_t bound) {
  return VS::Core::NextLiveRandomAveraged(bound);
}
}  // namespace VS::Items::Live
