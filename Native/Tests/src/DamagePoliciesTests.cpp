#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <span>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Battle/DamagePolicies.hpp"
#include "Battle/DamagePoliciesLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
#include "Items/EquipmentLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
}

namespace {
namespace b = VS::Battle;
using Vitals = std::array<b::StatFactorVital, 6>;
using Amounts = std::array<std::int16_t, 6>;
using Loss = std::array<std::int16_t, 8>;
using Gems = std::array<VS::Items::EquipmentPart, 3>;
std::int16_t LowHalf(int value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
struct Fixture {
  decltype(std::to_array(Battle_actors)) saved = std::to_array(Battle_actors);
  Loss saved_loss = std::to_array(Battle_limbArmorDpLoss);
  decltype(std::to_array(Battle_statCalculators)) saved_callbacks =
      std::to_array(Battle_statCalculators);
  std::uint32_t saved_last = Battle_lastActionStatValue;
  Battle_ActionState* saved_state = Battle_actionState;
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  Battle_ActionDefinition action{};
  Battle_HitResult hit{};
  Fixture() {
    std::fill(std::begin(Battle_actors), std::end(Battle_actors), nullptr);
    Battle_actors[40] = &actor;
    actor.stats = &stats;
    stats.actorId = 255;
    hit.target.targetActor = 40;
    hit.targetsPosition = 1;
    Battle_actionState = nullptr;
    for (int i = 0; i < 8; ++i) {
      Battle_limbArmorDpLoss[i] = static_cast<std::int16_t>(200 + i);
    }
    for (auto& limb : hit.limbs) limb.packed = 0xCAFE7FFFU;
  }
  ~Fixture() {
    std::copy(saved.begin(), saved.end(), Battle_actors);
    std::copy(saved_loss.begin(), saved_loss.end(), Battle_limbArmorDpLoss);
    std::copy(saved_callbacks.begin(), saved_callbacks.end(),
              Battle_statCalculators);
    Battle_lastActionStatValue = saved_last;
    Battle_actionState = saved_state;
  }
};
void CheckBinding(std::uint32_t address, void* function) {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto entry =
      std::ranges::find(symbols, address, &Content_ModuleSymbol::address);
  REQUIRE(entry != symbols.end());
  CHECK(entry->target == function);
}
void RelocateTable() {
  const auto tables =
      std::span(Content_modulePointers, Content_modulePointerCount);
  const auto table = std::ranges::find(
      tables, reinterpret_cast<void**>(Battle_statCalculators),
      &Content_ModulePointerTable::target);
  REQUIRE(table != tables.end());
  const std::filesystem::path root = "Native\\data";
  if (!VS::Content::DataStore::Open(root)) {
    SKIP("Extracted Native/data is unavailable");
  }
  auto& store = *VS::Content::DataStore::Get();
  const auto* module = store.Find("BATTLE/BATTLE.PRG");
  REQUIRE(module != nullptr);
  const auto image = store.ReadAll(*module);
  REQUIRE(image.has_value());
  REQUIRE(VS::Content::RelocateModulePointerTable(
      *table, image.value(),
      {Content_moduleSymbols, Content_moduleSymbolCount}));
}
}  // namespace

TEST_CASE("F00394 scans all three slots without stacking or equipment IDs",
          "[damage-policies][F00394]") {
  Gems weapon{}, shield{};
  const auto type = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const auto slot = GENERATE(0, 1, 2);
  weapon.at(slot).gem_effects = type == 1 ? 4 : 3;
  CHECK(b::ApplyDamageGemMultiplier(type, 100, weapon, shield) == 105);
  shield.at(slot).gem_effects = type == 1 ? 8 : 7;
  CHECK(b::ApplyDamageGemMultiplier(type, 100, weapon, shield) == 100);
  weapon.fill(weapon.at(slot));
  shield.fill(shield.at(slot));
  CHECK(b::ApplyDamageGemMultiplier(type, 100, weapon, shield) == 100);
  weapon = {};
  CHECK(b::ApplyDamageGemMultiplier(type, 100, weapon, shield) == 95);
}
TEST_CASE("F00394 wrong codes negative truncation and overflow",
          "[damage-policies][F00394]") {
  Gems weapon{}, shield{};
  weapon.at(0).gem_effects = 4;
  shield.at(0).gem_effects = 8;
  CHECK(b::ApplyDamageGemMultiplier(0, -101, weapon, shield) == -101);
  shield = {};
  CHECK(b::ApplyDamageGemMultiplier(1, -101, weapon, shield) == -106);
  CHECK(b::ApplyDamageGemMultiplier(1, 32767, weapon, shield) == -31131);
  CHECK(b::ApplyDamageGemMultiplier(1, -32768, weapon, shield) == 31130);
  weapon.at(0).gem_effects = 9;
  CHECK(b::ApplyDamageGemMultiplier(1, 100, weapon, shield) == 100);
}
TEST_CASE("F00394 live uses passed stats without actor table or drawn state",
          "[damage-policies][F00394]") {
  Fixture f;
  Battle_ActorStats target{};
  Battle_actors[40] = nullptr;
  f.action.type = 1;
  f.stats.weapon.gems[2].gemEffects = 4;
  target.shield.gems[2].gemEffects = 8;
  CHECK(b::LiveApplyDamageGemMultiplier(&f.action, 100, &f.stats, &target) ==
        100);
  target.shield.gems[2].gemEffects = 0;
  target.weapon.gems[0].gemEffects = 8;
  CHECK(b::LiveApplyDamageGemMultiplier(&f.action, 100, &f.stats, &target) ==
        105);
  CheckBinding(0x8007FE5C,
               reinterpret_cast<void*>(b::LiveApplyDamageGemMultiplier));
}

TEST_CASE("F00396 all categories and active signed maximum masks",
          "[damage-policies][F00396]") {
  const auto category = GENERATE(Catch::Generators::range(0, 16));
  const auto mask = GENERATE(Catch::Generators::range(0, 64));
  Vitals vitals{};
  Amounts hp{};
  hp.fill(32760);
  Loss dp{};
  dp.fill(-123);
  std::array<std::uint8_t, 6> splash{0, 100, 128, 255, 50, 1};
  int active = 0;
  for (int i = 0; i < 6; ++i) {
    if ((mask & (1 << i)) != 0) {
      vitals.at(i).maximum = (i & 1) != 0 ? -1 : 1;
      ++active;
    }
  }
  REQUIRE(
      b::DistributeHitLimbDamage(category, 3, -101, vitals, splash, hp, dp));
  for (int i = 0; i < 6; ++i) {
    int share = 0;
    if (category <= 4 && vitals.at(i).maximum != 0) share = -101 / active;
    if (category == 5 || category == 7 || category == 14) {
      share = -101 * splash.at(i) / 100;
    }
    if ((category == 6 || category == 8 || category == 11 || category == 12) &&
        i == 3) {
      share = -101;
    }
    CHECK(dp.at(i) == share);
    CHECK(hp.at(i) == LowHalf(32760 + share));
  }
  CHECK(dp.at(6) == -123);
  CHECK(dp.at(7) == -123);
}
TEST_CASE("F00396 splash unsigned percentages and all selected limbs wrap",
          "[damage-policies][F00396]") {
  const auto limb = GENERATE(0, 1, 2, 3, 4, 5);
  const auto percent = GENERATE(0, 100, 128, 255);
  const auto amount = GENERATE(-32768, -101, 32767);
  std::array<std::uint8_t, 6> splash{};
  splash.fill(static_cast<std::uint8_t>(percent));
  Amounts hp{};
  hp.fill(32767);
  Loss dp{};
  REQUIRE(b::DistributeHitLimbDamage(5, limb, amount, {}, splash, hp, dp));
  CHECK(dp.at(limb) == LowHalf(amount * percent / 100));
  CHECK(hp.at(limb) == LowHalf(32767 + amount * percent / 100));
  REQUIRE(b::DistributeHitLimbDamage(6, limb, amount, {}, {}, hp, dp));
  CHECK(dp.at(limb) == amount);
}
TEST_CASE("F00396 only used indices and stats are required",
          "[damage-policies][F00396]") {
  Amounts hp{};
  Loss dp{};
  Vitals vitals{};
  const auto limb = GENERATE(-128, -1, 6, 127);
  CHECK(b::DistributeHitLimbDamage(0, limb, 100, vitals, {}, hp, dp));
  CHECK(b::DistributeHitLimbDamage(15, limb, 100, {}, {}, hp, dp));
  CHECK_FALSE(b::DistributeHitLimbDamage(0, 0, 100, {}, {}, hp, dp));
  CHECK_FALSE(b::DistributeHitLimbDamage(5, limb, 100, {}, {}, hp, dp));
  CHECK_FALSE(b::DistributeHitLimbDamage(6, limb, 100, {}, {}, hp, dp));
  CHECK_FALSE(b::DistributeHitLimbDamage(5, 0, 100, {}, {}, hp, dp));
}
TEST_CASE("F00396 live unused stats stay null and neighboring DP stays intact",
          "[damage-policies][F00396]") {
  Fixture f;
  const auto category = GENERATE(6, 8, 9, 10, 11, 12, 13, 15);
  f.action.category = category;
  f.actor.stats = nullptr;
  const bool selected =
      category == 6 || category == 8 || category == 11 || category == 12;
  f.hit.target.targetLimb = selected ? 5 : -128;
  b::LiveDistributeHitLimbDamage(&f.action, &f.hit, 2);
  for (int i = 0; i < 6; ++i) {
    CHECK(f.hit.limbs[i].packed ==
          (selected && i == 5 ? 0xCAFE8001U : 0xCAFE7FFFU));
    CHECK(Battle_limbArmorDpLoss[i] == (selected && i == 5 ? 2 : 0));
  }
  CHECK(Battle_limbArmorDpLoss[6] == 206);
  CHECK(Battle_limbArmorDpLoss[7] == 207);
  CheckBinding(0x80080000,
               reinterpret_cast<void*>(b::LiveDistributeHitLimbDamage));
}
TEST_CASE("F00396 live splash selects each unsigned row ignoring position",
          "[damage-policies][F00396]") {
  Fixture f;
  const auto limb = GENERATE(0, 1, 2, 3, 4, 5);
  f.action.category = 14;
  f.hit.target.targetLimb = static_cast<signed char>(limb);
  f.stats.dpSplash[limb][2] = static_cast<char>(255);
  b::LiveDistributeHitLimbDamage(&f.action, &f.hit, 32767);
  CHECK(Battle_limbArmorDpLoss[2] == 18019);
  CHECK(f.hit.limbs[2].packed == 0xCAFEC662U);
  CHECK(Battle_limbArmorDpLoss[7] == 207);
}
TEST_CASE("F00396 live even shares count signed maxima and ignore limb",
          "[damage-policies][F00396]") {
  Fixture f;
  f.action.category = 4;
  f.hit.target.targetLimb = -128;
  f.stats.limbs[0].maxHp = -100;
  f.stats.limbs[4].maxHp = 100;
  b::LiveDistributeHitLimbDamage(&f.action, &f.hit, 101);
  CHECK(Battle_limbArmorDpLoss[0] == 50);
  CHECK(Battle_limbArmorDpLoss[4] == 50);
  CHECK(Battle_limbArmorDpLoss[2] == 0);
  CHECK(f.hit.limbs[0].packed == 0xCAFE8031U);
  CHECK(f.hit.limbs[2].packed == 0xCAFE7FFFU);
  f.stats.limbs[0].maxHp = 0;
  f.stats.limbs[4].maxHp = 0;
  b::LiveDistributeHitLimbDamage(&f.action, &f.hit, 101);
  CHECK(Battle_limbArmorDpLoss[0] == 0);
  CHECK(f.hit.limbs[0].packed == 0xCAFE8031U);
}

TEST_CASE("F00397 zero and cancelling totals preserve every stale amount",
          "[damage-policies][F00397]") {
  const auto cancellation = GENERATE(false, true);
  Vitals vitals{};
  if (cancellation) {
    vitals.at(0) = {0, 100};
    vitals.at(1) = {200, 100};
  }
  Amounts hp{1, 2, 3, 4, 5, 6};
  Loss dp{10, 20, 30, 40, 50, 60, 70, 80};
  b::DistributeLimbRecovery(-32768, vitals, hp, dp);
  CHECK(hp == Amounts{1, 2, 3, 4, 5, 6});
  CHECK(dp == Loss{10, 20, 30, 40, 50, 60, 70, 80});
}
TEST_CASE("F00397 negative totals include nonzero negative maxima",
          "[damage-policies][F00397]") {
  Vitals vitals{};
  vitals.at(0) = {100, -1};
  vitals.at(2) = {1, 0};
  vitals.at(5) = {50, 49};
  Amounts hp{32767, 2, 3, 4, 5, -32768};
  Loss dp{10, 20, 30, 40, 50, 60, 70, 80};
  b::DistributeLimbRecovery(-101, vitals, hp, dp);
  CHECK(dp == Loss{-100, 20, 30, 40, 50, 0, 70, 80});
  CHECK(hp == Amounts{32567, 2, 3, 4, 5, -32768});
}
TEST_CASE("F00397 narrows large signed quotient before doubled HP addition",
          "[damage-policies][F00397]") {
  Vitals vitals{};
  vitals.at(0) = {-32768, 32767};
  vitals.at(1) = {32767, -32767};
  Amounts hp{};
  Loss dp{};
  const auto amount = GENERATE(-32768, -1, 1, 32767);
  b::DistributeLimbRecovery(static_cast<std::int16_t>(amount), vitals, hp, dp);
  CHECK(dp.at(0) == LowHalf(amount * 65535));
  CHECK(dp.at(1) == LowHalf(amount * -65534));
  CHECK(hp.at(0) == LowHalf(2 * LowHalf(amount * 65535)));
  CHECK(hp.at(1) == LowHalf(2 * LowHalf(amount * -65534)));
}
TEST_CASE("F00397 live ignores action and preserves inactive hit neighbors",
          "[damage-policies][F00397]") {
  Fixture f;
  f.hit.target.targetLimb = -128;
  f.stats.limbs[2].maxHp = 100;
  f.stats.limbs[2].hp = 50;
  b::LiveDistributeLimbRecovery(nullptr, &f.hit, 32767);
  CHECK(Battle_limbArmorDpLoss[2] == 32767);
  CHECK(Battle_limbArmorDpLoss[0] == 200);
  CHECK(Battle_limbArmorDpLoss[7] == 207);
  CHECK(f.hit.limbs[2].packed == 0xCAFE7FFDU);
  CHECK(f.hit.limbs[0].packed == 0xCAFE7FFFU);
  CheckBinding(0x800801E0,
               reinterpret_cast<void*>(b::LiveDistributeLimbRecovery));
}
TEST_CASE("F00396 equipment wear consumes only the current first six DP losses",
          "[damage-policies][F00396][items]") {
  Fixture f;
  f.stats.actorId = 40;
  f.actor.weaponDrawn = 1;
  f.stats.weapon.currentDp = 1000;
  f.stats.shield.currentDp = 500;
  for (auto& limb : f.stats.limbs) limb.armor.currentDp = 500;
  f.action.category = 6;
  f.hit.target.targetLimb = 2;
  b::LiveDistributeHitLimbDamage(&f.action, &f.hit, 100);
  VS::Items::ConsumeLiveHitEquipmentDp(&f.action, &f.stats, &f.stats, 100);
  CHECK(f.stats.weapon.currentDp == 900);
  CHECK(f.stats.shield.currentDp == 400);
  CHECK(f.stats.limbs[2].armor.currentDp == 400);
  CHECK(f.stats.limbs[0].armor.currentDp == 500);
  CHECK(Battle_limbArmorDpLoss[6] == 206);
}
TEST_CASE(
    "F00397 equipment consumer sees preserved inactive DP from prior damage",
    "[damage-policies][F00397][items]") {
  Fixture f;
  f.stats.actorId = 40;
  f.actor.weaponDrawn = 1;
  for (auto& limb : f.stats.limbs) limb.armor.currentDp = 500;
  f.action.category = 6;
  f.hit.target.targetLimb = 2;
  b::LiveDistributeHitLimbDamage(&f.action, &f.hit, 100);
  f.stats.limbs[0].maxHp = 100;
  b::LiveDistributeLimbRecovery(nullptr, &f.hit, 25);
  VS::Items::ConsumeLiveHitEquipmentDp(&f.action, &f.stats, &f.stats, 0);
  CHECK(f.stats.limbs[0].armor.currentDp == 475);
  CHECK(f.stats.limbs[2].armor.currentDp == 400);
  CHECK(f.stats.limbs[1].armor.currentDp == 500);
  CHECK(Battle_limbArmorDpLoss[7] == 207);
}

TEST_CASE("F00408 last value ignores high word and every callback argument",
          "[damage-policies][F00408]") {
  Fixture f;
  const auto high = GENERATE(0U, 0x12340000U, 0xFFFF0000U);
  const auto low = GENERATE(0U, 1U, 0x7FFFU, 0x8000U, 0xFFFFU);
  Battle_lastActionStatValue = high | low;
  CHECK(b::LastActionStatValue(high | low) == LowHalf(low));
  CHECK(b::LiveLastActionStatValue(nullptr, nullptr, nullptr, -1, -1) ==
        LowHalf(low));
  CheckBinding(0x80081138, reinterpret_cast<void*>(b::LiveLastActionStatValue));
}
TEST_CASE("F00432 full unsigned last value wraps product before division",
          "[damage-policies][F00432]") {
  Fixture f;
  Battle_lastActionStatValue = 0x80000001U;
  f.action.hits[0].statFactor = 2;
  f.action.hits[1].statFactor = 31;
  CHECK(b::ScaleLastActionStatValue(0x80000001U, 2) == 0);
  CHECK(b::ScaleLastActionStatValue(0xFFFFFFFFU, 31) == -26218);
  CHECK(b::LiveScaleLastActionStatValue(&f.action, nullptr, nullptr, 0, -1) ==
        0);
  CHECK(b::LiveScaleLastActionStatValue(&f.action, nullptr, nullptr, 1, -1) ==
        -13105);
  CheckBinding(0x8008384C,
               reinterpret_cast<void*>(b::LiveScaleLastActionStatValue));
}
TEST_CASE(
    "F00432 each five bit factor scales both live hit slots independently",
    "[damage-policies][F00432]") {
  Fixture f;
  const auto factor = GENERATE(Catch::Generators::range(0, 32));
  const auto slot = GENERATE(0, 1);
  f.action.hits[slot].statFactor = factor;
  f.action.hits[1 - slot].statFactor = 31 - factor;
  Battle_lastActionStatValue = 0xFFFF8000U;
  const std::uint32_t product = 0xFFFF8000U * factor;
  CHECK(b::LiveScaleLastActionStatValue(&f.action, nullptr, nullptr, slot, 0) ==
        LowHalf(product / 10));
}
TEST_CASE("F00434 risk uses target slot and signed half without caps or gates",
          "[damage-policies][F00434]") {
  Fixture f;
  const auto risk = GENERATE(0, 100, 255, 256, 32767, 32768, 65535);
  f.stats.risk = static_cast<std::uint16_t>(risk);
  f.hit.target.targetLimb = -128;
  Battle_HitResult source{};
  source.target.targetActor = 255;
  CHECK(b::TargetRisk(risk) == LowHalf(risk));
  CHECK(b::LiveTargetRisk(nullptr, &source, &f.hit, -1, -1) == LowHalf(risk));
  CheckBinding(0x800838BC, reinterpret_cast<void*>(b::LiveTargetRisk));
}
TEST_CASE("C6 stat callbacks retain actual SHORT ABI in relocated table",
          "[damage-policies][F00408][F00432][F00434]") {
  Fixture f;
  RelocateTable();
  const std::array callbacks{b::LiveLastActionStatValue,
                             b::LiveScaleLastActionStatValue,
                             b::LiveTargetRisk};
  Battle_lastActionStatValue = 0xFFFF8000U;
  f.action.hits[1].statFactor = 3;
  f.stats.risk = 65535;
  constexpr std::array kExpected{-32768, 29491, -1};
  for (std::size_t i = 0; i < callbacks.size(); ++i) {
    const auto slot =
        std::find(std::begin(Battle_statCalculators),
                  std::end(Battle_statCalculators), callbacks.at(i));
    REQUIRE(slot != std::end(Battle_statCalculators));
    CHECK((*slot)(&f.action, nullptr, &f.hit, 1, -1) == kExpected.at(i));
  }
}
