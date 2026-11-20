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

#include "Actors/ActorRecord.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Content/ModuleData.hpp"
#include "State/Progress.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::ActionState;

namespace {
namespace b = VS::Battle;
struct LiveReset {
  ActionState* state = VS::Battle::actionState;
  decltype(VS::Battle::actors) actors = VS::Battle::actors;
  decltype(VS::Actors::actorRecords) records = VS::Actors::actorRecords;
  decltype(VS::Battle::actionDefinitions) definitions =
      VS::Battle::actionDefinitions;
  decltype(std::to_array(VS::State::learnedActionFlags)) learned =
      std::to_array(VS::State::learnedActionFlags);
  ~LiveReset() {
    VS::Battle::actionState = state;
    std::ranges::copy(actors, std::begin(VS::Battle::actors));
    std::ranges::copy(records, std::begin(VS::Actors::actorRecords));
    std::ranges::copy(definitions, std::begin(VS::Battle::actionDefinitions));
    std::ranges::copy(learned, std::begin(VS::State::learnedActionFlags));
  }
};
using Definition = ActionDefinition;
using DefinitionBytes = std::array<std::uint8_t, sizeof(Definition)>;

TEST_CASE("F00048 live native room slots retain their full target index",
          "[action-outcomes][F00048]") {
  const LiveReset reset;
  VS::Battle::Actor actor{};
  actor.room_spawn_index = 0x80;
  VS::Battle::actors[41] = &actor;
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
  const auto symbols = VS::Content::ModuleSymbols();
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
  auto state = std::make_unique<ActionState>();
  VS::Actors::ActorRecord actor{};
  actor.instance.position = {-32768, 32767, -9, 77};
  actor.instance.facing = 0x340;
  VS::Actors::actorRecords[15] = &actor;
  VS::Battle::actionState = state.get();
  state->queue_index = 0xFFFFFFFF;
  auto& action = state->queue[3];
  action.target_count = 255;
  for (auto& hit : action.targets) {
    hit.target.target_actor = 15;
    hit.target.impact_kind = 5;
  }
  CHECK(b::PlayLiveWeaponHitSounds(-1000) == 1);
  CHECK(action.target_count == 255);
  CHECK(actor.instance.position.vx == -32768);
  CHECK(actor.instance.position.vy == 32767);
  CHECK(actor.instance.position.pad == 77);
}
TEST_CASE("F00061 live applies real result equipment changes to its queued hit",
          "[action-outcomes][F00061][action-result-commit]") {
  const LiveReset reset;
  auto state = std::make_unique<ActionState>();
  VS::Battle::Actor actor{};
  VS::Battle::ActorStats stats{};
  actor.stats = &stats;
  VS::Battle::actors[15] = &actor;
  VS::Battle::actionState = state.get();
  state->queue_index = 0xFFFFFFFE;
  auto& hit = state->queue[2].primary_target;
  hit.target.target_actor = 15;
  hit.target.impact_kind = 2;
  hit.weapon_dp_change = 2;
  hit.weapon_dp_effect = 2;
  stats.weapon.current_dp = 65535;
  stats.weapon.max_dp = 100;
  stats.current_hp = -7;
  stats.action_recovery_timer = 65535;
  b::ApplyLiveActionSourceEffects(15, nullptr);
  CHECK(stats.weapon.current_dp == 1);
  CHECK(stats.current_hp == -7);
  CHECK(stats.action_recovery_timer == 65535);
  CHECK(state->queue[0].primary_target.weapon_dp_effect == 0);
}
TEST_CASE("F00061 live unused argument and signed early gate do not read state",
          "[action-outcomes][F00061]") {
  const LiveReset reset;
  VS::Battle::actionState = nullptr;
  SVECTOR unused{11, -22, 33, -44};
  b::ApplyLiveActionSourceEffects(16, &unused);
  b::ApplyLiveActionSourceEffects(256, nullptr);
  CHECK(std::bit_cast<std::array<std::int16_t, 4>>(unused) ==
        std::array<std::int16_t, 4>{11, -22, 33, -44});
  CHECK(VS::Battle::actionState == nullptr);
}
TEST_CASE("F00156 live refresh and living path require no unused action state",
          "[action-outcomes][F00156][actor-cleanup]") {
  const auto hp = GENERATE(-32768, -1, 1, 32767);
  const LiveReset reset;
  VS::Battle::actionState = nullptr;
  VS::Battle::actors[1] = nullptr;
  CHECK(b::BeginLiveActorDefeatAnimation(1) == 0);
  VS::Battle::Actor actor{};
  VS::Battle::ActorStats stats{};
  actor.stats = &stats;
  actor.command_state = 77;
  stats.current_hp = static_cast<std::int16_t>(hp);
  VS::Battle::actors[1] = &actor;
  CHECK(b::BeginLiveActorDefeatAnimation(1) == 0);
  CHECK(actor.command_state == 0);
  CHECK(VS::Battle::actionState == nullptr);
}
TEST_CASE("F01851 live changes only the unlock bit of every raw definition",
          "[action-outcomes][F01851]") {
  const auto pattern = GENERATE(0, 1, 2);
  const LiveReset reset;
  for (std::size_t index = 0; index < std::size(VS::Battle::actionDefinitions);
       ++index) {
    DefinitionBytes raw{};
    for (std::size_t byte = 0; byte < raw.size(); ++byte) {
      raw.at(byte) = static_cast<std::uint8_t>(index * 37 + byte * 109);
    }
    raw.at(0) = static_cast<std::uint8_t>(255 - index);
    VS::Battle::actionDefinitions[index] = std::bit_cast<ActionDefinition>(raw);
  }
  for (std::size_t byte = 0; byte < std::size(VS::State::learnedActionFlags);
       ++byte) {
    const auto bits = pattern == 0 ? 0x81 : pattern == 1 ? 0 : byte * 109 + 37;
    VS::State::learnedActionFlags[byte] = static_cast<char>(bits);
  }
  const auto flags = std::to_array(VS::State::learnedActionFlags);
  const auto before = VS::Battle::actionDefinitions;
  b::ApplyLiveLearnedActionUnlocks();
  for (std::size_t index = 0; index < before.size(); ++index) {
    auto expected = std::bit_cast<DefinitionBytes>(before.at(index));
    const auto bit =
        static_cast<std::uint8_t>(flags.at(index / 8)) & (0x80 >> (index % 8));
    expected.at(13) = static_cast<std::uint8_t>((expected.at(13) & 0x7F) |
                                                (bit != 0 ? 0x80 : 0));
    CHECK(std::bit_cast<DefinitionBytes>(
              VS::Battle::actionDefinitions[index]) == expected);
  }
  CHECK(std::to_array(VS::State::learnedActionFlags) == flags);
  std::ranges::fill(VS::State::learnedActionFlags, 0);
  b::ApplyLiveLearnedActionUnlocks();
  CHECK(VS::Battle::actionDefinitions[0].unlocked == 0);
  CHECK(VS::Battle::actionDefinitions[255].unlocked == 0);
}
}  // namespace
