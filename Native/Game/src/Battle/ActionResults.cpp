// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionResults.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
constexpr unsigned kWorldLabelIdMask = 0x1F0;
constexpr unsigned kWorldLabelIdTag = 0x80;

enum class ImpactKind : std::uint8_t {
  kHit = 0,
  kAlternateHit = 1,
  kBlock = 3,
  kAlternateBlock = 4,
  kRecovery = 5,
};

enum class Reaction : std::uint8_t { kHit, kBlock, kRecover, kNone };

Reaction NextReaction(Reaction current, std::uint8_t impact_kind,
                      ActionTargetEffects& effects) {
  switch (static_cast<ImpactKind>(impact_kind)) {
    case ImpactKind::kHit:
    case ImpactKind::kAlternateHit:
      return Reaction::kHit;
    case ImpactKind::kRecovery:
      return Reaction::kRecover;
    case ImpactKind::kBlock:
    case ImpactKind::kAlternateBlock:
      return current == Reaction::kNone && !effects.IsAreaAction()
                 ? Reaction::kBlock
                 : current;
    default:
      return current;
  }
}

using Field = ActionResultField;
constexpr std::uint16_t kMaximumStat = 999;
constexpr std::uint32_t kReductionLabel = 0x80000000;
constexpr std::uint8_t kAlternateHpReduction = 3;
constexpr std::array kValueLabels{0, 1, 8, 5, 6, 7, 3, 4};

ActionResultChange Change(const ActorActionResult& hit, Field field) {
  return hit.changes.at(static_cast<std::size_t>(field));
}

template <typename Value>
Value AddClamped(Value value, std::int16_t amount, Value maximum) {
  const auto sum = static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(value) + static_cast<std::uint32_t>(amount));
  if constexpr (std::is_signed_v<Value>) {
    return std::min(Core::Math::WrapSigned16(sum), maximum);
  } else {
    return std::min(sum, maximum);
  }
}

template <typename Value>
Value SubtractFloored(Value value, std::int16_t amount) {
  if (value <= amount) return 0;
  const auto difference = static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(value) - static_cast<std::uint32_t>(amount));
  if constexpr (std::is_signed_v<Value>) {
    return Core::Math::WrapSigned16(difference);
  } else {
    return difference;
  }
}

void ApplyAttribute(StatusAttributes& attribute, ActionResultChange change) {
  if (change.mode == 2) {
    attribute.total = AddClamped(attribute.total, change.amount, kMaximumStat);
    attribute.current =
        AddClamped(attribute.current, change.amount, kMaximumStat);
  } else {
    attribute.total = SubtractFloored(attribute.total, change.amount);
    attribute.current = SubtractFloored(attribute.current, change.amount);
  }
}

void ApplyHp(ActorVitalsState& state, const ActorActionResult& hit,
             std::uint8_t mode) {
  const auto amount = Change(hit, Field::kHp).amount;
  if (mode == 2) {
    state.status.hp = AddClamped(state.status.hp, amount, state.status.max_hp);
  } else {
    state.status.hp = SubtractFloored(state.status.hp, amount);
  }
  for (std::size_t index = 0; index < hit.limbs.size(); ++index) {
    auto& limb = state.status.limbs.at(index);
    const auto& delta = hit.limbs.at(index);
    if (mode == 2) {
      limb.hp = AddClamped(limb.hp, delta.hp, limb.max_hp);
    } else {
      limb.hp = SubtractFloored(limb.hp, delta.hp);
      limb.armor.current_dp =
          SubtractFloored(limb.armor.current_dp, delta.armor_dp);
    }
  }
}

void ApplyValue(ActorVitalsState& state, const ActorActionResult& hit,
                Field field, std::uint8_t mode) {
  auto change = Change(hit, field);
  change.mode = mode;  // The switch is selected before the label callback.
  switch (field) {
    case Field::kHp:
      ApplyHp(state, hit, mode);
      break;
    case Field::kMp:
      state.mp = mode == 2 ? AddClamped(state.mp, change.amount, state.max_mp)
                           : SubtractFloored(state.mp, change.amount);
      break;
    case Field::kRisk:
      state.risk = SubtractFloored(state.risk, change.amount);
      break;
    case Field::kStrength:
      ApplyAttribute(state.status.strength, change);
      break;
    case Field::kIntelligence:
      ApplyAttribute(state.status.intelligence, change);
      break;
    case Field::kAgility:
      ApplyAttribute(state.status.agility, change);
      break;
    case Field::kMaxHp:
      state.status.max_hp = AddClamped(state.status.max_hp, change.amount,
                                       static_cast<std::int16_t>(kMaximumStat));
      break;
    case Field::kMaxMp:
      state.max_mp = AddClamped(state.max_mp, change.amount,
                                static_cast<std::int16_t>(kMaximumStat));
      break;
    default:
      break;
  }
}

bool HasValueEffect(Field field, std::uint8_t mode) {
  if (field == Field::kRisk || field == Field::kMaxHp ||
      field == Field::kMaxMp) {
    return mode == 2;
  }
  return mode == 1 || mode == 2 ||
         (field == Field::kHp && mode == kAlternateHpReduction);
}

bool ApplyEquipment(ActorVitalsState& state, const ActorActionResult& hit) {
  auto& weapon = state.status.weapon;
  auto& shield = state.status.shield;
  const auto dp = Change(hit, Field::kWeaponDp);
  if (dp.mode == 2) {
    weapon.current_dp = AddClamped(weapon.current_dp, dp.amount, weapon.max_dp);
  }
  const auto pp = Change(hit, Field::kWeaponPp);
  if (pp.mode == 2) {
    weapon.current_pp = AddClamped(weapon.current_pp, pp.amount, weapon.max_pp);
  } else if (pp.mode == 1) {
    weapon.current_pp = SubtractFloored(weapon.current_pp, pp.amount);
  }
  const auto armor = Change(hit, Field::kArmorDp);
  if (armor.mode == 1) {
    if (hit.limb < 0 ||
        static_cast<std::size_t>(hit.limb) >= state.status.limbs.size()) {
      return false;
    }
    shield.current_dp = SubtractFloored(shield.current_dp, armor.amount);
    auto& limb = state.status.limbs.at(static_cast<std::size_t>(hit.limb));
    limb.armor.current_dp =
        SubtractFloored(limb.armor.current_dp, armor.amount);
  }
  const auto shield_pp = Change(hit, Field::kShieldPp);
  if (shield_pp.mode == 1) {
    shield.current_pp = SubtractFloored(shield.current_pp, shield_pp.amount);
  }
  return true;
}

std::uint32_t RequestedStatuses(ActorActionResultWorld& world, bool removing) {
  const auto hit = world.ReadHit();
  return removing ? hit.inflict_statuses : hit.remove_statuses;
}

void ApplyStatuses(ActorVitalsAccess& stats, int target,
                   ActorActionResultWorld& world, bool removing) {
  for (std::uint32_t bit = 1; bit != 0; bit <<= 1U) {
    const auto requested = RequestedStatuses(world, removing) & bit;
    const auto state = stats.Read();
    const bool active = (state.status.statuses & bit) != 0;
    if (requested == 0 || active != removing) continue;
    if (target == 0 || state.status.hp != 0) {
      world.StatusLabel(target, requested, removing);
    }
    const auto mask = RequestedStatuses(world, removing) & bit;
    if (removing) {
      world.RemoveStatuses(stats, mask);
    } else {
      world.InflictStatuses(stats, mask);
    }
  }
}
}  // namespace

// decomp: BATTLE.PRG func_8006CB04 (146C/func_8006CB04.s); compared with
// rood-boi's PC 146C.c callback.
void ApplyActionTargetEffects(int actor_id, ActionTargetEffects& effects) {
  if (actor_id >= kActionActorSlots) {
    if ((static_cast<unsigned>(actor_id) & kWorldLabelIdMask) ==
        kWorldLabelIdTag) {
      effects.QueueWorldLabel();
    }
    return;
  }
  Reaction reaction = Reaction::kNone;
  for (std::size_t index = 0; index < effects.HitCount(); ++index) {
    const ActionTargetHit hit = effects.Hit(index);
    if (hit.targets_position || hit.target_actor != actor_id) continue;
    reaction = NextReaction(reaction, hit.impact_kind, effects);
    effects.ApplyHit(index);
  }
  switch (reaction) {
    case Reaction::kHit:
      effects.PlayDamageReaction(actor_id);
      break;
    case Reaction::kBlock:
      // Gimmick actors have no actor record. func_8009DDF4 has no NULL check:
      // on PS1 it reads low RAM, on x64 it faults (#251).
      if (effects.HasActorRecord(actor_id)) effects.PlayDefense(actor_id);
      break;
    case Reaction::kRecover:
      if (actor_id == 0) effects.PlayRecoveryPose();
      break;
    case Reaction::kNone:
      break;
  }
}

// decomp: _applyBattleAbilityEffect (src/BATTLE/BATTLE.PRG/146C.c:14018).
bool ApplyActorActionResult(ActorActionResultWorld& world) {
  world.CommitEquipmentStats();
  auto hit = world.ReadHit();
  const int target = hit.target;
  auto* stats = world.TargetStats(hit.target);
  if (stats == nullptr) return false;
  if (hit.impact == static_cast<std::uint8_t>(ImpactKind::kAlternateBlock)) {
    world.Number(target, hit.limb, 0, 2);
  }
  auto state = stats->Read();
  for (std::size_t index = 0; index < kValueLabels.size(); ++index) {
    hit = world.ReadHit();
    const auto field = static_cast<Field>(index);
    const auto change = Change(hit, field);
    if (!HasValueEffect(field, change.mode)) continue;
    auto amount = static_cast<std::uint32_t>(change.amount);
    if (change.mode != 2 || field == Field::kRisk) amount |= kReductionLabel;
    stats->Write(state);
    world.Number(target, hit.limb, amount, kValueLabels.at(index));
    state = stats->Read();
    ApplyValue(state, world.ReadHit(), field, change.mode);
  }
  if (!ApplyEquipment(state, world.ReadHit())) return false;
  stats->Write(state);
  ApplyStatuses(*stats, target, world, true);
  ApplyStatuses(*stats, target, world, false);
  return true;
}
}  // namespace VS::Battle
