#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/ActionOutcomesLive.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "State/Progress.h"
}

namespace {
namespace b = VS::Battle;
struct LiveReset {
  Battle_ActionState* state = Battle_actionState;
  decltype(std::to_array(Battle_actors)) actors = std::to_array(Battle_actors);
  decltype(std::to_array(Actors_actorRecords)) records =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Battle_actionDefinitions)) definitions =
      std::to_array(Battle_actionDefinitions);
  decltype(std::to_array(State_learnedActionFlags)) learned =
      std::to_array(State_learnedActionFlags);
  ~LiveReset() {
    Battle_actionState = state;
    std::ranges::copy(actors, std::begin(Battle_actors));
    std::ranges::copy(records, std::begin(Actors_actorRecords));
    std::ranges::copy(definitions, std::begin(Battle_actionDefinitions));
    std::ranges::copy(learned, std::begin(State_learnedActionFlags));
  }
};
using Definition = Battle_ActionDefinition;
using DefinitionBytes = std::array<std::uint8_t, sizeof(Definition)>;

TEST_CASE("F00048 live native room slots retain their full target index",
          "[action-outcomes][F00048]") {
  const LiveReset reset;
  Battle_Actor actor{};
  actor.roomSpawnIndex = 0x80;
  Battle_actors[41] = &actor;
  b::ResolveLiveEnemyDefeat(-256, 41, -1);
  CHECK(actor.stats == nullptr);
  CHECK(actor.defeated == 0);
}
TEST_CASE("Action outcome module bindings preserve all five original ABIs",
          "[action-outcomes][redirect-binding]") {
  static_assert(std::is_same_v<decltype(&b::ResolveLiveEnemyDefeat),
                               void (*)(int, int, int)>);
  static_assert(
      std::is_same_v<decltype(&b::PlayLiveWeaponHitSounds), int (*)(int)>);
  static_assert(std::is_same_v<decltype(&b::ApplyLiveActionSourceEffects),
                               void (*)(int, SVECTOR*)>);
  static_assert(std::is_same_v<decltype(&b::BeginLiveActorDefeatAnimation),
                               int (*)(int)>);
  static_assert(
      std::is_same_v<decltype(&b::ApplyLiveLearnedActionUnlocks), void (*)()>);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](const char* module, std::uint32_t address) {
    return VS::Content::ResolveModuleAddress(symbols, module, address);
  };
  CHECK(resolve("BATTLE/BATTLE.PRG", 0x8006C1CC) ==
        reinterpret_cast<void*>(&b::ResolveLiveEnemyDefeat));
  CHECK(resolve("BATTLE/BATTLE.PRG", 0x8006C5AC) ==
        reinterpret_cast<void*>(&b::PlayLiveWeaponHitSounds));
  CHECK(resolve("BATTLE/BATTLE.PRG", 0x8006CA20) ==
        reinterpret_cast<void*>(&b::ApplyLiveActionSourceEffects));
  CHECK(resolve("BATTLE/BATTLE.PRG", 0x80074A20) ==
        reinterpret_cast<void*>(&b::BeginLiveActorDefeatAnimation));
  CHECK(resolve("BATTLE/INITBTL.PRG", 0x800F9818) ==
        reinterpret_cast<void*>(&b::ApplyLiveLearnedActionUnlocks));
}
TEST_CASE("F00059 live full native target count uses real placement services",
          "[action-outcomes][F00059][actors-state]") {
  const LiveReset reset;
  auto state = std::make_unique<Battle_ActionState>();
  Actors_ActorRecord actor{};
  actor.instance.position = {-32768, 32767, -9, 77};
  actor.instance.facing = 0x340;
  Actors_actorRecords[15] = &actor;
  Battle_actionState = state.get();
  state->queueIndex = 0xFFFFFFFF;
  auto& action = state->queue[3];
  action.targetCount = 255;
  for (auto& hit : action.targets) {
    hit.target.targetActor = 15;
    hit.target.impactKind = 5;
  }
  CHECK(b::PlayLiveWeaponHitSounds(-1000) == 1);
  CHECK(action.targetCount == 255);
  CHECK(actor.instance.position.vx == -32768);
  CHECK(actor.instance.position.vy == 32767);
  CHECK(actor.instance.position.pad == 77);
}
TEST_CASE("F00061 live applies real result equipment changes to its queued hit",
          "[action-outcomes][F00061][action-result-commit]") {
  const LiveReset reset;
  auto state = std::make_unique<Battle_ActionState>();
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  actor.stats = &stats;
  Battle_actors[15] = &actor;
  Battle_actionState = state.get();
  state->queueIndex = 0xFFFFFFFE;
  auto& hit = state->queue[2].primaryTarget;
  hit.target.targetActor = 15;
  hit.target.impactKind = 2;
  hit.weaponDpChange = 2;
  hit.weaponDpEffect = 2;
  stats.weapon.currentDp = 65535;
  stats.weapon.maxDp = 100;
  stats.currentHp = -7;
  stats.actionRecoveryTimer = 65535;
  b::ApplyLiveActionSourceEffects(15, nullptr);
  CHECK(stats.weapon.currentDp == 1);
  CHECK(stats.currentHp == -7);
  CHECK(stats.actionRecoveryTimer == 65535);
  CHECK(state->queue[0].primaryTarget.weaponDpEffect == 0);
}
TEST_CASE("F00061 live unused argument and signed early gate do not read state",
          "[action-outcomes][F00061]") {
  const LiveReset reset;
  Battle_actionState = nullptr;
  SVECTOR unused{11, -22, 33, -44};
  b::ApplyLiveActionSourceEffects(16, &unused);
  b::ApplyLiveActionSourceEffects(256, nullptr);
  CHECK(std::bit_cast<std::array<std::int16_t, 4>>(unused) ==
        std::array<std::int16_t, 4>{11, -22, 33, -44});
  CHECK(Battle_actionState == nullptr);
}
TEST_CASE("F00156 live refresh and living path require no unused action state",
          "[action-outcomes][F00156][actor-cleanup]") {
  const auto hp = GENERATE(-32768, -1, 1, 32767);
  const LiveReset reset;
  Battle_actionState = nullptr;
  Battle_actors[1] = nullptr;
  CHECK(b::BeginLiveActorDefeatAnimation(1) == 0);
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  actor.stats = &stats;
  actor.commandState = 77;
  stats.currentHp = static_cast<std::int16_t>(hp);
  Battle_actors[1] = &actor;
  CHECK(b::BeginLiveActorDefeatAnimation(1) == 0);
  CHECK(actor.commandState == 0);
  CHECK(Battle_actionState == nullptr);
}
TEST_CASE("F01851 live changes only the unlock bit of every raw definition",
          "[action-outcomes][F01851]") {
  const auto pattern = GENERATE(0, 1, 2);
  const LiveReset reset;
  for (std::size_t index = 0; index < std::size(Battle_actionDefinitions);
       ++index) {
    DefinitionBytes raw{};
    for (std::size_t byte = 0; byte < raw.size(); ++byte) {
      raw.at(byte) = static_cast<std::uint8_t>(index * 37 + byte * 109);
    }
    raw.at(0) = static_cast<std::uint8_t>(255 - index);
    Battle_actionDefinitions[index] =
        std::bit_cast<Battle_ActionDefinition>(raw);
  }
  for (std::size_t byte = 0; byte < std::size(State_learnedActionFlags);
       ++byte) {
    const auto bits = pattern == 0 ? 0x81 : pattern == 1 ? 0 : byte * 109 + 37;
    State_learnedActionFlags[byte] = static_cast<char>(bits);
  }
  const auto flags = std::to_array(State_learnedActionFlags);
  const auto before = std::to_array(Battle_actionDefinitions);
  b::ApplyLiveLearnedActionUnlocks();
  for (std::size_t index = 0; index < before.size(); ++index) {
    auto expected = std::bit_cast<DefinitionBytes>(before.at(index));
    const auto bit =
        static_cast<std::uint8_t>(flags.at(index / 8)) & (0x80 >> (index % 8));
    expected.at(13) = static_cast<std::uint8_t>((expected.at(13) & 0x7F) |
                                                (bit != 0 ? 0x80 : 0));
    CHECK(std::bit_cast<DefinitionBytes>(Battle_actionDefinitions[index]) ==
          expected);
  }
  CHECK(std::to_array(State_learnedActionFlags) == flags);
  std::ranges::fill(State_learnedActionFlags, 0);
  b::ApplyLiveLearnedActionUnlocks();
  CHECK(Battle_actionDefinitions[0].unlocked == 0);
  CHECK(Battle_actionDefinitions[255].unlocked == 0);
}
}  // namespace
