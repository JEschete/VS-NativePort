#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/ActorVitals.hpp"
#include "Battle/ActorVitalsLive.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/StatusEffects.h"
#include "State/SceneTimer.h"
#include "State/Timers.h"
}

namespace {
namespace battle = VS::Battle;
using Event = std::array<int, 4>;

struct Stats final : battle::ActorVitalsAccess {
  battle::ActorVitalsState state{};
  battle::ActorVitalsState Read() override { return state; }
  void Write(const battle::ActorVitalsState& value) override { state = value; }
};

struct Actor final : battle::VitalsActor {
  std::uint8_t spawn{};
  std::uint8_t retain{};
  std::uint32_t drawn{};
  ::Stats* stats{};
  std::uint8_t SpawnIndex() override { return spawn; }
  std::uint8_t RetainAfterDefeat() override { return retain; }
  std::uint32_t WeaponDrawn() override { return drawn; }
  battle::ActorVitalsAccess* Stats() override { return stats; }
};

struct World final : battle::ActorVitalsWorld {
  std::array<Actor*, 42> slots{};
  std::array<std::uint32_t, 42> flags{};
  std::array<int, 42> busy{};
  std::vector<Event> events;
  std::function<void(int)> on_flags;
  std::function<void(int)> on_defeat;
  std::function<void(battle::ActorVitalsAccess&)> on_status;
  std::int32_t counter = 1;
  std::int32_t ticks = 1;
  std::int32_t limb = 0;
  std::int16_t trigger = 0;
  int timer = 0;
  battle::VitalsActor* ActorAt(int slot) override {
    return slots.at(static_cast<std::size_t>(slot));
  }
  std::uint32_t StateFlags(int slot) override {
    events.push_back({0, slot, 0, 0});
    if (on_flags) on_flags(slot);
    return flags.at(static_cast<std::size_t>(slot));
  }
  std::int32_t ActionAnimationBusy(int slot) override {
    events.push_back({1, slot, 0, 0});
    return busy.at(static_cast<std::size_t>(slot));
  }
  void ResolveEnemyDefeat(int source, int target, int count_kill) override {
    events.push_back({2, source, target, count_kill});
    if (on_defeat) on_defeat(target);
  }
  void RestoreIdle(int slot) override { events.push_back({3, slot, 0, 0}); }
  void RemoveActor(int slot, int reason) override {
    events.push_back({4, slot, reason, 0});
  }
  void SuppressAshleyLabels() override { events.push_back({5, 0, 0, 0}); }
  std::int16_t EventTrigger(int event) override {
    events.push_back({6, event, 0, 0});
    return trigger;
  }
  void BeginQueuedCutscene() override { events.push_back({7, 0, 0, 0}); }
  void BeginDefeatAnimation(int slot) override {
    events.push_back({8, slot, 0, 0});
  }
  void StatusTimers(battle::ActorVitalsAccess& stats) override {
    events.push_back({9, stats.Read().status.hp, 0, 0});
    if (on_status) on_status(stats);
  }
  std::int32_t Counter() override { return counter; }
  void SetCounter(std::int32_t value) override { counter = value; }
  std::int32_t Ticks() override { return ticks; }
  std::int32_t RegeneratingLimb() override { return limb; }
  void SetRegeneratingLimb(std::int32_t value) override { limb = value; }
  int SceneTimer() override {
    events.push_back({10, counter, limb, 0});
    return timer;
  }
};

struct Fixture {
  Stats stats;
  Actor actor;
  World world;
  Fixture() {
    actor.stats = &stats;
    world.slots.at(0) = &actor;
    world.flags.fill(1);
    stats.state.status.hp = 10;
    stats.state.status.max_hp = 20;
    stats.state.mp = 10;
    stats.state.max_mp = 20;
    stats.state.risk = 10;
    stats.state.recovery = 36;
    for (auto& limb : stats.state.status.limbs) limb.max_hp = 10;
  }
};
}  // namespace

TEST_CASE("Defeat skips empty slots and restores any nonzero signed HP",
          "[actor-vitals][F00194]") {
  Fixture f;
  CHECK(battle::ResolveActorDefeat(3, 2, 7, f.world) == 0);
  CHECK(f.world.events.empty());
  f.stats.state.status.hp = GENERATE(1, -1, -32768);
  CHECK(battle::ResolveActorDefeat(3, 0, 7, f.world) == 0);
  CHECK(f.world.events == std::vector<Event>{{3, 0, 0, 0}});
  f.actor.stats = nullptr;
  CHECK(battle::ResolveActorDefeat(3, 0, 7, f.world) == std::nullopt);
}

TEST_CASE("Ashley defeat preserves arguments and ordered discarded services",
          "[actor-vitals][F00194]") {
  Fixture f;
  f.stats.state.status.hp = 0;
  f.world.trigger = -1;
  CHECK(battle::ResolveActorDefeat(41, 0, -7, f.world) == 1);
  CHECK(f.world.events ==
        std::vector<Event>{
            {2, 41, 0, -7}, {5, 0, 0, 0}, {6, 64, 0, 0}, {7, 0, 0, 0}});
}

TEST_CASE("Enemy defeat rereads the table instead of the defeated actor",
          "[actor-vitals][F00194]") {
  Fixture f;
  Actor replacement;
  replacement.retain = GENERATE(0, 1, 255);
  f.world.slots.at(2) = &f.actor;
  f.stats.state.status.hp = 0;
  f.actor.retain = 1;
  f.world.on_defeat = [&](int slot) {
    f.world.slots.at(static_cast<std::size_t>(slot)) = &replacement;
  };
  CHECK(battle::ResolveActorDefeat(5, 2, 1, f.world) == 0);
  CHECK(f.world.events.front() == Event{2, 5, 2, 1});
  CHECK(f.world.events.size() == (replacement.retain == 0 ? 2 : 1));
  if (replacement.retain == 0) {
    CHECK(f.world.events.back() == Event{4, 2, 1, 0});
  }
  f.world.on_defeat = [&](int) { f.world.slots.at(2) = nullptr; };
  f.world.slots.at(2) = &f.actor;
  CHECK(battle::ResolveActorDefeat(5, 2, 1, f.world) == std::nullopt);
}

TEST_CASE("Room vitals caches actor before flags and stats after flags",
          "[actor-vitals][F00525]") {
  Fixture f;
  Stats original, selected, other;
  Actor replacement;
  replacement.stats = &other;
  original.state.recovery = 100;
  selected.state.recovery = 100;
  other.state.recovery = 100;
  f.actor.stats = &original;
  f.world.on_flags = [&](int) {
    f.actor.stats = &selected;
    f.world.slots.at(0) = &replacement;
  };
  CHECK(battle::UpdateActorVitals(f.world) == 0);
  CHECK(original.state.recovery == 100);
  CHECK(selected.state.recovery == 65);
  CHECK(other.state.recovery == 100);
}

TEST_CASE("Room vitals limits slots and exempts Ashley from spawn filtering",
          "[actor-vitals][F00525]") {
  Fixture f;
  f.actor.spawn = 128;
  f.world.slots.at(1) = &f.actor;
  f.world.slots.at(15) = &f.actor;
  f.world.slots.at(16) = &f.actor;
  f.world.slots.at(41) = &f.actor;
  CHECK(battle::UpdateActorVitals(f.world) == 0);
  CHECK(f.stats.state.recovery == 1);
  CHECK(f.world.events == std::vector<Event>{{0, 0, 0, 0}, {10, 2, 0, 0}});
}

TEST_CASE("Defeated flag precedes active gating and busy compares full word",
          "[actor-vitals][F00525]") {
  Fixture f;
  const auto busy = GENERATE(0, 1, 2, 65537, -1);
  f.world.flags.at(0) = 0x01002001;
  f.world.busy.at(0) = busy;
  f.stats.state.status.hp = 0;
  CHECK(battle::UpdateActorVitals(f.world) == (busy == 1 ? 0 : 1));
  CHECK(f.stats.state.recovery == 36);
  CHECK(f.world.events.at(1) == Event{1, 0, 0, 0});
}

TEST_CASE("Normal flags and missing stats skip without touching recovery",
          "[actor-vitals][F00525]") {
  Fixture f;
  f.world.flags.at(0) = GENERATE(0U, 0x2001U, 0x2000U, 2U);
  CHECK(battle::UpdateActorVitals(f.world) == 0);
  CHECK(f.stats.state.recovery == 36);
  f.actor.stats = nullptr;
  f.world.flags.at(0) = 1;
  CHECK(battle::UpdateActorVitals(f.world) == 0);
}

TEST_CASE("Recovery subtracts 35 per call independently of frame ticks",
          "[actor-vitals][F00525]") {
  Fixture f;
  const auto recovery = GENERATE(0, 1, 34, 35, 36, 65535);
  f.stats.state.recovery = static_cast<std::uint16_t>(recovery);
  f.world.ticks = GENERATE(0, 1, 99, -99);
  CHECK(battle::UpdateActorVitals(f.world) == 0);
  CHECK(f.stats.state.recovery == (recovery > 35 ? recovery - 35 : 0));
}

TEST_CASE("Vitals drawn and sheathed cadence includes zero and negative ticks",
          "[actor-vitals][F00525]") {
  Fixture f;
  const int counter = GENERATE(0, 30, 60, 120, 180, 240, 719, 720, -120);
  const bool drawn = GENERATE(false, true);
  f.actor.drawn = drawn ? 3 : 2;
  f.world.counter = counter;
  f.world.limb = 5;
  f.stats.state.status.weapon.current_pp = 2;
  f.stats.state.status.shield.current_pp = 1;
  CHECK(battle::UpdateActorVitals(f.world) == 0);
  CHECK(f.stats.state.risk == (counter % (drawn ? 120 : 30) == 0 ? 9 : 10));
  CHECK(f.stats.state.mp == (counter % (drawn ? 180 : 60) == 0 ? 11 : 10));
  CHECK(f.stats.state.status.hp ==
        (counter % (drawn ? 240 : 120) == 0 ? 11 : 10));
  CHECK(f.stats.state.status.limbs.at(5).hp ==
        (counter % (drawn ? 240 : 120) == 0 ? 1 : 0));
  CHECK(f.stats.state.status.weapon.current_pp ==
        (drawn && counter % 180 == 0 ? 1 : 2));
  CHECK(f.stats.state.status.shield.current_pp ==
        (drawn && counter % 180 == 0 ? 0 : 1));
  CHECK(f.stats.state.status.limbs.at(4).hp == 0);
}

TEST_CASE("Risk eight slows sheathed decay while capped MP still drains PP",
          "[actor-vitals][F00525]") {
  Fixture f;
  f.world.counter = 30;
  f.stats.state.status.statuses = 8;
  CHECK(battle::UpdateActorVitals(f.world) == 0);
  CHECK(f.stats.state.risk == 10);
  f.world.counter = 180;
  f.actor.drawn = 1;
  f.stats.state.mp = f.stats.state.max_mp;
  f.stats.state.status.weapon.current_pp = 1;
  CHECK(battle::UpdateActorVitals(f.world) == 0);
  CHECK(f.stats.state.mp == 20);
  CHECK(f.stats.state.status.weapon.current_pp == 0);
  CHECK(f.stats.state.status.shield.current_pp == 0);
}

TEST_CASE("Vitals zero HP does not regenerate but negative HP does",
          "[actor-vitals][F00525]") {
  Fixture f;
  const auto hp = GENERATE(0, -1, -32768);
  f.world.counter = 0;
  f.stats.state.status.hp = static_cast<std::int16_t>(hp);
  CHECK(battle::UpdateActorVitals(f.world) == 0);
  CHECK(f.stats.state.status.hp == (hp == 0 ? 0 : hp + 1));
  CHECK(f.stats.state.mp == (hp == 0 ? 10 : 11));
  CHECK(f.world.events.at(1).at(0) == (hp == 0 || hp == -1 ? 8 : 9));
}

TEST_CASE("Only a used regenerating limb is checked",
          "[actor-vitals][F00525]") {
  Fixture f;
  f.world.limb = GENERATE(-1, 6, 255);
  CHECK(battle::UpdateActorVitals(f.world) == 0);
  f.world.counter = 120;
  CHECK(battle::UpdateActorVitals(f.world) == std::nullopt);
  f.stats.state.status.hp = 0;
  CHECK(battle::UpdateActorVitals(f.world) == 0);
}

TEST_CASE(
    "Status callbacks retain cached stats and actor but refresh next slot",
    "[actor-vitals][F00525]") {
  Fixture f;
  Stats next_stats;
  Actor next_actor;
  next_actor.stats = &next_stats;
  next_stats.state.recovery = 100;
  f.world.counter = 60;
  f.world.on_status = [&](battle::ActorVitalsAccess& owner) {
    auto state = owner.Read();
    state.status.hp = 0;
    owner.Write(state);
    f.actor.stats = &next_stats;
    f.actor.retain = 0;
    f.world.slots.at(0) = &next_actor;
    f.world.slots.at(1) = &next_actor;
    f.world.counter = 31;
    f.world.ticks = 9;
  };
  CHECK(battle::UpdateActorVitals(f.world) == 0);
  CHECK(f.stats.state.status.hp == 0);
  CHECK(next_stats.state.recovery == 65);
  CHECK(f.world.events.at(2) == Event{8, 0, 0, 0});
  CHECK(f.world.counter == 40);
}

TEST_CASE(
    "Status callback revive suppresses defeat and retain change enables it",
    "[actor-vitals][F00525]") {
  Fixture f;
  const bool kill = GENERATE(false, true);
  f.world.counter = 60;
  f.world.on_status = [&](battle::ActorVitalsAccess& owner) {
    auto state = owner.Read();
    state.status.hp = kill ? 0 : 3;
    owner.Write(state);
    f.actor.retain = 1;
  };
  CHECK(battle::UpdateActorVitals(f.world) == (kill ? 1 : 0));
  CHECK(f.stats.state.status.hp == (kill ? 0 : 3));
  CHECK(f.world.events.size() == (kill ? 7 : 3));
}

TEST_CASE("Enemy event outcome prioritizes one and scene timer overrides",
          "[actor-vitals][F00525]") {
  Fixture f;
  Stats enemy_stats;
  Actor enemy;
  enemy.stats = &enemy_stats;
  enemy.retain = 1;
  f.world.slots.at(0) = nullptr;
  f.world.slots.at(1) = &enemy;
  f.world.slots.at(2) = &enemy;
  f.world.flags.fill(0x01000000);
  f.world.on_defeat = [&](int slot) { f.world.trigger = slot == 2 ? 1 : -1; };
  CHECK(battle::UpdateActorVitals(f.world) == 1);
  f.world.timer = -7;
  CHECK(battle::UpdateActorVitals(f.world) == -7);
  CHECK(f.world.events.at(3) == Event{6, 12, 0, 0});
}

TEST_CASE("Counters wrap before signed remainder including zero-tick advance",
          "[actor-vitals][F00525]") {
  World world;
  world.counter = std::numeric_limits<std::int32_t>::max();
  world.ticks = 1;
  CHECK(battle::UpdateActorVitals(world) == 0);
  CHECK(world.counter == std::numeric_limits<std::int32_t>::min() % 720);
  world.counter = 0;
  world.ticks = 0;
  world.limb = std::numeric_limits<std::int32_t>::max();
  CHECK(battle::UpdateActorVitals(world) == 0);
  CHECK(world.limb == std::numeric_limits<std::int32_t>::min() % 6);
  world.limb = 5;
  CHECK(battle::UpdateActorVitals(world) == 0);
  CHECK(world.limb == 0);
}

TEST_CASE("Room wrapper executes the real native vitals callee and return",
          "[actor-vitals][F00526]") {
  Fixture f;
  f.world.timer = 7;
  CHECK(battle::UpdateRoomActorVitals(f.world) == 7);
  CHECK(f.stats.state.recovery == 1);
  CHECK(f.world.counter == 2);
  CHECK(f.world.events == std::vector<Event>{{0, 0, 0, 0}, {10, 2, 0, 0}});
}

TEST_CASE(
    "Defeat and room vitals live module bindings execute typed entrypoints",
    "[actor-vitals][F00194][F00525][F00526][live]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](std::uint32_t address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  const auto defeat =
      reinterpret_cast<int (*)(int, int, int)>(resolve(0x800792E4));
  const auto update = reinterpret_cast<int (*)()>(resolve(0x80088554));
  const auto room = reinterpret_cast<int (*)()>(resolve(0x80088B6C));
  REQUIRE(defeat == battle::ResolveLiveActorDefeat);
  REQUIRE(update == battle::UpdateLiveActorVitals);
  REQUIRE(room == battle::UpdateLiveRoomActorVitals);
  auto* saved = Battle_actors[41];
  Battle_actors[41] = nullptr;
  CHECK(defeat(0, 41, 0) == 0);
  Battle_actors[41] = saved;
}

TEST_CASE(
    "Live room wrapper executes vitals counter writes through module binding",
    "[actor-vitals][F00525][F00526][live]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto room =
      reinterpret_cast<int (*)()>(VS::Content::ResolveModuleAddress(
          symbols, "BATTLE/BATTLE.PRG", 0x80088B6C));
  REQUIRE(room == battle::UpdateLiveRoomActorVitals);
  std::array<Battle_Actor*, 16> saved{};
  for (std::size_t slot = 0; slot < saved.size(); ++slot) {
    saved.at(slot) = Battle_actors[slot];
    Battle_actors[slot] = nullptr;
  }
  const auto counter = Battle_vitalsTickCounter;
  const auto limb = Battle_regeneratingLimb;
  const auto ticks = State_tickSpeed;
  const auto timer = State_sceneTimer;
  State_sceneTimer = {};
  State_tickSpeed = 0;
  Battle_vitalsTickCounter = 0;
  Battle_regeneratingLimb = 5;
  const auto result = room();
  const auto updated_counter = Battle_vitalsTickCounter;
  const auto updated_limb = Battle_regeneratingLimb;
  for (std::size_t slot = 0; slot < saved.size(); ++slot) {
    Battle_actors[slot] = saved.at(slot);
  }
  Battle_vitalsTickCounter = counter;
  Battle_regeneratingLimb = limb;
  State_tickSpeed = ticks;
  State_sceneTimer = timer;
  CHECK(result == 0);
  CHECK(updated_counter == 0);
  CHECK(updated_limb == 0);
}
