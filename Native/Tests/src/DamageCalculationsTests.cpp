#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/DamageCalculations.hpp"
#include "Battle/DamageCalculationsLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
}

namespace {
namespace b = VS::Battle;
using Calculator = decltype(&b::CalculateSpellHitDamage);
struct Owner final : b::DamageOwner {
  b::DamageStats state{};
  std::array<b::DamageLimb, 6> limbs{};
  std::uint32_t statuses{};
  int reads{};
  std::vector<int> growth;
  std::function<void()> callback;
  b::DamageStats Read() override {
    ++reads;
    return state;
  }
  std::optional<b::DamageLimb> Limb(int limb) override {
    return limb >= 0 && limb < 6 ? std::optional{limbs.at(limb)} : std::nullopt;
  }
  std::uint32_t Statuses() override { return statuses; }
  std::uint16_t Risk() override { return state.risk; }
  std::uint8_t EnemyClass() override { return state.enemy_class; }
  void GrowAttacker(int kind, int affinity) override {
    growth.insert(growth.end(), {kind, affinity});
    if (callback) callback();
  }
  void GrowDefender(int kind, int affinity, int limb) override {
    growth.insert(growth.end(), {kind, affinity, limb});
    if (callback) callback();
  }
};
struct World final : b::DamageWorld {
  std::array<Owner, 2> owners{};
  std::array<b::DamageOwner*, 2> table{&owners.at(0), &owners.at(1)};
  std::array<std::optional<std::uint32_t>, 42> drawn{};
  std::array<b::DamageHit, 2> hits{};
  std::optional<std::uint8_t> action_type = 0, action_id = 6;
  std::array<std::uint16_t, 12> bonuses{};
  b::DamageTarget source{0, 0}, target{1, 0};
  int position{}, position_reads{}, bonus_reads{}, critical = -7;
  int draw = 0, average = 5;
  std::vector<int> calls;
  std::function<void(int)> random_callback;
  World() {
    drawn.fill(0);
    owners.at(0).state.actor_id = 40;
    owners.at(1).state.actor_id = 41;
    owners.at(0).state.strength = owners.at(0).state.intelligence = 100;
    owners.at(1).state.strength = owners.at(1).state.intelligence = 20;
    hits.fill({0, 6, 0});
  }
  b::DamageOwner* Stats(std::uint8_t slot) override {
    return slot < table.size() ? table.at(slot) : nullptr;
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    return slot < drawn.size() ? drawn.at(slot) : std::nullopt;
  }
  std::optional<b::DamageHit> Hit(int hit) override {
    return hit >= 0 && hit < 2 ? std::optional{hits.at(hit)} : std::nullopt;
  }
  std::optional<std::uint8_t> ActionType() override { return action_type; }
  std::optional<std::uint8_t> ActionId() override { return action_id; }
  std::optional<std::uint16_t> BladeBonus(std::uint8_t category) override {
    ++bonus_reads;
    return category < bonuses.size() ? std::optional{bonuses.at(category)}
                                     : std::nullopt;
  }
  std::int32_t TargetPosition() override {
    ++position_reads;
    return position;
  }
  std::int32_t RandomBounded(int bound) override {
    calls.push_back(bound);
    if (random_callback) random_callback(bound);
    return draw;
  }
  std::int32_t RandomAveraged(int bound) override {
    calls.push_back(-bound);
    if (random_callback) random_callback(bound);
    return average;
  }
  void SetCriticalHit() override { critical = 1; }
  std::optional<std::int16_t> Run(Calculator calculate, int hit = 0,
                                  int random = 0) {
    return calculate(&source, &target, hit, random, *this);
  }
};
struct LiveFixture {
  std::array<Battle_Actor*, 42> saved{};
  std::array<std::remove_reference_t<decltype(Battle_statCalculators[0])>, 27>
      saved_calculators{};
  std::array<Battle_Actor, 2> actors{};
  std::array<Battle_ActorStats, 2> stats{};
  Battle_ActionDefinition action{};
  Battle_HitResult source{}, target{};
  LiveFixture() {
    std::copy(std::begin(Battle_actors), std::end(Battle_actors),
              saved.begin());
    std::copy(std::begin(Battle_statCalculators),
              std::end(Battle_statCalculators), saved_calculators.begin());
    for (std::size_t i = 0; i < actors.size(); ++i) {
      actors.at(i).stats = &stats.at(i);
      stats.at(i).actorId = static_cast<std::uint8_t>(i);
      Battle_actors[i] = &actors.at(i);
    }
    target.target.targetActor = 1;
    stats.at(0).strength = stats.at(0).intelligence = 100;
    stats.at(1).strength = stats.at(1).intelligence = 20;
    action.hits[0].affinity = action.hits[1].affinity = 6;
  }
  ~LiveFixture() {
    std::copy(saved.begin(), saved.end(), std::begin(Battle_actors));
    std::copy(saved_calculators.begin(), saved_calculators.end(),
              std::begin(Battle_statCalculators));
  }
};
std::vector<std::uint8_t> Module(const char* name) {
  const std::filesystem::path root = "Native\\data";
  if (!std::filesystem::exists(root / VS::Content::DataStore::ManifestName)) {
    SKIP("Extracted data unavailable");
  }
  REQUIRE(VS::Content::DataStore::Open(root));
  auto& store = *VS::Content::DataStore::Get();
  const auto* asset = store.Find(name);
  REQUIRE(asset != nullptr);
  const auto bytes = store.ReadAll(*asset);
  REQUIRE(bytes.has_value());
  return bytes.value();
}
}  // namespace

TEST_CASE("Physical chooses seven affinities with positive strict maxima",
          "[damage-calculations][F00409]") {
  World w;
  w.drawn.at(40) = w.drawn.at(41) = 1;
  w.hits.at(0).affinity = 0;
  auto& weapon = w.owners.at(0).state.weapon;
  weapon.blade.id = 1;
  weapon.class_affinity.affinities.at(0).fill(-1);
  weapon.class_affinity.affinities.at(0).at(7) = 30000;
  for (std::size_t affinity = 0; affinity < 7; ++affinity) {
    weapon.class_affinity.affinities.at(0).at(affinity) = 40;
    w.owners.at(0).growth.clear();
    w.owners.at(1).growth.clear();
    REQUIRE(b::CalculatePhysicalHitDamage(&w.source, &w.target, 0, 1, 0, w) ==
            111);
    CHECK(w.owners.at(1).growth ==
          std::vector<int>{1, static_cast<int>(affinity + 1), 0});
    weapon.class_affinity.affinities.at(0).at(affinity) = -1;
  }
}
TEST_CASE("Physical wraps target affinity totals and keeps earliest ties",
          "[damage-calculations][F00409]") {
  World w;
  w.drawn.at(41) = 1;
  auto& limb = w.owners.at(1).limbs.at(0);
  limb.affinities.at(1) = 32767;
  limb.armor.class_affinity.affinities.at(0).at(1) = 1;
  limb.affinities.at(2) = limb.affinities.at(3) = 40;
  REQUIRE(b::CalculatePhysicalHitDamage(&w.source, &w.target, 0, 1, 0, w));
  CHECK(w.owners.at(0).growth == std::vector<int>{1, 3});
  w.owners.at(0).growth.clear();
  w.owners.at(1).state.shield.class_affinity.affinities.at(0).at(6) = 41;
  REQUIRE(b::CalculatePhysicalHitDamage(&w.source, &w.target, 0, 1, 0, w));
  CHECK(w.owners.at(0).growth == std::vector<int>{1, 7});
}
TEST_CASE("Physical accepts all stored classes and unsigned shield types",
          "[damage-calculations][F00409]") {
  World w;
  const auto type = GENERATE(1, 2, 3);
  const auto enemy = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  w.hits.at(0).type = static_cast<std::uint8_t>(type);
  w.owners.at(0).state.enemy_class = static_cast<std::uint8_t>(enemy);
  w.drawn.at(41) = 1;
  w.owners.at(1).state.shield.attributes.current_strength = 10;
  w.owners.at(1).state.shield.types.at(type) = 255;
  w.owners.at(1).state.shield.class_affinity.classes.at(0).at(enemy) = 45;
  CHECK(b::CalculatePhysicalHitDamage(&w.source, &w.target, 0, 0, 0, w) == 60);
}
TEST_CASE("Physical stages signed shorts and wrapped unsigned PP product",
          "[damage-calculations][F00409]") {
  World w;
  w.drawn.at(40) = 1;
  auto& weapon = w.owners.at(0).state.weapon;
  weapon.current_pp = 65535;
  weapon.max_pp = 1;
  w.bonuses.at(0) = 65535;
  CHECK(b::CalculatePhysicalHitDamage(&w.source, &w.target, 0, 0, 0, w) == 100);
  weapon.max_pp = 0;
  weapon.blade.category = 255;
  weapon.class_affinity.affinities.at(0).at(5) = 32767;
  weapon.damage_type_value = 1;
  CHECK(b::CalculatePhysicalHitDamage(&w.source, &w.target, 0, 0, 0, w) == 0);
  CHECK(w.bonus_reads == 1);
}
TEST_CASE(
    "Physical callback ordering preserves cached owners and current reads",
    "[damage-calculations][F00409]") {
  World w;
  w.random_callback = [&](int bound) {
    w.table.fill(nullptr);
    w.target.actor = 255;
    w.target.limb = -128;
    w.owners.at(0).statuses = 2;
    w.action_id = 5;
    if (bound == 100) w.owners.at(0).state.risk = 65535;
  };
  w.owners.at(0).callback = [&] { w.owners.at(0).state.enemy_class = 7; };
  CHECK(b::CalculatePhysicalHitDamage(&w.source, &w.target, 0, -1, -1, w) ==
        100);
  CHECK(w.calls == std::vector<int>{5, 100});
  CHECK(w.critical == 1);
  CHECK(w.owners.at(0).growth == std::vector<int>{1, 1});
  CHECK(w.owners.at(1).growth == std::vector<int>{8, 6, 0});
  CHECK(w.owners.at(0).reads == 1);
}
TEST_CASE("Physical keeps critical flag and no second negative floor",
          "[damage-calculations][F00409]") {
  World w;
  w.owners.at(0).state.strength = 32767;
  CHECK(b::CalculatePhysicalHitDamage(&w.source, &w.target, 1, 1, 0, w) ==
        -32768);
  CHECK(w.critical == -7);
  CHECK(w.position_reads == 0);
  w.owners.at(0).statuses = 2;
  CHECK(b::CalculatePhysicalHitDamage(&w.source, &w.target, 1, 1, 1, w) ==
        -16384);
}
TEST_CASE("Without debuffs forwards the second hit and nonboolean random flag",
          "[damage-calculations][F00410]") {
  World w;
  w.owners.at(0).statuses = 2;
  w.hits.at(1).factor = 2;
  CHECK(w.Run(b::PhysicalDamageWithoutDebuffs, 1, -3) == 121);
  CHECK(w.calls == std::vector<int>{5});
  CHECK(w.position_reads == 0);
  CHECK(w.Run(b::PhysicalDamageWithoutDebuffs, 0, 0) == 100);
  CHECK_FALSE(w.Run(b::PhysicalDamageWithoutDebuffs, 2, 0));
}
TEST_CASE("With debuffs divides signed short toward zero and forwards preview",
          "[damage-calculations][F00433]") {
  World w;
  w.owners.at(0).statuses = 2;
  w.hits.at(1).factor = 1;
  CHECK(w.Run(b::PhysicalDamageWithDebuffs, 1, -1) == 55);
  CHECK(w.Run(b::PhysicalDamageWithDebuffs, 0, 0) == 50);
  w.owners.at(0).state.strength = 32766;
  w.draw = 2;
  CHECK(w.Run(b::PhysicalDamageWithDebuffs, 0, 1) == -16383);
  CHECK_FALSE(w.Run(b::PhysicalDamageWithDebuffs, -1));
}
TEST_CASE("Spell zero type omits limb armor shield type but keeps accessories",
          "[damage-calculations][F00411]") {
  World w;
  w.drawn.at(41) = 1;
  w.owners.at(1).state.intelligence = 30;
  w.owners.at(1).state.shield.attributes.current_intelligence = 10;
  w.owners.at(1).state.shield.types.at(0) = 255;
  w.owners.at(1).limbs.at(0).types.at(0) = 30000;
  w.owners.at(0).state.accessory.types.at(0) = 40;
  CHECK(w.Run(b::CalculateSpellHitDamage) == 90);
  w.owners.at(1).state.accessory.types.at(0) = 100;
  CHECK(w.Run(b::CalculateSpellHitDamage) == 80);
}
TEST_CASE(
    "Spell affinity zero auto selects but explicit physical still defends",
    "[damage-calculations][F00411]") {
  World w;
  w.hits.at(0).affinity = 1;
  w.owners.at(1).state.intelligence = 120;
  w.owners.at(1).limbs.at(0).affinities.at(0) = 100;
  CHECK(w.Run(b::CalculateSpellHitDamage) == 0);
  w.drawn.at(40) = 1;
  w.owners.at(0).state.weapon.blade.id = 1;
  w.owners.at(0).state.weapon.class_affinity.affinities.at(0).at(6) = 40;
  w.hits.at(0).affinity = 0;
  CHECK(w.Run(b::CalculateSpellHitDamage, 0, 1) == 11);
  CHECK(w.owners.at(1).growth == std::vector<int>{0, 7, 0});
}
TEST_CASE("Spell overfull DP is signed and PP guards ignore unused category",
          "[damage-calculations][F00411]") {
  World w;
  w.drawn.at(40) = 1;
  auto& weapon = w.owners.at(0).state.weapon;
  weapon.current_dp = 102;
  weapon.max_dp = 100;
  weapon.current_pp = 8;
  weapon.max_pp = 100;
  weapon.blade.category = 255;
  CHECK(w.Run(b::CalculateSpellHitDamage) == 102);
  CHECK(w.bonus_reads == 0);
  w.owners.at(0).state.intelligence = 65535;
  CHECK(w.Run(b::CalculateSpellHitDamage) == 0);
}
TEST_CASE("Spell variance wraps negative without post callback recalculation",
          "[damage-calculations][F00411]") {
  World w;
  w.owners.at(0).state.intelligence = 32767;
  w.random_callback = [&](int) {
    w.position = 1;
    w.owners.at(0).state.intelligence = 0;
    w.hits.at(0).affinity = 1;
  };
  CHECK(w.Run(b::CalculateSpellHitDamage, 0, -1) == -32768);
  CHECK(w.owners.at(1).growth.empty());
  CHECK(w.critical == -7);
}
TEST_CASE("Healing diagnoses only undefined affinity inputs",
          "[damage-calculations][F00421]") {
  World w;
  w.hits.at(0).affinity = 0;
  CHECK_FALSE(w.Run(b::CalculateSpellHealing));
  w.hits.at(0).affinity = 1;
  CHECK(w.Run(b::CalculateSpellHealing) == 100);
  w.drawn.at(40) = 1;
  CHECK_FALSE(w.Run(b::CalculateSpellHealing));
  w.hits.at(0).affinity = 6;
  w.owners.at(0).state.weapon.class_affinity.affinities.at(0).at(5) = 40;
  CHECK(w.Run(b::CalculateSpellHealing) == 110);
  CHECK(w.calls.empty());
}
TEST_CASE("Healing physical affinity omits defense affinity but not accessory",
          "[damage-calculations][F00421]") {
  World w;
  w.hits.at(0).affinity = 1;
  w.owners.at(1).state.intelligence = 30;
  w.owners.at(1).limbs.at(0).affinities.at(0) = 30000;
  w.owners.at(1).state.accessory.affinities.at(0) = 100;
  CHECK(w.Run(b::CalculateSpellHealing, 0, 1) == 80);
  CHECK(w.position_reads == 0);
  CHECK(w.owners.at(1).growth.empty());
}
TEST_CASE("Healing averages eleven and floors negative variance and overflow",
          "[damage-calculations][F00421]") {
  World w;
  w.average = 0;
  w.owners.at(0).state.intelligence = 3;
  CHECK(w.Run(b::CalculateSpellHealing, 1, -1) == 0);
  CHECK(w.calls == std::vector<int>{-11});
  w.owners.at(0).state.intelligence = 32767;
  w.average = 10;
  CHECK(w.Run(b::CalculateSpellHealing, 1, 1) == 0);
  CHECK(w.Run(b::CalculateSpellHealing, 1, 0) == 32767);
}
TEST_CASE("Healing retains affinity limb and owner through aliased callback",
          "[damage-calculations][F00421]") {
  World w;
  w.table.at(1) = &w.owners.at(0);
  w.target.limb = 5;
  w.random_callback = [&](int) {
    w.table.fill(nullptr);
    w.target.limb = -1;
    w.hits.at(0).affinity = 1;
    w.owners.at(0).state.weapon.damage_type_value = 255;
  };
  CHECK(w.Run(b::CalculateSpellHealing, 0, 1) == 20);
  CHECK(w.owners.at(0).growth == std::vector<int>{0, 6, 5});
  CHECK(w.owners.at(0).state.weapon.damage_type_value == 255);
}
TEST_CASE("Damage refuses required records and used indices even for positions",
          "[damage-calculations][F00409][F00410][F00411][F00421][F00433]") {
  const auto function =
      GENERATE(b::PhysicalDamageWithoutDebuffs, b::PhysicalDamageWithDebuffs,
               b::CalculateSpellHitDamage, b::CalculateSpellHealing);
  World w;
  w.position = 1;
  w.target.limb = -1;
  CHECK_FALSE(w.Run(function));
  w.target.limb = 6;
  CHECK_FALSE(w.Run(function));
  w.target.limb = 0;
  w.drawn.at(40).reset();
  CHECK_FALSE(w.Run(function));
  w.drawn.at(40) = 0;
  w.table.at(1) = nullptr;
  CHECK_FALSE(w.Run(function));
  CHECK(w.calls.empty());
}
TEST_CASE("Physical guards fallback type and only used PP categories",
          "[damage-calculations][F00409]") {
  World w;
  w.owners.at(0).state.weapon.damage_type = 4;
  CHECK_FALSE(b::CalculatePhysicalHitDamage(&w.source, &w.target, 0, 0, 0, w));
  w.hits.at(0).type = 2;
  CHECK(b::CalculatePhysicalHitDamage(&w.source, &w.target, 0, 0, 0, w) == 100);
  w.drawn.at(40) = 1;
  w.owners.at(0).state.weapon.blade.category = 12;
  w.owners.at(0).state.weapon.max_pp = 1;
  CHECK_FALSE(b::CalculatePhysicalHitDamage(&w.source, &w.target, 0, 0, 0, w));
}
TEST_CASE("Live calculators execute real gems with unsigned shield layout",
          "[damage-calculations][F00409][F00410][F00411][F00421][F00433]") {
  LiveFixture f;
  f.stats.at(0).weapon.gems[2].gemEffects = 3;
  f.stats.at(0).statuses = 2;
  CHECK(b::LiveCalculatePhysicalHitDamage(&f.action, &f.source, &f.target, 0, 0,
                                          0) == 105);
  CHECK(b::LivePhysicalDamageWithoutDebuffs(&f.action, &f.source, &f.target, 0,
                                            0) == 105);
  CHECK(b::LivePhysicalDamageWithDebuffs(&f.action, &f.source, &f.target, 0,
                                         0) == 52);
  f.action.type = 1;
  f.stats.at(0).weapon.gems[2].gemEffects = 4;
  CHECK(b::LiveCalculateSpellHitDamage(&f.action, &f.source, &f.target, 0, 0) ==
        105);
  CHECK(b::LiveCalculateSpellHealing(&f.action, &f.source, &f.target, 0, 0) ==
        105);
}
TEST_CASE(
    "Original healing definitions select only supported authored affinities",
    "[damage-calculations][F00421]") {
  const auto image = Module("SLUS_010.40");
  constexpr std::size_t kActions = 256, kActionOffset = 0x3C1DC;
  REQUIRE(image.size() >=
          kActionOffset + kActions * sizeof(Battle_ActionDefinition));
  std::array<Battle_ActionDefinition, kActions> definitions{};
  std::memcpy(definitions.data(), image.data() + kActionOffset,
              sizeof(definitions));
  std::vector<unsigned> selected;
  for (std::size_t i = 0; i < definitions.size(); ++i) {
    for (std::size_t hit = 0; hit < 2; ++hit) {
      if (definitions.at(i).hits[hit].statCalculatorId != 13) continue;
      CHECK(hit == 0);
      CHECK(definitions.at(i).hits[hit].affinity == 6);
      selected.push_back(static_cast<unsigned>(i));
    }
  }
  CHECK(selected == std::vector<unsigned>{65, 96});
}
TEST_CASE(
    "Physical auto affinity ignores negative maxima and breaks ties first",
    "[damage-calculations][F00409]") {
  World w;
  w.drawn.at(40) = 1;
  w.hits.at(0).affinity = 0;
  auto& weapon = w.owners.at(0).state.weapon;
  weapon.blade.id = 1;
  weapon.class_affinity.affinities.at(0).fill(-40);
  CHECK(w.Run(b::PhysicalDamageWithoutDebuffs, 0, 1) == 91);
  CHECK(w.owners.at(1).growth == std::vector<int>{1, 1, 0});
  w.owners.at(1).growth.clear();
  weapon.class_affinity.affinities.at(0).at(2) = 40;
  weapon.class_affinity.affinities.at(0).at(3) = 40;
  CHECK(w.Run(b::PhysicalDamageWithoutDebuffs, 0, 1) == 111);
  CHECK(w.owners.at(1).growth == std::vector<int>{1, 3, 0});
}
TEST_CASE("Physical PP wraps before signed division rather than after it",
          "[damage-calculations][F00409]") {
  World w;
  w.drawn.at(40) = 1;
  auto& weapon = w.owners.at(0).state.weapon;
  weapon.current_pp = weapon.max_pp = 65535;
  w.bonuses.at(0) = 65535;
  CHECK(w.Run(b::PhysicalDamageWithoutDebuffs) == 100);
  weapon.max_pp = 2;
  CHECK(w.Run(b::PhysicalDamageWithoutDebuffs) == 100);
  w.bonuses.at(0) = 100;
  CHECK(w.Run(b::PhysicalDamageWithoutDebuffs) == 88);
}
TEST_CASE("Physical critical doubling preserves negative low half",
          "[damage-calculations][F00409]") {
  World w;
  w.action_id = 0;
  w.owners.at(0).state.strength = 20000;
  CHECK(w.Run(b::PhysicalDamageWithoutDebuffs, 1, 1) == -25534);
  CHECK(w.critical == 1);
  w.action_id.reset();
  CHECK(w.Run(b::PhysicalDamageWithoutDebuffs, 1, 0) == 20000);
  CHECK(w.critical == 1);
}
TEST_CASE("Physical self target uses shared owner without committing snapshots",
          "[damage-calculations][F00409]") {
  World w;
  w.table.at(1) = &w.owners.at(0);
  w.owners.at(0).callback = [&] {
    w.owners.at(0).state.enemy_class = 7;
    w.owners.at(0).state.strength = 777;
  };
  CHECK(w.Run(b::PhysicalDamageWithoutDebuffs, 0, 1) == 21);
  CHECK(w.owners.at(0).growth == std::vector<int>{1, 1, 8, 6, 0});
  CHECK(w.owners.at(0).state.strength == 777);
}
TEST_CASE("Spell and healing guard used encoded affinity and type slots",
          "[damage-calculations][F00411][F00421]") {
  const auto function =
      GENERATE(b::CalculateSpellHitDamage, b::CalculateSpellHealing);
  World w;
  w.hits.at(0).affinity = 8;
  CHECK_FALSE(w.Run(function));
  w.hits.at(0).affinity = 7;
  w.hits.at(0).type = 4;
  CHECK_FALSE(w.Run(function));
  w.hits.at(0).type = 3;
  w.hits.at(0).factor = 32;
  CHECK_FALSE(w.Run(function));
  w.hits.at(0).factor = 31;
  w.owners.at(0).state.weapon.blade.category = 255;
  CHECK(w.Run(function) == 410);
}
TEST_CASE(
    "Authored healing actions run through live checked drawn and undrawn paths",
    "[damage-calculations][F00421]") {
  LiveFixture f;
  const auto image = Module("SLUS_010.40");
  const auto index = GENERATE(65, 96);
  constexpr std::size_t kActionOffset = 0x3C1DC;
  REQUIRE(image.size() >= kActionOffset + (index + 1) * sizeof(f.action));
  std::memcpy(&f.action,
              image.data() + kActionOffset + index * sizeof(f.action),
              sizeof(f.action));
  const auto expected = 100 + 10 * f.action.hits[0].statFactor;
  CHECK(b::LiveCalculateSpellHealing(&f.action, &f.source, &f.target, 0, 0) ==
        expected);
  f.actors.at(0).weaponDrawn = 1;
  CHECK(b::LiveCalculateSpellHealing(&f.action, &f.source, &f.target, 0, 0) ==
        expected);
}
TEST_CASE(
    "Original D5 module addresses relocate to exact short callback bindings",
    "[damage-calculations][F00410][F00411][F00421][F00433]") {
  LiveFixture f;
  const auto image = Module("BATTLE/BATTLE.PRG");
  const auto tables =
      std::span(Content_modulePointers, Content_modulePointerCount);
  auto* target = reinterpret_cast<void**>(Battle_statCalculators);
  const auto table =
      std::ranges::find(tables, target, &Content_ModulePointerTable::target);
  REQUIRE(table != tables.end());
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  REQUIRE(VS::Content::RelocateModulePointerTable(*table, image, symbols));
  const std::array callbacks{
      b::LivePhysicalDamageWithoutDebuffs, b::LiveCalculateSpellHitDamage,
      b::LiveCalculateSpellHealing, b::LivePhysicalDamageWithDebuffs};
  for (const auto callback : callbacks) {
    CHECK(std::ranges::count(Battle_statCalculators, callback) == 1);
    CHECK(callback(&f.action, &f.source, &f.target, 0, 0) == 100);
  }
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80081148) ==
        reinterpret_cast<void*>(&b::LiveCalculatePhysicalHitDamage));
}
