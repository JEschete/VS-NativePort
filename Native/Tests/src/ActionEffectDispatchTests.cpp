#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/ActionEffectDispatch.hpp"
#include "Battle/ActionEffectDispatchLive.hpp"
extern "C" {
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
}

namespace {
namespace battle = VS::Battle;
struct Injuries final : battle::LimbInjuryQueue {
  std::array<std::uint8_t, 2> mobility{4, 5};
  std::array<std::int16_t, 6> hp{2, 2, 2, 2, 2, 2};
  std::array<battle::ActionResultChange, 6> pending{};
  std::array<std::uint8_t, 4> ids{1, 2, 3, 5};
  std::array<std::uint32_t, 32> masks{};
  std::uint32_t active{};
  bool player{};
  std::vector<unsigned> reads;
  std::vector<std::pair<unsigned, bool>> calls;
  std::function<void()> callback;
  std::uint8_t MobilityLimb(std::size_t i) override { return mobility.at(i); }
  std::int16_t CurrentHp(std::uint8_t limb) override {
    reads.push_back(limb);
    return hp.at(limb);
  }
  battle::ActionResultChange PendingHp(std::uint8_t limb) override {
    return pending.at(limb);
  }
  std::uint32_t ActiveStatuses() override { return active; }
  bool PlayerTarget() override { return player; }
  std::uint8_t InjuryRule(std::size_t limb) override { return ids.at(limb); }
  std::uint32_t RuleMask(std::uint8_t rule) override { return masks.at(rule); }
  void QueueStatus(std::uint8_t rule, bool remove) override {
    calls.emplace_back(rule, remove);
    if (callback) callback();
  }
};

TEST_CASE("F00506 predicts zero mobility entries before the logical none gate",
          "[resource-dispatch][F00506]") {
  Injuries queue;
  queue.mobility = {0, 0};
  queue.hp.at(0) = 0;
  battle::DeriveLimbInjuryStatuses(queue);
  CHECK(queue.reads == std::vector<unsigned>{0, 0});
  CHECK(queue.calls.empty());
  queue.mobility = {0, 5};
  queue.hp.at(5) = 1;
  battle::DeriveLimbInjuryStatuses(queue);
  CHECK(queue.calls == std::vector<std::pair<unsigned, bool>>{{4, false}});
}
TEST_CASE("F00506 narrows signed predicted HP before the mobility threshold",
          "[resource-dispatch][F00506]") {
  Injuries queue;
  queue.hp.at(4) = 32767;
  queue.hp.at(5) = 1;
  queue.pending.at(4) = {1, 2};
  battle::DeriveLimbInjuryStatuses(queue);
  CHECK(queue.calls == std::vector<std::pair<unsigned, bool>>{{4, false}});
  queue.calls.clear();
  queue.active = 0x10;
  queue.pending.at(4) = {-1, 3};
  battle::DeriveLimbInjuryStatuses(queue);
  CHECK(queue.calls.empty());
  queue.pending.at(5) = {-1, 1};
  battle::DeriveLimbInjuryStatuses(queue);
  CHECK(queue.calls == std::vector<std::pair<unsigned, bool>>{{4, true}});
}
TEST_CASE("F00506 rereads player identity modes active statuses and rules",
          "[resource-dispatch][F00506]") {
  Injuries queue;
  queue.hp.fill(0);
  queue.callback = [&] {
    queue.player = true;
    queue.active = 0x40;
    queue.ids.at(1) = 6;
    queue.masks.at(6) = 0x40;
    queue.pending.at(1) = {2, 2};
  };
  battle::DeriveLimbInjuryStatuses(queue);
  CHECK(queue.calls ==
        std::vector<std::pair<unsigned, bool>>{
            {4, false}, {1, false}, {6, true}, {3, false}, {5, false}});
}

struct EffectWorld final : battle::ActionEffectWorld {
  bool target_position{}, source_position{}, counted{};
  std::uint8_t actor_id = 1, type = 2, source_slot{}, target_slot = 1;
  std::int16_t hp = 2, pending = 2;
  std::uint32_t statuses{}, removed{}, flag = 10, last = 99;
  std::uint8_t category = 29, learned{};
  std::uint16_t kills = 1, required = 2;
  std::vector<int> events;
  std::function<void()> dispatch_callback, flag_callback;
  bool TargetPosition() override { return target_position; }
  std::uint8_t TargetActorId() override { return actor_id; }
  std::uint32_t TargetStatuses() override { return statuses; }
  std::uint8_t ActionType() override { return type; }
  void QueueExemptDamage() override { events.push_back(1); }
  void ClearLastValue() override { last = 0; }
  void Dispatch(int hit, int execute) override {
    events.insert(events.end(), {2, hit, execute});
    if (dispatch_callback) dispatch_callback();
  }
  void QueueStatusRemoval(std::uint32_t mask) override { removed |= mask; }
  bool SourcePosition() override { return source_position; }
  std::uint8_t SourceSlot() override { return source_slot; }
  std::int16_t TargetHp() override { return hp; }
  std::int16_t PendingHp() override { return pending; }
  std::uint8_t TargetSlot() override { return target_slot; }
  bool KillCounted() override { return counted; }
  void MarkKillCounted() override {
    counted = true;
    events.push_back(3);
  }
  std::uint8_t WeaponCategory() override {
    events.push_back(4);
    return category;
  }
  std::uint32_t ProgressFlag() override {
    events.push_back(5);
    if (flag_callback) flag_callback();
    return flag;
  }
  std::uint8_t LearnedArts(std::uint8_t cat) override {
    events.push_back(cat);
    return learned;
  }
  std::uint16_t WeaponKills(std::uint8_t cat) override {
    events.push_back(cat);
    return kills;
  }
  std::uint16_t RequiredKills(std::uint8_t cat, std::uint8_t count) override {
    events.insert(events.end(), {cat, count});
    return required;
  }
  void SetWeaponKills(std::uint8_t cat, std::uint16_t value) override {
    events.push_back(cat);
    kills = value;
  }
};
TEST_CASE("F00507 keeps outer spell branch but uses fresh callback target",
          "[resource-dispatch][F00507]") {
  EffectWorld world;
  world.type = 1;
  world.dispatch_callback = [&] {
    world.type = 3;
    world.statuses = 0xFFFFFFFF;
  };
  battle::ApplyActionHitEffect(world, 0, 1);
  CHECK(world.events == std::vector<int>{2, 1, 0});
  CHECK(world.removed == 0x61000);
  world.events.clear();
  world.removed = 0;
  world.type = 2;
  world.dispatch_callback = [&] { world.type = 1; };
  battle::ApplyActionHitEffect(world, 0, 0);
  CHECK(world.removed == 0);
}
TEST_CASE("F00507 suppresses blocked spells without dispatching",
          "[resource-dispatch][F00507]") {
  EffectWorld world;
  world.type = 1;
  world.statuses = 0x80061000;
  battle::ApplyActionHitEffect(world, 0, -1);
  CHECK(world.last == 0);
  CHECK(world.removed == 0x61000);
  CHECK(world.events.empty());
}
TEST_CASE("F00507 counts first then reads low flag byte and fresh progression",
          "[resource-dispatch][F00507]") {
  EffectWorld world;
  world.flag = 0x1000A;
  world.flag_callback = [&] {
    CHECK(world.counted);
    world.category = 0;
    world.learned = 3;
    world.kills = 65534;
    world.required = 65535;
  };
  battle::ApplyActionHitEffect(world, -1, 1);
  CHECK(world.kills == 65535);
  CHECK(world.events == std::vector<int>{2, 1, -1, 3, 4, 5, 9, 9, 9, 3, 9});
}
TEST_CASE("F00507 keeps counted kills when later progress gates fail",
          "[resource-dispatch][F00507]") {
  for (const auto flag : {0x10000U, 9U, 10U, 255U}) {
    for (const auto learned : {3U, 4U, 255U}) {
      EffectWorld world;
      world.flag = flag;
      world.learned = static_cast<std::uint8_t>(learned);
      battle::ApplyActionHitEffect(world, 1, 0);
      CHECK(world.counted);
      CHECK(world.kills == ((flag & 255) >= 10 && learned < 4 ? 2 : 1));
      const auto before = world.kills;
      battle::ApplyActionHitEffect(world, 1, 0);
      CHECK(world.kills == before);
    }
  }
}
TEST_CASE("F00507 tail gates never read character progress prematurely",
          "[resource-dispatch][F00507]") {
  for (unsigned gate = 0; gate < 9; ++gate) {
    EffectWorld world;
    if (gate == 0) world.source_position = true;
    if (gate == 1) world.source_slot = 1;
    if (gate == 2) world.target_position = true;
    if (gate == 3) world.actor_id = 128;
    if (gate == 4) world.type = 7;
    if (gate == 5) world.hp = 3;
    if (gate == 6) world.target_slot = 0;
    if (gate == 7) world.counted = true;
    battle::ApplyActionHitEffect(world, gate == 8 ? 0 : -1, 1);
    CHECK(world.events == (gate == 3
                               ? std::vector<int>{1}
                               : std::vector<int>{2, 1, gate == 8 ? 0 : -1}));
  }
}

struct LiveFixture {
  decltype(std::to_array(Battle_actors)) saved = std::to_array(Battle_actors);
  decltype(std::to_array(Battle_statusEffectParams)) rules =
      std::to_array(Battle_statusEffectParams);
  decltype(std::to_array(Battle_limbInjuryStatusIds)) ids =
      std::to_array(Battle_limbInjuryStatusIds);
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  Battle_HitResult hit{};
  LiveFixture() {
    actor.stats = &stats;
    Battle_actors[0] = Battle_actors[41] = &actor;
  }
  ~LiveFixture() {
    std::copy(saved.begin(), saved.end(), Battle_actors);
    std::copy(rules.begin(), rules.end(), Battle_statusEffectParams);
    std::copy(ids.begin(), ids.end(), Battle_limbInjuryStatusIds);
  }
};
TEST_CASE("F00506 live injuries use low HP only and the actual status queue",
          "[resource-dispatch][F00506]") {
  LiveFixture fixture;
  fixture.stats.mobilityLimb0 = 4;
  fixture.stats.mobilityLimb1 = 5;
  Battle_statusEffectParams[4].statusFlags = 0x10;
  fixture.hit.target.targetActor = 41;
  fixture.hit.hpEffect = 2;
  for (auto& limb : fixture.hit.limbs) limb.effects.opaque2 = -1;
  battle::DeriveLiveLimbInjuryStatuses(&fixture.hit);
  CHECK(fixture.hit.removeStatuses == 0x10);
  CHECK(fixture.stats.statuses == 0);
  fixture.stats.statuses = 0x10;
  fixture.stats.limbs[5].hp = 2;
  battle::DeriveLiveLimbInjuryStatuses(&fixture.hit);
  CHECK(fixture.hit.inflictStatuses == 0x10);
}
TEST_CASE("F00507 live exemption ignores genuinely unused action and source",
          "[resource-dispatch][F00507]") {
  LiveFixture fixture;
  fixture.stats.actorId = 128;
  fixture.hit.target.targetActor = 41;
  fixture.hit.target.hp = 32767;
  fixture.hit.hpEffect = 3;
  battle::ApplyLiveActionHitEffect(nullptr, nullptr, &fixture.hit, 0, -1);
  CHECK(fixture.hit.target.hp == -31770);
  CHECK(fixture.hit.hpEffect == 1);
}
}  // namespace
