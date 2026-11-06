#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Abilities.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/AttackPrerequisites.hpp"
#include "Battle/AttackPrerequisitesLive.hpp"
#include "Battle/BattleMode.hpp"
#include "Battle/BattleModeLive.hpp"
#include "Battle/Targeting.hpp"
#include "Battle/TargetingLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/AttackPrerequisiteTable.h"
#include "Battle/AttackPrerequisites.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/Sphere.h"
#include "Battle/StatusEffects.h"
#include "Battle/TargetVolume.h"
#include "Battle/TargetingState.h"
}

namespace {
namespace battle = VS::Battle;

// ---------------------------------------------------------------------------
// Shared helpers.

// "Name(1,2)": one recorded host call.
std::string Call(std::string_view name, std::initializer_list<int> args = {}) {
  std::string text(name);
  text += '(';
  bool first = true;
  for (const int arg : args) {
    if (!first) text += ',';
    text += std::to_string(arg);
    first = false;
  }
  text += ')';
  return text;
}

template <typename Map, typename Key, typename Value>
Value ValueOr(const Map& map, const Key& key, Value fallback) {
  const auto found = map.find(key);
  return found != map.end() ? found->second : fallback;
}

// Collects the targeting and ability warnings and errors while it lives.
struct BattleLog {
  std::vector<std::string> messages;
  VS::Core::LogObserver saved =
      VS::Core::SetLogObserver([this](const VS::Core::LogRecord& record) {
        if ((record.channel == "battle.targeting" ||
             record.channel == "battle.ability") &&
            record.level >= VS::Core::LogLevel::Warn) {
          messages.emplace_back(record.message);
        }
      });

  BattleLog() = default;
  BattleLog(const BattleLog&) = delete;
  BattleLog& operator=(const BattleLog&) = delete;
  BattleLog(BattleLog&&) = delete;
  BattleLog& operator=(BattleLog&&) = delete;
  ~BattleLog() { VS::Core::SetLogObserver(std::move(saved)); }

  [[nodiscard]] bool Saw(std::string_view text) const {
    return std::any_of(messages.begin(), messages.end(),
                       [text](const std::string& message) {
                         return message.find(text) != std::string::npos;
                       });
  }
};

// Replaces a global for one test and restores it afterwards.
template <typename T>
class ScopedOverride {
 public:
  ScopedOverride(T& target, T value) : target_(target), saved_(target) {
    target_ = std::move(value);
  }
  ScopedOverride(const ScopedOverride&) = delete;
  ScopedOverride& operator=(const ScopedOverride&) = delete;
  ScopedOverride(ScopedOverride&&) = delete;
  ScopedOverride& operator=(ScopedOverride&&) = delete;
  ~ScopedOverride() { target_ = saved_; }

 private:
  T& target_;
  T saved_;
};

// A zeroed battle actor with its stats.
struct LiveActor {
  std::unique_ptr<Battle_ActorStats> stats =
      std::make_unique<Battle_ActorStats>();
  std::unique_ptr<Battle_Actor> actor = std::make_unique<Battle_Actor>();

  LiveActor() { actor->stats = stats.get(); }
};

// ---------------------------------------------------------------------------
// Attack prerequisites.

struct FakeTarget final : battle::PrerequisiteTarget {
  bool valid = true;
  std::map<int, battle::StatusEffectRule> rules;
  std::uint32_t immunities = 0;
  std::uint32_t statuses = 0;
  int enemy_class = 0;
  std::array<int, battle::kArmorLimbCount> armor{};
  int shield = 0;
  int blade = 0;
  mutable int valid_reads = 0;

  [[nodiscard]] bool Valid() const override {
    ++valid_reads;
    return valid;
  }
  [[nodiscard]] battle::StatusEffectRule EffectRule(int effect) const override {
    return ValueOr(rules, effect, battle::StatusEffectRule{});
  }
  [[nodiscard]] std::uint32_t StatusImmunities() const override {
    return immunities;
  }
  [[nodiscard]] std::uint32_t Statuses() const override { return statuses; }
  [[nodiscard]] int EnemyClass() const override { return enemy_class; }
  [[nodiscard]] int LimbArmorId(int limb) const override {
    return armor.at(static_cast<std::size_t>(limb));
  }
  [[nodiscard]] int ShieldId() const override { return shield; }
  [[nodiscard]] int BladeId() const override { return blade; }
};

struct StatusRuleCase {
  battle::PrerequisiteRule rule;
  int effect;
};

// The rules that only check their status effect.
constexpr std::array<StatusRuleCase, 16> kStatusOnlyRules{{
    {&battle::CanApplyHalfMovement, 4},
    {&battle::CanApplyStrengthDown, 5},
    {&battle::CanApplyStrengthUp, 6},
    {&battle::CanApplyIntelligenceDown, 7},
    {&battle::CanApplyIntelligenceUp, 8},
    {&battle::CanApplyAgilityDown, 9},
    {&battle::CanApplyAgilityUp, 10},
    {&battle::CanApplyQuicken, 11},
    {&battle::CanApplySilence, 12},
    {&battle::CanApplyParalysis, 13},
    {&battle::CanApplyPoison, 14},
    {&battle::CanApplyNumbness, 15},
    {&battle::CanApplyCurse, 16},
    {&battle::CanApplyRegeneration, 17},
    {&battle::CanApplyMagicWard, 18},
    {&battle::CanApplyBanish, 29},
}};

constexpr std::uint32_t kEffectFlag = 0x10;
constexpr std::uint32_t kBlockingStatus = 0x200;

// Gives the target the rule of `effect`: its immunity flag and blocking status.
void SetEffectRule(FakeTarget& target, int effect) {
  target.rules[effect] = {kEffectFlag, kBlockingStatus};
}

TEST_CASE("Attack prerequisite table keeps the original slot order",
          "[battle][battle-targeting]") {
  const std::array<battle::PrerequisiteRule, battle::kPrerequisiteCount>
      expected{
          &battle::RejectAttackPrerequisite,
          &battle::AcceptAttackPrerequisite,
          &battle::AcceptAttackPrerequisite,
          &battle::AcceptAttackPrerequisite,
          &battle::IsAttackTargetUndead,
          &battle::IsAttackTargetLiving,
          &battle::RejectAttackPrerequisite,
          &battle::RejectAttackPrerequisite,
          &battle::RejectAttackPrerequisite,
          &battle::CanApplyStrengthDown,
          &battle::CanApplyIntelligenceDown,
          &battle::CanApplyAgilityDown,
          &battle::CanApplyEquipmentDown,
          &battle::CanApplyHalfMovement,
          &battle::CanApplyStrengthUp,
          &battle::CanApplyIntelligenceUp,
          &battle::CanApplyAgilityUp,
          &battle::CanApplyEquipmentUp,
          &battle::CanApplyQuicken,
          &battle::CanApplySilence,
          &battle::CanApplyMagicWard,
          &battle::CanApplyRegeneration,
          &battle::CanApplyParalysis,
          &battle::CanApplyPoison,
          &battle::CanApplyNumbness,
          &battle::CanApplyCurse,
          &battle::CanApplyAirAttackUp,
          &battle::CanApplyFireAttackUp,
          &battle::CanApplyEarthAttackUp,
          &battle::CanApplyWaterAttackUp,
          &battle::CanApplyAirDefenseUp,
          &battle::CanApplyFireDefenseUp,
          &battle::CanApplyEarthDefenseUp,
          &battle::CanApplyWaterDefenseUp,
          &battle::CanApplyBanish,
          &battle::CanApplyExorcise,
          &battle::CanApplyDrain,
          &battle::IgnoreAttackPrerequisite,
          &battle::IgnoreAttackPrerequisite,
          &battle::IgnoreAttackPrerequisite,
      };
  for (std::size_t rule = 0; rule < expected.size(); ++rule) {
    INFO("rule " << rule);
    CHECK(battle::AttackPrerequisiteRule(static_cast<int>(rule)) ==
          expected.at(rule));
  }
  CHECK(battle::AttackPrerequisiteRule(-1) == nullptr);
  CHECK(battle::AttackPrerequisiteRule(40) == nullptr);
}

TEST_CASE("Status prerequisites check their own effect's immunity and blocks",
          "[battle][battle-targeting]") {
  for (const auto& [rule, effect] : kStatusOnlyRules) {
    INFO("effect " << effect);
    FakeTarget target;
    SetEffectRule(target, effect);
    CHECK(rule(target));
    target.statuses = kBlockingStatus >> 1;
    target.immunities = kEffectFlag << 1;
    CHECK(rule(target));
    target.immunities = kEffectFlag;
    CHECK_FALSE(rule(target));
    target.immunities = 0;
    target.statuses = kBlockingStatus;
    CHECK_FALSE(rule(target));
    target.statuses = 0;
    target.valid = false;
    CHECK_FALSE(rule(target));
  }
}

TEST_CASE("Status blocks and enemy class come from the target",
          "[battle][battle-targeting]") {
  FakeTarget target;
  SetEffectRule(target, 5);
  target.immunities = kEffectFlag;
  CHECK(battle::IsStatusEffectBlocked(5, target));
  CHECK_FALSE(battle::IsStatusEffectBlocked(6, target));
  target.enemy_class = 7;
  CHECK(battle::TargetEnemyClass(target) == 7);
}

TEST_CASE("Equipment prerequisites need the gear their effect changes",
          "[battle][battle-targeting]") {
  for (const auto& [rule, effect] :
       {StatusRuleCase{&battle::CanApplyEquipmentDown, 19},
        StatusRuleCase{&battle::CanApplyEquipmentUp, 20}}) {
    INFO("effect " << effect);
    FakeTarget target;
    SetEffectRule(target, effect);
    CHECK_FALSE(rule(target));
    target.armor.back() = 9;
    CHECK(rule(target));
    target.armor.back() = 0;
    target.shield = 3;
    CHECK(rule(target));
    target.shield = 0;
    target.blade = 4;
    CHECK(rule(target));
    target.immunities = kEffectFlag;
    CHECK_FALSE(rule(target));
  }
}

TEST_CASE("Attack up prerequisites need an equipped blade",
          "[battle][battle-targeting]") {
  for (const auto& [rule, effect] :
       {StatusRuleCase{&battle::CanApplyAirAttackUp, 21},
        StatusRuleCase{&battle::CanApplyFireAttackUp, 22},
        StatusRuleCase{&battle::CanApplyEarthAttackUp, 23},
        StatusRuleCase{&battle::CanApplyWaterAttackUp, 24}}) {
    INFO("effect " << effect);
    FakeTarget target;
    SetEffectRule(target, effect);
    target.armor.front() = 2;
    target.shield = 3;
    CHECK_FALSE(rule(target));
    target.blade = 4;
    CHECK(rule(target));
    target.statuses = kBlockingStatus;
    CHECK_FALSE(rule(target));
  }
}

TEST_CASE("Defense up prerequisites need body armor or a shield",
          "[battle][battle-targeting]") {
  for (const auto& [rule, effect] :
       {StatusRuleCase{&battle::CanApplyAirDefenseUp, 25},
        StatusRuleCase{&battle::CanApplyFireDefenseUp, 26},
        StatusRuleCase{&battle::CanApplyEarthDefenseUp, 27},
        StatusRuleCase{&battle::CanApplyWaterDefenseUp, 28}}) {
    INFO("effect " << effect);
    FakeTarget target;
    SetEffectRule(target, effect);
    target.blade = 4;
    CHECK_FALSE(rule(target));
    target.armor.at(3) = 5;
    CHECK(rule(target));
    target.armor.at(3) = 0;
    target.shield = 3;
    CHECK(rule(target));
    target.valid = false;
    CHECK_FALSE(rule(target));
  }
}

TEST_CASE("Undead prerequisites split targets by enemy class",
          "[battle][battle-targeting]") {
  FakeTarget target;
  SetEffectRule(target, 30);
  target.enemy_class = battle::kUndeadEnemyClass;
  CHECK(battle::IsAttackTargetUndead(target));
  CHECK_FALSE(battle::IsAttackTargetLiving(target));
  CHECK(battle::CanApplyExorcise(target));
  CHECK_FALSE(battle::CanApplyDrain(target));
  target.enemy_class = 1;
  CHECK_FALSE(battle::IsAttackTargetUndead(target));
  CHECK(battle::IsAttackTargetLiving(target));
  CHECK_FALSE(battle::CanApplyExorcise(target));
  CHECK(battle::CanApplyDrain(target));
}

TEST_CASE("Undead prerequisites refuse immune or absent targets",
          "[battle][battle-targeting]") {
  FakeTarget target;
  SetEffectRule(target, 30);
  target.enemy_class = 1;
  target.immunities = kEffectFlag;
  CHECK_FALSE(battle::CanApplyDrain(target));
  target.immunities = 0;
  target.valid = false;
  CHECK_FALSE(battle::IsAttackTargetLiving(target));
  CHECK_FALSE(battle::CanApplyDrain(target));
  target.enemy_class = battle::kUndeadEnemyClass;
  CHECK_FALSE(battle::IsAttackTargetUndead(target));
  CHECK_FALSE(battle::CanApplyExorcise(target));
}

TEST_CASE("Fixed prerequisites never read the target",
          "[battle][battle-targeting]") {
  FakeTarget target;
  target.valid = false;
  CHECK_FALSE(battle::RejectAttackPrerequisite(target));
  CHECK(battle::AcceptAttackPrerequisite(target));
  CHECK(battle::IgnoreAttackPrerequisite(target));
  CHECK(target.valid_reads == 0);
}

TEST_CASE("Hit prerequisite skips follow-up hits on impact kind 4",
          "[battle][battle-targeting]") {
  FakeTarget target;
  target.enemy_class = battle::kUndeadEnemyClass;
  using Request = battle::HitPrerequisiteRequest;
  CHECK(battle::CheckActionHitPrerequisite(Request{0, 1, 4}, target));
  CHECK_FALSE(battle::CheckActionHitPrerequisite(Request{1, 1, 4}, target));
  CHECK(battle::CheckActionHitPrerequisite(Request{1, 1, 3}, target));
  CHECK(battle::CheckActionHitPrerequisite(Request{1, 2, 4}, target));
  CHECK(battle::CheckActionHitPrerequisite(Request{0, 4, 0}, target));
  CHECK_FALSE(battle::CheckActionHitPrerequisite(Request{0, 5, 0}, target));
  CHECK_FALSE(battle::CheckActionHitPrerequisite(Request{0, 0, 0}, target));
  CHECK_FALSE(battle::CheckActionHitPrerequisite(Request{0, 40, 0}, target));
  CHECK_FALSE(battle::CheckActionHitPrerequisite(Request{0, -1, 0}, target));
}

TEST_CASE("Attack prerequisite slots read the live target actor",
          "[battle][battle-targeting]") {
  LiveActor live;
  live.stats->enemyClass = battle::kUndeadEnemyClass;
  const ScopedOverride<Battle_Actor*> slot(Battle_actors[5], live.actor.get());
  Battle_StatusEffectParams params{};
  params.statusFlags = 0x40;
  params.blockingStatuses = 0x8;
  const ScopedOverride<Battle_StatusEffectParams> strength(
      Battle_statusEffectParams[5], params);
  std::array<u_char, 4> bytes{5, 0, 0, 0};
  CHECK(Battle_IsAttackTargetUndeadSlot(nullptr, bytes.data()) == 1);
  CHECK(Battle_IsAttackTargetLivingSlot(nullptr, bytes.data()) == 0);
  CHECK(Battle_CanApplyStrengthDownSlot(nullptr, bytes.data()) == 1);
  live.stats->statusImmunities = 0x40;
  CHECK(Battle_CanApplyStrengthDownSlot(nullptr, bytes.data()) == 0);
  live.stats->statusImmunities = 0;
  live.stats->statuses = 0x8;
  CHECK(Battle_CanApplyStrengthDownSlot(nullptr, bytes.data()) == 0);
}

TEST_CASE("Attack prerequisite slots read the live target's equipment",
          "[battle][battle-targeting]") {
  LiveActor live;
  const ScopedOverride<Battle_Actor*> slot(Battle_actors[5], live.actor.get());
  std::array<u_char, 4> bytes{5, 0, 0, 0};
  CHECK(Battle_CanApplyAirAttackUpSlot(nullptr, bytes.data()) == 0);
  live.stats->weapon.blade.id = 3;
  CHECK(Battle_CanApplyAirAttackUpSlot(nullptr, bytes.data()) == 1);
  CHECK(Battle_CanApplyAirDefenseUpSlot(nullptr, bytes.data()) == 0);
  live.stats->limbs[2].armor.armor.id = 7;
  CHECK(Battle_CanApplyAirDefenseUpSlot(nullptr, bytes.data()) == 1);
  CHECK(Battle_RejectAttackPrerequisiteSlot(nullptr, bytes.data()) == 0);
  CHECK(Battle_AcceptAttackPrerequisiteSlot(nullptr, bytes.data()) == 1);
  // The original unused slots return the dispatcher's non-zero v0.
  CHECK(Battle_IgnoreAttackPrerequisiteSlot(nullptr, nullptr) == 1);
}

TEST_CASE("Attack prerequisite slots refuse an absent target actor",
          "[battle][battle-targeting]") {
  const BattleLog log;
  const ScopedOverride<Battle_Actor*> slot(Battle_actors[7], nullptr);
  std::array<u_char, 4> bytes{7, 0, 0, 0};
  CHECK(Battle_CanApplyPoisonSlot(nullptr, bytes.data()) == 0);
  CHECK(log.Saw("attack prerequisite for absent target actor=7"));
  bytes[0] = 42;
  CHECK(Battle_CanApplyCurseSlot(nullptr, bytes.data()) == 0);
  CHECK(log.Saw("attack prerequisite for absent target actor=42"));
  CHECK(Battle_AcceptAttackPrerequisiteSlot(nullptr, bytes.data()) == 1);
}

TEST_CASE("Live hit prerequisite reads the action hit and impact byte",
          "[battle][battle-targeting]") {
  LiveActor live;
  const ScopedOverride<Battle_Actor*> slot(Battle_actors[3], live.actor.get());
  Battle_ActionDefinition action{};
  action.id = 77;
  action.hits[0].prerequisiteFunctionId = 4;
  action.hits[1].prerequisiteFunctionId = 1;
  std::array<u_char, 4> bytes{3, 0, 0, 4};
  live.stats->enemyClass = battle::kUndeadEnemyClass;
  CHECK(battle::LiveCheckActionHitPrerequisite(&action, bytes.data(), 0) == 1);
  live.stats->enemyClass = 1;
  CHECK(battle::LiveCheckActionHitPrerequisite(&action, bytes.data(), 0) == 0);
  CHECK(battle::LiveCheckActionHitPrerequisite(&action, bytes.data(), 1) == 0);
  bytes[3] = 2;
  CHECK(battle::LiveCheckActionHitPrerequisite(&action, bytes.data(), 1) == 1);
}

TEST_CASE("Live hit prerequisite refuses missing or unknown inputs",
          "[battle][battle-targeting]") {
  const BattleLog log;
  Battle_ActionDefinition action{};
  action.id = 77;
  std::array<u_char, 4> bytes{};
  CHECK(battle::LiveCheckActionHitPrerequisite(nullptr, bytes.data(), 0) == 0);
  CHECK(log.Saw("hit prerequisite without an action"));
  CHECK(battle::LiveCheckActionHitPrerequisite(&action, nullptr, 0) == 0);
  CHECK(log.Saw("hit prerequisite without a target result"));
  CHECK(battle::LiveCheckActionHitPrerequisite(&action, bytes.data(), 2) == 0);
  CHECK(log.Saw("hit prerequisite for hit=2 of action=77"));
  CHECK(battle::LiveCheckActionHitPrerequisite(&action, bytes.data(), -1) == 0);
  action.hits[0].prerequisiteFunctionId = 45;
  CHECK(battle::LiveCheckActionHitPrerequisite(&action, bytes.data(), 0) == 0);
  CHECK(log.Saw("unknown hit prerequisite rule=45 of action=77"));
}

// ---------------------------------------------------------------------------
// Battle mode.

struct FakeBattleModeHost final : battle::BattleModeHost {
  std::map<int, int> weapon_drawn;
  std::map<int, int> kinds;
  int character_weapon_drawn = 0;
  std::vector<std::string> calls;

  [[nodiscard]] int ActorWeaponDrawn(int actor_slot) const override {
    return ValueOr(weapon_drawn, actor_slot, 0);
  }
  void SetActorWeaponDrawn(int actor_slot, int drawn) override {
    weapon_drawn[actor_slot] = drawn;
    calls.push_back(Call("SetWeapon", {actor_slot, drawn}));
  }
  [[nodiscard]] int ActorKind(int actor_slot) const override {
    return ValueOr(kinds, actor_slot, 0);
  }
  void SetActorKind(int actor_slot, int kind) override {
    kinds[actor_slot] = kind;
    calls.push_back(Call("SetKind", {actor_slot, kind}));
  }
  void ShowWeaponDrawn(int actor_slot, int drawn, int transition) override {
    calls.push_back(Call("Show", {actor_slot, drawn, transition}));
  }
  [[nodiscard]] int CharacterWeaponDrawn() const override {
    return character_weapon_drawn;
  }
  void SetHudBattleMode(int drawn) override {
    calls.push_back(Call("Hud", {drawn}));
  }
};

using Calls = std::vector<std::string>;

TEST_CASE("Battle mode toggle flips the drawn bit and snaps the pose",
          "[battle][battle-targeting]") {
  FakeBattleModeHost host;
  host.weapon_drawn[3] = 6;
  CHECK_FALSE(battle::IsActorWeaponDrawn(3, host));
  battle::SetActorBattleMode(3, battle::kToggleWeapon, host);
  CHECK(host.calls == Calls{"SetWeapon(3,7)", "Show(3,1,0)"});
  CHECK(battle::IsActorWeaponDrawn(3, host));
  host.calls.clear();
  host.weapon_drawn[0] = 1;
  host.character_weapon_drawn = 5;
  battle::SetActorBattleMode(0, battle::kToggleWeapon, host);
  CHECK(host.calls == Calls{"SetWeapon(0,0)", "Show(0,0,0)", "Hud(1)"});
}

TEST_CASE("Battle mode switches enemy kinds and animates the player",
          "[battle][battle-targeting]") {
  FakeBattleModeHost host;
  host.kinds[4] = 8;
  battle::SetActorBattleMode(4, battle::kDrawWeapon, host);
  CHECK(host.kinds[4] == 4);
  battle::SetActorBattleMode(4, battle::kSheatheWeapon, host);
  CHECK(host.kinds[4] == 8);
  battle::SetActorBattleMode(4, 3, host);
  CHECK(host.kinds[4] == 4);
  host.kinds[5] = 2;
  host.calls.clear();
  battle::SetActorBattleMode(5, battle::kDrawWeapon, host);
  CHECK(host.calls.empty());
  host.weapon_drawn[0] = 6;
  host.character_weapon_drawn = 2;
  battle::SetActorBattleMode(0, battle::kDrawWeapon, host);
  CHECK(host.calls == Calls{"SetWeapon(0,7)", "Show(0,1,1)", "Hud(0)"});
  host.calls.clear();
  battle::SetActorBattleMode(0, battle::kSheatheWeapon, host);
  CHECK(host.calls == Calls{"SetWeapon(0,6)", "Show(0,0,1)", "Hud(0)"});
}

TEST_CASE("Live battle mode switches an enemy's weapon kind",
          "[battle][battle-targeting]") {
  LiveActor live;
  live.actor->kind = 8;
  live.actor->weaponDrawn = 3;
  const ScopedOverride<Battle_Actor*> enemy(Battle_actors[17],
                                            live.actor.get());
  CHECK(battle::LiveIsActorWeaponDrawn(17));
  VS::Battle::LiveSetActorBattleMode(17, battle::kDrawWeapon);
  CHECK(live.actor->kind == 4);
  battle::LiveSetActorBattleMode(17, battle::kSheatheWeapon);
  CHECK(live.actor->kind == 8);
}

TEST_CASE("Live battle mode refuses absent actors and character state",
          "[battle][battle-targeting]") {
  const ScopedOverride<Battle_Actor*> absent(Battle_actors[18], nullptr);
  const BattleLog log;
  CHECK_FALSE(battle::LiveIsActorWeaponDrawn(18));
  CHECK(log.Saw("weapon state of absent actor slot=18"));
  VS::Battle::LiveSetActorBattleMode(18, battle::kDrawWeapon);
  CHECK(log.Saw("battle mode for absent actor slot=18"));
  VS::Battle::LiveSetActorBattleMode(-1, battle::kDrawWeapon);
  CHECK(log.Saw("battle mode for absent actor slot=-1"));
  LiveActor player;
  player.actor->weaponDrawn = 0;
  const ScopedOverride<Battle_Actor*> slot0(Battle_actors[0],
                                            player.actor.get());
  const ScopedOverride<Battle_Actor*> character(Battle_characterState, nullptr);
  VS::Battle::LiveSetActorBattleMode(0, battle::kDrawWeapon);
  CHECK(log.Saw("battle mode without a character state"));
  CHECK(player.actor->weaponDrawn == 0);
}

// ---------------------------------------------------------------------------
// Battle abilities.

using Field = battle::AbilityField;
using battle::AbilityKind;

constexpr int kFirstChainActionId = 40;
constexpr int kFirstDefenseActionId = 60;
constexpr int kUnsetRequirement = 0xFFFF;
constexpr std::uint32_t kCircle = 0x20;
constexpr std::uint32_t kTriangle = 0x10;
constexpr std::uint32_t kSquare = 0x80;

struct FakeAbilityHost final : battle::AbilityHost {
  std::map<battle::AbilityField, int> fields;
  std::uint32_t queue_index = 0;
  std::uint32_t chain_step = 0;
  battle::ActionRequestView queued;
  battle::ActionRequestView selection;
  int tick_speed = 2;
  int reward_mode = 0;
  std::vector<battle::SelectionTargetView> targets;
  std::array<std::array<int, battle::kAbilityInputSlots>, 2> mapped{};
  std::set<int> available;
  std::set<int> unlocked;
  std::map<int, int> spawn_index;
  std::map<int, int> hp;
  std::map<int, int> default_limb;
  std::map<int, std::vector<int>> spell_classes;
  int weapon_category = 0;
  std::map<int, int> arts_learned;
  std::map<int, int> category_kills;
  std::map<std::pair<int, int>, int> break_requirements;
  int abilities_unlocked = 0;
  std::map<int, int> ability_requirements;
  int total_kills = 0;
  int max_chain = 0;
  int chain_streak = 0;
  std::map<int, int> script_flags;
  std::uint32_t pressed = 0;
  bool effects_busy = false;
  std::set<int> busy_actors;
  std::vector<std::string> calls;

  [[nodiscard]] std::uint32_t QueueIndex() const override {
    return queue_index;
  }
  void SetQueueIndex(std::uint32_t index) override { queue_index = index; }
  [[nodiscard]] std::uint32_t ChainStep() const override { return chain_step; }
  void SetChainStep(std::uint32_t step) override { chain_step = step; }
  [[nodiscard]] int Field(battle::AbilityField field) const override {
    return ValueOr(fields, field, 0);
  }
  void SetField(battle::AbilityField field, int value) override {
    fields[field] = value;
  }
  [[nodiscard]] battle::ActionRequestView QueuedAction() const override {
    return queued;
  }
  [[nodiscard]] battle::ActionRequestView Selection() const override {
    return selection;
  }
  [[nodiscard]] int TickSpeed() const override { return tick_speed; }
  void SetTickSpeed(int speed) override { tick_speed = speed; }
  void SetActionFlowMode(int mode) override {
    calls.push_back(Call("Flow", {mode}));
  }
  [[nodiscard]] int RecoveryRewardMode() const override { return reward_mode; }
  [[nodiscard]] battle::SelectionTargetView SelectionTarget(
      int index) const override {
    return targets.at(static_cast<std::size_t>(index));
  }
  void CopySelectionTargetToPrimary(int index) override {
    calls.push_back(Call("CopyToPrimary", {index}));
  }
  void SetSelectionPrimaryVitals(int vital_hp, int vital_mp) override {
    calls.push_back(Call("Vitals", {vital_hp, vital_mp}));
  }
  void SavePrimaryTarget() override { calls.emplace_back("SavePrimary()"); }
  void RestoreSavedTargetAsFirst(int limb) override {
    calls.push_back(Call("RestoreFirst", {limb}));
  }
  void SetSelectionTargetCount(int count) override {
    calls.push_back(Call("TargetCount", {count}));
  }
  void FocusSelectionOnActor(int actor) override {
    calls.push_back(Call("Focus", {actor}));
  }
  [[nodiscard]] int MappedAbility(AbilityKind kind, int index) const override {
    return mapped.at(kind == AbilityKind::kChain ? 0 : 1)
        .at(static_cast<std::size_t>(index));
  }
  [[nodiscard]] bool ActionAvailable(int action_id) const override {
    return available.contains(action_id);
  }
  [[nodiscard]] bool ActionDefinitionUnlocked(int action_id) const override {
    return unlocked.contains(action_id);
  }
  void SetActionDefinitionUnlocked(int action_id) override {
    unlocked.insert(action_id);
    calls.push_back(Call("Unlock", {action_id}));
  }
  [[nodiscard]] int ActorRoomSpawnIndex(int actor) const override {
    return ValueOr(spawn_index, actor, 0);
  }
  [[nodiscard]] int ActorHp(int actor) const override {
    return ValueOr(hp, actor, 1);
  }
  [[nodiscard]] int ActorDefaultTargetLimb(int actor) const override {
    return ValueOr(default_limb, actor, 0);
  }
  [[nodiscard]] int SpellClassCount(int spell_class) const override {
    const auto found = spell_classes.find(spell_class);
    return found != spell_classes.end() ? static_cast<int>(found->second.size())
                                        : 0;
  }
  [[nodiscard]] int SpellClassSpellId(int spell_class,
                                      int index) const override {
    return spell_classes.at(spell_class).at(static_cast<std::size_t>(index));
  }
  [[nodiscard]] int AbilityActionId(AbilityKind kind,
                                    int index) const override {
    return (kind == AbilityKind::kChain ? kFirstChainActionId
                                        : kFirstDefenseActionId) +
           index;
  }
  [[nodiscard]] int EquippedWeaponCategory() const override {
    return weapon_category;
  }
  [[nodiscard]] int ArtsLearned(int category) const override {
    return ValueOr(arts_learned, category, 0);
  }
  [[nodiscard]] int WeaponCategoryKills(int category) const override {
    return ValueOr(category_kills, category, 0);
  }
  [[nodiscard]] int BreakArtRequirement(int category, int tier) const override {
    return ValueOr(break_requirements, std::pair{category, tier},
                   kUnsetRequirement);
  }
  [[nodiscard]] int BattleAbilitiesUnlocked() const override {
    return abilities_unlocked;
  }
  [[nodiscard]] int BattleAbilityRequirement(int index) const override {
    return ValueOr(ability_requirements, index, kUnsetRequirement);
  }
  [[nodiscard]] int TotalKills() const override { return total_kills; }
  void SetTotalKills(int kills) override { total_kills = kills; }
  [[nodiscard]] int MaxChain() const override { return max_chain; }
  void SetMaxChain(int chain) override { max_chain = chain; }
  [[nodiscard]] int ChainStreak() const override { return chain_streak; }
  void SetChainStreak(int streak) override { chain_streak = streak; }
  [[nodiscard]] int ScriptFlag(int flag) const override {
    return ValueOr(script_flags, flag, 0);
  }
  void WaitForPostActionMovement() override { calls.emplace_back("Wait()"); }
  void FinishBattleAbilitySequence() override {
    calls.emplace_back("Finish()");
  }
  void PlayAbilityLearnedPose() override { calls.emplace_back("Pose()"); }
  void SpawnAbilityLearnedEffect() override { calls.emplace_back("Effect()"); }
  void PauseMovingPlatforms() override {
    calls.emplace_back("PausePlatforms()");
  }
  void PauseFieldForMenu() override { calls.emplace_back("PauseField()"); }
  void ShowActionLearnedNotification(int action_id) override {
    calls.push_back(Call("Learned", {action_id}));
  }
  void OpenAbilityUnlockMenu(battle::PendingAbilityUnlocks unlocks) override {
    calls.push_back(Call("UnlockMenu", {unlocks.break_art ? 1 : 0,
                                        unlocks.battle_ability ? 1 : 0}));
  }
  void ReleaseActorSequence(int actor, int kind) override {
    calls.push_back(Call("Release", {actor, kind}));
  }
  void NotifyQueuedActionRelationships() override {
    calls.emplace_back("Notify()");
  }
  void ResolveSelectionIntoQueue(int action_id) override {
    calls.push_back(Call("Resolve", {action_id}));
  }
  void QueueChainCount(int count) override {
    calls.push_back(Call("ChainCount", {count}));
  }
  void ShowAbilityTimingCue(int frames) override {
    calls.push_back(Call("Cue", {frames}));
  }
  void QueueAbilityTimingResult(int result) override {
    calls.push_back(Call("Result", {result}));
  }
  void ClearAbilityInputResult() override {
    calls.emplace_back("ClearResult()");
  }
  [[nodiscard]] std::uint32_t PressedButtons() const override {
    return pressed;
  }
  [[nodiscard]] bool EffectsBusy() const override { return effects_busy; }
  [[nodiscard]] bool IsActorActionAnimationBusy(int actor) const override {
    return busy_actors.contains(actor);
  }
  void RestoreActorIdlePose(int actor) override {
    calls.push_back(Call("Idle", {actor}));
  }
};

// The player's attack on actor 3 with the Triangle chain ability mapped.
void SetUpChain(FakeAbilityHost& host) {
  host.queued = {.action_id = 10, .target_count = 1, .first_target_actor = 3};
  host.mapped.at(0).at(1) = 41;
  host.available.insert(41);
}

// Actor 5's attack on the player with the Circle defense ability mapped.
void SetUpDefense(FakeAbilityHost& host) {
  host.queued = {.action_id = 70, .primary_actor = 5, .target_count = 1};
  host.mapped.at(1).at(0) = 61;
  host.available.insert(61);
}

TEST_CASE("Action unlock queries accept actions 1 to 255",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  host.unlocked = {0, 255, 256};
  CHECK_FALSE(battle::IsActionUnlocked(0, host));
  CHECK(battle::IsActionUnlocked(255, host));
  CHECK_FALSE(battle::IsActionUnlocked(256, host));
  host.unlocked.clear();
  battle::UnlockAction(0, host);
  battle::UnlockAction(256, host);
  CHECK(host.unlocked.empty());
  battle::UnlockAction(9, host);
  CHECK(host.calls == Calls{"Unlock(9)"});
  CHECK(battle::IsActionUnlocked(9, host));
}

TEST_CASE("Spell class and break art unlock queries",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  host.spell_classes[2] = {30, 31, 32};
  CHECK_FALSE(battle::HasUnlockedSpellClass(2, host));
  host.unlocked = {32};
  CHECK(battle::HasUnlockedSpellClass(2, host));
  CHECK_FALSE(battle::HasUnlockedSpellClass(1, host));
  host.weapon_category = 3;
  host.unlocked = {0xBF, 0xC4};
  CHECK_FALSE(battle::HasUnlockedWeaponBreakArt(host));
  host.unlocked = {0xC0};
  CHECK(battle::HasUnlockedWeaponBreakArt(host));
  host.unlocked = {0xC3};
  CHECK(battle::HasUnlockedWeaponBreakArt(host));
}

TEST_CASE("Battle ability unlock queries split chain and defense",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  host.unlocked = {39, 54, 59, 74};
  CHECK_FALSE(battle::HasUnlockedBattleAbility(0, host));
  CHECK_FALSE(battle::HasUnlockedBattleAbility(1, host));
  host.unlocked = {53};
  CHECK(battle::HasUnlockedBattleAbility(0, host));
  CHECK_FALSE(battle::HasUnlockedBattleAbility(1, host));
  host.unlocked = {60};
  CHECK(battle::HasUnlockedBattleAbility(7, host));
}

TEST_CASE("Pending unlocks prefer the weapon category break art",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  host.weapon_category = 13;
  host.arts_learned[3] = 1;
  host.category_kills[3] = 20;
  host.break_requirements[{3, 1}] = 20;
  host.abilities_unlocked = 4;
  host.ability_requirements[4] = 100;
  host.total_kills = 100;
  battle::PendingAbilityUnlocks pending =
      battle::PendingBattleAbilityUnlocks(host);
  CHECK(pending.break_art);
  CHECK_FALSE(pending.battle_ability);
  host.category_kills[3] = 19;
  pending = battle::PendingBattleAbilityUnlocks(host);
  CHECK_FALSE(pending.break_art);
  CHECK(pending.battle_ability);
  host.total_kills = 99;
  CHECK_FALSE(battle::PendingBattleAbilityUnlocks(host).Any());
}

TEST_CASE("Pending unlocks skip learned arts and met ability counts",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  host.weapon_category = 13;
  host.arts_learned[3] = 4;
  host.category_kills[3] = kUnsetRequirement;
  host.break_requirements[{3, 1}] = 20;
  host.abilities_unlocked = 4;
  host.ability_requirements[4] = 100;
  host.total_kills = 100;
  const battle::PendingAbilityUnlocks pending =
      battle::PendingBattleAbilityUnlocks(host);
  CHECK_FALSE(pending.break_art);
  CHECK(pending.battle_ability);
  host.abilities_unlocked = 22;
  host.ability_requirements[22] = 0;
  CHECK_FALSE(battle::PendingBattleAbilityUnlocks(host).Any());
}

TEST_CASE("Pending reward poses only for an earned or new action",
          "[battle][battle-targeting]") {
  const Calls pose{"Flow(13)", "Pose()", "Effect()", "PausePlatforms()"};
  FakeAbilityHost host;
  host.weapon_category = 1;
  host.category_kills[1] = 5;
  host.break_requirements[{1, 0}] = 5;
  battle::BeginPendingAbilityReward(host);
  CHECK(host.calls == Calls{"Wait()"});
  host.calls.clear();
  host.reward_mode = 2;
  battle::BeginPendingAbilityReward(host);
  CHECK(host.calls == pose);
  host.calls.clear();
  host.reward_mode = 1;
  battle::BeginPendingAbilityReward(host);
  CHECK(host.calls == Calls{"Wait()"});
  host.calls.clear();
  host.selection = {.action_id = 99};
  battle::BeginPendingAbilityReward(host);
  CHECK(host.calls == pose);
  host.calls.clear();
  host.selection.primary_actor = 2;
  battle::BeginPendingAbilityReward(host);
  CHECK(host.calls == Calls{"Wait()"});
  host.calls.clear();
  host.selection.primary_actor = 0;
  host.fields[Field::kChainContinued] = 1;
  battle::BeginPendingAbilityReward(host);
  CHECK(host.calls == Calls{"Wait()"});
}

TEST_CASE("Granting learns a new action or opens the unlock menu",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  host.selection = {.action_id = 99};
  battle::GrantPendingAbilityUnlock(host);
  CHECK(host.calls ==
        Calls{"Flow(14)", "PauseField()", "Learned(99)", "Unlock(99)"});
  host.calls.clear();
  battle::GrantPendingAbilityUnlock(host);
  CHECK(host.calls == Calls{"Flow(14)", "PauseField()", "UnlockMenu(0,0)"});
  host.calls.clear();
  host.weapon_category = 1;
  host.category_kills[1] = 5;
  host.break_requirements[{1, 0}] = 5;
  battle::GrantPendingAbilityUnlock(host);
  CHECK(host.calls == Calls{"Flow(14)", "PauseField()", "UnlockMenu(1,0)"});
}

TEST_CASE("Chain ability trigger needs a fresh mapped input",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpChain(host);
  CHECK(battle::CanTriggerBattleAbility(0, host));
  CHECK(battle::CanTriggerBattleAbility(2, host));
  CHECK_FALSE(battle::CanTriggerBattleAbility(1, host));
  host.fields[Field::kPreviousChainInput] = 2;
  CHECK_FALSE(battle::CanTriggerBattleAbility(2, host));
  CHECK(battle::CanTriggerBattleAbility(0, host));
  host.fields.clear();
  host.fields[Field::kAbilityInputAttempted] = 1;
  CHECK_FALSE(battle::CanTriggerBattleAbility(0, host));
  host.fields.clear();
  host.spawn_index[3] = 0x80;
  CHECK_FALSE(battle::CanTriggerBattleAbility(0, host));
}

TEST_CASE("Chain ability trigger needs a basic attack on one actor",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpChain(host);
  host.queued.action_id = 40;
  CHECK_FALSE(battle::CanTriggerBattleAbility(0, host));
  host.queued.action_id = 39;
  CHECK(battle::CanTriggerBattleAbility(0, host));
  host.queued.target_count = 0;
  CHECK_FALSE(battle::CanTriggerBattleAbility(0, host));
  host.queued.target_count = 1;
  host.mapped.at(0).at(1) = 0;
  CHECK_FALSE(battle::CanTriggerBattleAbility(0, host));
}

TEST_CASE("Chain ability trigger refuses position or self targets",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpChain(host);
  host.queued.first_target_targets_position = true;
  CHECK_FALSE(battle::CanTriggerBattleAbility(0, host));
  host.queued.first_target_targets_position = false;
  host.queued.primary_targets_position = true;
  CHECK_FALSE(battle::CanTriggerBattleAbility(0, host));
  host.queued.primary_targets_position = false;
  host.queued.first_target_actor = 0;
  CHECK_FALSE(battle::CanTriggerBattleAbility(0, host));
  host.queued.first_target_actor = 3;
  CHECK(battle::CanTriggerBattleAbility(0, host));
}

TEST_CASE("Defense ability trigger needs an attack on the player",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpDefense(host);
  CHECK(battle::CanTriggerBattleAbility(0, host));
  CHECK(battle::CanTriggerBattleAbility(1, host));
  host.queued.first_target_actor = 2;
  CHECK_FALSE(battle::CanTriggerBattleAbility(0, host));
  host.queued.first_target_actor = 0;
  host.queued.action_id = 45;
  CHECK_FALSE(battle::CanTriggerBattleAbility(0, host));
  host.queued.action_id = 54;
  CHECK(battle::CanTriggerBattleAbility(0, host));
  const BattleLog log;
  CHECK_FALSE(battle::CanTriggerBattleAbility(4, host));
  CHECK(log.Saw("invalid ability input slot=4"));
  CHECK_FALSE(battle::CanTriggerBattleAbility(-1, host));
  CHECK(log.Saw("invalid ability input slot=-1"));
}

TEST_CASE("Defense timing window opens for attacks on the player",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpDefense(host);
  host.fields[Field::kAbilityTimer] = 30;
  host.tick_speed = 3;
  battle::ArmDefenseAbilityTimingWindow(1, host);
  CHECK(host.Field(Field::kAbilityWindow) == 0);
  battle::ArmDefenseAbilityTimingWindow(0, host);
  CHECK(host.Field(Field::kAbilityWindow) == 36);
  host.fields[Field::kAbilityTimer] = 50;
  battle::ArmDefenseAbilityTimingWindow(0, host);
  CHECK(host.Field(Field::kAbilityWindow) == 36);
  FakeAbilityHost chain;
  SetUpChain(chain);
  battle::ArmDefenseAbilityTimingWindow(0, chain);
  CHECK(chain.Field(Field::kAbilityWindow) == 0);
  FakeAbilityHost unmapped;
  SetUpDefense(unmapped);
  unmapped.mapped.at(1).at(0) = 0;
  battle::ArmDefenseAbilityTimingWindow(0, unmapped);
  CHECK(unmapped.Field(Field::kAbilityWindow) == 0);
}

TEST_CASE("Ability timing cues the window lead time from the margin",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpChain(host);
  host.busy_actors = {0};
  host.fields[Field::kAbilityWindow] = 50;
  host.fields[Field::kAbilityTimer] = 46;
  battle::UpdateBattleAbilityTiming(host);
  CHECK(host.calls == Calls{"Cue(10)"});
  CHECK(host.Field(Field::kAbilityTimer) == 48);
  host.calls.clear();
  battle::UpdateBattleAbilityTiming(host);
  CHECK(host.calls.empty());
  CHECK(host.Field(Field::kAbilityTimer) == 50);
}

TEST_CASE("Defense ability timing cues from the queue and tick speed",
          "[battle][battle-targeting]") {
  FakeAbilityHost defense;
  SetUpDefense(defense);
  defense.busy_actors = {5};
  defense.script_flags[1] = 1;
  defense.queue_index = 6;
  defense.tick_speed = 4;
  defense.fields[Field::kAbilityWindow] = 50;
  defense.fields[Field::kAbilityTimer] = 42;
  battle::UpdateBattleAbilityTiming(defense);
  CHECK(defense.calls == Calls{"Cue(5)"});
  defense.calls.clear();
  defense.queue_index = 60;
  defense.fields[Field::kAbilityTimer] = 42;
  battle::UpdateBattleAbilityTiming(defense);
  CHECK(defense.calls == Calls{"Cue(1)"});
  // A zero tick speed gives the R3000 quotient instead of trapping.
  defense.calls.clear();
  defense.tick_speed = 0;
  defense.fields[Field::kAbilityTimer] = 50;
  battle::UpdateBattleAbilityTiming(defense);
  CHECK(defense.calls == Calls{"Cue(-1)"});
}

TEST_CASE("Ability timing grades a chain input against the window",
          "[battle][battle-targeting]") {
  const std::array<std::pair<int, int>, 8> grades{
      {{40, 0}, {41, 1}, {50, 1}, {51, 2}, {60, 2}, {61, 3}, {70, 3}, {71, 4}}};
  for (const auto& [timer, result] : grades) {
    INFO("timer " << timer);
    FakeAbilityHost host;
    SetUpChain(host);
    host.busy_actors = {0};
    host.pressed = kTriangle;
    host.fields[Field::kAbilityWindow] = 50;
    host.fields[Field::kAbilityTimer] = timer;
    battle::UpdateBattleAbilityTiming(host);
    CHECK(host.Field(Field::kAbilityInputAttempted) == 1);
    if (result == 2) {
      CHECK(host.calls == Calls{"Result(2)"});
      CHECK(host.Field(Field::kAbilityInputSucceeded) == 1);
      CHECK(host.Field(Field::kChainAbilitySlot) == 2);
      CHECK(host.Field(Field::kPreviousChainInput) == 2);
    } else {
      CHECK(host.calls == Calls{Call("Result", {result}), "ClearResult()"});
      CHECK(host.Field(Field::kAbilityInputSucceeded) == 0);
      CHECK(host.Field(Field::kChainAbilitySlot) == 0);
    }
  }
}

TEST_CASE("Ability timing ignores unmapped and refused inputs",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpChain(host);
  host.busy_actors = {0};
  host.pressed = kSquare;
  host.fields[Field::kAbilityWindow] = 50;
  host.fields[Field::kAbilityTimer] = 55;
  battle::UpdateBattleAbilityTiming(host);
  CHECK(host.calls.empty());
  CHECK(host.Field(Field::kAbilityInputAttempted) == 0);
  host.pressed = kTriangle;
  host.fields[Field::kAbilityWindow] = 0;
  battle::UpdateBattleAbilityTiming(host);
  CHECK(host.calls == Calls{"Result(1)"});
  CHECK(host.Field(Field::kAbilityInputAttempted) == 1);
  CHECK(host.Field(Field::kAbilityInputSucceeded) == 0);
}

TEST_CASE("Ability timing ignores a repeated chain input",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpChain(host);
  host.busy_actors = {0};
  host.pressed = kTriangle;
  host.fields[Field::kAbilityWindow] = 50;
  host.fields[Field::kAbilityTimer] = 55;
  host.fields[Field::kPreviousChainInput] = 2;
  host.fields[Field::kAbilityInputSucceeded] = 1;
  battle::UpdateBattleAbilityTiming(host);
  CHECK(host.calls.empty());
  CHECK(host.Field(Field::kAbilityInputAttempted) == 1);
  CHECK(host.Field(Field::kAbilityInputSucceeded) == 0);
}

TEST_CASE("Ability timing accepts a fresh defense input and counts it",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpDefense(host);
  host.busy_actors = {5};
  host.pressed = kCircle;
  host.chain_streak = 4;
  host.fields[Field::kAbilityWindow] = 50;
  host.fields[Field::kAbilityTimer] = 63;
  battle::UpdateBattleAbilityTiming(host);
  CHECK(host.calls == Calls{"Result(2)"});
  CHECK(host.Field(Field::kAbilityInputSucceeded) == 2);
  CHECK(host.Field(Field::kChainAbilitySlot) == 1);
  CHECK(host.chain_streak == 5);
}

TEST_CASE("Ability timing caps the streak and ignores a held button",
          "[battle][battle-targeting]") {
  FakeAbilityHost capped;
  SetUpDefense(capped);
  capped.busy_actors = {5};
  capped.pressed = kCircle;
  capped.chain_streak = 0xFFFF;
  capped.fields[Field::kAbilityWindow] = 50;
  capped.fields[Field::kAbilityTimer] = 63;
  battle::UpdateBattleAbilityTiming(capped);
  CHECK(capped.Field(Field::kAbilityInputSucceeded) == 2);
  CHECK(capped.chain_streak == 0xFFFF);
  FakeAbilityHost held;
  SetUpDefense(held);
  held.busy_actors = {5};
  held.pressed = kCircle;
  held.fields[Field::kPreviousChainButtons] = static_cast<int>(kCircle);
  held.fields[Field::kAbilityWindow] = 50;
  held.fields[Field::kAbilityTimer] = 63;
  battle::UpdateBattleAbilityTiming(held);
  CHECK(held.calls == Calls{"Result(2)", "ClearResult()"});
  CHECK(held.Field(Field::kAbilityInputAttempted) == 0);
}

TEST_CASE("Ability timing restores idle poses before advancing",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpChain(host);
  host.tick_speed = 3;
  host.hp[3] = 0;
  battle::UpdateBattleAbilityTiming(host);
  CHECK(host.calls == Calls{"Idle(0)", "Idle(0)", "Finish()"});
  CHECK(host.tick_speed == 2);
  FakeAbilityHost waiting;
  SetUpChain(waiting);
  waiting.effects_busy = true;
  waiting.fields[Field::kChainAbilitySlot] = 1;
  battle::UpdateBattleAbilityTiming(waiting);
  CHECK(waiting.calls.empty());
  CHECK(waiting.Field(Field::kAbilityTimer) == 2);
}

TEST_CASE("Ability sequence releases spell and break art actors",
          "[battle][battle-targeting]") {
  const std::array<std::pair<int, std::string>, 5> releases{{
      {0x8C, ""},
      {0x8D, "Release(4,5)"},
      {0xB8, "Release(4,4)"},
      {0xDF, "Release(4,4)"},
      {0xE0, ""},
  }};
  for (const auto& [action, release] : releases) {
    INFO("action " << action);
    FakeAbilityHost host;
    host.queued = {.action_id = action, .primary_actor = 4};
    host.tick_speed = 5;
    battle::AdvanceBattleAbilitySequence(host);
    Calls expected;
    if (!release.empty()) expected.push_back(release);
    expected.emplace_back("Finish()");
    CHECK(host.calls == expected);
    CHECK(host.tick_speed == 2);
  }
  FakeAbilityHost limit;
  SetUpChain(limit);
  limit.queue_index = 999;
  limit.fields[Field::kAbilityInputSucceeded] = 1;
  limit.fields[Field::kChainAbilitySlot] = 2;
  battle::AdvanceBattleAbilitySequence(limit);
  CHECK(limit.calls == Calls{"Finish()"});
}

// A successful Triangle chain input on the fourth queued hit.
void SetUpChainStep(FakeAbilityHost& host) {
  SetUpChain(host);
  host.fields[Field::kAbilityInputSucceeded] = 1;
  host.fields[Field::kChainAbilitySlot] = 2;
  host.queue_index = 3;
  host.chain_step = 7;
  host.max_chain = 2;
  host.script_flags[0x82] = 10;
  host.abilities_unlocked = 3;
  host.total_kills = 7;
}

TEST_CASE("Chain step resolves the mapped chain ability",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpChainStep(host);
  battle::AdvanceBattleAbilitySequence(host);
  CHECK(host.calls ==
        Calls{"Notify()", "Flow(8)", "Resolve(41)", "ChainCount(4)"});
  CHECK(host.chain_step == 8);
  CHECK(host.queue_index == 4);
  CHECK(host.max_chain == 4);
  CHECK(host.total_kills == 8);
}

TEST_CASE("Chain step keeps a longer chain record and an unset kill count",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpChainStep(host);
  host.max_chain = 9;
  host.total_kills = kUnsetRequirement;
  battle::AdvanceBattleAbilitySequence(host);
  CHECK(host.calls ==
        Calls{"Notify()", "Flow(8)", "Resolve(41)", "ChainCount(4)"});
  CHECK(host.max_chain == 9);
  CHECK(host.total_kills == kUnsetRequirement);
}

TEST_CASE("Chain step finishes on a dead target or an invalid slot",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpChainStep(host);
  host.hp[3] = 0;
  battle::AdvanceBattleAbilitySequence(host);
  CHECK(host.calls == Calls{"Finish()"});
  const BattleLog log;
  host.calls.clear();
  host.hp[3] = 5;
  host.fields[Field::kChainAbilitySlot] = 0;
  battle::AdvanceBattleAbilitySequence(host);
  CHECK(host.calls == Calls{"Finish()"});
  CHECK(log.Saw("chain step with invalid slot=0"));
}

TEST_CASE("Defense step collapses the selection onto the attacker",
          "[battle][battle-targeting]") {
  FakeAbilityHost host;
  SetUpDefense(host);
  host.fields[Field::kAbilityInputSucceeded] = 2;
  host.fields[Field::kChainAbilitySlot] = 1;
  host.selection = {.primary_actor = 6, .target_count = 4};
  host.targets = {{.actor = 0, .hp = 10, .mp = 0, .has_vital_effect = true},
                  {.actor = 0, .hp = 5, .mp = 2, .has_vital_effect = false},
                  {.actor = 0, .hp = 3, .mp = 1, .has_vital_effect = true},
                  {.actor = 4, .hp = 99, .mp = 0, .has_vital_effect = true}};
  host.default_limb[6] = 2;
  host.queue_index = 1;
  host.total_kills = 3;
  battle::AdvanceBattleAbilitySequence(host);
  CHECK(host.calls == Calls{"Notify()", "Flow(8)", "SavePrimary()",
                            "CopyToPrimary(2)", "Vitals(18,3)",
                            "RestoreFirst(2)", "TargetCount(1)", "Focus(6)",
                            "Resolve(61)"});
  CHECK(host.chain_step == 1);
  CHECK(host.queue_index == 2);
  CHECK(host.Field(Field::kChainContinued) == 1);
  CHECK(host.total_kills == 4);
  const BattleLog log;
  host.calls.clear();
  host.fields[Field::kChainAbilitySlot] = 4;
  battle::AdvanceBattleAbilitySequence(host);
  CHECK(host.calls == Calls{"Finish()"});
  CHECK(log.Saw("defense step with invalid slot=4"));
}

TEST_CASE("Live ability bindings refuse a missing character state",
          "[battle][battle-targeting]") {
  const ScopedOverride<Battle_ActionState*> state(Battle_actionState, nullptr);
  const ScopedOverride<Battle_Actor*> character(Battle_characterState, nullptr);
  const BattleLog log;
  CHECK(VS::Battle::LivePendingBattleAbilityUnlocks() == 0);
  CHECK(log.Saw("pending ability unlocks without a character state"));
  CHECK(VS::Battle::LiveHasUnlockedWeaponBreakArt() == 0);
  CHECK(log.Saw("break art unlocks without a character state"));
}

TEST_CASE("Live ability rewards refuse a missing action state",
          "[battle][battle-targeting]") {
  const ScopedOverride<Battle_ActionState*> state(Battle_actionState, nullptr);
  const ScopedOverride<Battle_Actor*> character(Battle_characterState, nullptr);
  const BattleLog log;
  battle::LiveArmDefenseAbilityTimingWindow(0);
  CHECK(log.Saw("arm defense window without an action state"));
  VS::Battle::LiveBeginPendingAbilityReward();
  CHECK(log.Saw("pending ability reward without an action state"));
  battle::LiveGrantPendingAbilityUnlock();
  CHECK(log.Saw("grant ability unlock without an action state"));
}

TEST_CASE("Live ability sequence refuses a missing action state",
          "[battle][battle-targeting]") {
  const ScopedOverride<Battle_ActionState*> state(Battle_actionState, nullptr);
  const ScopedOverride<Battle_Actor*> character(Battle_characterState, nullptr);
  const BattleLog log;
  VS::Battle::LiveAdvanceBattleAbilitySequence();
  CHECK(log.Saw("advance ability sequence without an action state"));
  CHECK(VS::Battle::LiveCanTriggerBattleAbility(0) == 0);
  CHECK(log.Saw("ability trigger check without an action state"));
  VS::Battle::LiveUpdateBattleAbilityTiming();
  CHECK(log.Saw("ability timing without an action state"));
}

TEST_CASE("Live action unlocks round-trip the action table",
          "[battle][battle-targeting]") {
  const ScopedOverride<Battle_ActionDefinition> definition(
      Battle_actionDefinitions[200], Battle_ActionDefinition{});
  CHECK(battle::LiveIsActionUnlocked(200) == 0);
  battle::LiveUnlockAction(200);
  CHECK(Battle_actionDefinitions[200].unlocked == 1);
  CHECK(battle::LiveIsActionUnlocked(200) == 1);
  CHECK(battle::LiveIsActionUnlocked(0) == 0);
  CHECK(battle::LiveIsActionUnlocked(256) == 0);
  battle::LiveUnlockAction(256);
}

// ---------------------------------------------------------------------------
// Action targeting.

using battle::ActorLimbTarget;
using battle::TargetingVolumeSlot;
using battle::TargetPoint;
using TargetPreview = battle::ActionTargetPreview;

constexpr std::size_t kRangeSlot = 0;
constexpr std::size_t kAreaSlot = 1;

// One recorded hit target: an actor and limb, or a mode and position.
struct RecordedTarget {
  int actor = -1;
  int limb = 0;
  int mode = 0;
  TargetPoint point{};

  friend bool operator==(const RecordedTarget&,
                         const RecordedTarget&) = default;
};

[[nodiscard]] RecordedTarget ActorTarget(int actor, int limb) {
  return RecordedTarget{.actor = actor, .limb = limb};
}

[[nodiscard]] RecordedTarget PositionTarget(int mode, TargetPoint point) {
  return RecordedTarget{.mode = mode, .point = point};
}

[[nodiscard]] std::int16_t Coordinate(int value) {
  return static_cast<std::int16_t>(value);
}

// Bone positions: x names the actor and bone, y and z the query kind.
[[nodiscard]] TargetPoint BonePoint(int x, int y_and_z, TargetPoint current) {
  current.x = Coordinate(x);
  current.y = Coordinate(y_and_z);
  current.z = Coordinate(y_and_z + 1);
  return current;
}

struct FakeActionHost final : battle::ActionTargetingHost {
  std::map<int, battle::TargetingActionView> actions;
  std::map<int, battle::TargetCategoryRules> categories;
  int action_id = 0;
  int primary = 0;
  bool primary_targets_position = false;
  int target_count = 0;
  std::map<int, RecordedTarget> targets;
  TargetPoint focus;
  bool player = false;
  int source_id = -1;
  int selected = battle::kNoPreviewSelected;
  int camera = 0;
  TargetPoint area_center;
  TargetPoint saved_center;
  std::array<battle::TargetingVolume, 2> volumes{};
  std::vector<TargetPreview> previews =
      std::vector<TargetPreview>(battle::kTargetPreviewCapacity);
  int preview_count = 0;
  std::array<std::set<int>, 2> outside_x;
  std::map<int, battle::TargetingActor> actors;
  std::map<int, std::uint32_t> state_flags;
  std::map<int, TargetPoint> placements;
  std::map<std::pair<int, int>, int> body_parts;
  std::vector<battle::EnvironmentTrigger> triggers;
  std::vector<battle::PlatformPoint> platforms;
  std::vector<battle::TargetChest> chests;
  std::map<std::pair<int, int>, int> floors;
  bool weapon_drawn = false;
  std::vector<std::string> calls;

  [[nodiscard]] battle::TargetingActionView Action(int id) const override {
    return ValueOr(actions, id, battle::TargetingActionView{});
  }
  [[nodiscard]] battle::TargetCategoryRules CategoryRules(
      int category) const override {
    return ValueOr(categories, category, battle::TargetCategoryRules{});
  }
  [[nodiscard]] int SelectionActionId() const override { return action_id; }
  void SetSelectionActionId(int id) override {
    action_id = id;
    calls.push_back(Call("ActionId", {id}));
  }
  void ResetSelection(int source_actor, int id) override {
    primary = source_actor;
    action_id = id;
    target_count = 0;
    targets.clear();
    calls.push_back(Call("Reset", {source_actor, id}));
  }
  [[nodiscard]] int PrimaryActor() const override { return primary; }
  [[nodiscard]] bool PrimaryTargetsPosition() const override {
    return primary_targets_position;
  }
  [[nodiscard]] int SelectionTargetCount() const override {
    return target_count;
  }
  void SetSelectionTargetCount(int count) override { target_count = count; }
  void SetSelectionActorTarget(int index, ActorLimbTarget target) override {
    targets[index] = ActorTarget(target.actor, target.limb);
  }
  void SetSelectionPositionTarget(int index, int mode,
                                  TargetPoint point) override {
    targets[index] = PositionTarget(mode, point);
  }
  [[nodiscard]] TargetPoint FocusPosition() const override { return focus; }
  void SetFocusPosition(TargetPoint point) override { focus = point; }
  [[nodiscard]] bool IsPlayerAction() const override { return player; }
  void SetIsPlayerAction(bool player_action) override {
    player = player_action;
  }
  void SetSourceActorId(int source) override { source_id = source; }
  [[nodiscard]] int SelectedPreview() const override { return selected; }
  void SetSelectedPreview(int index) override { selected = index; }
  void SetCameraPreview(int index) override { camera = index; }
  void ResetSelectionTimers() override { calls.push_back(Call("Timers")); }
  [[nodiscard]] TargetPoint AreaCenter() const override { return area_center; }
  void SetAreaCenter(TargetPoint point) override { area_center = point; }
  [[nodiscard]] TargetPoint SavedAreaCenter() const override {
    return saved_center;
  }
  void SetSavedAreaCenter(TargetPoint point) override { saved_center = point; }
  [[nodiscard]] battle::TargetingVolume Volume(
      TargetingVolumeSlot slot) const override {
    return volumes.at(static_cast<std::size_t>(slot));
  }
  void SetVolume(TargetingVolumeSlot slot,
                 const battle::TargetingVolume& volume) override {
    volumes.at(static_cast<std::size_t>(slot)) = volume;
  }
  [[nodiscard]] TargetPreview Preview(int index) const override {
    return previews.at(static_cast<std::size_t>(index));
  }
  void SetPreview(int index, const TargetPreview& preview) override {
    previews.at(static_cast<std::size_t>(index)) = preview;
  }
  [[nodiscard]] int PreviewCount() const override { return preview_count; }
  void SetPreviewCount(int count) override { preview_count = count; }
  [[nodiscard]] bool IsPointInVolume(TargetingVolumeSlot slot,
                                     TargetPoint point) const override {
    return !outside_x.at(static_cast<std::size_t>(slot)).contains(point.x);
  }
  [[nodiscard]] battle::ActionPreviewResult PreviewActionAgainstLimb(
      int id, int source_actor, int target_actor, int limb) override {
    calls.push_back(Call("PreviewHit", {id, source_actor, target_actor, limb}));
    return battle::ActionPreviewResult{.action_id = id,
                                       .damage_type = 2,
                                       .effect_variant = 3,
                                       .target_actor = target_actor,
                                       .target_hp = 40,
                                       .target_mp = 7,
                                       .hit_threshold = 75,
                                       .remove_statuses = 0xFFFFFFFF};
  }
  [[nodiscard]] std::optional<battle::TargetingActor> Actor(
      int slot) const override {
    const auto found = actors.find(slot);
    if (found == actors.end()) return std::nullopt;
    return found->second;
  }
  [[nodiscard]] std::uint32_t ActorStateFlags(int slot) const override {
    return ValueOr(state_flags, slot, std::uint32_t{0});
  }
  void SetActorDisplayFlags(int slot, std::uint32_t flags) override {
    state_flags[slot] = flags;
    calls.push_back(Call("Display", {slot, static_cast<int>(flags)}));
  }
  [[nodiscard]] std::optional<TargetPoint> ActorPlacement(
      int slot) const override {
    const auto found = placements.find(slot);
    if (found == placements.end()) return std::nullopt;
    return found->second;
  }
  [[nodiscard]] int FindBoneTag(int slot, int tag, int kind) const override {
    return kind == 2 ? ValueOr(body_parts, std::pair{slot, tag}, -1) : -1;
  }
  [[nodiscard]] int BoneTag(int slot, int bone, int kind) const override {
    return slot * 40 + bone + kind;
  }
  [[nodiscard]] TargetPoint LowGroupBonePosition(
      int slot, int limb, TargetPoint current) const override {
    return BonePoint(slot * 100 + 10 + limb, 1, current);
  }
  [[nodiscard]] TargetPoint TaggedBonePosition(
      int slot, int tag, TargetPoint current) const override {
    return BonePoint(slot * 100 + tag % 100, 3, current);
  }
  [[nodiscard]] TargetPoint BonePosition(int slot, int bone,
                                         TargetPoint current) const override {
    return BonePoint(slot * 100 + bone, 5, current);
  }
  void RestoreIdlePose(int slot) override {
    calls.push_back(Call("Idle", {slot}));
  }
  void FreezeActorAnimations() override { calls.push_back(Call("Freeze")); }
  [[nodiscard]] std::vector<battle::EnvironmentTrigger> RoomTriggers()
      const override {
    return triggers;
  }
  [[nodiscard]] std::vector<battle::PlatformPoint> MovingPlatformPoints()
      const override {
    return platforms;
  }
  [[nodiscard]] std::vector<battle::TargetChest> RoomChests() const override {
    return chests;
  }
  [[nodiscard]] int FloorHeightAt(int x, int z) const override {
    return ValueOr(floors, std::pair{x, z}, 0);
  }
  void InitSphere() override { calls.push_back(Call("InitSphere")); }
  void FreeSphere() override { calls.push_back(Call("FreeSphere")); }
  void ClearSphereTargets() override { calls.push_back(Call("ClearTargets")); }
  void ClearPrimarySphereVolume() override {
    calls.push_back(Call("ClearPrimary"));
  }
  void ClearSecondarySphereVolume() override {
    calls.push_back(Call("ClearSecondary"));
  }
  void ShowRangeVolume(bool with_area, int count) override {
    calls.push_back(Call("ShowRange", {with_area ? 1 : 0, count}));
  }
  void ShowAreaVolume() override { calls.push_back(Call("ShowArea")); }
  void ShowSphereTargets(int count, int mode) override {
    calls.push_back(Call("ShowTargets", {count, mode}));
  }
  void SetFieldControlMode(int mode) override {
    calls.push_back(Call("FieldMode", {mode}));
  }
  void SetActionFlowMode(int mode) override {
    calls.push_back(Call("Flow", {mode}));
  }
  [[nodiscard]] bool WeaponDrawn() const override { return weapon_drawn; }
  void SetHudBattleMode(int mode) override {
    calls.push_back(Call("Hud", {mode}));
  }
  void ResumeAfterActionSelection() override {
    calls.push_back(Call("Resume"));
  }
  void PauseFieldForMenu() override { calls.push_back(Call("PauseField")); }
  void ReleaseMenuRows() override { calls.push_back(Call("ReleaseRows")); }
  void BeginPostActionRecovery(int mode) override {
    calls.push_back(Call("Recovery", {mode}));
  }
  void CommitEnvironmentAction(int mode) override {
    calls.push_back(Call("Commit", {mode}));
  }
  void StorePreferredCameraDistance() override {
    calls.push_back(Call("CameraDistance"));
  }
  void ResetRiskPenaltyTimer() override { calls.push_back(Call("RiskTimer")); }
  void TransformRoomPalettes(
      const battle::RoomPaletteTransform& transform) override {
    calls.push_back(
        Call("Palettes", {transform.mode, transform.duration, transform.red,
                          transform.green, transform.blue}));
  }
  void SetMinimapVisible(bool visible) override {
    calls.push_back(Call("Minimap", {visible ? 1 : 0}));
  }
  void SetRoomAnimationsPaused(bool paused) override {
    calls.push_back(Call("RoomAnims", {paused ? 1 : 0}));
  }
  void SetMovingPlatformsPaused(bool paused) override {
    calls.push_back(Call("Platforms", {paused ? 1 : 0}));
  }
  void SetActorMarkersHidden(bool hidden) override {
    calls.push_back(Call("Markers", {hidden ? 1 : 0}));
  }
  void SetClutAnimationEnabled(bool enabled) override {
    calls.push_back(Call("Clut", {enabled ? 1 : 0}));
  }
};

// Action 5 (category 4, effect 0x3A) previewed against actor 3.
void SetUpPreviewAction(FakeActionHost& host) {
  host.actions[5] = battle::TargetingActionView{.category = 4, .effect = 0x3A};
  host.actors[3] = battle::TargetingActor{.enemy_class = 6};
}

constexpr battle::ActionPreviewResult kPreviewResult{
    .action_id = 5,
    .damage_type = 2,
    .effect_variant = 1,
    .target_actor = 0x13,
    .target_hp = 40,
    .target_mp = 7,
    .hit_threshold = 0x1004B,
    .remove_statuses = 0xFFFFFFFF,
};

TEST_CASE("Target preview takes the amount its effect names",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  SetUpPreviewAction(host);
  const battle::ActionPreviewResult result = kPreviewResult;
  TargetPreview preview{.preview_amount = 99};
  battle::PopulateActionTargetPreview(host, preview, result);
  CHECK(preview.preview_amount == 40);
  CHECK(preview.effect == 0x3A);
  CHECK(preview.hit_chance == 75);
  CHECK(preview.removed_statuses == 0x1FFFFFE0);
  CHECK(preview.action_kind == 4);
  CHECK(preview.damage_type == 2);
  CHECK(preview.effect_variant == 1);
  CHECK(preview.enemy_class == 6);
}

TEST_CASE("Target preview takes MP or keeps its amount by effect",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  SetUpPreviewAction(host);
  const battle::ActionPreviewResult result = kPreviewResult;
  TargetPreview preview{.preview_amount = 99};
  for (const int effect : {0x3B, 0x3F}) {
    host.actions[5].effect = effect;
    preview.preview_amount = 99;
    battle::PopulateActionTargetPreview(host, preview, result);
    CHECK(preview.preview_amount == 7);
  }
  host.actions[5].effect = 0x3E;
  battle::PopulateActionTargetPreview(host, preview, result);
  CHECK(preview.preview_amount == 40);
  host.actions[5].effect = 0x10;
  preview.preview_amount = 99;
  battle::PopulateActionTargetPreview(host, preview, result);
  CHECK(preview.preview_amount == 99);
  CHECK(preview.effect == 0x10);
}

TEST_CASE("Target preview keeps an absent actor's enemy class",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  SetUpPreviewAction(host);
  battle::ActionPreviewResult result = kPreviewResult;
  const BattleLog log;
  result.target_actor = 0x18;
  TargetPreview absent{.enemy_class = 9};
  battle::PopulateActionTargetPreview(host, absent, result);
  CHECK(absent.enemy_class == 9);
  CHECK(absent.hit_chance == 75);
  CHECK(log.Saw("previewed actor 8 is absent; enemy class kept"));
}

TEST_CASE("Returning to the action menu restores the menu flow",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.action_id = 9;
  host.weapon_drawn = true;
  battle::ReturnToBattleActionMenu(host);
  CHECK(host.action_id == 0);
  CHECK(host.calls == Calls{"FieldMode(5)", "ActionId(0)", "Flow(1)", "Hud(3)",
                            "Resume()", "PauseField()"});
  host.calls.clear();
  host.weapon_drawn = false;
  battle::ReturnToBattleActionMenu(host);
  CHECK(host.calls == Calls{"FieldMode(5)", "ActionId(0)", "Flow(1)", "Hud(2)",
                            "Resume()", "PauseField()"});
}

TEST_CASE("Area filter cancels when the area cursor was dismissed",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.action_id = 7;
  host.area_center = TargetPoint{.x = 1, .y = 2, .z = 3, .pad = 1};
  battle::FilterAreaActionTargets(host);
  CHECK(host.saved_center == host.area_center);
  CHECK(host.calls == Calls{"ClearPrimary()", "ClearTargets()",
                            "ClearPrimary()", "ClearSecondary()",
                            "FreeSphere()", "ReleaseRows()", "Recovery(0)"});
  host.calls.clear();
  host.actions[7].returns_to_menu_on_cancel = true;
  battle::FilterAreaActionTargets(host);
  CHECK(host.calls == Calls{"ClearPrimary()", "ClearTargets()",
                            "ClearPrimary()", "ClearSecondary()",
                            "FreeSphere()", "FieldMode(5)", "ActionId(0)",
                            "Flow(1)", "Hud(2)", "Resume()", "PauseField()"});
}

// A player's whole-actor area action: actor 3's preview lies outside the
// area and actor 9 is absent.
void SetUpAreaFilter(FakeActionHost& host) {
  host.action_id = 7;
  host.primary = 1;
  host.player = true;
  host.actions[7].category = 2;
  host.area_center = TargetPoint{.x = 10, .z = 20};
  host.volumes[kAreaSlot] = battle::TargetingVolume{.shape = 3, .kind = 1};
  host.actors[2] = battle::TargetingActor{.default_limb = 5, .enemy_class = 1};
  host.actors[4] = battle::TargetingActor{.default_limb = 1};
  host.previews[0] = TargetPreview{
      .position = {.x = 100}, .kind = 3, .actor_id = 2, .limb = 2};
  host.previews[1] = TargetPreview{.position = {.x = 200}, .actor_id = 3};
  host.previews[2] = TargetPreview{.position = {.x = 300}, .actor_id = 9};
  host.previews[3] = TargetPreview{.position = {.x = 400}, .actor_id = 4};
  host.preview_count = 4;
  host.outside_x[kAreaSlot] = {200};
}

TEST_CASE("Area filter rebuilds whole-actor targets inside the area",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  SetUpAreaFilter(host);
  battle::FilterAreaActionTargets(host);
  CHECK(host.volumes[kAreaSlot] ==
        battle::TargetingVolume{
            .shape = 3, .kind = 1, .origin = {.x = 10, .z = 20}});
  CHECK(host.preview_count == 2);
  CHECK(host.previews[0].actor_id == 2);
  CHECK(host.previews[0].kind == battle::kWholeActorTargetKind);
  CHECK(host.previews[0].limb == battle::kNoTargetLimb);
  CHECK(host.previews[0].enemy_class == 1);
  CHECK(host.previews[1].actor_id == 4);
  CHECK(host.previews[1].position.x == 400);
  CHECK(host.previews[1].kind == battle::kWholeActorTargetKind);
}

TEST_CASE("Area filter previews each actor it keeps",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  SetUpAreaFilter(host);
  const BattleLog log;
  battle::FilterAreaActionTargets(host);
  CHECK(log.Saw("area target actor 9 is absent"));
  CHECK(host.calls == Calls{"ClearPrimary()", "ShowArea()",
                            "PreviewHit(7,1,2,5)", "PreviewHit(7,1,4,1)",
                            "ShowTargets(2,1)", "Flow(3)"});
}

TEST_CASE("Area filter keeps body-part records inside the area",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.action_id = 7;
  host.actions[7].category = 2;
  host.categories[2].targets_body_parts = true;
  host.previews[0] = TargetPreview{
      .position = {.x = 100}, .kind = 7, .actor_id = 2, .limb = 3};
  host.previews[1] = TargetPreview{.position = {.x = 200}, .actor_id = 9};
  host.previews[2] = TargetPreview{
      .position = {.x = 300}, .kind = 8, .actor_id = 9, .limb = 1};
  host.preview_count = 3;
  host.outside_x[kAreaSlot] = {200};
  const BattleLog log;
  battle::FilterAreaActionTargets(host);
  CHECK(host.preview_count == 2);
  CHECK(host.previews[0].kind == 7);
  CHECK(host.previews[0].limb == 3);
  CHECK(host.previews[1].kind == 8);
  CHECK(host.previews[1].actor_id == 9);
  CHECK(log.Saw("previewed actor 9 is absent; enemy class kept"));
  CHECK(host.calls == Calls{"ClearPrimary()", "PreviewHit(7,0,2,3)",
                            "PreviewHit(7,0,9,1)", "ShowTargets(2,1)",
                            "Flow(3)"});
}

TEST_CASE("Aiming behind a source steps half a turn from its facing",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.placements[2] = TargetPoint{.x = 100, .y = -5, .z = 200, .pad = 1024};
  const TargetPoint focus{.x = 1, .y = 2, .z = 3, .pad = 77};
  const std::optional<TargetPoint> aimed =
      battle::AimActionBehindSource(host, 2, focus);
  REQUIRE(aimed);
  CHECK(*aimed == TargetPoint{.x = -3996, .y = -5, .z = 200, .pad = 77});
  host.placements[2].pad = 0;
  CHECK(battle::AimActionBehindSource(host, 2, focus) ==
        TargetPoint{.x = 100, .y = -5, .z = 200 - 4096, .pad = 77});
  const BattleLog log;
  CHECK_FALSE(battle::AimActionBehindSource(host, 5, focus));
  CHECK(log.Saw("source actor 5 has no placement; focus kept"));
}

TEST_CASE("Single target needs a selected preview",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.target_count = 9;
  const BattleLog log;
  battle::SetSingleActionTarget(host);
  CHECK(log.Saw("no preview is selected (255)"));
  host.selected = -1;
  battle::SetSingleActionTarget(host);
  CHECK(log.Saw("no preview is selected (-1)"));
  CHECK(host.target_count == 9);
}

TEST_CASE("Single target takes a selected position preview",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.previews[1] =
      TargetPreview{.position = {.x = 10, .y = -20, .z = 30}, .actor_id = 0x30};
  host.selected = 1;
  battle::SetSingleActionTarget(host);
  CHECK(host.target_count == 1);
  CHECK(host.targets[0] ==
        PositionTarget(4, {.x = 10, .y = -20, .z = 30, .pad = 0x30}));
  CHECK(host.focus == TargetPoint{.x = 10, .y = -20, .z = 30});
  host.previews[1].actor_id = 0x21;
  battle::SetSingleActionTarget(host);
  CHECK(host.targets[0] ==
        PositionTarget(1, {.x = 10, .y = -20, .z = 30, .pad = 0x21}));
}

TEST_CASE("Single target takes a selected actor preview",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  const BattleLog log;
  host.previews[2] = TargetPreview{
      .position = {.x = 5, .y = 6, .z = 7}, .actor_id = 3, .limb = -1};
  host.actors[3].default_limb = 4;
  host.selected = 2;
  battle::SetSingleActionTarget(host);
  CHECK(host.target_count == 1);
  CHECK(host.targets[0] == ActorTarget(3, 4));
  CHECK(host.focus == TargetPoint{.x = 5, .y = 6, .z = 7});
  host.previews[2].limb = 2;
  battle::SetSingleActionTarget(host);
  CHECK(host.targets[0] == ActorTarget(3, 2));
  host.previews[2].actor_id = 6;
  host.previews[2].limb = -1;
  battle::SetSingleActionTarget(host);
  CHECK(host.targets[0] == ActorTarget(6, -1));
  CHECK(log.Saw("target actor 6 is absent; its default limb is unknown"));
}

TEST_CASE("Single target on the acting actor aims behind it",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.primary = 3;
  host.selected = 0;
  host.previews[0] =
      TargetPreview{.position = {.x = 5}, .actor_id = 3, .limb = 1};
  host.placements[3] = TargetPoint{.x = 100, .y = -5, .z = 200, .pad = 1024};
  host.focus = TargetPoint{.pad = 9};
  battle::SetSingleActionTarget(host);
  CHECK(host.targets[0] == ActorTarget(3, 1));
  CHECK(host.focus == TargetPoint{.x = -3996, .y = -5, .z = 200, .pad = 9});
  host.primary_targets_position = true;
  battle::SetSingleActionTarget(host);
  CHECK(host.focus == TargetPoint{.x = 5});
}

TEST_CASE("All targets take every preview and focus the command point",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.previews[0] =
      TargetPreview{.position = {.x = 1, .y = 2, .z = 3}, .actor_id = 0x30};
  host.previews[1] = TargetPreview{.actor_id = 2, .limb = 1};
  host.preview_count = 2;
  host.saved_center = TargetPoint{.x = 7, .y = 8, .z = 9};
  battle::SetAllActionTargets(host);
  CHECK(host.target_count == 2);
  CHECK(host.targets[0] ==
        PositionTarget(1, {.x = 1, .y = 2, .z = 3, .pad = 0x30}));
  CHECK(host.targets[1] == ActorTarget(2, 1));
  CHECK(host.focus == host.saved_center);
  host.primary = 2;
  host.actors[2].command_point = TargetPoint{.x = 4, .y = 5, .z = 6, .pad = 7};
  battle::SetAllActionTargets(host);
  CHECK(host.focus == TargetPoint{.x = 4, .y = 5, .z = 6, .pad = 7});
}

TEST_CASE("All targets keep the focus and cap the preview count",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.focus = TargetPoint{.x = 4, .y = 5, .z = 6, .pad = 7};
  host.preview_count = 2;
  const BattleLog log;
  host.primary = 5;
  battle::SetAllActionTargets(host);
  CHECK(log.Saw("source actor 5 is absent; focus kept"));
  CHECK(host.focus == TargetPoint{.x = 4, .y = 5, .z = 6, .pad = 7});
  host.primary = 0;
  host.preview_count = 300;
  battle::SetAllActionTargets(host);
  CHECK(host.target_count == battle::kTargetPreviewCapacity);
  CHECK(log.Saw("preview count 300 exceeds capacity 255"));
}

// Three room triggers (two active) and actor 0 facing +x.
void SetUpEnvironment(FakeActionHost& host) {
  host.triggers = {{.kind = 3, .tile_x = 2, .tile_z = 3},
                   {.kind = 16, .tile_x = 5, .tile_z = 5},
                   {.kind = 0, .tile_x = 0, .tile_z = 1}};
  host.floors[{320, 448}] = 0x7FF0;
  host.floors[{64, 192}] = 0x10050;
  host.placements[0] = TargetPoint{.x = 100, .y = -5, .z = 200, .pad = 1024};
}

TEST_CASE("Environment actions target active triggers",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  SetUpEnvironment(host);
  host.action_id = 93;
  battle::CollectEnvironmentActionTargets(host);
  CHECK(host.target_count == 2);
  CHECK(host.targets[0] ==
        PositionTarget(3, {.x = 320, .y = -16, .z = 448, .pad = 128}));
  CHECK(host.targets[1] ==
        PositionTarget(3, {.x = 64, .y = 0x50, .z = 192, .pad = 128}));
  CHECK(host.focus == TargetPoint{.x = -3996, .y = -5, .z = 200});
  host.action_id = 241;
  host.targets.clear();
  battle::CollectEnvironmentActionTargets(host);
  CHECK(host.target_count == 2);
}

TEST_CASE("Environment actions target platforms or need a placement",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  SetUpEnvironment(host);
  host.action_id = 79;
  host.platforms = {{.x = 10, .height = 20, .z = 30}};
  host.targets.clear();
  battle::CollectEnvironmentActionTargets(host);
  CHECK(host.target_count == 1);
  CHECK(host.targets[0] ==
        PositionTarget(5, {.x = 10, .y = 20, .z = 30, .pad = 64}));
  host.action_id = 12;
  host.placements.clear();
  const BattleLog log;
  battle::CollectEnvironmentActionTargets(host);
  CHECK(host.target_count == 0);
  CHECK(log.Saw("source actor 0 has no placement; focus kept"));
}

// A player's source-area action: actors 1 and 5 are in reach, actor 2 is not
// targetable, actor 3 is hidden, actor 4 is excluded and 5 lies outside.
void SetUpSourceArea(FakeActionHost& host) {
  host.action_id = 9;
  host.player = true;
  host.actions[9] = battle::TargetingActionView{
      .category = 1, .range = {.x = 2, .y = 3, .z = 4, .shape = 5}};
  host.categories[1] =
      battle::TargetCategoryRules{.area_target = true, .actor_mask = 1};
  host.actors[0] = battle::TargetingActor{.kind = 1};
  host.actors[1] = battle::TargetingActor{.kind = 1, .default_limb = 2};
  host.actors[2] = battle::TargetingActor{.kind = 2};
  host.actors[3] = battle::TargetingActor{.kind = 1};
  host.actors[4] = battle::TargetingActor{.kind = 0x8001};
  host.actors[5] = battle::TargetingActor{.kind = 1};
  for (const int slot : {0, 1, 2, 4, 5}) host.state_flags[slot] = 1;
  host.state_flags[3] = 0x1000001;
  host.outside_x[kAreaSlot] = {555};
}

TEST_CASE("Source area actions aim the area from the root bone",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  SetUpSourceArea(host);
  battle::CollectSourceAreaActionTargets(host);
  CHECK(host.volumes[kAreaSlot] ==
        battle::TargetingVolume{.shape = 5,
                                .kind = 1,
                                .x = 2,
                                .y = 3,
                                .z = 4,
                                .angle = 64,
                                .origin = {.y = 5, .z = 6}});
  CHECK(host.selected == 240);
  CHECK(host.calls == Calls{"ShowArea()", "PreviewHit(9,0,1,2)",
                            "ShowTargets(1,1)", "Flow(3)"});
}

TEST_CASE("Source area actions preview the whole actors in reach",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  SetUpSourceArea(host);
  battle::CollectSourceAreaActionTargets(host);
  CHECK(host.preview_count == 1);
  CHECK(host.previews[0].actor_id == 1);
  CHECK(host.previews[0].kind == battle::kWholeActorTargetKind);
  CHECK(host.previews[0].limb == battle::kNoTargetLimb);
  CHECK(host.previews[0].position == TargetPoint{.x = 155, .y = 3, .z = 4});
}

TEST_CASE("Source area actions face the commanded point",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.primary = 2;
  const TargetPoint command{.x = 300, .y = 105, .z = 106, .pad = 1};
  host.actors[2] = battle::TargetingActor{.command_point = command};
  battle::CollectSourceAreaActionTargets(host);
  CHECK(host.focus == command);
  const battle::TargetingVolume& area = host.volumes[kAreaSlot];
  CHECK(area.origin == TargetPoint{.x = 200, .y = 5, .z = 6});
  CHECK(area.facing == VS::Core::Math::Arctangent2(100, 100));
  CHECK(area.angle == VS::Core::Math::Arctangent2(
                          100, VS::Core::Math::SquareRootInteger(20000)) /
                              16 +
                          64);
  CHECK(area.angle != 64);
  host.primary = 7;
  const BattleLog log;
  battle::CollectSourceAreaActionTargets(host);
  CHECK(log.Saw("source actor 7 is absent; area level"));
  CHECK(host.volumes[kAreaSlot].facing == 0);
  CHECK(host.volumes[kAreaSlot].angle == 64);
}

// The direct-target scene: actor 0 attacks with its weapon range; actor 1
// stands in range and four chests lie in the room.
void SetUpDirectTargets(FakeActionHost& host) {
  host.action_id = 11;
  host.player = true;
  host.actions[11] = battle::TargetingActionView{
      .category = 3, .range = {.x = 0xFF, .shape = 2, .angle = 9}};
  host.categories[3] =
      battle::TargetCategoryRules{.needs_target_selection = true,
                                  .includes_acting_actor = true,
                                  .actor_mask = 0x81};
  host.actors[0] = battle::TargetingActor{
      .kind = 1,
      .risk = 50,
      .weapon_range = {.x = 1, .y = 2, .z = 3, .angle = 4}};
  host.actors[1] = battle::TargetingActor{.kind = 1, .default_limb = 3};
  host.state_flags[0] = 1;
  host.state_flags[1] = 1;
  host.placements[0] = TargetPoint{.pad = 512};
  host.volumes[kRangeSlot].origin.pad = 8;
  host.chests = {{.tile_x = 1, .tile_z = 1, .lock_level = 2},
                 {.tile_x = 2, .tile_z = 2, .opened_flag = 64, .lock_level = 3},
                 {.tile_x = 3, .tile_z = 3, .lock_level = 4},
                 {.tile_x = 4, .tile_z = 4, .lock_level = 3, .is_open = true}};
  host.floors[{192, 192}] = 0x7FF0;
}

// Collects the direct-target scene over a stale chest preview record.
void CollectDirectTargets(FakeActionHost& host) {
  SetUpDirectTargets(host);
  host.previews[2].effect = 5;
  host.previews[2].preview_amount = 9;
  battle::CollectDirectActionTargets(host, 2);
}

TEST_CASE("Direct targets fill the range with actors and chests",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  CollectDirectTargets(host);
  CHECK(host.source_id == 2);
  CHECK(host.volumes[kRangeSlot] ==
        battle::TargetingVolume{.shape = 2,
                                .kind = 1,
                                .facing = 512,
                                .x = 1,
                                .y = 2,
                                .z = 3,
                                .angle = 32,
                                .origin = {.x = 50, .y = 3, .z = 4, .pad = 8}});
  REQUIRE(host.preview_count == 5);
  CHECK(host.previews[0].actor_id == 0);
  CHECK(host.previews[1].actor_id == 1);
  CHECK(host.selected == battle::kNoPreviewSelected);
  CHECK(host.calls == Calls{"ShowRange(0,0)", "PreviewHit(11,0,0,0)",
                            "PreviewHit(11,0,1,3)", "ShowTargets(5,0)",
                            "Flow(1)"});
}

TEST_CASE("Direct targets preview a closed chest with its risk",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  CollectDirectTargets(host);
  REQUIRE(host.preview_count == 5);
  const TargetPreview& chest = host.previews[2];
  CHECK(chest.position == TargetPoint{.x = 192, .y = -16, .z = 192});
  CHECK(chest.kind == battle::kWholeActorTargetKind);
  CHECK(chest.limb == 16);
  CHECK(chest.actor_id == 0x20);
  CHECK(chest.hit_chance == 78);
  CHECK(chest.action_kind == 10);
  CHECK(chest.effect == 0);
  CHECK(chest.preview_amount == 0);
}

TEST_CASE("Direct targets give locked or opened chests no hit chance",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  CollectDirectTargets(host);
  REQUIRE(host.preview_count == 5);
  CHECK(host.previews[3].actor_id == 0x22);
  CHECK(host.previews[3].hit_chance == 0);
  CHECK(host.previews[4].actor_id == 0x23);
  CHECK(host.previews[4].hit_chance == 0);
}

TEST_CASE("Direct targets follow the category mask and action range",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  CollectDirectTargets(host);
  host.categories[3].actor_mask = 1;
  host.categories[3].includes_acting_actor = false;
  host.outside_x[kRangeSlot] = {155};
  host.actions[11].range = battle::RangeShape{.x = 6, .y = 7, .z = 8};
  battle::CollectDirectActionTargets(host, 2);
  CHECK(host.preview_count == 0);
  CHECK(host.volumes[kRangeSlot].x == 6);
  CHECK(host.volumes[kRangeSlot].angle == 0);
}

TEST_CASE("Direct targets select the commanded actor",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.primary = 4;
  const BattleLog log;
  battle::CollectDirectActionTargets(host, 2);
  CHECK(log.Saw("attacker 4 is absent; direct targets not collected"));
  host.actors[4] =
      battle::TargetingActor{.command_actor = 1, .command_limb = -1};
  battle::CollectDirectActionTargets(host, 2);
  CHECK(host.calls.empty());
  CHECK(host.source_id == -1);
  host.placements[4] = TargetPoint{};
  host.action_id = 11;
  host.actions[11] =
      battle::TargetingActionView{.category = 3, .range = {.x = 1}};
  host.categories[3] = battle::TargetCategoryRules{
      .needs_target_selection = true, .actor_mask = 1};
  for (const int slot : {1, 2, 6}) {
    host.actors[slot] = battle::TargetingActor{.kind = 1};
    host.state_flags[slot] = 1;
  }
  battle::CollectDirectActionTargets(host, 2);
  CHECK(host.preview_count == 3);
  CHECK(host.selected == 0);
  CHECK(host.calls == Calls{"PreviewHit(11,4,1,0)", "PreviewHit(11,4,2,0)",
                            "PreviewHit(11,4,6,0)", "Flow(1)"});
  host.selected = battle::kNoPreviewSelected;
  host.actors[4].command_actor = 6;
  host.actors[4].command_limb = 3;
  battle::CollectDirectActionTargets(host, 2);
  CHECK(host.selected == 2);
}

TEST_CASE("Direct body-part targets list each exposed limb in range",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.primary = 3;
  host.action_id = 11;
  host.actions[11] =
      battle::TargetingActionView{.category = 3, .range = {.x = 1}};
  host.categories[3] =
      battle::TargetCategoryRules{.needs_target_selection = true,
                                  .targets_body_parts = true,
                                  .actor_mask = 1};
  host.actors[3] =
      battle::TargetingActor{.kind = 2, .command_actor = 1, .command_limb = 7};
  host.placements[3] = TargetPoint{};
  host.actors[1] = battle::TargetingActor{.kind = 1};
  host.state_flags[1] = 1;
  host.body_parts[{1, 0}] = 4;
  host.body_parts[{1, 2}] = 5;
  host.body_parts[{1, 5}] = 6;
  host.outside_x[kRangeSlot] = {115};
  battle::CollectDirectActionTargets(host, 3);
  REQUIRE(host.preview_count == 2);
  CHECK(host.previews[0] ==
        TargetPreview{.position = {.x = 110, .y = 1, .z = 2},
                      .kind = 44,
                      .actor_id = 1,
                      .limb = 0,
                      .effect = 0,
                      .preview_amount = 0,
                      .hit_chance = 75,
                      .removed_statuses = 0x1FFFFFE0,
                      .action_kind = 3,
                      .damage_type = 2,
                      .effect_variant = 3,
                      .enemy_class = 0});
  CHECK(host.previews[1].kind == 45);
  CHECK(host.previews[1].limb == 2);
  CHECK(host.selected == 1);
  CHECK(host.calls ==
        Calls{"PreviewHit(11,3,1,0)", "PreviewHit(11,3,1,2)", "Flow(1)"});
  host.selected = battle::kNoPreviewSelected;
  host.actors[3].command_limb = 0;
  battle::CollectDirectActionTargets(host, 3);
  CHECK(host.selected == 0);
}

TEST_CASE("Ground area actions place targets without a volume test",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.primary = 1;
  host.player = true;
  host.action_id = 13;
  host.actions[13] = battle::TargetingActionView{
      .category = 5,
      .range = {.x = 0xFF, .y = 2, .z = 3, .shape = 1, .angle = 2},
      .area = {.x = 5, .y = 6, .z = 7, .shape = 4, .angle = 1}};
  host.categories[5] = battle::TargetCategoryRules{
      .needs_target_selection = true, .area_target = true, .actor_mask = 1};
  host.actors[0] = battle::TargetingActor{.kind = 1};
  host.actors[1] = battle::TargetingActor{
      .kind = 1,
      .weapon_range = {.x = 40},
      .command_point = {.x = 7, .y = 8, .z = 9, .pad = 3}};
  host.actors[12] = battle::TargetingActor{.kind = 1};
  for (const int slot : {0, 1, 12}) host.state_flags[slot] = 1;
  host.placements[1] = TargetPoint{.pad = 100};
  host.volumes[kAreaSlot] =
      battle::TargetingVolume{.kind = 6, .facing = 5, .origin = {.x = 9}};
  host.previews[0].kind = 99;
  host.outside_x = {std::set<int>{55}, std::set<int>{55}};
  battle::CollectGroundAreaActionTargets(host, 0);
  CHECK(host.source_id == 0);
  CHECK(host.volumes[kRangeSlot] ==
        battle::TargetingVolume{.shape = 1,
                                .kind = 2,
                                .facing = 100,
                                .x = 0xFF,
                                .y = 2,
                                .z = 3,
                                .angle = 16,
                                .origin = {.x = 150, .y = 3, .z = 4}});
  CHECK(host.volumes[kAreaSlot] == battle::TargetingVolume{.shape = 4,
                                                           .kind = 6,
                                                           .x = 5,
                                                           .y = 6,
                                                           .z = 7,
                                                           .angle = 8,
                                                           .origin = {.x = 9}});
  CHECK(host.preview_count == 1);
  CHECK(host.previews[0] == TargetPreview{.position = {.x = 55, .y = 3, .z = 4},
                                          .kind = 99,
                                          .actor_id = 0,
                                          .limb = -1});
  CHECK(host.area_center == TargetPoint{.x = 7, .y = 8, .z = 9});
  CHECK(host.calls == Calls{"ShowRange(1,1)", "Flow(2)"});
}

TEST_CASE("Ground area body parts and an absent source",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.primary = 1;
  host.action_id = 13;
  host.actions[13] = battle::TargetingActionView{.category = 5};
  host.categories[5] =
      battle::TargetCategoryRules{.needs_target_selection = true,
                                  .area_target = true,
                                  .targets_body_parts = true,
                                  .actor_mask = 1};
  host.actors[0] = battle::TargetingActor{.kind = 1};
  host.actors[1] = battle::TargetingActor{.kind = 1};
  host.state_flags[0] = 1;
  host.state_flags[1] = 1;
  host.placements[1] = TargetPoint{};
  host.body_parts[{0, 1}] = 2;
  battle::CollectGroundAreaActionTargets(host, 1);
  CHECK(host.source_id == 1);
  CHECK(host.preview_count == 1);
  CHECK(host.previews[0] == TargetPreview{.position = {.x = 11, .y = 1, .z = 2},
                                          .kind = 2,
                                          .actor_id = 0,
                                          .limb = 1});
  CHECK(host.area_center == TargetPoint{});
  CHECK(host.calls == Calls{"Flow(2)"});
  host.primary = 4;
  const BattleLog log;
  battle::CollectGroundAreaActionTargets(host, 1);
  CHECK(log.Saw("source actor 4 is absent; ground area not collected"));
}

TEST_CASE("Action zero shows the fixed test volumes",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.placements[0] = TargetPoint{.pad = 77};
  host.volumes[kAreaSlot].facing = 9;
  host.body_parts[{1, 1}] = 3;
  const BattleLog log;
  battle::InitializeActionTargeting(host, 0);
  CHECK(log.Saw("actor 1 has no placement; facing kept"));
  CHECK(host.volumes[kRangeSlot] ==
        battle::TargetingVolume{.shape = 1,
                                .kind = 3,
                                .facing = 77,
                                .x = 4,
                                .y = 4,
                                .z = 4,
                                .origin = {.x = 50, .y = 3, .z = 4}});
  CHECK(host.volumes[kAreaSlot] ==
        battle::TargetingVolume{.shape = 2,
                                .kind = 3,
                                .facing = 9,
                                .x = 4,
                                .y = 4,
                                .z = 4,
                                .origin = {.x = 150, .y = 3, .z = 4}});
  CHECK(host.preview_count == 1);
  CHECK(host.previews[0] ==
        TargetPreview{.position = {.x = 111, .y = 1, .z = 2},
                      .kind = 43,
                      .actor_id = 1,
                      .limb = 1});
  CHECK(host.calls == Calls{"ActionId(0)", "ClearTargets()", "ClearPrimary()",
                            "ClearSecondary()", "ShowRange(0,0)", "ShowArea()",
                            "ShowTargets(1,0)", "Flow(1)"});
}

struct DispatchCase {
  battle::TargetCategoryRules rules;
  int source_id = -1;
  std::string_view last_call;
};

TEST_CASE("Action targeting dispatches on the target category",
          "[battle][battle-targeting]") {
  const std::array<DispatchCase, 6> cases{{
      {{.needs_target_selection = true,
        .area_target = true,
        .targets_body_parts = true},
       1,
       "Flow(2)"},
      {{.needs_target_selection = true, .area_target = true}, 0, "Flow(2)"},
      {{.needs_target_selection = true, .targets_body_parts = true},
       3,
       "Flow(1)"},
      {{.needs_target_selection = true}, 2, "Flow(1)"},
      {{.area_target = true}, -1, "Flow(3)"},
      {{}, -1, "Commit(6)"},
  }};
  for (const DispatchCase& entry : cases) {
    FakeActionHost host;
    host.actors[0] = battle::TargetingActor{};
    host.placements[0] = TargetPoint{};
    host.actions[20].category = 1;
    host.categories[1] = entry.rules;
    battle::InitializeActionTargeting(host, 20);
    CHECK(host.source_id == entry.source_id);
    REQUIRE_FALSE(host.calls.empty());
    CHECK(host.calls.front() == "ActionId(20)");
    CHECK(host.calls.back() == entry.last_call);
  }
}

// The player (actor 0) begins action 50 aimed at actor 5's limb 2.
void BeginPlayerSelection(FakeActionHost& host) {
  host.actors[0] =
      battle::TargetingActor{.command_target_byte = 5, .command_limb_byte = 2};
  host.state_flags[0] = 4;
  host.state_flags[2] = 5;
  host.state_flags[3] = 2;
  host.previews[0].kind = 7;
  host.target_count = 3;
  battle::BeginActorActionSelection(host, 0, 50);
}

TEST_CASE("Action selection prepares the player's targeting view",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  BeginPlayerSelection(host);
  CHECK(host.calls == Calls{"FieldMode(2)",     "Reset(0,50)",
                            "CameraDistance()", "RiskTimer()",
                            "Timers()",         "Idle(0)",
                            "InitSphere()",     "Palettes(13,4,-4,-4,0)",
                            "Freeze()",         "Display(2,1)",
                            "Minimap(0)",       "RoomAnims(1)",
                            "Platforms(1)",     "Markers(1)",
                            "ActionId(50)",     "ClearTargets()",
                            "ClearPrimary()",   "ClearSecondary()",
                            "Commit(6)",        "Clut(0)"});
  CHECK(host.player);
  CHECK(host.selected == battle::kNoPreviewSelected);
  CHECK(host.camera == battle::kNoPreviewSelected);
}

TEST_CASE("Action selection seeds the first preview from the command",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  BeginPlayerSelection(host);
  CHECK(host.previews[0].actor_id == 5);
  CHECK(host.previews[0].limb == 2);
  CHECK(host.previews[0].kind == 7);
  CHECK(host.target_count == 0);
  CHECK(host.state_flags[0] == 4);
  CHECK(host.state_flags[2] == 1);
}

TEST_CASE("Enemy action selection skips the player's view",
          "[battle][battle-targeting]") {
  FakeActionHost host;
  host.player = true;
  host.actors[4] = battle::TargetingActor{.command_target_byte = 0x21,
                                          .command_limb_byte = 0xFF};
  battle::BeginActorActionSelection(host, 4, 50);
  CHECK(host.calls == Calls{"FieldMode(2)", "Reset(4,50)", "CameraDistance()",
                            "RiskTimer()", "Timers()", "Idle(4)", "Freeze()",
                            "RoomAnims(1)", "Platforms(1)", "ActionId(50)",
                            "ClearTargets()", "ClearPrimary()",
                            "ClearSecondary()", "Commit(6)", "Clut(0)"});
  CHECK_FALSE(host.player);
  CHECK(host.previews[0].actor_id == 0x21);
  CHECK(host.previews[0].limb == -1);
  host.calls.clear();
  const BattleLog log;
  battle::BeginActorActionSelection(host, 3, 50);
  CHECK(log.Saw("action selection refused: actor 3 is absent"));
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------------------
// Target selection.

constexpr std::uint32_t kL2 = 1u << 0;
constexpr std::uint32_t kCross = 1u << 6;
constexpr std::uint32_t kUp = 1u << 12;
constexpr std::uint32_t kRight = 1u << 13;
constexpr std::uint32_t kDown = 1u << 14;
constexpr std::uint32_t kLeft = 1u << 15;
constexpr std::size_t kCandidates = 0;
constexpr std::size_t kAreaTargets = 1;

// A sphere and target list in memory. The calls that draw, play a sound,
// highlight an actor or show text are recorded.
struct FakeSelectionHost final : battle::TargetSelectionHost {
  std::array<battle::SphereVolume, 2> volumes{
      battle::SphereVolume{.kind = 2,
                           .extents = {0, 4, 0, 0},
                           .position = {.x = 4000, .y = -200, .z = 4000}}};
  battle::SphereCursor cursor;
  int wire_shape = 0;
  int camera_yaw = 0;
  std::array<std::vector<TargetPreview>, 2> lists;
  int selected = battle::kNoPreviewSelected;
  bool all_targets = false;
  battle::HighlightedTarget highlighted;
  std::uint32_t held = 0;
  std::uint32_t pressed = 0;
  std::uint32_t repeated = 0;
  battle::StickInput stick;
  battle::SphereBasis basis{.m00 = 4096, .m22 = 4096};
  int map_dimensions = 0x04000400;
  int step = 0;
  battle::TargetColumns columns;
  std::array<battle::TargetSelectionLabel, battle::kTargetLabelCount> labels{};
  bool labels_settled = false;
  bool page_buttons = false;
  int page_offset = 0;
  std::map<int, int> actor_kinds;
  std::map<int, battle::TargetText> texts;
  std::map<int, int> integers;
  Calls calls;

  [[nodiscard]] battle::SphereVolume Volume(int index) const override {
    return volumes.at(static_cast<std::size_t>(index));
  }
  void SetVolume(int index, const battle::SphereVolume& volume) override {
    volumes.at(static_cast<std::size_t>(index)) = volume;
  }
  [[nodiscard]] battle::SphereCursor Cursor() const override { return cursor; }
  void SetCursor(const battle::SphereCursor& value) override { cursor = value; }
  [[nodiscard]] int WireShape() const override { return wire_shape; }
  [[nodiscard]] int CameraYaw() const override { return camera_yaw; }
  [[nodiscard]] int TargetCount() const override {
    return static_cast<int>(lists[kCandidates].size());
  }
  [[nodiscard]] int SecondaryTargetCount() const override {
    return static_cast<int>(lists[kAreaTargets].size());
  }
  [[nodiscard]] int SelectedTarget() const override { return selected; }
  void SetSelectedTarget(int index) override { selected = index; }
  [[nodiscard]] bool AllTargetsMode() const override { return all_targets; }
  [[nodiscard]] battle::HighlightedTarget Highlighted() const override {
    return highlighted;
  }
  void SetHighlighted(battle::HighlightedTarget target) override {
    highlighted = target;
    calls.push_back(Call("Highlighted", {target.group, target.pulse_slot}));
  }
  void SetHighlightTint(int rgb) override {
    calls.push_back(Call("HighlightTint", {rgb}));
  }
  [[nodiscard]] TargetPreview SphereTarget(int list, int index) const override {
    return lists.at(static_cast<std::size_t>(list))
        .at(static_cast<std::size_t>(index));
  }
  void BuildCameraBasis() override { calls.push_back(Call("CameraBasis")); }
  [[nodiscard]] battle::SphereBasis BuildVolumeTransform() override {
    calls.push_back(Call("Transform"));
    return basis;
  }
  void CacheTerrainRow(int row) override {
    calls.push_back(Call("TerrainRow", {row}));
  }
  void UpdateTargetHighlights() override {
    calls.push_back(Call("Highlights"));
  }
  void DrawTerrainIntersection(TargetPoint point, int scale) override {
    calls.push_back(Call("Terrain", {point.x, point.y, point.z, scale}));
  }
  [[nodiscard]] int CollisionMapDimensions() const override {
    return map_dimensions;
  }
  void DrawPositionCursor(TargetPoint point, bool lock_pitch) override {
    calls.push_back(
        Call("Cursor", {point.x, point.y, point.z, lock_pitch ? 1 : 0}));
  }
  void ClearPrimarySphereVolume() override {
    calls.push_back(Call("ClearPrimary"));
  }
  void SetReuseSphereVolumes(bool reuse) override {
    calls.push_back(Call("Reuse", {reuse ? 1 : 0}));
  }
  void DisablePalettePulse(int actor, int pulse_slot) override {
    calls.push_back(Call("Unpulse", {actor, pulse_slot}));
  }
  void EnablePalettePulse(int actor, int pulse_flags, int pulse_slot) override {
    calls.push_back(Call("Pulse", {actor, pulse_flags, pulse_slot}));
  }
  void SetActorTint(int actor, bool tinted) override {
    calls.push_back(Call("Tint", {actor, tinted ? 1 : 0}));
  }
  [[nodiscard]] std::optional<int> ActorKind(int actor) const override {
    const auto found = actor_kinds.find(actor);
    if (found == actor_kinds.end()) return std::nullopt;
    return found->second;
  }
  [[nodiscard]] std::uint32_t ButtonsHeld() const override { return held; }
  [[nodiscard]] std::uint32_t ButtonsPressed() const override {
    return pressed;
  }
  [[nodiscard]] std::uint32_t ButtonsRepeated() const override {
    return repeated;
  }
  [[nodiscard]] battle::StickInput RightStick() const override { return stick; }
  void PlayLeaveSound() override { calls.push_back(Call("Leave")); }
  void PlaySelectSound() override { calls.push_back(Call("Select")); }
  void PlayChangeSound() override { calls.push_back(Call("Change")); }
  [[nodiscard]] int SelectionStep() const override { return step; }
  void SetSelectionStep(int value) override { step = value; }
  [[nodiscard]] battle::TargetColumns Columns() const override {
    return columns;
  }
  void SetColumns(const battle::TargetColumns& value) override {
    columns = value;
    calls.push_back(Call("Columns"));
  }
  [[nodiscard]] battle::TargetSelectionLabel Label(int index) const override {
    return labels.at(static_cast<std::size_t>(index));
  }
  void SetLabel(int index, const battle::TargetSelectionLabel& label) override {
    labels.at(static_cast<std::size_t>(index)) = label;
  }
  void SetLabelText(int index, battle::TargetLabelText text) override {
    calls.push_back(Call("LabelText", {index, text.limb_name,
                                       text.skip_first_character ? 1 : 0}));
  }
  [[nodiscard]] bool LabelsSettled() const override { return labels_settled; }
  [[nodiscard]] bool HandlePageButtons() override {
    calls.push_back(Call("Page"));
    return page_buttons;
  }
  [[nodiscard]] int MapPageIndex(int index) const override {
    return index + page_offset;
  }
  void InitInformationBox() override { calls.push_back(Call("InitInfo")); }
  void ShowNoTargetsMessage() override { calls.push_back(Call("NoTargets")); }
  void SetContextText(int slot, battle::TargetText text) override {
    texts[slot] = text;
  }
  void SetContextInteger(int slot, int value) override {
    integers[slot] = value;
  }
  void ShowTargetInfo() override { calls.push_back(Call("ShowInfo")); }
  void DismissInformationBox() override {
    calls.push_back(Call("DismissInfo"));
  }
};

TEST_CASE("Sphere cursor rests on the volume while the wire shape draws",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.wire_shape = 3;
  host.volumes[0].position = {.x = 10, .y = -20, .z = 30};
  TargetPoint target{.x = 1, .y = 2, .z = 3, .pad = 7};
  CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
  CHECK(target == TargetPoint{.x = 10, .y = -20, .z = 30});
  CHECK(host.calls == Calls{"TerrainRow(2)"});
}

TEST_CASE("Sphere cursor cancels on Cross before it locks",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.cursor = {.y = -40};
  host.pressed = kCross;
  TargetPoint target;
  CHECK(battle::UpdateSphereTargetPosition(host, target));
  CHECK(target.pad == 1);
  CHECK(host.cursor == battle::SphereCursor{.y = -40});
  CHECK(host.calls == Calls{"CameraBasis()", "Leave()"});
}

TEST_CASE("Sphere cursor locks on Circle and drags the area volume",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.volumes[0].position = {.x = 100, .y = -200, .z = 300};
  host.volumes[1].kind = 0x83;
  host.volumes[1].range_kind = 2;
  host.cursor = {.x = 64, .y = -40, .z = -32};
  host.held = kUp;
  host.pressed = kCircle;
  TargetPoint target;
  CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
  CHECK(host.cursor ==
        battle::SphereCursor{.x = 64, .y = -48, .z = -32, .locked = 1});
  CHECK(target == TargetPoint{.x = 164, .y = -248, .z = 268});
  CHECK(host.volumes[0].kind == 0x82);
  CHECK(host.volumes[1] == battle::SphereVolume{.kind = 3, .position = target});
  CHECK(host.calls == Calls{"CameraBasis()", "Select()", "Transform()",
                            "Terrain(100,-248,300,4096)",
                            "Cursor(164,-248,268,1)"});
}

TEST_CASE("Locked sphere cursor confirms on Circle",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.cursor = {.y = -40, .locked = 1};
  host.volumes[1].kind = 0x82;
  host.pressed = kCircle;
  TargetPoint target;
  CHECK(battle::UpdateSphereTargetPosition(host, target));
  CHECK(target == TargetPoint{.x = 4000, .y = -240, .z = 4000});
  CHECK(host.volumes[1] == battle::SphereVolume{.kind = 0x82});
  CHECK(host.calls == Calls{"CameraBasis()", "Highlights()", "Select()",
                            "Transform()", "Terrain(4000,-240,4000,4096)",
                            "Cursor(4000,-240,4000,0)", "Highlights()"});
}

TEST_CASE("Locked sphere cursor unlocks on Cross and stops area pulses",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.cursor = {.y = -40, .locked = 1};
  host.volumes[0].kind = 0x82;
  host.volumes[1].kind = 0x02;
  host.lists[kAreaTargets] = {TargetPreview{.actor_id = 2, .limb = 3},
                              TargetPreview{.actor_id = 0x31, .limb = 1},
                              TargetPreview{.actor_id = 5, .limb = -1}};
  host.pressed = kCross;
  TargetPoint target;
  CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
  CHECK(host.cursor == battle::SphereCursor{.y = -40});
  CHECK(host.volumes[0].kind == 2);
  CHECK(host.volumes[1] == battle::SphereVolume{.kind = 0x82});
  CHECK(host.calls == Calls{"CameraBasis()", "Highlights()", "Leave()",
                            "Unpulse(2,3)", "Unpulse(5,-1)", "Transform()",
                            "Terrain(4000,-240,4000,4096)",
                            "Cursor(4000,-240,4000,0)"});
}

// A ground-locked sphere volume 1000 units along x from the default.
void SetUpLockedSphere(FakeSelectionHost& host) {
  host.volumes[0].position.x = 5000;
  host.volumes[1].kind = 0x82;
}

TEST_CASE("Locked sphere cursor slides across the ground in camera space",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpLockedSphere(host);
  TargetPoint target;
  SECTION("the pad steps like a digital stick") {
    host.cursor = {.x = 100, .y = -40, .locked = 1};
    host.held = kRight;
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    CHECK(host.cursor ==
          battle::SphereCursor{.x = -156, .y = -40, .locked = 1});
    CHECK(target == TargetPoint{.x = 4844, .y = -240, .z = 4000});
  }
  SECTION("the stick turns with the camera") {
    host.cursor = {.x = 100, .y = -40, .locked = 1};
    host.camera_yaw = 1024;
    host.stick = {.x = 64};
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    CHECK(host.cursor ==
          battle::SphereCursor{.x = 100, .y = -40, .z = 256, .locked = 1});
    CHECK(target == TargetPoint{.x = 5100, .y = -240, .z = 4256});
  }
}

TEST_CASE("Locked sphere cursor stays inside the unit circle",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpLockedSphere(host);
  TargetPoint target;
  SECTION("pushing out of the unit circle keeps the cursor") {
    host.cursor = {.x = -4000, .y = -40, .locked = 1};
    host.held = kRight;
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    CHECK(host.cursor ==
          battle::SphereCursor{.x = -4000, .y = -40, .locked = 1});
  }
  SECTION("sliding along the rim lands back on it") {
    host.cursor = {.x = -4090, .y = -40, .locked = 1};
    host.stick = {.y = -512};
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    // The table square root reads this 4574 length as 4550.
    CHECK(VS::Core::Math::SquareRootInteger(4090 * 4090 + 2048 * 2048) == 4550);
    CHECK(host.cursor ==
          battle::SphereCursor{.x = -3681, .y = -40, .z = -1843, .locked = 1});
    CHECK(target == TargetPoint{.x = 1319, .y = -240, .z = 2157});
  }
}

TEST_CASE("Square holds the locked sphere cursor on its pitch",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpLockedSphere(host);
  TargetPoint target;
  host.cursor = {.x = 100, .y = -40, .locked = 1};
  host.held = kRight | kSquare | kDown;
  CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
  CHECK(host.cursor == battle::SphereCursor{.x = 100, .y = -32, .locked = 1});
  CHECK(host.calls.back() == "Cursor(5100,-232,4000,1)");
}

struct PitchCase {
  int kind = 0;
  int start = 0;
  std::uint32_t held = 0;
  int stick_y = 0;
  int expected = 0;
};

TEST_CASE("Sphere cursor pitch stays inside the volume's height",
          "[battle][battle-targeting]") {
  const std::array<PitchCase, 10> cases{{
      {.kind = 2, .start = 120, .held = kDown, .expected = 128},
      {.kind = 2, .start = 124, .held = kDown, .expected = 128},
      {.kind = 1, .held = kDown, .expected = 8},
      {.kind = 3, .held = kDown, .expected = 0},
      {.kind = 4, .start = -4, .held = kDown, .expected = 0},
      {.kind = 2, .start = -126, .held = kUp, .expected = -128},
      {.kind = 2, .held = kUp | kDown, .expected = 0},
      {.kind = 2, .held = kUp, .stick_y = 80, .expected = 10},
      {.kind = 2, .stick_y = -7, .expected = -1},
      {.kind = 0x82, .held = kDown, .expected = 8},
  }};
  for (const PitchCase& entry : cases) {
    INFO("kind " << entry.kind << " start " << entry.start);
    FakeSelectionHost host;
    host.volumes[0].kind = entry.kind;
    host.cursor.y = entry.start;
    host.held = entry.held;
    host.stick.y = entry.stick_y;
    TargetPoint target;
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    CHECK(host.cursor.y == entry.expected);
  }
}

struct ScaleCase {
  int kind = 0;
  int expected = 0;
};

TEST_CASE("Sphere cursor reach follows the volume shape at its pitch",
          "[battle][battle-targeting]") {
  // Radius 4 and pitch -48: the sphere's ratio is -96 / 4.
  const std::array<ScaleCase, 6> cases{{
      {.kind = 1,
       .expected = VS::Core::Math::SquareRootFixed(4096 - (24 * 24))},
      {.kind = 2, .expected = 4096},
      {.kind = 3, .expected = 4096},
      {.kind = 4, .expected = 2560},
      {.kind = 5, .expected = 1536},
      {.kind = 6, .expected = 4096},
  }};
  for (const ScaleCase& entry : cases) {
    INFO("kind " << entry.kind);
    FakeSelectionHost host;
    host.volumes[0].kind = entry.kind;
    host.cursor = {.x = 100, .y = -48};
    TargetPoint target;
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    REQUIRE(host.calls.size() == 4);
    CHECK(host.calls[2] == Call("Terrain", {4000, -248, 4000, entry.expected}));
    const int reach = ((100 * entry.expected) + 2048) >> 12;
    CHECK(target.x == 4000 + reach);
  }
}

TEST_CASE("A sphere volume below ground lifts the cursor above it",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  TargetPoint target;
  host.volumes[0].position.y = 50;
  CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
  CHECK(host.cursor.y == -52);
  CHECK(target == TargetPoint{.x = 4000, .y = -2, .z = 4000});
  CHECK(host.calls.at(2) == "Terrain(4000,-2,4000,4096)");
  CHECK(host.volumes[1].position == target);
}

TEST_CASE("Sphere cursor stays just under the ground",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  TargetPoint target;
  SECTION("ground level counts as below") {
    host.volumes[0].position.y = -1;
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    CHECK(host.cursor.y == -1);
    CHECK(target.y == -2);
  }
  SECTION("just above ground is kept") {
    host.volumes[0].position.y = -2;
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    CHECK(host.cursor.y == 0);
    CHECK(target.y == -2);
  }
}

TEST_CASE("Sphere target clamps to the collision map and refits the cursor",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.map_dimensions = 0x00100010;
  host.cursor.y = -40;
  TargetPoint target;
  SECTION("before the map's start") {
    host.volumes[0].position = {.x = 10, .y = -100, .z = 50};
    host.cursor = {.x = -100, .y = -40, .z = -32};
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    CHECK(target == TargetPoint{.x = 0, .y = -140, .z = 18});
    CHECK(host.cursor == battle::SphereCursor{.x = -10, .y = -40, .z = -32});
    CHECK(host.calls.back() == "Cursor(0,-140,18,1)");
  }
  SECTION("past the map's end") {
    host.volumes[0].position = {.x = 120, .y = -100, .z = 120};
    host.cursor = {.x = 64, .y = -40, .z = 64};
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    CHECK(target == TargetPoint{.x = 127, .y = -140, .z = 127});
    CHECK(host.cursor == battle::SphereCursor{.x = 7, .y = -40, .z = 7});
  }
}

TEST_CASE("Sphere target refits the cursor through the volume transform",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.map_dimensions = 0x00100010;
  host.cursor.y = -40;
  TargetPoint target;
  SECTION("through a turned volume") {
    host.basis = {.m00 = 0, .m02 = 4096, .m20 = -4096, .m22 = 0};
    host.volumes[0].position = {.x = 10, .y = -100, .z = 10};
    host.cursor = {.x = -50, .y = -40, .z = 30};
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    CHECK(target == TargetPoint{.x = 0, .y = -140, .z = 0});
    CHECK(host.cursor == battle::SphereCursor{.x = -10, .y = -40, .z = 10});
  }
  SECTION("a missing transform cannot refit") {
    host.basis = {};
    host.volumes[0].position = {.x = 200, .y = -100, .z = 50};
    host.cursor = {.x = 5, .y = -40, .z = 5};
    CHECK_FALSE(battle::UpdateSphereTargetPosition(host, target));
    CHECK(target == TargetPoint{.x = 200, .y = -140, .z = 50});
    CHECK(host.cursor == battle::SphereCursor{.x = 5, .y = -40, .z = 5});
  }
}

TEST_CASE("Target highlights leave position groups alone",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  battle::ClearTargetActorHighlight(host, 3, 2);
  battle::ClearTargetActorHighlight(host, 0x13, 2);
  battle::SetTargetActorHighlight(host, 4, 1, 5);
  battle::SetTargetActorHighlight(host, 0x20, 1, 5);
  battle::SetTargetActorColorOverride(host, 6, true);
  battle::SetTargetActorColorOverride(host, 7, false);
  battle::SetTargetActorColorOverride(host, 0xF0, true);
  CHECK(host.calls ==
        Calls{"Unpulse(3,2)", "Pulse(4,1,5)", "Tint(6,1)", "Tint(7,0)"});
}

// Candidates naming only their group (an actor slot or an encoded id).
[[nodiscard]] std::vector<TargetPreview> Groups(
    std::initializer_list<int> groups) {
  std::vector<TargetPreview> list;
  for (const int group : groups) {
    list.push_back(TargetPreview{.actor_id = static_cast<std::uint8_t>(group)});
  }
  return list;
}

[[nodiscard]] std::vector<TargetPreview> HighlightCandidates() {
  return {TargetPreview{.actor_id = 2, .limb = 1},
          TargetPreview{.actor_id = 0x30},
          TargetPreview{.actor_id = 4, .limb = -1}};
}

TEST_CASE("All-target highlights cover every actor candidate",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.all_targets = true;
  host.selected = 1;
  host.lists[kCandidates] = HighlightCandidates();
  battle::UpdateTargetSelectionHighlight(host, 3);
  CHECK(host.calls.empty());
  battle::UpdateTargetSelectionHighlight(host, 1);
  CHECK(host.calls ==
        Calls{"Unpulse(2,1)", "Tint(2,0)", "Unpulse(4,-1)", "Tint(4,0)"});
  host.calls.clear();
  battle::UpdateTargetSelectionHighlight(host, 2);
  CHECK(host.calls ==
        Calls{"Pulse(2,0,1)", "Tint(2,1)", "Pulse(4,0,-1)", "Tint(4,1)"});
}

TEST_CASE("Selection highlight moves from the old target to the new",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.lists[kCandidates] = HighlightCandidates();
  host.highlighted = {.group = 2, .pulse_slot = 5};
  battle::UpdateTargetSelectionHighlight(host, 3);
  CHECK(host.calls.empty());
  host.selected = 1;
  battle::UpdateTargetSelectionHighlight(host, 3);
  CHECK(host.calls == Calls{"Unpulse(2,5)", "Tint(2,0)", "Highlighted(48,0)"});
  host.calls.clear();
  host.selected = 2;
  battle::UpdateTargetSelectionHighlight(host, 3);
  CHECK(host.calls == Calls{"Highlighted(4,-1)", "Pulse(4,0,-1)", "Tint(4,1)"});
  host.calls.clear();
  battle::UpdateTargetSelectionHighlight(host, 1);
  CHECK(host.calls == Calls{"Unpulse(4,-1)", "Tint(4,0)"});
  host.calls.clear();
  battle::UpdateTargetSelectionHighlight(host, 2);
  CHECK(host.calls == Calls{"Highlighted(4,-1)", "Pulse(4,0,-1)", "Tint(4,1)"});
}

// A target list column from its entries, groups and group starts.
struct ColumnSpec {
  std::vector<int> entries{};
  std::vector<int> groups{};
  std::vector<int> firsts{};
};

[[nodiscard]] battle::TargetColumn MakeColumn(const ColumnSpec& spec) {
  battle::TargetColumn column;
  for (const int entry : spec.entries) {
    column.entries.at(static_cast<std::size_t>(column.entry_count++)) =
        static_cast<std::uint8_t>(entry);
  }
  for (std::size_t index = 0; index < spec.groups.size(); ++index) {
    column.groups.at(index) = static_cast<std::uint8_t>(spec.groups[index]);
    column.group_first_targets.at(index) =
        static_cast<std::uint8_t>(spec.firsts.at(index));
  }
  column.group_count = static_cast<int>(spec.groups.size());
  return column;
}

TEST_CASE("Target groups join the shorter column",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.lists[kCandidates] = Groups({1, 1, 1, 7, 2, 7});
  battle::TargetColumns columns;
  columns.columns[0].entry_count = 2;
  columns.columns[1].entry_count = 1;
  battle::AppendTargetGroupToColumn(host, columns, 7);
  const battle::TargetColumn& second = columns.columns[1];
  CHECK(second.entry_count == 3);
  CHECK(second.entries[1] == 3);
  CHECK(second.entries[2] == 5);
  CHECK(second.group_count == 1);
  CHECK(second.groups[0] == 7);
  CHECK(second.group_first_targets[0] == 3);
  CHECK(columns.columns[0].group_count == 0);
}

TEST_CASE("A target group without candidates still takes a column slot",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.lists[kCandidates] = Groups({1, 1, 1, 7, 2, 7});
  battle::TargetColumns columns;
  columns.columns[0].entry_count = 2;
  columns.columns[1].entry_count = 3;
  // Its start wraps to 0xFF.
  battle::AppendTargetGroupToColumn(host, columns, 9);
  const battle::TargetColumn& first = columns.columns[0];
  CHECK(first.entry_count == 2);
  CHECK(first.group_count == 1);
  CHECK(first.groups[0] == 9);
  CHECK(first.group_first_targets[0] == 0xFF);
}

TEST_CASE("Full target columns drop groups with an error",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.lists[kCandidates] = Groups({7, 7});
  battle::TargetColumns columns;
  columns.columns[0].group_count = 8;
  columns.columns[1].entry_count = 1;
  const BattleLog log;
  battle::AppendTargetGroupToColumn(host, columns, 5);
  CHECK(log.Saw("target column groups are full; group 5 dropped"));
  CHECK(columns.columns[0].group_count == 8);
  CHECK(columns.columns[1].group_count == 0);
}

TEST_CASE("Full target columns drop candidates with an error",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.lists[kCandidates] = Groups({7, 7});
  battle::TargetColumns columns;
  const BattleLog log;
  columns.columns[0].entry_count = 16;
  columns.columns[1].entry_count = 16;
  battle::AppendTargetGroupToColumn(host, columns, 7);
  CHECK(log.Saw("target column is full; target 0 dropped"));
  CHECK(log.Saw("target column is full; target 1 dropped"));
  CHECK(columns.columns[0].entry_count == 16);
  CHECK(columns.columns[0].group_count == 1);
  CHECK(columns.columns[0].groups[0] == 7);
  CHECK(columns.columns[0].group_first_targets[0] == 0);
}

TEST_CASE("Target columns sort groups by size and then position groups",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.columns.columns[0].entry_count = 9;
  host.columns.columns[1].group_count = 3;
  host.lists[kCandidates] = Groups({2, 2, 5, 5, 5, 0x30, 0x31});
  battle::TargetColumns expected;
  expected.columns[0] = MakeColumn(
      {.entries = {2, 3, 4, 6}, .groups = {5, 0x31}, .firsts = {2, 6}});
  expected.columns[1] =
      MakeColumn({.entries = {0, 1, 5}, .groups = {2, 0x30}, .firsts = {0, 5}});
  expected.top_offset = 66;
  CHECK(battle::BuildTargetSelectionColumns(host) == expected);
  // decomp: a position group joins once per candidate.
  host.lists[kCandidates] = Groups({0x30, 0x30});
  battle::TargetColumns twice;
  twice.columns[0] =
      MakeColumn({.entries = {0, 1}, .groups = {0x30}, .firsts = {0}});
  twice.columns[1] = twice.columns[0];
  twice.top_offset = 77;
  CHECK(battle::BuildTargetSelectionColumns(host) == twice);
  // decomp: an actor group larger than six is never listed.
  host.lists[kCandidates] = Groups({3, 3, 3, 3, 3, 3, 3});
  battle::TargetColumns none;
  none.top_offset = 88;
  CHECK(battle::BuildTargetSelectionColumns(host) == none);
}

TEST_CASE("Dismissing the target list sends each column off its side",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.lists[kCandidates] = Groups({1, 2, 3, 4});
  host.selected = 0;
  host.highlighted = {.group = 1, .pulse_slot = 4};
  host.columns.columns[0] = MakeColumn({.entries = {2, 3}});
  host.columns.columns[1] = MakeColumn({.entries = {0}});
  host.labels[2].ring_size = 9;
  battle::DismissTargetSelectionRows(host);
  CHECK(host.calls == Calls{"Unpulse(1,4)", "Tint(1,0)"});
  CHECK(host.labels[2].state == 3);
  CHECK(host.labels[2].ring_size == 9);
  CHECK(host.labels[3].state == 3);
  CHECK(host.labels[0].state == 2);
  CHECK(host.labels[1].state == 0);
}

TEST_CASE("Target rows start off screen on their column's side",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.lists[kCandidates] = Groups({0, 0, 0, 0, 0});
  host.lists[kCandidates][4].position = {.x = 1, .y = 2, .z = 3, .pad = 4};
  host.lists[kCandidates][4].kind = 7;
  host.labels[4].ring_size = 5;
  battle::InitTargetSelectionRow(host, 4, {.state = 2, .index = 3});
  CHECK(host.labels[4] ==
        battle::TargetSelectionLabel{
            .state = 2,
            .screen_x = 320,
            .offset_y = 65,
            .world_position = {.x = 1, .y = 2, .z = 3, .pad = 4}});
  CHECK(host.calls == Calls{"LabelText(4,7,1)"});
  host.calls.clear();
  battle::InitTargetSelectionRow(host, 4, {.state = 3, .index = 21});
  CHECK(host.labels[4].offset_y == 7);
  CHECK(host.labels[4].screen_x == -72);
  CHECK(host.labels[4].fade == 1);
  CHECK(host.calls == Calls{"LabelText(4,7,0)"});
}

TEST_CASE("Target rows without a limb name get no label text",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.lists[kCandidates] = Groups({0, 0, 0, 0, 0});
  host.lists[kCandidates][4].kind = 20;
  const BattleLog log;
  battle::InitTargetSelectionRow(host, 4, {.state = 2, .index = 0});
  CHECK(log.Saw("target kind 20 has no limb name"));
  CHECK(host.calls.empty());
  CHECK(host.labels[4].offset_y == 32);
}

[[nodiscard]] battle::TargetText StatusText(int index) {
  return {.source = battle::TargetTextSource::kStatusIndex, .value = index};
}

TEST_CASE("Target status text names the first removed status",
          "[battle][battle-targeting]") {
  const battle::TargetText none{
      .source = battle::TargetTextSource::kStatusOffset, .value = 246};
  CHECK(battle::FirstTargetStatusText(0) == none);
  CHECK(battle::FirstTargetStatusText(0x1F) == none);
  CHECK(battle::FirstTargetStatusText(1u << 29) == none);
  CHECK(battle::FirstTargetStatusText(1u << 5) == StatusText(57));
  CHECK(battle::FirstTargetStatusText((1u << 7) | (1u << 9)) == StatusText(59));
  CHECK(battle::FirstTargetStatusText(1u << 28) == StatusText(80));
}

// One frame of the target list.
struct MenuFrame {
  bool closed = false;
  int target = 0;
};

MenuFrame Frame(FakeSelectionHost& host) {
  MenuFrame frame;
  frame.closed = battle::UpdateTargetSelectionMenu(host, frame.target);
  return frame;
}

// A list of actor 2's whole body and actor 5's limb 2, the first selected.
void SetUpOpeningList(FakeSelectionHost& host) {
  host.lists[kCandidates] = {
      TargetPreview{.kind = 16, .actor_id = 2, .limb = 1},
      TargetPreview{.kind = 3, .actor_id = 5, .limb = 2}};
  host.selected = 0;
}

TEST_CASE("Target list opens with a highlight and reveals its rows",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpOpeningList(host);
  const MenuFrame frame = Frame(host);
  CHECK_FALSE(frame.closed);
  CHECK(frame.target == battle::kNoPreviewSelected);
  CHECK(host.calls == Calls{"HighlightTint(8421504)", "InitInfo()",
                            "Highlighted(2,1)", "Pulse(2,0,1)", "Tint(2,1)",
                            "Columns()", "LabelText(0,16,1)",
                            "LabelText(1,3,0)"});
  CHECK(host.step == 1);
  CHECK(host.labels[0].state == 2);
  CHECK(host.labels[1].state == 3);
}

TEST_CASE("Target list waits for its rows to settle",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpOpeningList(host);
  CHECK_FALSE(Frame(host).closed);
  host.calls.clear();
  CHECK_FALSE(Frame(host).closed);
  CHECK(host.step == 18);
  CHECK(host.calls.empty());
  CHECK_FALSE(Frame(host).closed);
  CHECK(host.step == 18);
  host.labels_settled = true;
  CHECK_FALSE(Frame(host).closed);
  CHECK(host.step == 19);
}

// Two columns revealing their rows: groups 5, 5 and 7, then 9 and 8.
void SetUpRevealingRows(FakeSelectionHost& host) {
  host.lists[kCandidates] = Groups({5, 5, 7, 9, 8});
  host.columns.columns[0] = MakeColumn({.entries = {0, 1, 2}});
  host.columns.columns[1] = MakeColumn({.entries = {3, 4}});
  host.step = 1;
  host.labels[3].fade = 1;
}

TEST_CASE("Target rows alternate their fade when the group changes",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpRevealingRows(host);
  CHECK_FALSE(Frame(host).closed);
  CHECK(host.step == 2);
  CHECK(host.labels[1].fade == 0);
  CHECK(host.labels[1].offset_y == 43);
  CHECK(host.labels[4].fade == 0);
  CHECK(host.labels[4].state == 3);
}

TEST_CASE("Target rows reveal one row per column each frame",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpRevealingRows(host);
  CHECK_FALSE(Frame(host).closed);
  CHECK_FALSE(Frame(host).closed);
  CHECK(host.step == 3);
  CHECK(host.labels[2].fade == 1);
  CHECK_FALSE(Frame(host).closed);
  CHECK(host.step == 18);
}

TEST_CASE("A target list without targets waits for a button and closes",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.volumes[0].range_kind = 2;
  CHECK_FALSE(Frame(host).closed);
  CHECK(host.step == 16);
  CHECK(host.calls == Calls{"HighlightTint(8421504)", "InitInfo()", "Columns()",
                            "NoTargets()"});
  host.calls.clear();
  host.pressed = kCircle;
  CHECK_FALSE(Frame(host).closed);
  CHECK(host.calls == Calls{"NoTargets()", "Leave()"});
  CHECK(host.step == 17);
}

TEST_CASE("A target list without targets closes after leaving",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.volumes[0].range_kind = 2;
  CHECK_FALSE(Frame(host).closed);
  host.pressed = kCircle;
  CHECK_FALSE(Frame(host).closed);
  host.calls.clear();
  CHECK_FALSE(Frame(host).closed);
  CHECK(host.calls == Calls{"Reuse(1)"});
  CHECK(host.step == 21);
  host.calls.clear();
  const MenuFrame last = Frame(host);
  CHECK(last.closed);
  CHECK(last.target == battle::kNoPreviewSelected);
  CHECK(host.calls == Calls{"DismissInfo()", "ClearPrimary()"});
}

// A list choosing among two of actor 2's limbs, actor 5 and a position group:
// the first column holds 0 and 1, the second 2 and 3.
void SetUpChoosing(FakeSelectionHost& host) {
  host.lists[kCandidates] = {
      TargetPreview{.kind = 16, .actor_id = 2, .limb = 1},
      TargetPreview{.kind = 4, .actor_id = 2, .limb = 3},
      TargetPreview{.kind = 16, .actor_id = 5, .limb = -1},
      TargetPreview{.kind = 16, .actor_id = 0x30}};
  host.columns = battle::BuildTargetSelectionColumns(host);
  host.actor_kinds = {{2, 0}, {5, 0}};
  host.step = 19;
  host.selected = 0;
  host.highlighted = {.group = 2, .pulse_slot = 1};
}

TEST_CASE("Page buttons take the choosing frame",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpChoosing(host);
  host.page_buttons = true;
  const MenuFrame frame = Frame(host);
  CHECK_FALSE(frame.closed);
  CHECK(frame.target == battle::kNoPreviewSelected);
  CHECK(host.calls == Calls{"Page()"});
  CHECK(host.step == 19);
}

// Presses Cross on the second row of the choosing list.
[[nodiscard]] MenuFrame CrossFromSecondRow(FakeSelectionHost& host) {
  SetUpChoosing(host);
  host.selected = 1;
  host.labels[1].ring_size = 0x21;
  host.labels[1].depth = 1;
  host.pressed = kCross;
  return Frame(host);
}

TEST_CASE("Cross closes the target list and sends the rows away",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  const MenuFrame frame = CrossFromSecondRow(host);
  CHECK_FALSE(frame.closed);
  CHECK(frame.target == battle::kNoPreviewSelected);
  CHECK(host.labels[1].ring_size == 0x12);
  CHECK(host.labels[1].depth == 0);
  CHECK(host.calls ==
        Calls{"Page()", "Leave()", "Unpulse(2,1)", "Tint(2,0)", "Reuse(0)"});
  CHECK(host.step == 21);
}

TEST_CASE("Cross sends each target row off its column's side",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  CHECK_FALSE(CrossFromSecondRow(host).closed);
  CHECK(host.labels[0].state == 3);
  CHECK(host.labels[1].state == 3);
  CHECK(host.labels[2].state == 2);
  CHECK(host.labels[3].state == 2);
}

TEST_CASE("Circle confirms the selected target through its page",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpChoosing(host);
  host.selected = 2;
  host.page_offset = 100;
  host.pressed = kCircle;
  MenuFrame frame = Frame(host);
  CHECK_FALSE(frame.closed);
  CHECK(frame.target == 102);
  CHECK(host.step == 20);
  CHECK(host.calls == Calls{"Page()", "Unpulse(2,1)", "Tint(2,0)"});
  host.calls.clear();
  frame = Frame(host);
  CHECK(frame.closed);
  CHECK(frame.target == 102);
  CHECK(host.calls == Calls{"DismissInfo()", "ClearPrimary()"});
}

// The choosing list on page 100 with Down repeating from the first row.
void SetUpMovingDown(FakeSelectionHost& host) {
  SetUpChoosing(host);
  host.page_offset = 100;
  host.repeated = kDown;
  host.labels[1].ring_size = 7;
}

TEST_CASE("Moving the selection rings the new label and describes it",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpMovingDown(host);
  CHECK(Frame(host).target == 100);
  CHECK(host.selected == 1);
  CHECK(host.labels[1].depth == 1);
  CHECK(host.labels[1].ring_size == 0);
  CHECK(host.labels[0].ring_size == 1);
  CHECK(host.labels[0].depth == 0);
  CHECK(host.calls == Calls{"Page()", "Change()", "Unpulse(2,1)", "Tint(2,0)",
                            "Highlighted(2,3)", "Pulse(2,0,3)", "Tint(2,1)",
                            "ShowInfo()"});
  CHECK(host.texts.at(0) == StatusText(4));
}

TEST_CASE("A held selection grows its ring", "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpMovingDown(host);
  CHECK(Frame(host).target == 100);
  host.calls.clear();
  host.repeated = 0;
  CHECK(Frame(host).target == 101);
  CHECK(host.labels[1].ring_size == 1);
  CHECK(host.labels[1].depth == 1);
  CHECK(host.calls == Calls{"Page()", "ShowInfo()"});
}

struct NavigationCase {
  int from = 0;
  std::uint32_t pressed = 0;
  std::uint32_t repeated = 0;
  int to = 0;
};

// Runs one choosing frame with the case's pad; the selection it lands on.
[[nodiscard]] int NavigateFrom(FakeSelectionHost& host,
                               const NavigationCase& entry) {
  host.selected = entry.from;
  host.pressed = entry.pressed;
  host.repeated = entry.repeated;
  CHECK(Frame(host).target == entry.from);
  return host.selected;
}

TEST_CASE("The pad moves the selection through both target columns",
          "[battle][battle-targeting]") {
  const std::array<NavigationCase, 18> cases{{
      {.from = 0, .repeated = kDown, .to = 1},
      {.from = 1, .repeated = kDown, .to = 2},
      {.from = 0, .repeated = kUp, .to = 3},
      {.from = 1, .repeated = kUp, .to = 0},
      {.from = 1, .pressed = kLeft, .to = 3},
      {.from = 0, .pressed = kLeft, .to = 2},
      {.from = 2, .pressed = kRight, .to = 0},
      {.from = 3, .pressed = kRight, .to = 1},
      {.from = 2, .repeated = kUp, .to = 1},
      {.from = 3, .repeated = kUp, .to = 2},
      {.from = 3, .repeated = kDown, .to = 0},
      {.from = 2, .repeated = kDown, .to = 3},
      {.from = 0, .to = 0},
      {.from = 0, .repeated = kL2, .to = 2},
      {.from = 2, .repeated = kL2, .to = 3},
      {.from = 3, .repeated = kL2, .to = 0},
      {.from = 2, .pressed = kLeft, .to = 2},
      {.from = 1, .pressed = kRight, .to = 1},
  }};
  for (const NavigationCase& entry : cases) {
    INFO("from " << entry.from << " pressed " << entry.pressed << " repeated "
                 << entry.repeated);
    FakeSelectionHost host;
    SetUpChoosing(host);
    const BattleLog log;
    CHECK(NavigateFrom(host, entry) == entry.to);
    CHECK(log.messages.empty());
  }
}

TEST_CASE("A single target column wraps within itself",
          "[battle][battle-targeting]") {
  const std::array<NavigationCase, 4> cases{{
      {.from = 0, .repeated = kUp, .to = 2},
      {.from = 2, .repeated = kDown, .to = 0},
      {.from = 1, .pressed = kLeft, .to = 1},
      {.from = 0, .repeated = kL2, .to = 0},
  }};
  for (const NavigationCase& entry : cases) {
    INFO("from " << entry.from << " pressed " << entry.pressed << " repeated "
                 << entry.repeated);
    FakeSelectionHost host;
    host.lists[kCandidates] = Groups({2, 2, 2});
    host.columns = battle::BuildTargetSelectionColumns(host);
    host.step = 19;
    CHECK(NavigateFrom(host, entry) == entry.to);
  }
}

TEST_CASE("Targets outside the columns keep the selection with a warning",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  SetUpChoosing(host);
  host.lists[kCandidates].push_back(TargetPreview{.kind = 2, .actor_id = 9});
  const BattleLog log;
  SECTION("moving") {
    CHECK(NavigateFrom(host, {.from = 4, .repeated = kDown}) == 4);
    CHECK(log.Saw("target 4 is in no column"));
  }
  SECTION("cycling groups") {
    CHECK(NavigateFrom(host, {.from = 4, .repeated = kL2}) == 4);
    CHECK(log.Saw("target group 9 is in no column"));
  }
  SECTION("an empty first column") {
    host.columns.columns[0] = {};
    CHECK(NavigateFrom(host, {.from = 2, .repeated = kDown}) == 2);
    CHECK(log.Saw("target 2 is in no column"));
  }
}

// Lists one candidate and runs a choosing frame, which describes it.
void DescribeCandidate(FakeSelectionHost& host,
                       const TargetPreview& candidate) {
  host.lists[kCandidates] = {candidate};
  host.columns = battle::BuildTargetSelectionColumns(host);
  host.step = 19;
  host.selected = 0;
  CHECK_FALSE(Frame(host).closed);
}

using TextSlots = std::map<int, battle::TargetText>;
using IntegerSlots = std::map<int, int>;

[[nodiscard]] battle::TargetText ActorNameText(int actor) {
  return {.source = battle::TargetTextSource::kActorName, .value = actor};
}

TEST_CASE("Target information describes an object's durability",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.actor_kinds[3] = 0x10;
  TargetPreview object{.kind = 16,
                       .actor_id = 3,
                       .preview_amount = 9,
                       .action_kind = 1,
                       .damage_type = 2,
                       .effect_variant = 3,
                       .enemy_class = 4};
  SECTION("an indestructible object") {
    DescribeCandidate(host, object);
    CHECK(host.texts == TextSlots{{0, ActorNameText(3)},
                                  {1, StatusText(25)},
                                  {2, StatusText(26)},
                                  {3, StatusText(42)},
                                  {4, StatusText(47)},
                                  {5, battle::TargetText{}}});
    CHECK(host.integers == IntegerSlots{{0, 9}, {1, 0}});
  }
  SECTION("a breakable object") {
    object.hit_chance = -1;
    DescribeCandidate(host, object);
    CHECK(host.texts.at(1) == StatusText(24));
    CHECK(host.integers.at(1) == 65535);
  }
}

struct AmountCase {
  int effect = 0;
  int detail = 0;
};

TEST_CASE("Target information previews an HP or MP amount",
          "[battle][battle-targeting]") {
  const std::array<AmountCase, 4> cases{{
      {.effect = 0x3A, .detail = 21},
      {.effect = 0x3B, .detail = 22},
      {.effect = 0x3E, .detail = 21},
      {.effect = 0x3F, .detail = 22},
  }};
  for (const AmountCase& entry : cases) {
    INFO("effect " << entry.effect);
    FakeSelectionHost host;
    DescribeCandidate(
        host, TargetPreview{.kind = 3,
                            .actor_id = 2,
                            .effect = static_cast<std::uint8_t>(entry.effect),
                            .preview_amount = 30,
                            .hit_chance = 80,
                            .damage_type = 1,
                            .enemy_class = 2});
    CHECK(host.texts == TextSlots{{0, StatusText(3)},
                                  {1, StatusText(entry.detail)},
                                  {2, StatusText(25)},
                                  {3, StatusText(41)},
                                  {4, StatusText(44)},
                                  {5, StatusText(53)}});
    CHECK(host.integers == IntegerSlots{{0, 30}, {1, 80}});
  }
}

TEST_CASE("Target information without an amount shows the removed status",
          "[battle][battle-targeting]") {
  FakeSelectionHost host;
  host.actor_kinds[4] = 0;
  SECTION("an actor") {
    DescribeCandidate(host, TargetPreview{.kind = 16,
                                          .actor_id = 4,
                                          .removed_statuses = 1u << 6,
                                          .action_kind = 2});
    const battle::TargetText line{
        .source = battle::TargetTextSource::kStatusLineTemplate};
    CHECK(host.texts == TextSlots{{0, ActorNameText(4)},
                                  {1, line},
                                  {2, StatusText(27)},
                                  {3, battle::TargetText{}},
                                  {4, battle::TargetText{}},
                                  {5, battle::TargetText{}},
                                  {6, StatusText(58)}});
    CHECK(host.integers == IntegerSlots{{0, 0}, {1, 0}});
  }
  SECTION("a position group") {
    DescribeCandidate(host, TargetPreview{.kind = 16, .actor_id = 0x40});
    CHECK(host.texts.at(0) == StatusText(20));
    CHECK(host.texts.at(6) ==
          battle::TargetText{.source = battle::TargetTextSource::kStatusOffset,
                             .value = 246});
  }
  SECTION("an absent actor") {
    const BattleLog log;
    DescribeCandidate(host,
                      TargetPreview{.kind = 16, .actor_id = 6, .effect = 0x3A});
    CHECK(log.Saw("target actor 6 is absent"));
    CHECK(host.texts.at(1) == StatusText(21));
  }
}

struct ExtentCase {
  battle::RangeShape shape;
  int expected = 0;
};

TEST_CASE("Range shape extent follows the shape's axis and angle",
          "[battle][battle-targeting]") {
  const std::array<ExtentCase, 8> cases{{
      {.shape = {.z = 4, .shape = 1}, .expected = 128},
      {.shape = {.y = 3, .angle = 8}, .expected = 96},
      {.shape = {.y = 2, .shape = 3}, .expected = 64},
      {.shape = {.z = 5, .shape = 2, .angle = 16}, .expected = 160},
      {.shape = {.z = 3, .shape = 4, .angle = 8}, .expected = 0},
      {.shape = {.z = 2, .shape = 9}, .expected = 64},
      {.shape = {.y = 7, .shape = 5}, .expected = 0},
      {.shape = {.x = 9, .y = 1, .z = 9, .shape = 3}, .expected = 32},
  }};
  for (const ExtentCase& entry : cases) {
    INFO("shape " << entry.shape.shape << " angle " << entry.shape.angle);
    CHECK(battle::CalculateRangeShapeExtent(entry.shape) == entry.expected);
  }
}

TEST_CASE("Action area reach comes from its radial or shaped area",
          "[battle][battle-targeting]") {
  battle::TargetingActionView action{.area = {.y = 3, .z = 4, .shape = 1}};
  CHECK(battle::CalculateActionAreaReach(action, 1000) == 0);
  action.shaped_area = true;
  CHECK(battle::CalculateActionAreaReach(action, 129) == 128);
  CHECK(battle::CalculateActionAreaReach(action, 128) == 0);
  action.radial_area = true;
  CHECK(battle::CalculateActionAreaReach(action, 0) == 96);
}

TEST_CASE("Action range squared trims its margin and borrows the weapon's",
          "[battle][battle-targeting]") {
  const battle::RangeShape weapon{.z = 2, .shape = 1};
  battle::TargetingActionView action{.range = {.x = 1, .z = 4, .shape = 1}};
  CHECK(battle::CalculateActionRangeSquared(action, weapon) == 96 * 96);
  action.range = {.x = 1, .z = 1, .shape = 1};
  CHECK(battle::CalculateActionRangeSquared(action, weapon) == 32 * 32);
  action.range = {.x = battle::kWeaponRangeSentinel};
  CHECK(battle::CalculateActionRangeSquared(action, weapon) == 32 * 32);
  action.range = {.x = 1, .shape = 5};
  CHECK(battle::CalculateActionRangeSquared(action, weapon) == 0);
}

// ---------------------------------------------------------------------------
// Live bindings.

TEST_CASE("Live action targeting refuses without an action state",
          "[battle][battle-targeting]") {
  const ScopedOverride<Battle_ActionState*> state(Battle_actionState, nullptr);
  const BattleLog log;
  battle::LiveReturnToBattleActionMenu();
  VS::Battle::LiveFilterAreaActionTargets();
  battle::LiveSetSingleActionTarget();
  battle::LiveSetAllActionTargets();
  battle::LiveCollectEnvironmentActionTargets();
  battle::LiveInitializeActionTargeting(5);
  VS::Battle::LiveBeginActorActionSelection(0, 5);
  for (const std::string_view operation :
       {"return to the action menu", "area target filter", "single target",
        "all targets", "environment targets", "action targeting",
        "action selection"}) {
    INFO(operation);
    CHECK(log.Saw(std::string(operation) + " refused: no action state"));
  }
}

TEST_CASE("Live sphere cursor refuses without a target or a sphere",
          "[battle][battle-targeting]") {
  const ScopedOverride<Battle_TargetSphereSetupView*> sphere(
      Battle_targetSphere, nullptr);
  const BattleLog log;
  CHECK(battle::LiveUpdateSphereTargetPosition(nullptr) == 0);
  CHECK(log.Saw("sphere cursor refused: no target"));
  SVECTOR position{};
  CHECK(battle::LiveUpdateSphereTargetPosition(&position) == 0);
  CHECK(log.Saw("sphere cursor refused: no targeting sphere"));
}

TEST_CASE("Live target selection refuses without a sphere",
          "[battle][battle-targeting]") {
  const ScopedOverride<Battle_TargetSphereSetupView*> sphere(
      Battle_targetSphere, nullptr);
  const BattleLog log;
  u_char target = 0;
  CHECK(VS::Battle::LiveUpdateTargetSelectionMenu(&target) == 1);
  CHECK(target == 0xFF);
  CHECK(log.Saw("target list refused: no targeting sphere"));
  VS::Battle::LiveUpdateTargetSelectionHighlight(3);
  CHECK(log.Saw("target highlight refused: no targeting sphere"));
  // A null target only resets the list.
  const ScopedOverride<u_char> step(Battle_targetSelectionStep, 7);
  CHECK(battle::LiveUpdateTargetSelectionMenu(nullptr) == 0);
  CHECK(Battle_targetSelectionStep == 0);
}

TEST_CASE("Live selection highlight records the selected position group",
          "[battle][battle-targeting]") {
  auto sphere = std::make_unique<Battle_TargetSphere>();
  std::vector<Battle_TargetPreview> previews(
      static_cast<std::size_t>(battle::kTargetPreviewCapacity));
  previews[1].actorId = 0x30;
  previews[1].limb = 2;
  sphere->targetLists[0] = previews.data();
  sphere->selectedTarget = 1;
  sphere->reserved2F = 0x20;
  const ScopedOverride<Battle_TargetSphereSetupView*> live(
      Battle_targetSphere,
      reinterpret_cast<Battle_TargetSphereSetupView*>(sphere.get()));
  const BattleLog log;
  VS::Battle::LiveUpdateTargetSelectionHighlight(3);
  CHECK(sphere->reserved2F == 0x30);
  CHECK(sphere->reserved30 == 2);
  CHECK(log.messages.empty());
}

// A position group 0x30 preview, then actor 3's limb 2.
void SetUpSingleTargetPreviews(Battle_ActionState& state) {
  state.previews[0].actorId = 0x30;
  state.previews[0].position = SVECTOR{10, -20, 30, 0};
  state.previews[1].actorId = 3;
  state.previews[1].limb = 2;
  state.previews[1].position = SVECTOR{40, -50, 60, 0};
}

TEST_CASE("Live single target carries a position group's id in the pad",
          "[battle][battle-targeting]") {
  auto state = std::make_unique<Battle_ActionState>();
  SetUpSingleTargetPreviews(*state);
  const ScopedOverride<Battle_ActionState*> live(Battle_actionState,
                                                 state.get());
  const Battle_ActionRequest& selection = state->selection;
  const Battle_HitResult& hit = selection.targets[0];
  battle::LiveSetSingleActionTarget();
  CHECK(selection.targetCount == 1);
  CHECK(hit.targetsPosition == 4);
  const auto point = std::bit_cast<SVECTOR>(hit.target);
  CHECK(point.vx == 10);
  CHECK(point.vy == -20);
  CHECK(point.vz == 30);
  CHECK(point.pad == 0x30);
}

TEST_CASE("Live single target focuses on a position group's point",
          "[battle][battle-targeting]") {
  auto state = std::make_unique<Battle_ActionState>();
  SetUpSingleTargetPreviews(*state);
  const ScopedOverride<Battle_ActionState*> live(Battle_actionState,
                                                 state.get());
  const Battle_ActionRequest& selection = state->selection;
  battle::LiveSetSingleActionTarget();
  CHECK(selection.focusPosition.vx == 10);
  CHECK(selection.focusPosition.vz == 30);
  CHECK(selection.focusPosition.pad == 0);
}

TEST_CASE("Live single target writes an actor's limb and focus",
          "[battle][battle-targeting]") {
  auto state = std::make_unique<Battle_ActionState>();
  SetUpSingleTargetPreviews(*state);
  const ScopedOverride<Battle_ActionState*> live(Battle_actionState,
                                                 state.get());
  const Battle_ActionRequest& selection = state->selection;
  const Battle_HitResult& hit = selection.targets[0];
  state->selectedPreview = 1;
  battle::LiveSetSingleActionTarget();
  CHECK(selection.targetCount == 1);
  CHECK(hit.targetsPosition == 0);
  CHECK(hit.target.targetActor == 3);
  CHECK(hit.target.targetLimb == 2);
  CHECK(selection.focusPosition.vx == 40);
  CHECK(selection.focusPosition.vy == -50);
}

TEST_CASE("Live action range reads the action table",
          "[battle][battle-targeting]") {
  const ScopedOverride<Battle_ActionDefinition> saved(
      Battle_actionDefinitions[250], Battle_ActionDefinition{});
  Battle_ActionDefinition& action = Battle_actionDefinitions[250];
  action.rangeX = 1;
  action.rangeZ = 4;
  action.shape = 1;
  const BattleLog log;
  CHECK(battle::LiveCalculateActionRangeSquared(nullptr, 250) == 96 * 96);
  CHECK(VS::Battle::LiveCalculateActionRangeSquared(nullptr, 250) == 96 * 96);
  action.rangeX = 0xFF;
  CHECK(battle::LiveCalculateActionRangeSquared(nullptr, 250) == 0);
  CHECK(log.Saw("action 250 range refused: no attacker weapon range"));
}

TEST_CASE("Live action range borrows the attacker's weapon range",
          "[battle][battle-targeting]") {
  const ScopedOverride<Battle_ActionDefinition> saved(
      Battle_actionDefinitions[250], Battle_ActionDefinition{});
  Battle_ActionDefinition& action = Battle_actionDefinitions[250];
  action.rangeX = 0xFF;
  action.rangeZ = 4;
  action.shape = 1;
  const BattleLog log;
  Battle_Ai_ActionRangeView2 weapon{};
  weapon.opaque38.rangeZ = 4;
  weapon.opaque38.shape = 1;
  Battle_Ai_ActionRangeView view{};
  view.opaque5C = &weapon;
  CHECK(battle::LiveCalculateActionRangeSquared(&view, 250) == 96 * 96);
  action.aiFlags.radialArea = 1;
  action.areaRangeY = 3;
  CHECK(battle::LiveCalculateActionAreaReach(0, 250, 1000) == 96);
  CHECK(battle::LiveCalculateActionRangeSquared(&view, 300) == 0);
  CHECK(log.Saw("action 300 is outside the action table"));
}

TEST_CASE("Live range shape extent refuses without a shape",
          "[battle][battle-targeting]") {
  const BattleLog log;
  CHECK(VS::Battle::LiveCalculateRangeShapeExtent(nullptr) == 0);
  CHECK(log.Saw("range extent refused: no range shape"));
  Battle_Ai_ActionRangeView2 weapon{};
  weapon.opaque38.rangeZ = 4;
  weapon.opaque38.shape = 1;
  CHECK(VS::Battle::LiveCalculateRangeShapeExtent(&weapon.opaque38) == 128);
}

TEST_CASE("Battle targeting bindings retire the module entries they replace",
          "[battle][battle-targeting]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  // The prerequisites, battle mode, abilities and targeting are typed C++
  // now; no BATTLE.PRG image or table stores these addresses.
  constexpr std::array<unsigned, 43> kRetired{
      0x80069C6Cu, 0x8006F630u, 0x8006F760u, 0x8006F89Cu, 0x8006FA20u,
      0x8006FD1Cu, 0x800704D8u, 0x8007053Cu, 0x8007087Cu, 0x800708ECu,
      0x80070B04u, 0x80070CACu, 0x800719FCu, 0x80071F68u, 0x80072734u,
      0x80072BA8u, 0x80072EC4u, 0x80078828u, 0x80078AB4u, 0x8007C088u,
      0x8007C0ACu, 0x8007D2B4u, 0x8007D2FCu, 0x8007F4B0u, 0x8007F518u,
      0x8007FDD8u, 0x800C4794u, 0x800C513Cu, 0x800C5164u, 0x800C518Cu,
      0x800C51B4u, 0x800C5360u, 0x800C553Cu, 0x800C56C0u, 0x800C5798u,
      0x800C58A4u, 0x800C58F8u, 0x800CAEACu, 0x800CAF40u, 0x800CAFB4u,
      0x800E5568u, 0x800E5600u, 0x800E5698u};
  for (const auto address : kRetired) {
    INFO("address " << address);
    CHECK(resolve(address) == nullptr);
  }
  // Battle_attackPrerequisiteFunctions still stores the rule slots.
  using Slot = decltype(&Battle_RejectAttackPrerequisiteSlot);
  constexpr std::array<std::pair<unsigned, Slot>, 40> kSlots{{
      {0x8007F548u, &Battle_RejectAttackPrerequisiteSlot},
      {0x8007F550u, &Battle_AcceptAttackPrerequisiteSlot},
      {0x8007F558u, &Battle_AcceptAttackPrerequisiteSlot},
      {0x8007F560u, &Battle_AcceptAttackPrerequisiteSlot},
      {0x8007F568u, &Battle_IsAttackTargetUndeadSlot},
      {0x8007F58Cu, &Battle_IsAttackTargetLivingSlot},
      {0x8007F5B0u, &Battle_RejectAttackPrerequisiteSlot},
      {0x8007F5B8u, &Battle_RejectAttackPrerequisiteSlot},
      {0x8007F5C0u, &Battle_RejectAttackPrerequisiteSlot},
      {0x8007F5C8u, &Battle_CanApplyStrengthDownSlot},
      {0x8007F5E8u, &Battle_CanApplyIntelligenceDownSlot},
      {0x8007F608u, &Battle_CanApplyAgilityDownSlot},
      {0x8007F628u, &Battle_CanApplyEquipmentDownSlot},
      {0x8007F6D8u, &Battle_CanApplyHalfMovementSlot},
      {0x8007F6F8u, &Battle_CanApplyStrengthUpSlot},
      {0x8007F718u, &Battle_CanApplyIntelligenceUpSlot},
      {0x8007F738u, &Battle_CanApplyAgilityUpSlot},
      {0x8007F758u, &Battle_CanApplyEquipmentUpSlot},
      {0x8007F808u, &Battle_CanApplyQuickenSlot},
      {0x8007F828u, &Battle_CanApplySilenceSlot},
      {0x8007F848u, &Battle_CanApplyMagicWardSlot},
      {0x8007F868u, &Battle_CanApplyRegenerationSlot},
      {0x8007F888u, &Battle_CanApplyParalysisSlot},
      {0x8007F8A8u, &Battle_CanApplyPoisonSlot},
      {0x8007F8C8u, &Battle_CanApplyNumbnessSlot},
      {0x8007F8E8u, &Battle_CanApplyCurseSlot},
      {0x8007F908u, &Battle_CanApplyAirAttackUpSlot},
      {0x8007F96Cu, &Battle_CanApplyFireAttackUpSlot},
      {0x8007F9D0u, &Battle_CanApplyEarthAttackUpSlot},
      {0x8007FA34u, &Battle_CanApplyWaterAttackUpSlot},
      {0x8007FA98u, &Battle_CanApplyAirDefenseUpSlot},
      {0x8007FB34u, &Battle_CanApplyFireDefenseUpSlot},
      {0x8007FBD0u, &Battle_CanApplyEarthDefenseUpSlot},
      {0x8007FC6Cu, &Battle_CanApplyWaterDefenseUpSlot},
      {0x8007FD08u, &Battle_CanApplyBanishSlot},
      {0x8007FD28u, &Battle_CanApplyExorciseSlot},
      {0x8007FD74u, &Battle_CanApplyDrainSlot},
      {0x8007FDC0u, &Battle_IgnoreAttackPrerequisiteSlot},
      {0x8007FDC8u, &Battle_IgnoreAttackPrerequisiteSlot},
      {0x8007FDD0u, &Battle_IgnoreAttackPrerequisiteSlot},
  }};
  for (const auto& [address, slot] : kSlots) {
    INFO("address " << address);
    CHECK(resolve(address) == reinterpret_cast<void*>(slot));
  }
}
}  // namespace
