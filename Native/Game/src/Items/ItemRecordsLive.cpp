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

using VS::Battle::ActionDefinition;

namespace VS::Items::Live {
void WarnRefusal(const char* operation, ItemsStatus status) {
  if (status == ItemsStatus::kOk) return;
  const auto reason = Describe(status);
  VS::Core::Warnf("items", "%s refused: %.*s", operation,
                  static_cast<int>(reason.size()), reason.data());
}

void WarnNullRecord(const char* operation) {
  VS::Core::Warnf("items", "%s refused: null record", operation);
}

ItemAction ToItemAction(const ActionDefinition& action) {
  ItemAction typed{};
  typed.type = static_cast<std::uint8_t>(action.type);
  typed.category = static_cast<std::uint8_t>(action.category);
  typed.first_hit_affinity = static_cast<std::uint8_t>(action.hits[0].affinity);
  for (std::size_t i = 0; i < kActionHitCount; ++i) {
    typed.hit_stat_factors[i] =
        static_cast<std::uint8_t>(action.hits[i].stat_factor);
  }
  return typed;
}

VS::Battle::Actor* BattleActor(std::int32_t actor_id) {
  if (actor_id < 0 ||
      static_cast<std::size_t>(actor_id) >= std::size(VS::Battle::actors)) {
    return nullptr;
  }
  return VS::Battle::actors[actor_id];
}

DefenderEquipment ToDefender(const VS::Battle::ActorStats& stats) {
  DefenderEquipment defender{};
  defender.shield = stats.shield;
  for (std::size_t i = 0; i < kLimbCount; ++i) {
    auto& limb = defender.limbs[i];
    limb.max_hp = stats.limbs[i].max_hp;
    limb.name_index = stats.limbs[i].name_index;
    limb.armor = stats.limbs[i].armor;
  }
  if (const auto* actor = BattleActor(stats.actor_id); actor != nullptr) {
    defender.weapon_drawn = (actor->weapon_drawn & 1) != 0;
  }
  defender.shows_notices = stats.actor_id == 0;
  return defender;
}

void CommitDefender(VS::Battle::ActorStats& stats,
                    const DefenderEquipment& defender) {
  stats.shield = defender.shield;
  for (std::size_t i = 0; i < kLimbCount; ++i) {
    stats.limbs[i].armor = defender.limbs[i].armor;
  }
}

VS::Battle::ActorStats* AshleyStats(const char* operation) {
  if (VS::Battle::characterState == nullptr ||
      VS::Battle::characterState->stats == nullptr) {
    WarnRefusal(operation, ItemsStatus::kActorOutOfRange);
    return nullptr;
  }
  return VS::Battle::characterState->stats;
}

AshleyWeaponState ToAshleyWeapon(const VS::Battle::ActorStats& stats,
                                 const VS::Battle::Actor& actor) {
  return AshleyWeaponState{
      .weapon = stats.weapon,
      .reach = static_cast<std::uint8_t>(stats.reach),
      .equipped_weapon_category = actor.equipped_weapon_category,
      .current_range = static_cast<std::uint8_t>(stats.current_range),
      .current_range_y = static_cast<std::uint8_t>(stats.current_range_y),
      .current_range_z = static_cast<std::uint8_t>(stats.current_range_z),
      .current_attack_shape =
          static_cast<std::uint8_t>(stats.current_attack_shape),
      .current_attack_shape_angle =
          static_cast<std::uint8_t>(stats.current_attack_shape_angle),
  };
}

void CommitAshleyWeapon(VS::Battle::ActorStats& stats, VS::Battle::Actor& actor,
                        const AshleyWeaponState& ashley) {
  stats.weapon = ashley.weapon;
  actor.equipped_weapon_category = ashley.equipped_weapon_category;
  stats.current_range = ashley.current_range;
  stats.current_range_y = ashley.current_range_y;
  stats.current_range_z = ashley.current_range_z;
  stats.current_attack_shape = ashley.current_attack_shape;
  stats.current_attack_shape_angle = ashley.current_attack_shape_angle;
}

std::int32_t LiveItemRandom::NextBounded(std::int32_t bound) {
  return VS::Core::NextLiveRandomBounded(bound);
}

std::int32_t LiveItemRandom::NextAveraged(std::int32_t bound) {
  return VS::Core::NextLiveRandomAveraged(bound);
}
}  // namespace VS::Items::Live
