#include "Battle/CombatAction.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Battle/ActionCosts.hpp"
#include "Battle/ActionEffectDispatch.hpp"
#include "Battle/DamagePolicies.hpp"
#include "Battle/DamageRecoveryEffects.hpp"
#include "Battle/HitCalculations.hpp"
#include "Battle/HitModifiers.hpp"
#include "Items/EquipmentStats.hpp"
#include "Items/EquipmentWear.hpp"

namespace VS::Battle {
namespace {
constexpr std::uint8_t kExemptActor = 128;
constexpr std::uint32_t kDamageRemovedStatuses = 0x8000;
constexpr unsigned kEffectMask = 3;
constexpr unsigned kStatusFeedbackShift = 16;
constexpr int kExemptDamage = 999;

class RuleRefusal final : public std::runtime_error {
 public:
  explicit RuleRefusal(std::string_view reason)
      : std::runtime_error(std::string(reason)) {}
};

void Require(bool valid, std::string_view reason) {
  if (!valid) throw RuleRefusal(reason);
}

template <typename T>
T RequireValue(std::optional<T> value, std::string_view reason) {
  if (!value) throw RuleRefusal(reason);
  return *value;
}

void RequireItems(Items::ItemsStatus status) {
  if (status != Items::ItemsStatus::kOk) {
    throw RuleRefusal(Items::Describe(status));
  }
}

bool Named(std::uint8_t code, std::span<const Content::EnumValue> values,
           std::string_view name) {
  return std::ranges::any_of(values, [=](const auto& value) {
    return value.code == code && value.name == name;
  });
}

Items::DefenderEquipment Defender(const CombatActorState& actor) {
  Items::DefenderEquipment result{
      .shield = actor.damage.shield,
      .weapon_drawn = (actor.weapon_drawn & 1U) != 0,
      .shows_notices = actor.damage.actor_id == 0};
  for (std::size_t i = 0; i < actor.limbs.size(); ++i) {
    const auto& limb = actor.limbs.at(i);
    result.limbs.at(i) = {limb.hp.maximum, limb.name, limb.damage.armor};
  }
  return result;
}

void CommitDefender(CombatActorState& actor,
                    const Items::DefenderEquipment& equipment) {
  actor.damage.shield = equipment.shield;
  for (std::size_t i = 0; i < actor.limbs.size(); ++i) {
    actor.limbs.at(i).damage.armor = equipment.limbs.at(i).armor;
  }
}

struct Rules {
  const CombatRuleInputs& inputs;
  const Content::ActionHit& hit;
  CombatActionResult result;
  std::uint32_t prerequisite{}, previous{}, last_value{};
  bool critical{};
  std::array<std::int16_t, kArmorLossSlots> armor_loss{};

  Rules(const CombatActionIntent& intent, const CombatRuleInputs& input,
        const Content::ActionHit& first_hit)
      : inputs(input), hit(first_hit) {
    result.intent = intent;
    result.actors.assign(input.actors.begin(), input.actors.end());
    result.progress = input.progress;
  }
  CombatActorState& Actor(int slot) {
    const auto found =
        std::ranges::find(result.actors, slot, &CombatActorState::slot);
    Require(found != result.actors.end(), "combat actor slot is not an input");
    return *found;
  }
  CombatActorState& Actor(Actors::SpawnedActor identity) {
    const auto found =
        std::ranges::find(result.actors, identity, &CombatActorState::identity);
    Require(found != result.actors.end(),
            "combat actor identity is not an input");
    return *found;
  }
  Items::ItemAction ItemAction() const {
    return {inputs.definition.kind,
            inputs.definition.category,
            hit.affinity,
            {hit.stat_factor, 0}};
  }
};

class Growth final : public Items::EquipmentGrowthHost {
 public:
  explicit Growth(Rules& rules) : rules_(rules) {}
  std::int32_t NextBounded(std::int32_t bound) override {
    return rules_.inputs.random.NextBounded(bound);
  }
  std::int32_t NextAveraged(std::int32_t bound) override {
    return rules_.inputs.random.NextAveraged(bound);
  }
  void QueueStatChange(std::int32_t label, std::int32_t stat,
                       Items::StatChange change) override {
    rules_.result.equipment_notices.push_back(
        {rules_.Actor(0).identity, label, stat, change});
  }

 private:
  Rules& rules_;
};

class DamageAccess final : public DamageOwner {
 public:
  DamageAccess(Rules& rules, CombatActorState& actor)
      : rules_(rules), actor_(actor) {}
  DamageStats Read() override { return actor_.damage; }
  std::optional<DamageLimb> Limb(int limb) override {
    if (limb < 0 || static_cast<std::size_t>(limb) >= actor_.limbs.size()) {
      return std::nullopt;
    }
    return actor_.limbs.at(limb).damage;
  }
  std::uint32_t Statuses() override { return actor_.statuses; }
  std::uint16_t Risk() override { return actor_.damage.risk; }
  std::uint8_t EnemyClass() override { return actor_.damage.enemy_class; }
  void GrowAttacker(int target_class, int target_affinity) override {
    Growth growth(rules_);
    RequireItems(Items::GrowAttackerWeaponRatings(
        rules_.ItemAction(), actor_.damage.weapon, actor_.damage.actor_id == 0,
        {target_class, target_affinity}, growth));
  }
  void GrowDefender(int source_class, int source_affinity, int limb) override {
    auto defender = Defender(actor_);
    Growth growth(rules_);
    RequireItems(Items::GrowDefenderEquipmentRatings(
        rules_.ItemAction(), defender, {source_class, source_affinity}, limb,
        growth));
    CommitDefender(actor_, defender);
  }

 private:
  Rules& rules_;
  CombatActorState& actor_;
};

class DamageAdapter final : public DamageWorld {
 public:
  DamageAdapter(Rules& rules, const ActionHit& target)
      : rules_(rules), target_(target) {
    owners_.reserve(rules.result.actors.size());
    for (auto& actor : rules.result.actors) owners_.emplace_back(rules, actor);
  }
  DamageOwner* Stats(std::uint8_t slot) override {
    auto& actor = rules_.Actor(slot);
    const auto index = &actor - rules_.result.actors.data();
    return &owners_.at(index);
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    return rules_.Actor(slot).weapon_drawn;
  }
  std::optional<DamageHit> Hit(int hit) override {
    if (hit != 0) return std::nullopt;
    return DamageHit{rules_.hit.damage_type, rules_.hit.affinity,
                     rules_.hit.stat_factor};
  }
  std::optional<std::uint8_t> ActionType() override {
    return rules_.inputs.definition.kind;
  }
  std::optional<std::uint8_t> ActionId() override {
    return static_cast<std::uint8_t>(rules_.result.intent.action);
  }
  std::optional<std::uint16_t> BladeBonus(std::uint8_t category) override {
    if (category >= rules_.inputs.blade_pp_bonuses.size()) return std::nullopt;
    return rules_.inputs.blade_pp_bonuses[category];
  }
  std::int32_t TargetPosition() override { return target_.position_kind; }
  std::int32_t RandomBounded(int bound) override {
    return rules_.inputs.random.NextBounded(bound);
  }
  std::int32_t RandomAveraged(int bound) override {
    return rules_.inputs.random.NextAveraged(bound);
  }
  void SetCriticalHit() override { rules_.critical = true; }

 private:
  Rules& rules_;
  const ActionHit& target_;
  std::vector<DamageAccess> owners_;
};

HitModifierStats ModifierStats(const CombatActorState& actor) {
  HitModifierStats stats{.actor_id = actor.damage.actor_id,
                         .risk = actor.damage.risk};
  for (std::size_t i = 0; i < stats.weapon_gems.size(); ++i) {
    stats.weapon_gems.at(i) = actor.damage.weapon.gems.at(i).gem_effects;
    stats.shield_gems.at(i) = actor.damage.shield.gems.at(i).gem_effects;
  }
  return stats;
}

class Modifiers final : public HitModifierWorld {
 public:
  explicit Modifiers(Rules& rules) : rules_(rules) {}
  std::optional<HitModifierStats> Stats(std::uint8_t slot) override {
    return ModifierStats(rules_.Actor(slot));
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    return rules_.Actor(slot).weapon_drawn;
  }
  std::optional<HitModifierAction> Action() override {
    return HitModifierAction{
        static_cast<std::uint8_t>(rules_.result.intent.action),
        rules_.inputs.definition.kind,
        {rules_.hit.effect, 0}};
  }

 private:
  Rules& rules_;
};

class HitAccess final : public HitAttributeAccess {
 public:
  HitAccess(Rules& rules, CombatActorState& actor)
      : rules_(rules), actor_(actor) {}
  HitAttributeStats Read() override {
    const auto& damage = actor_.damage;
    HitAttributeStats stats{
        .actor_id = damage.actor_id,
        .risk = damage.risk,
        .hp = actor_.hp,
        .max_hp = actor_.max_hp,
        .agility = {actor_.agility, damage.accessory.current_agility,
                    damage.weapon.attributes.current_agility,
                    damage.shield.attributes.current_agility},
        .intelligence = {damage.intelligence,
                         damage.accessory.current_intelligence,
                         damage.weapon.attributes.current_intelligence,
                         damage.shield.attributes.current_intelligence}};
    for (std::size_t i = 0; i < actor_.limbs.size(); ++i) {
      const auto& limb = actor_.limbs.at(i);
      stats.agility.armor.at(i) = limb.damage.armor.attributes.current_agility;
      stats.intelligence.armor.at(i) =
          limb.damage.armor.attributes.current_intelligence;
      stats.defense.at(i) = limb.agility_defense;
    }
    return stats;
  }
  std::optional<std::int16_t> AttackBonus() override {
    Modifiers world(rules_);
    const auto stats = ModifierStats(actor_);
    return AttackGemHitBonus(&stats, world);
  }
  std::optional<std::int16_t> DefenseBonus(int hit) override {
    Modifiers world(rules_);
    const auto stats = ModifierStats(actor_);
    return DefenseGemHitBonus(&stats, hit, world);
  }

 private:
  Rules& rules_;
  CombatActorState& actor_;
};

class HitAdapter final : public HitAttributeWorld {
 public:
  explicit HitAdapter(Rules& rules) : rules_(rules) {
    owners_.reserve(rules.result.actors.size());
    for (auto& actor : rules.result.actors) owners_.emplace_back(rules, actor);
  }
  HitAttributeAccess* Stats(std::uint8_t slot) override {
    auto& actor = rules_.Actor(slot);
    return &owners_.at(&actor - rules_.result.actors.data());
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    return rules_.Actor(slot).weapon_drawn;
  }
  std::optional<std::uint8_t> Cost() override {
    return rules_.inputs.definition.cost;
  }
  std::int32_t RandomAveraged(int bound) override {
    return rules_.inputs.random.NextAveraged(bound);
  }

 private:
  Rules& rules_;
  std::vector<HitAccess> owners_;
};

class Calculation final : public ActionCalculationWorld {
 public:
  Calculation(Rules& rules, ActionHit& source, ActionHit& target)
      : rules_(rules), source_(source), target_(target) {}
  std::optional<CalculationTarget> Target() override {
    return CalculationTarget{target_.actor, target_.position_kind,
                             target_.threshold};
  }
  std::optional<CalculationTarget> Source() override {
    return CalculationTarget{source_.actor, source_.position_kind,
                             source_.threshold};
  }
  std::optional<CalculationActor> Actor(std::uint8_t slot) override {
    const auto& actor = rules_.Actor(slot);
    return CalculationActor{actor.damage.actor_id,
                            std::bit_cast<std::int32_t>(actor.statuses)};
  }
  std::optional<std::uint8_t> ActionType() override {
    return rules_.inputs.definition.kind;
  }
  std::optional<int> HitCalculator(int hit, int execute) override {
    HitAdapter world(rules_);
    const HitModifierTarget source{source_.actor,
                                   std::bit_cast<std::int8_t>(source_.limb),
                                   source_.position_kind};
    const HitModifierTarget target{target_.actor,
                                   std::bit_cast<std::int8_t>(target_.limb),
                                   target_.position_kind};
    return AgilityHitChance(&source, &target, hit, execute, world);
  }
  std::optional<std::int16_t> StatCalculator(int hit, int execute) override {
    DamageAdapter world(rules_, target_);
    const DamageTarget source{source_.actor,
                              std::bit_cast<std::int8_t>(source_.limb)};
    const DamageTarget target{target_.actor,
                              std::bit_cast<std::int8_t>(target_.limb)};
    const bool statuses =
        Named(rules_.hit.stat_calculator, Content::HitStatCalculators(),
              "physical-damage-with-status-modifiers");
    return CalculatePhysicalHitDamage(&source, &target, hit, execute, statuses,
                                      world);
  }
  std::uint32_t Prerequisite() override { return rules_.prerequisite; }
  void StoreThreshold(std::uint8_t threshold) override {
    target_.threshold = threshold;
  }
  void StoreLastValue(std::uint32_t value) override {
    rules_.last_value = value;
  }
  std::int32_t RandomBounded(int bound) override {
    return rules_.inputs.random.NextBounded(bound);
  }

 private:
  Rules& rules_;
  ActionHit& source_;
  ActionHit& target_;
};

class DamageQueue final : public DamageRecoveryQueue {
 public:
  DamageQueue(Rules& rules, ActionHit& source, ActionHit& target, int hit,
              int execute)
      : rules_(rules),
        source_(source),
        target_(target),
        hit_(hit),
        execute_(execute) {}
  void CaptureDamageOwners() override {
    source_stats_ = &rules_.Actor(source_.actor);
    CaptureTargetOwner();
  }
  void CaptureTargetOwner() override {
    target_stats_ = &rules_.Actor(target_.actor);
  }
  void ClearCriticalHit() override { rules_.critical = false; }
  bool CriticalHit() override { return rules_.critical; }
  std::int16_t Calculate() override {
    Calculation world(rules_, source_, target_);
    return RequireValue(CalculateActionStatChange(hit_, execute_, world),
                        "physical damage calculation refused its inputs");
  }
  ActionResultChange ReadChange(ActionResultField field) override {
    return ReadActionHitResult(target_).changes.at(
        static_cast<std::size_t>(field));
  }
  void WriteChange(ActionResultField field,
                   ActionResultChange change) override {
    auto result = ReadActionHitResult(target_);
    result.changes.at(static_cast<std::size_t>(field)) = change;
    WriteActionHitResult(result, target_);
  }
  bool TargetsPosition() override { return target_.position_kind != 0; }
  void DistributeDamage(std::int16_t amount) override {
    auto& actor = rules_.Actor(target_.actor);
    auto result = ReadActionHitResult(target_);
    std::array<StatFactorVital, Items::kLimbCount> vitals{};
    std::array<std::int16_t, Items::kLimbCount> limbs{};
    for (std::size_t i = 0; i < limbs.size(); ++i) {
      vitals.at(i) = actor.limbs.at(i).hp;
      limbs.at(i) = result.limbs.at(i).hp;
    }
    const auto category = rules_.inputs.definition.category;
    std::span<const std::uint8_t> splash;
    if (DamageDistribution(category) == LimbDistribution::kSplash) {
      Require(target_.limb < actor.armor_splash.size(),
              "physical splash damage requires a valid limb");
      splash = actor.armor_splash.at(target_.limb);
    }
    Require(DistributeHitLimbDamage(
                category, std::bit_cast<std::int8_t>(target_.limb), amount,
                vitals, splash, limbs, rules_.armor_loss),
            "physical limb damage refused its inputs");
    for (std::size_t i = 0; i < limbs.size(); ++i) {
      result.limbs.at(i).hp = limbs.at(i);
    }
    WriteActionHitResult(result, target_);
  }
  void RecoverLimbs(std::int16_t) override {
    throw RuleRefusal("physical damage cannot dispatch limb recovery");
  }
  std::uint32_t TargetRemovalStatuses() override {
    const auto& actor = TargetOwner();
    return actor.damage.actor_id == kExemptActor
               ? 0
               : actor.statuses & kDamageRemovedStatuses;
  }
  void QueueRemovalStatuses(std::uint32_t statuses) override {
    target_.inflict_statuses |= statuses;
  }
  void ConsumeEquipmentDp(std::int16_t amount) override {
    auto& target = TargetOwner();
    auto defender = Defender(target);
    RequireItems(Items::ConsumeHitEquipmentDp(
        rules_.ItemAction(), SourceOwner().damage.weapon, defender,
        std::span(rules_.armor_loss).first<Items::kLimbCount>(), amount));
    CommitDefender(target, defender);
  }
  void AccumulateEquipmentPp(std::int16_t amount) override {
    auto& target = TargetOwner();
    auto defender = Defender(target);
    RequireItems(Items::AccumulateHitEquipmentPp(
        rules_.ItemAction(), SourceOwner().damage.weapon, defender, amount));
    CommitDefender(target, defender);
  }
  std::int16_t TargetMp() override { return TargetOwner().mp; }
  void StoreLastValue(std::int16_t amount) override {
    rules_.last_value = static_cast<std::uint32_t>(amount);
  }
  bool HasWeapon() override {
    return TargetOwner().damage.weapon.blade.id != 0;
  }
  bool HasShield() override { return TargetOwner().damage.shield.base.id != 0; }
  std::uint16_t WeaponPp() override {
    return TargetOwner().damage.weapon.current_pp;
  }
  std::uint16_t ShieldPp() override {
    return TargetOwner().damage.shield.current_pp;
  }
  std::uint8_t SelectedPrimaryHpMode() override {
    throw RuleRefusal("physical damage cannot dispatch selected-action drain");
  }

 private:
  CombatActorState& SourceOwner() {
    Require(source_stats_ != nullptr, "physical source owner was not captured");
    return *source_stats_;
  }
  CombatActorState& TargetOwner() {
    Require(target_stats_ != nullptr, "physical target owner was not captured");
    return *target_stats_;
  }
  Rules& rules_;
  ActionHit& source_;
  ActionHit& target_;
  int hit_, execute_;
  CombatActorState* source_stats_{};
  CombatActorState* target_stats_{};
};

class Effects final : public ActionEffectWorld {
 public:
  Effects(Rules& rules, ActionHit& source, ActionHit& target)
      : rules_(rules), source_(source), target_(target) {}
  void Refuse(std::string_view reason) override {
    rules_.result.warnings.emplace_back(reason);
  }
  bool TargetPosition() override { return target_.position_kind != 0; }
  std::uint8_t TargetActorId() override {
    return TargetState().damage.actor_id;
  }
  std::uint32_t TargetStatuses() override { return TargetState().statuses; }
  std::uint8_t ActionType() override { return rules_.inputs.definition.kind; }
  void QueueExemptDamage() override {
    target_.effects = (target_.effects & ~kEffectMask) | 1U;
    target_.hp = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(target_.hp + kExemptDamage));
  }
  void ClearLastValue() override { rules_.last_value = 0; }
  void Dispatch(int hit, int execute) override {
    DamageQueue queue(rules_, source_, target_, hit, execute);
    QueueHitDamageAndEquipmentWear(queue, execute != 0);
  }
  void QueueStatusRemoval(std::uint32_t mask) override {
    target_.inflict_statuses |= mask;
  }
  bool SourcePosition() override { return source_.position_kind != 0; }
  std::uint8_t SourceSlot() override { return source_.actor; }
  std::int16_t TargetHp() override { return TargetState().hp; }
  std::int16_t PendingHp() override { return target_.hp; }
  std::uint8_t TargetSlot() override { return target_.actor; }
  bool KillCounted() override { return TargetState().kill_counted; }
  void MarkKillCounted() override { TargetState().kill_counted = true; }
  void ClearKillCounted() override { TargetState().kill_counted = false; }
  std::uint8_t WeaponCategory() override {
    return rules_.result.progress.weapon_category;
  }
  std::uint32_t ProgressFlag() override {
    return rules_.result.progress.story_progress;
  }
  std::uint8_t LearnedArts(std::uint8_t) override {
    return rules_.result.progress.learned_arts;
  }
  std::uint16_t WeaponKills(std::uint8_t) override {
    return rules_.result.progress.weapon_kills;
  }
  std::optional<int> RequiredKills(std::uint8_t, std::uint8_t) override {
    return rules_.result.progress.required_kills;
  }
  void SetWeaponKills(std::uint8_t, std::uint16_t kills) override {
    rules_.result.progress.weapon_kills = kills;
  }

 private:
  CombatActorState& TargetState() { return rules_.Actor(target_.actor); }
  Rules& rules_;
  ActionHit& source_;
  ActionHit& target_;
};

class InjuryQueue final : public LimbInjuryQueue {
 public:
  InjuryQueue(Rules& rules, ActionHit& hit)
      : rules_(rules), hit_(hit), actor_(rules.Actor(hit.actor)) {}
  std::uint8_t MobilityLimb(std::size_t index) override {
    return actor_.mobility.at(index);
  }
  std::int16_t CurrentHp(std::uint8_t limb) override {
    Require(limb < actor_.limbs.size(), "injury requires a valid limb");
    return actor_.limbs.at(limb).hp.current;
  }
  ActionResultChange PendingHp(std::uint8_t limb) override {
    const auto result = ReadActionHitResult(hit_);
    Require(limb < result.limbs.size(), "injury requires a valid hit limb");
    return {result.limbs.at(limb).hp, result.changes.front().mode};
  }
  std::uint32_t ActiveStatuses() override { return actor_.statuses; }
  bool PlayerTarget() override { return hit_.actor == 0; }
  std::uint8_t InjuryRule(std::size_t limb) override {
    Require(limb < rules_.inputs.injury_rules.size(),
            "combat injury rule is not an input");
    return rules_.inputs.injury_rules[limb];
  }
  std::uint32_t RuleMask(std::uint8_t rule) override {
    return Rule(rule).statuses;
  }
  void QueueStatus(std::uint8_t rule, bool remove) override {
    const auto& definition = Rule(rule);
    const StatusTarget target{actor_.statuses, actor_.immunities,
                              actor_.damage.actor_id == kExemptActor};
    QueuedStatuses queued{
        hit_.remove_statuses, hit_.inflict_statuses,
        static_cast<StatusFeedback>((hit_.effects >> kStatusFeedbackShift) &
                                    kEffectMask)};
    if (remove) {
      QueueStatusRemovalMask(definition, target, queued);
    } else {
      QueueApplicableStatusChanges(definition, target, queued);
    }
    hit_.remove_statuses = queued.added;
    hit_.inflict_statuses = queued.removed;
    hit_.effects =
        (hit_.effects & ~(kEffectMask << kStatusFeedbackShift)) |
        (static_cast<std::uint32_t>(queued.feedback) << kStatusFeedbackShift);
  }

 private:
  const StatusRule& Rule(std::uint8_t index) {
    Require(index < rules_.inputs.status_rules.size(),
            "combat status rule is not an input");
    return rules_.inputs.status_rules[index];
  }
  Rules& rules_;
  ActionHit& hit_;
  CombatActorState& actor_;
};

class CostActor final : public ActionCostActor {
 public:
  explicit CostActor(CombatActorState& actor) : actor_(actor) {}
  ActionResources Resources() override {
    const auto& weapon = actor_.damage.weapon;
    return {actor_.hp,
            actor_.mp,
            actor_.damage.risk,
            weapon.current_pp,
            static_cast<std::uint8_t>(weapon.action_type),
            static_cast<std::uint8_t>(weapon.risk)};
  }
  void Pay(CostResource resource, std::uint16_t value) override {
    switch (resource) {
      case CostResource::kMp:
        actor_.mp = std::bit_cast<std::int16_t>(value);
        break;
      case CostResource::kRisk:
        actor_.damage.risk = value;
        break;
      case CostResource::kHp:
        actor_.hp = std::bit_cast<std::int16_t>(value);
        break;
      case CostResource::kWeaponPp:
        actor_.damage.weapon.current_pp = value;
        break;
      case CostResource::kNone:
        break;
    }
  }
  std::uint16_t RecoveryTimer() override { return actor_.recovery_timer; }
  void SetRecoveryTimer(std::uint16_t value) override {
    actor_.recovery_timer = value;
  }
  std::uint8_t RecoveryBonus() override { return actor_.recovery_bonus; }
  std::uint8_t WeaponRecoveryDelay() override {
    return static_cast<std::uint8_t>(actor_.damage.weapon.recovery_delay);
  }
  std::uint32_t Statuses() override { return actor_.statuses; }

 private:
  CombatActorState& actor_;
};

class RuleDefinition final : public ResolutionDefinition,
                             public ActionCostDefinition {
 public:
  explicit RuleDefinition(Rules& rules) : rules_(rules) {}
  ResolutionDescriptor Hit(int hit) override {
    if (hit != 0) return {};
    const auto& value = rules_.hit;
    return {value.prerequisite, value.stat_calculator, value.damage_type,
            value.affinity};
  }
  bool Redirects() override { return false; }
  std::uint8_t Category() override { return rules_.inputs.definition.category; }
  PreviewVolume Range() override {
    throw RuleRefusal("combat targets must be collected before resolution");
  }
  PreviewVolume Area() override {
    throw RuleRefusal("combat targets must be collected before resolution");
  }
  std::uint8_t Type() override { return rules_.inputs.definition.kind; }
  std::uint8_t Cost() override { return rules_.inputs.definition.cost; }
  std::uint8_t RecoveryDelay() override {
    return rules_.inputs.definition.recovery_delay;
  }
  bool Unlocked() override {
    return (rules_.inputs.definition.behavior &
            Content::kBehaviorStartsUnlocked) != 0;
  }

 private:
  Rules& rules_;
};

class Costs final : public ActionCostWorld {
 public:
  explicit Costs(Rules& rules)
      : rules_(rules),
        definition_(rules),
        actor_(rules.Actor(rules.result.intent.source)) {}
  ActionCostDefinition* Definition(std::uint32_t action) override {
    Require(action == rules_.result.intent.action, "combat action changed");
    return &definition_;
  }
  std::optional<ActionCostActor*> Actor(int slot) override {
    Require(slot == rules_.Actor(rules_.result.intent.source).slot,
            "cost owner is not the combat source");
    return &actor_;
  }
  std::optional<std::uint32_t> ChainStep() override {
    return rules_.inputs.chain_step;
  }
  std::optional<std::uint32_t> WeaponDrawn(int slot) override {
    return rules_.Actor(slot).weapon_drawn;
  }
  bool Recover(std::uint32_t action) {
    return AddActionRecoveryDelay(action, &actor_, 1, *this);
  }
  std::optional<std::uint32_t> Pay(std::uint32_t action, int update) {
    const auto cost = ActionCostAndAvailability(action, &actor_, update, *this);
    return cost ? std::optional{cost->Packed()} : std::nullopt;
  }

 private:
  Rules& rules_;
  RuleDefinition definition_;
  CostActor actor_;
};

class ResolutionAccess final : public ResolutionActor {
 public:
  explicit ResolutionAccess(CombatActorState& actor) : actor_(actor) {}
  std::uint8_t Slot() override { return actor_.damage.actor_id; }
  std::uint8_t DefaultLimb() override { return actor_.default_limb; }
  bool Cancels() override { return actor_.cancels_results; }
  bool HasBlade() override { return actor_.damage.weapon.blade.id != 0; }
  std::uint8_t DamageType() override {
    return actor_.damage.weapon.damage_type;
  }
  std::array<std::int16_t, kActionAffinityCount> Affinities() override {
    std::array<std::int16_t, kActionAffinityCount> result{};
    const auto& affinities =
        actor_.damage.weapon.class_affinity.affinities.front();
    std::copy_n(affinities.begin(), result.size(), result.begin());
    return result;
  }

 private:
  CombatActorState& actor_;
};

class Resolution final : public ActionPreviewWorld {
 public:
  explicit Resolution(Rules& rules)
      : rules_(rules), definition_(rules), costs_(rules) {
    owners_.reserve(rules.result.actors.size());
    for (auto& actor : rules.result.actors) owners_.emplace_back(actor);
  }
  ResolutionDefinition* Definition(std::uint32_t action) override {
    Require(action == rules_.result.intent.action, "combat action changed");
    return &definition_;
  }
  std::optional<ResolutionActor*> Actor(int slot) override {
    auto& actor = rules_.Actor(slot);
    return &owners_.at(&actor - rules_.result.actors.data());
  }
  std::optional<std::uint32_t> WeaponDrawn(int slot) override {
    return rules_.Actor(slot).weapon_drawn;
  }
  std::optional<std::uint8_t> SpawnId(int slot) override {
    return rules_.Actor(slot).spawn;
  }
  std::optional<ActionPoint> SourceOrigin(int) override { return std::nullopt; }
  std::optional<ActionPoint> TargetPosition(int, int) override {
    throw RuleRefusal("combat trajectories must be resolved before rules");
  }
  std::optional<ActionPoint> LimbPosition(int, int) override {
    throw RuleRefusal("combat targets must be collected before rules");
  }
  std::optional<ActionPoint> PreviewPosition(bool) override {
    throw RuleRefusal("combat rules do not own preview geometry");
  }
  bool Contains(const PreviewVolume&, ActionPoint) override {
    throw RuleRefusal("combat targets must be collected before rules");
  }
  std::optional<ActionPoint> Trace(ActionPoint, ActionPoint, int) override {
    throw RuleRefusal("combat trajectories must be resolved before rules");
  }
  void ResetHitState() override {
    rules_.previous = 0;
    rules_.last_value = 0;
  }
  void Prerequisite(std::uint32_t value) override {
    rules_.prerequisite = value;
  }
  void PreviousHit(std::uint32_t value) override { rules_.previous = value; }
  std::optional<std::uint32_t> QueueIndex() override { return 0; }
  std::uint32_t Check(ResolutionDefinition&, ActionHit&, int hit) override {
    Require(hit == 0, "physical combat has only one hit");
    return 1;
  }
  std::uint32_t Roll(ResolutionDefinition&, ActionHit& source,
                     ActionHit& target, int hit, int execute) override {
    Calculation world(rules_, source, target);
    return static_cast<std::uint32_t>(
        RequireValue(RollActionHit(hit, execute, world),
                     "physical hit calculation refused its inputs"));
  }
  void Effect(ResolutionDefinition&, ActionHit& source, ActionHit& target,
              int execute, int hit) override {
    Effects world(rules_, source, target);
    ApplyActionHitEffect(world, execute, hit);
  }
  void Injuries(ActionHit& target) override {
    InjuryQueue queue(rules_, target);
    DeriveLimbInjuryStatuses(queue);
  }
  bool Recovery(std::uint32_t action, ResolutionActor&) override {
    return costs_.Recover(action);
  }
  std::optional<std::uint32_t> Cost(std::uint32_t action, ResolutionActor*,
                                    int update) override {
    return costs_.Pay(action, update);
  }

 private:
  Rules& rules_;
  RuleDefinition definition_;
  Costs costs_;
  std::vector<ResolutionAccess> owners_;
};

void ValidateActors(const CombatRuleInputs& inputs) {
  for (std::size_t i = 0; i < inputs.actors.size(); ++i) {
    const auto& actor = inputs.actors[i];
    Require(actor.identity != Actors::SpawnedActor::kNone,
            "combat actor identity is empty");
    for (std::size_t j = 0; j < i; ++j) {
      Require(actor.identity != inputs.actors[j].identity &&
                  actor.slot != inputs.actors[j].slot,
              "combat actor inputs contain duplicate identities or slots");
    }
  }
}

ActionHit RequestTarget(const CombatTarget& target, Rules& rules) {
  ActionHit hit{};
  if (const auto* actor = std::get_if<CombatActorTarget>(&target)) {
    Require(actor->limb >= 0 &&
                static_cast<std::size_t>(actor->limb) < Items::kLimbCount,
            "combat target limb is outside the actor");
    hit.actor = rules.Actor(actor->actor).slot;
    hit.limb = static_cast<std::uint8_t>(actor->limb);
  } else {
    const auto& position = std::get<CombatWorldTarget>(target);
    Require(position.kind != 0, "combat world target has actor kind");
    hit.position_kind = position.kind;
    hit.SetPosition(position.position);
  }
  return hit;
}

CombatHitResult ResultTarget(const ActionHit& hit, Rules& rules) {
  CombatHitResult result{
      .changes = ReadActionHitResult(hit),
      .threshold = hit.threshold,
      .feedback = static_cast<StatusFeedback>(
          (hit.effects >> kStatusFeedbackShift) & kEffectMask)};
  if (hit.position_kind == 0) {
    result.target = CombatActorTarget{rules.Actor(hit.actor).identity,
                                      std::bit_cast<std::int8_t>(hit.limb)};
  } else {
    result.target = CombatWorldTarget{
        static_cast<std::uint8_t>(hit.position_kind), hit.Position()};
  }
  return result;
}

void RecordPhysicalAttack(Rules& rules) {
  auto& result = rules.result;
  if (result.intent.action != 1 ||
      rules.Actor(result.intent.source).slot != 0) {
    return;
  }
  const auto* target =
      result.hits.empty() ? &rules.Actor(0)
      : std::holds_alternative<CombatActorTarget>(result.hits.front().target)
          ? &rules.Actor(
                std::get<CombatActorTarget>(result.hits.front().target).actor)
          : nullptr;
  if (target == nullptr || target->spawn == kExemptActor) return;
  Require(result.progress.weapon_category <= Items::kUnarmedWeaponCategory,
          "physical attack score has an invalid weapon category");
  if (result.progress.weapon_attacks !=
      std::numeric_limits<std::uint16_t>::max()) {
    ++result.progress.weapon_attacks;
  }
}

CombatActionResult Resolve(const CombatActionIntent& intent,
                           const CombatRuleInputs& inputs,
                           const Content::ActionHit& first_hit, bool execute) {
  ValidateActors(inputs);
  Require(intent.action < Content::kActionCount, "combat action is invalid");
  Require(intent.targets.size() <= kActionTargetCapacity,
          "combat target count exceeds action capacity");
  Rules rules(intent, inputs, first_hit);
  ActionRequest request{};
  request.action = intent.action;
  request.primary.actor = rules.Actor(intent.source).slot;
  request.primary.limb = static_cast<std::uint8_t>(intent.source_limb);
  request.count = static_cast<std::uint16_t>(intent.targets.size());
  request.focus = intent.focus;
  for (std::size_t i = 0; i < intent.targets.size(); ++i) {
    request.targets.at(i) = RequestTarget(intent.targets.at(i), rules);
  }
  ActionRequest result{};
  Resolution world(rules);
  Require(ResolveActionTargets(intent.action, result, request,
                               static_cast<int>(execute), world),
          "physical action refused its rule inputs");
  if (!execute) {
    Costs costs(rules);
    rules.result.availability =
        RequireValue(costs.Pay(intent.action, 0),
                     "physical preview refused its action cost");
  }
  rules.result.affinity = result.affinity;
  rules.result.damage_type = result.damage_type;
  rules.result.category = inputs.definition.category;
  rules.result.hit_prerequisites = {first_hit.prerequisite, 0};
  rules.result.source = ResultTarget(result.primary, rules);
  rules.result.hits.reserve(result.count);
  for (std::size_t i = 0; i < result.count; ++i) {
    rules.result.hits.push_back(ResultTarget(result.targets.at(i), rules));
  }
  if (execute) RecordPhysicalAttack(rules);
  return std::move(rules.result);
}
}  // namespace

bool IsPhysicalCombatRule(const Content::ActionRule& rule) {
  const auto& hit = rule.hits.front();
  if (!hit || rule.hits.back()) return false;
  return Named(hit->prerequisite, Content::HitPrerequisites(), "always-1") &&
         Named(hit->effect, Content::HitEffects(), "damage") &&
         Named(hit->hit_function, Content::HitFunctions(),
               "agility-difference") &&
         (Named(hit->stat_calculator, Content::HitStatCalculators(),
                "physical-damage-with-status-modifiers") ||
          Named(hit->stat_calculator, Content::HitStatCalculators(),
                "physical-damage-without-status-modifiers"));
}

std::expected<CombatActionResult, std::string> ResolvePhysicalAction(
    const CombatActionIntent& intent, const CombatRuleInputs& inputs,
    bool execute) {
  const auto& hit = inputs.definition.hits.front();
  if (!hit || !IsPhysicalCombatRule(inputs.definition)) {
    return std::unexpected("definition is not a single-hit physical action");
  }
  try {
    return Resolve(intent, inputs, *hit, execute);
  } catch (const RuleRefusal& refusal) {
    return std::unexpected(refusal.what());
  }
}
}  // namespace VS::Battle
