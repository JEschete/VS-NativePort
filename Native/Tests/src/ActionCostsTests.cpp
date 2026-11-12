#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/ActionCosts.hpp"
#include "Battle/ActionCostsLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
#include "Items/EquipmentLive.hpp"
#include "Items/EquipmentStats.hpp"
extern "C" {
#include "Actors/CharacterInit.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
}

namespace {
namespace battle = VS::Battle;
struct CostActorStub final : battle::ActionCostActor {
  battle::ActionResources resources{100, 100, 0, 100, 2, 1};
  std::uint16_t timer{};
  std::uint8_t bonus{}, delay{};
  std::uint32_t statuses{};
  int payments{}, timer_writes{};
  battle::ActionResources Resources() override { return resources; }
  void Pay(battle::CostResource resource, std::uint16_t value) override {
    ++payments;
    switch (resource) {
      case battle::CostResource::kMp:
        resources.mp = std::bit_cast<std::int16_t>(value);
        break;
      case battle::CostResource::kRisk:
        resources.risk = value;
        break;
      case battle::CostResource::kHp:
        resources.hp = std::bit_cast<std::int16_t>(value);
        break;
      case battle::CostResource::kWeaponPp:
        resources.weapon_pp = value;
        break;
      case battle::CostResource::kNone:
        FAIL("no resource must not be paid");
    }
  }
  std::uint16_t RecoveryTimer() override { return timer; }
  void SetRecoveryTimer(std::uint16_t value) override {
    timer = value;
    ++timer_writes;
  }
  std::uint8_t RecoveryBonus() override { return bonus; }
  std::uint8_t WeaponRecoveryDelay() override { return delay; }
  std::uint32_t Statuses() override { return statuses; }
};
struct CostDefinitionStub final : battle::ActionCostDefinition {
  std::uint8_t type = 1, cost = 10, delay{};
  bool unlocked = true;
  std::uint8_t Type() override { return type; }
  std::uint8_t Cost() override { return cost; }
  std::uint8_t RecoveryDelay() override { return delay; }
  bool Unlocked() override { return unlocked; }
};
struct World final : battle::ActionCostWorld {
  CostDefinitionStub definition, other_definition;
  CostActorStub actor, other;
  std::optional<std::uint32_t> chain = 0, drawn = 1;
  bool missing_definition{}, missing_actor{}, other_actor{}, null_stats{};
  bool other_definition_selected{};
  int definitions{}, actors{}, chain_reads{}, drawn_reads{}, evaluations{};
  std::uint32_t last_action{};
  int last_update{}, last_slot{};
  battle::ActionCostActor* last_owner{};
  std::function<void()> callback;
  battle::ActionCostDefinition* Definition(std::uint32_t action) override {
    ++definitions;
    if (missing_definition || action >= 256) return nullptr;
    return other_definition_selected ? &other_definition : &definition;
  }
  std::optional<battle::ActionCostActor*> Actor(int slot) override {
    ++actors;
    last_slot = slot;
    if (missing_actor || slot < 0 || slot >= 42) return std::nullopt;
    if (null_stats) return nullptr;
    return other_actor ? &other : &actor;
  }
  std::optional<std::uint32_t> ChainStep() override {
    ++chain_reads;
    return chain;
  }
  std::optional<std::uint32_t> WeaponDrawn(int) override {
    ++drawn_reads;
    return drawn;
  }
  std::optional<battle::ActionCost> Evaluate(std::uint32_t action,
                                             battle::ActionCostActor* owner,
                                             int update) override {
    ++evaluations;
    last_action = action;
    last_owner = owner;
    last_update = update;
    const auto result = ActionCostWorld::Evaluate(action, owner, update);
    if (callback) callback();
    return result;
  }
  std::optional<battle::ActionCost> Cost(std::uint32_t action = 22,
                                         int update = 0) {
    return battle::ActionCostAndAvailability(action, &actor, update, *this);
  }
};
struct LiveFixture {
  decltype(std::to_array(Battle_actors)) saved = std::to_array(Battle_actors);
  decltype(std::to_array(Battle_actionDefinitions)) definitions =
      std::to_array(Battle_actionDefinitions);
  Battle_ActionState* saved_state = Battle_actionState;
  Battle_ActionState state{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  LiveFixture() {
    std::fill(std::begin(Battle_actors), std::end(Battle_actors), nullptr);
    Battle_actors[0] = &actor;
    Battle_actors[41] = &actor;
    actor.stats = &stats;
    actor.weaponDrawn = 2;
    Battle_actionState = &state;
    Battle_actionDefinitions[22] = {};
    Battle_actionDefinitions[22].type = 6;
    Battle_actionDefinitions[22].unlocked = 1;
  }
  ~LiveFixture() {
    std::copy(saved.begin(), saved.end(), std::begin(Battle_actors));
    std::copy(definitions.begin(), definitions.end(),
              std::begin(Battle_actionDefinitions));
    Battle_actionState = saved_state;
  }
};
struct EquipHost final : VS::Items::EquipStatusHost {
  std::int32_t AshleyStatuses() override { return 0; }
  void QueueStatusLabels(std::int32_t) override {
    FAIL("empty statuses must not queue labels");
  }
  void RemoveStatuses(std::int32_t) override {
    FAIL("empty statuses must not be removed");
  }
};
template <typename T>
auto Bytes(const T& value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}
std::vector<std::uint8_t> ReadAsset(const char* name) {
  const std::filesystem::path root = "Native\\data";
  if (!std::filesystem::exists(root / VS::Content::DataStore::ManifestName)) {
    SKIP("Extracted Native data is unavailable");
  }
  REQUIRE(VS::Content::DataStore::Open(root));
  auto* store = VS::Content::DataStore::Get();
  REQUIRE(store != nullptr);
  const auto* asset = store->Find(name);
  REQUIRE(asset != nullptr);
  const auto bytes = store->ReadAll(*asset);
  REQUIRE(bytes.has_value());
  if (!bytes) std::abort();
  return bytes.value();
}
template <typename T>
T ReadRecord(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
  REQUIRE(offset + sizeof(T) <= bytes.size());
  T record{};
  std::memcpy(&record, bytes.data() + offset, sizeof(T));
  return record;
}
Actors_CharacterInit ZoneNineActor(std::size_t index) {
  const auto zone = ReadAsset("MAP/ZONE009.ZND");
  const auto section = ReadRecord<std::uint32_t>(zone, 8);
  const auto count = ReadRecord<std::uint32_t>(zone, section);
  REQUIRE(index < count);
  return ReadRecord<Actors_CharacterInit>(
      zone, section + 4 + count * 8 + index * sizeof(Actors_CharacterInit));
}
}  // namespace

TEST_CASE("Action costs select every outer type", "[action-costs][F00510]") {
  World world;
  const auto type = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  world.definition.type = static_cast<std::uint8_t>(type);
  const auto cost = world.Cost();
  REQUIRE(cost);
  if (!cost) std::abort();
  CHECK(cost.value().requirements_met ==
        ((type >= 1 && type <= 4) || type == 6));
  CHECK(cost.value().effective_type == (type == 6 ? 2 : type));
  CHECK(cost.value().cost == (type == 6 ? 1 : 10));
  CHECK(world.actor.payments == 0);
  CHECK(world.chain_reads == (type == 2 || type == 6 ? 1 : 0));
}
TEST_CASE("Weapon costs use their declared resource",
          "[action-costs][F00510]") {
  World world;
  world.definition.type = 6;
  world.actor.resources.weapon_cost = 10;
  world.actor.resources.weapon_type =
      static_cast<std::uint8_t>(GENERATE(1, 2, 3, 4));
  const auto result = world.Cost(22, -1);
  REQUIRE(result);
  if (!result) std::abort();
  CHECK(result.value().requirements_met);
  CHECK(result.value().cost == 10);
  CHECK(world.actor.payments == 1);
  CHECK(result.value().effective_type == world.actor.resources.weapon_type);
}
TEST_CASE("Empty weapon has explicit no-resource cost without a balance",
          "[action-costs][F00510]") {
  World world;
  world.definition.type = 6;
  world.actor.resources = {-32768, -32768, 65535, 65535, 0, 0};
  world.chain.reset();
  const auto result = world.Cost(22, -2147483647 - 1);
  REQUIRE(result);
  if (!result) std::abort();
  CHECK(result.value().resource == battle::CostResource::kNone);
  CHECK(result.value().Packed() == 0x01000000);
  CHECK(world.actor.payments == 0);
  CHECK(world.chain_reads == 0);
}
TEST_CASE("Unknown weapon resource combinations refuse instead of guessing",
          "[action-costs][F00510]") {
  World world;
  world.definition.type = 6;
  world.actor.resources.weapon_type =
      static_cast<std::uint8_t>(GENERATE(0, 5, 6, 7, 255));
  world.actor.resources.weapon_cost =
      static_cast<std::uint8_t>(GENERATE(0, 1, 255));
  if (world.actor.resources.weapon_type == 0 &&
      world.actor.resources.weapon_cost == 0) {
    return;
  }
  CHECK_FALSE(world.Cost(22, 1));
  CHECK(world.actor.payments == 0);
  CHECK(world.chain_reads == 0);
}
TEST_CASE("Outer no-resource definitions retain defined zero balance",
          "[action-costs][F00510]") {
  World world;
  world.definition.type = static_cast<std::uint8_t>(GENERATE(0, 5, 7));
  world.definition.cost = static_cast<std::uint8_t>(GENERATE(0, 1, 255));
  const auto result = battle::ActionCostAndAvailability(0, nullptr, 1, world);
  REQUIRE(result);
  if (!result) std::abort();
  CHECK(result.value().requirements_met == (world.definition.cost == 0));
  CHECK(result.value().cost == world.definition.cost);
  CHECK(world.actor.payments == 0);
}
TEST_CASE("Signed MP and HP and unsigned PP keep their full widths",
          "[action-costs][F00510]") {
  World world;
  const int value = GENERATE(-32768, -1, 0, 9, 10, 11, 32767);
  const int type = GENERATE(1, 3, 4);
  world.definition.type = static_cast<std::uint8_t>(type);
  world.actor.resources.hp = static_cast<std::int16_t>(value);
  world.actor.resources.mp = static_cast<std::int16_t>(value);
  world.actor.resources.weapon_pp = static_cast<std::uint16_t>(value);
  const auto result = world.Cost(22, 7);
  REQUIRE(result);
  if (!result) std::abort();
  const int available = type == 4 ? static_cast<std::uint16_t>(value) : value;
  const bool enough = available >= (type == 3 ? 11 : 10);
  CHECK(result.value().requirements_met == enough);
  CHECK(result.value().cost == (type == 3 && !enough ? 11 : 10));
  const int after = enough ? available - 10 : available;
  CHECK((type == 1   ? world.actor.resources.mp
         : type == 3 ? world.actor.resources.hp
                     : world.actor.resources.weapon_pp) == after);
  CHECK(world.actor.payments == 1);
}
TEST_CASE("Risk is allowed beyond balance and clamps only insufficient risk",
          "[action-costs][F00510]") {
  World world;
  world.definition.type = 2;
  const int risk = GENERATE(0, 89, 90, 91, 100, 101, 65535);
  world.actor.resources.risk = static_cast<std::uint16_t>(risk);
  const auto result = world.Cost(22, 1);
  REQUIRE(result);
  if (!result) std::abort();
  CHECK(result.value().requirements_met);
  CHECK(world.actor.resources.risk == (risk <= 90 ? risk + 10 : 100));
  CHECK(result.value().cost == 10);
}
TEST_CASE("Chain surcharge wraps square before unsigned division",
          "[action-costs][F00510]") {
  World world;
  world.definition.type = 2;
  const auto action = GENERATE(21U, 22U, 39U, 40U);
  const auto chain =
      GENERATE(9U, 10U, 65535U, 65536U, 0x80000000U, 0xFFFFFFFFU);
  world.chain = chain;
  const auto result = world.Cost(action);
  REQUIRE(result);
  if (!result) std::abort();
  const auto extra =
      chain >= 10 && action >= 22 && action <= 39 ? (chain * chain) / 10U : 0U;
  CHECK(result.value().cost == 10U + extra);
  CHECK(result.value().Packed() == 0x01020000U + 10U + extra);
  CHECK(world.chain_reads == 1);
}
TEST_CASE("Packed action costs preserve carries into availability",
          "[action-costs][F00510][F00548]") {
  World world;
  world.definition.type = 2;
  world.chain = 65535;
  const auto cost = world.Cost();
  REQUIRE(cost);
  if (!cost) std::abort();
  CHECK(cost.value().Packed() == 0x1A9B6670);
  CHECK((cost.value().Packed() & 0xFF000000) != 0x01000000);
  CHECK(battle::ActorActionRestrictionFlags(1, 22, world) == 0);
}
TEST_CASE("Risk always needs chain state even outside surcharge IDs",
          "[action-costs][F00510]") {
  World world;
  world.chain.reset();
  world.definition.type = 2;
  CHECK_FALSE(world.Cost(255));
  world.definition.type = 1;
  CHECK(world.Cost(255).has_value());
  CHECK_FALSE(world.Cost(256));
}
TEST_CASE("Preview resolves full actor slot and runs real cost without writes",
          "[action-costs][F00511]") {
  World world;
  const auto slot = GENERATE(0, 41);
  CHECK(battle::PreviewActorActionCost(255, slot, world));
  CHECK(world.last_slot == slot);
  CHECK(world.last_action == 255);
  CHECK(world.last_owner == &world.actor);
  CHECK(world.last_update == 0);
  CHECK(world.evaluations == 1);
  CHECK(world.actor.payments == 0);
}
TEST_CASE("Preview refuses used actor definition and chain state",
          "[action-costs][F00511]") {
  World world;
  CHECK_FALSE(battle::PreviewActorActionCost(0, -1, world));
  CHECK_FALSE(battle::PreviewActorActionCost(0, 256, world));
  CHECK_FALSE(battle::PreviewActorActionCost(-1, 0, world));
  world.missing_actor = true;
  CHECK_FALSE(battle::PreviewActorActionCost(0, 0, world));
  world.missing_actor = false;
  world.definition.type = 2;
  world.chain.reset();
  CHECK_FALSE(battle::PreviewActorActionCost(0, 0, world));
}
TEST_CASE("Preview forwards absent unused stats to the real cost policy",
          "[action-costs][F00511]") {
  World world;
  world.null_stats = true;
  world.definition.type = 5;
  CHECK(battle::PreviewActorActionCost(0, 0, world));
  CHECK(world.last_owner == nullptr);
  world.definition.type = 1;
  CHECK_FALSE(battle::PreviewActorActionCost(0, 0, world));
  LiveFixture fixture;
  fixture.actor.stats = nullptr;
  Battle_actionDefinitions[22].type = 5;
  battle::LivePreviewActorActionCost(22, 0);
}
TEST_CASE("Recovery rejection does not resolve unused data",
          "[action-costs][F00512]") {
  World world;
  world.missing_definition = true;
  CHECK(battle::AddActionRecoveryDelay(0xFFFFFFFF, nullptr, 0, world));
  CHECK(battle::AddActionRecoveryDelay(22, nullptr, -1, world));
  CHECK(battle::AddActionRecoveryDelay(53, nullptr, 1, world));
  CHECK(world.definitions == 0);
  CHECK(world.actor.timer_writes == 0);
}
TEST_CASE("Recovery uses action or weapon delay bonus and low timer half",
          "[action-costs][F00512]") {
  World world;
  world.definition.delay = static_cast<std::uint8_t>(GENERATE(0, 1, 254, 255));
  world.actor.delay = 255;
  world.actor.bonus = 63;
  world.actor.timer = 65535;
  const auto action = GENERATE(21U, 54U, 255U);
  CHECK(battle::AddActionRecoveryDelay(action, &world.actor, -1, world));
  const int base = world.definition.delay == 255 ? 255 : world.definition.delay;
  CHECK(world.actor.timer ==
        static_cast<std::uint16_t>(65535 + ((base + 63) << 8)));
  CHECK(world.actor.timer_writes == 1);
}
TEST_CASE("Recovery refuses only used definitions and owners",
          "[action-costs][F00512]") {
  World world;
  CHECK_FALSE(battle::AddActionRecoveryDelay(256, &world.actor, 1, world));
  CHECK_FALSE(battle::AddActionRecoveryDelay(21, nullptr, 1, world));
  CHECK(world.actor.timer_writes == 0);
}
TEST_CASE("Restriction flags separate recovery cost and player unlock",
          "[action-costs][F00548]") {
  World world;
  world.actor.timer = 1;
  world.actor.resources.mp = 0;
  world.definition.unlocked = false;
  CHECK(battle::ActorActionRestrictionFlags(0, 0, world) == 7);
  CHECK(battle::ActorActionRestrictionFlags(1, 0, world) == 3);
  CHECK(world.actor.payments == 0);
}
TEST_CASE("Restrictions apply status and drawn full word at signed boundaries",
          "[action-costs][F00548]") {
  World world;
  const auto action = GENERATE(140, 141, 223, 224);
  const auto status = GENERATE(0U, 0x1000U, 0x2000U);
  world.actor.statuses = status;
  world.drawn = GENERATE(0U, 2U, 0x80000000U);
  const bool blocked = action < 141 ? status == 0x1000
                       : action < 224
                           ? status == 0x2000 || world.drawn.value() == 0
                           : false;
  CHECK(battle::ActorActionRestrictionFlags(1, action, world) ==
        (blocked ? 4U : 0U));
  CHECK(world.drawn_reads == (action >= 141 && action < 224 ? 1 : 0));
}
TEST_CASE("Restrictions cache timer and owner but read current later fields",
          "[action-costs][F00548]") {
  World world;
  world.actor.timer = 7;
  world.actor.statuses = 0;
  world.callback = [&] {
    world.actor.timer = 0;
    world.actor.statuses = 0x2000;
    world.other_actor = true;
    world.drawn = 2;
    world.definition.unlocked = false;
  };
  CHECK(battle::ActorActionRestrictionFlags(0, 141, world) == 5);
  CHECK(world.actors == 1);
  CHECK(world.drawn_reads == 1);
  CHECK(world.last_owner == &world.actor);
}
TEST_CASE("Restrictions read unlock after cost and reload drawn actor",
          "[action-costs][F00548]") {
  World world;
  world.definition.unlocked = false;
  world.other_definition.unlocked = false;
  world.callback = [&] {
    world.definition.unlocked = true;
    world.other_definition_selected = true;
    world.drawn = 0;
  };
  CHECK(battle::ActorActionRestrictionFlags(0, 140, world) == 0);
  CHECK(battle::ActorActionRestrictionFlags(0, 141, world) == 4);
}
TEST_CASE("Restrictions refuse used fields while skipping unused drawn state",
          "[action-costs][F00548]") {
  World world;
  world.drawn.reset();
  CHECK(battle::ActorActionRestrictionFlags(0, 140, world) == 0);
  CHECK_FALSE(battle::ActorActionRestrictionFlags(0, 141, world));
  CHECK(battle::ActorActionRestrictionFlags(0, 224, world) == 0);
  CHECK_FALSE(battle::ActorActionRestrictionFlags(-1, 224, world));
  CHECK_FALSE(battle::ActorActionRestrictionFlags(42, 224, world));
  CHECK_FALSE(battle::ActorActionRestrictionFlags(0, -1, world));
}
TEST_CASE("Live costs preserve unrelated fields and preview never writes",
          "[action-costs][F00510][F00511]") {
  LiveFixture fixture;
  fixture.stats.currentMp = 30;
  fixture.stats.weapon.actionType = 1;
  fixture.stats.weapon.risk = 10;
  fixture.stats.maxMp = 33;
  fixture.stats.statuses = 0x8000;
  const auto before = Bytes(fixture.stats);
  CHECK(battle::LiveActionCostAndAvailability(22, &fixture.stats, 0) ==
        0x0101000A);
  battle::LivePreviewActorActionCost(22, 41);
  CHECK(Bytes(fixture.stats) == before);
  CHECK(battle::LiveActionCostAndAvailability(22, &fixture.stats, -1) ==
        0x0101000A);
  fixture.stats.currentMp = 30;
  CHECK(Bytes(fixture.stats) == before);
}
TEST_CASE("Actual Items empty and weapon transitions feed live cost queries",
          "[action-costs][F00510][items]") {
  LiveFixture fixture;
  VS::Items::InventoryContents inventory{};
  VS::Items::WeaponLoadout weapon{};
  REQUIRE(VS::Items::LoadInventoryWeaponStats(weapon, nullptr, inventory) ==
          VS::Items::ItemsStatus::kOk);
  fixture.stats.weapon = std::bit_cast<Items_WeaponLoadout>(weapon);
  Battle_actionState = nullptr;
  CHECK(battle::LiveActionCostAndAvailability(22, &fixture.stats, 1) ==
        0x01000000);
  Items_WeaponSpawnLoadout natural{};
  VS::Items::BuildLiveWeaponStatsFromParts(&fixture.stats.weapon, &natural);
  CHECK(battle::LiveActionCostAndAvailability(22, &fixture.stats, 0) ==
        0x01000000);
  natural.blade.costType = 2;
  natural.blade.cost = 1;
  VS::Items::BuildLiveWeaponStatsFromParts(&fixture.stats.weapon, &natural);
  Battle_actionState = &fixture.state;
  CHECK(battle::LiveActionCostAndAvailability(22, &fixture.stats, 1) ==
        0x01020001);
  CHECK(fixture.stats.risk == 1);
  REQUIRE(VS::Items::LoadInventoryWeaponStats(weapon, nullptr, inventory) ==
          VS::Items::ItemsStatus::kOk);
  fixture.stats.weapon = std::bit_cast<Items_WeaponLoadout>(weapon);
  CHECK(battle::LiveActionCostAndAvailability(22, &fixture.stats, 1) ==
        0x01000000);
}
TEST_CASE("Live recovery and restrictions preserve their exact interfaces",
          "[action-costs][F00512][F00548]") {
  LiveFixture fixture;
  Battle_actionDefinitions[54] = {};
  Battle_actionDefinitions[54].recoveryDelay = 255;
  fixture.stats.weapon.recoveryDelay = static_cast<char>(255);
  fixture.stats.recoveryDelayBonus = 63;
  fixture.stats.actionRecoveryTimer = 65535;
  auto expected = fixture.stats;
  expected.actionRecoveryTimer = 15871;
  battle::LiveAddActionRecoveryDelay(54, &fixture.stats, -1);
  CHECK(Bytes(fixture.stats) == Bytes(expected));
  battle::LiveAddActionRecoveryDelay(0xFFFFFFFF, nullptr, 0);
  CHECK(battle::LiveActorActionRestrictionFlags(0, 22) == 1);
}
TEST_CASE("Equipping and unequipping preserves the live resource contract",
          "[action-costs][F00510][items]") {
  LiveFixture fixture;
  VS::Items::InventoryContents inventory{};
  auto& blade = inventory.blades.at(0);
  blade.id = 15;
  blade.category = 2;
  blade.cost = 1;
  blade.type_bits = 2 << VS::Items::kBladeCostTypeShift;
  inventory.weapons.at(0).blade = 1;
  VS::Items::AshleyWeaponState ashley;
  EquipHost host;
  REQUIRE(VS::Items::EquipAshleyWeapon(ashley, &inventory.weapons.at(0),
                                       inventory,
                                       host) == VS::Items::ItemsStatus::kOk);
  fixture.stats.weapon = std::bit_cast<Items_WeaponLoadout>(ashley.weapon);
  CHECK(battle::LiveActionCostAndAvailability(22, &fixture.stats, 1) ==
        0x01020001);
  CHECK(fixture.stats.risk == 1);
  REQUIRE(VS::Items::EquipAshleyWeapon(ashley, nullptr, inventory, host) ==
          VS::Items::ItemsStatus::kOk);
  CHECK(ashley.equipped_weapon_category == VS::Items::kUnarmedWeaponCategory);
  fixture.stats.weapon = std::bit_cast<Items_WeaponLoadout>(ashley.weapon);
  CHECK(battle::LiveActionCostAndAvailability(22, &fixture.stats, 1) ==
        0x01000000);
  CHECK(fixture.stats.risk == 1);
}
TEST_CASE(
    "Shipped natural weapon uses real base action without resource payment",
    "[action-costs][F00510][items]") {
  LiveFixture fixture;
  const auto natural = ZoneNineActor(2);
  const auto main = ReadAsset("SLUS_010.40");
  const auto action = natural.limbs[0].actions[0].actionId;
  REQUIRE(action == 1);
  Battle_actionDefinitions[action] = ReadRecord<Battle_ActionDefinition>(
      main, 0x3C1DC + action * sizeof(Battle_ActionDefinition));
  VS::Items::BuildLiveWeaponStatsFromParts(&fixture.stats.weapon,
                                           &natural.weapon);
  fixture.stats.currentHp = natural.hp;
  fixture.stats.currentMp = natural.mp;
  const auto before = Bytes(fixture.stats);
  Battle_actionState = nullptr;
  CHECK(battle::LiveActionCostAndAvailability(action, &fixture.stats, 0) ==
        0x01000000);
  CHECK(battle::LiveActionCostAndAvailability(action, &fixture.stats, 1) ==
        0x01000000);
  CHECK(Bytes(fixture.stats) == before);
}
TEST_CASE(
    "Shipped NPC weapon keeps its resource cost through the Items builder",
    "[action-costs][F00510][items]") {
  LiveFixture fixture;
  const auto armed = ZoneNineActor(9);
  VS::Items::BuildLiveWeaponStatsFromParts(&fixture.stats.weapon,
                                           &armed.weapon);
  CHECK(battle::LiveActionCostAndAvailability(22, &fixture.stats, 1) ==
        0x01020002);
  CHECK(fixture.stats.risk == 2);
}
TEST_CASE(
    "Cost group original module bindings have WORD3 VOID2 VOID3 INT2 arity",
    "[action-costs][F00510][F00511][F00512][F00548]") {
  LiveFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto binding = [&](std::uint32_t address) {
    const auto pointer = VS::Content::ResolveModuleAddress(
        symbols, "BATTLE/BATTLE.PRG", address);
    REQUIRE(pointer != nullptr);
    return pointer;
  };
  using Cost = decltype(&battle::LiveActionCostAndAvailability);
  using Preview = decltype(&battle::LivePreviewActorActionCost);
  using Recovery = decltype(&battle::LiveAddActionRecoveryDelay);
  using Flags = decltype(&battle::LiveActorActionRestrictionFlags);
  CHECK(reinterpret_cast<Cost>(binding(0x8008574C))(22, &fixture.stats, 0) ==
        0x01000000);
  reinterpret_cast<Preview>(binding(0x80085978))(22, 0);
  reinterpret_cast<Recovery>(binding(0x800859B4))(0xFFFFFFFF, nullptr, 0);
  CHECK(reinterpret_cast<Flags>(binding(0x8008A5D0))(0, 22) == 0);
}
