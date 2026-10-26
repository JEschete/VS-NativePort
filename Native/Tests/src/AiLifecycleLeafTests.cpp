#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/BiosRand.h"
#include "Core/Random.hpp"
#include "Core/Unported.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
}

static_assert(std::is_same_v<decltype(&Battle_Ai_ClearActorAiNavigationReset),
                             void (*)(int)>);

namespace {
struct AiLifecycleFixture {
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved =
      std::to_array(Battle_Ai_actorAiStates);
  Battle_Ai_ActorAiState state{};

  AiLifecycleFixture() {
    std::fill_n(Battle_Ai_actorAiStates, saved.size(), nullptr);
  }

  ~AiLifecycleFixture() {
    std::copy(saved.begin(), saved.end(), Battle_Ai_actorAiStates);
  }
};

struct HostilityFixture {
  unsigned saved_random = Core_Random_State();
  Battle_Ai_FrameContext saved_frame = *Battle_Ai_FrameState();
  Battle_Actor* saved_actor = Battle_actors[3];
  Battle_Ai_ActorAiState state{};
  Battle_Actor actor{};

  HostilityFixture() {
    state.actorId = 3;
    Battle_actors[3] = &actor;
  }

  ~HostilityFixture() {
    Core_Random_Seed(saved_random);
    *Battle_Ai_FrameState() = saved_frame;
    Battle_actors[3] = saved_actor;
  }

  void SetRelation(int target, std::uint32_t word) {
    std::memcpy(&state.relations[target], &word, sizeof(word));
  }

  void MakeHostile(int target) {
    Battle_Ai_MakeTargetHostile(
        reinterpret_cast<Battle_Ai_ReactionView*>(&state), target);
  }

  void CheckInitialReaction(unsigned busy) const {
    REQUIRE(state.relations[7].classification == 3);
    CHECK(state.relations[7].counter == 0);
    CHECK(state.opaque18 == 1);
    CHECK(state.awarenessMarker == 6);
    CHECK(state.awarenessFramesRemaining == 40);
    CHECK(state.decisionDelay == (busy == 0 ? 30 : (busy & 1)));
    CHECK(actor.commandFlags == (busy == 0 ? 2 : 0x5A));
  }
};

auto StateBytes(const Battle_Ai_ActorAiState& state) {
  std::array<std::byte, sizeof(state)> bytes{};
  std::memcpy(bytes.data(), &state, bytes.size());
  return bytes;
}

std::uint64_t ExtensionHookHits() {
  for (const auto& hit : VS::Core::UnportedSnapshot()) {
    if (hit.site == "Battle::Ai::InitializeActorAiExtensionHook") {
      return hit.count;
    }
  }
  return 0;
}
}  // namespace

TEST_CASE("AI persistence retains the missing-state sentinel for every slot",
          "[ai][lifecycle-leaves]") {
  AiLifecycleFixture fixture;
  for (int slot = 0; slot < 16; ++slot) {
    CHECK(Battle_Ai_PackActorAiPersistenceFlags(slot) == -1);
  }
}

TEST_CASE("AI persistence packs every original bit without mutating live state",
          "[ai][lifecycle-leaves]") {
  const int slot = GENERATE(0, 1, 15);
  const unsigned noise = GENERATE(0u, 0xC80FFF3Fu);
  AiLifecycleFixture fixture;
  Battle_Ai_actorAiStates[slot] = &fixture.state;
  fixture.state.actorId = 7;
  fixture.state.opaque1A = 0xA5;
  for (unsigned expected = 0; expected < 8192; ++expected) {
    const std::uint32_t relation = noise | (((expected >> 1) & 3) << 6) |
                                   (((expected >> 3) & 3) << 28) |
                                   (((expected >> 6) & 127) << 20);
    std::memcpy(&fixture.state.relations[0], &relation, sizeof(relation));
    fixture.state.usesRoomActionDelay = static_cast<u_char>(expected & 1);
    fixture.state.behaviorMode = (expected & 32) ? 0 : 0xA5;
    const auto before = StateBytes(fixture.state);
    REQUIRE(Battle_Ai_PackActorAiPersistenceFlags(slot) ==
            static_cast<int>(expected));
    CHECK(StateBytes(fixture.state) == before);
  }
}

TEST_CASE("AI persistence preserves the complete original delay byte",
          "[ai][lifecycle-leaves]") {
  AiLifecycleFixture fixture;
  Battle_Ai_actorAiStates[4] = &fixture.state;
  fixture.state.behaviorMode = 1;
  for (unsigned delay = 0; delay < 256; ++delay) {
    fixture.state.usesRoomActionDelay = static_cast<u_char>(delay);
    CHECK(Battle_Ai_PackActorAiPersistenceFlags(4) == static_cast<int>(delay));
  }
}

TEST_CASE(
    "The original empty AI extension hook preserves state without reporting a "
    "gap",
    "[ai][lifecycle-leaves]") {
  AiLifecycleFixture fixture;
  fixture.state.opaque1A = 0xA5;
  Battle_Ai_actorAiStates[3] = &fixture.state;
  const auto state = StateBytes(fixture.state);
  const auto hits = ExtensionHookHits();
  Battle_Ai_InitializeActorAiExtensionHook();
  Battle_Ai_InitializeActorAiExtensionHook();
  CHECK(StateBytes(fixture.state) == state);
  CHECK(Battle_Ai_actorAiStates[3] == &fixture.state);
  CHECK(ExtensionHookHits() == hits);
}

TEST_CASE("AI navigation reset clears only the original hold byte",
          "[ai][lifecycle-leaves]") {
  const bool present = GENERATE(false, true);
  AiLifecycleFixture fixture;
  for (int slot = 0; slot < 16; ++slot) {
    fixture.state.opaque1A = 0xA5;
    fixture.state.actorId = 7;
    fixture.state.behaviorMode = 0x5A;
    Battle_Ai_actorAiStates[slot] = present ? &fixture.state : nullptr;
    auto expected = StateBytes(fixture.state);
    if (present) {
      expected[offsetof(Battle_Ai_ActorAiState, opaque1A)] = std::byte{0};
    }
    Battle_Ai_ClearActorAiNavigationReset(slot);
    CHECK(StateBytes(fixture.state) == expected);
  }
}

TEST_CASE("AI hostility preserves relation bits and both original RNG streams",
          "[ai][hostility]") {
  const int target = GENERATE(0, 1, 7, 15);
  const unsigned classification = GENERATE(0u, 1u, 2u, 3u);
  const std::uint32_t noise = GENERATE(0u, 0xFFFF803Fu);
  HostilityFixture fixture;
  for (unsigned counter = 0; counter < 128; ++counter) {
    fixture.state = {};
    fixture.state.actorId = 3;
    fixture.state.opaque18 = 0xA5;
    fixture.state.awarenessMarker = 0x77;
    fixture.state.awarenessFramesRemaining = 0x66;
    for (int id = 0; id < 16; ++id) {
      fixture.SetRelation(id, 0xA5C10000u | static_cast<unsigned>(id));
    }
    const auto word = noise | (classification << 6) | (counter << 8);
    fixture.SetRelation(target, word);
    fixture.actor.commandFlags = 0x5A;
    auto expected = fixture.state;
    const std::uint32_t hostile = (word & 0xFFFF803Fu) | 0xC0u;
    std::memcpy(&expected.relations[target], &hostile, sizeof(hostile));
    Core_Random_Seed(1234);
    VS::Core::Rand15 expected_random(1234);
    auto& frame = *Battle_Ai_FrameState();
    frame.randomState = 0xFFFF8001u;
    frame.lastBudgetTick = 7;
    if (classification != 3) {
      expected_random.Next();
      expected.opaque18 = 1;
      expected.awarenessMarker = 6;
      expected.awarenessFramesRemaining = 40;
      expected.decisionDelay = 30;
    }
    fixture.MakeHostile(target);
    REQUIRE(StateBytes(fixture.state) == StateBytes(expected));
    CHECK(Core_Random_State() == expected_random.State());
    CHECK(frame.randomState ==
          (classification == 3 ? 0xFFFF8001u : 0xFFFF8008u));
    CHECK(fixture.actor.commandFlags == (classification == 3 ? 0x5A : 2));
  }
}

TEST_CASE(
    "AI hostility respects busy reactions and does not refresh on reentry",
    "[ai][hostility]") {
  const unsigned busy = GENERATE(0u, 1u, 2u, 3u);
  HostilityFixture fixture;
  fixture.state.decisionDelay = static_cast<u_char>(busy & 1);
  fixture.state.nativePointer18C = (busy & 2) ? &fixture.actor : nullptr;
  fixture.SetRelation(7, 0xABCD7F7Fu);
  fixture.actor.commandFlags = 0x5A;
  Core_Random_Seed(1234);
  VS::Core::Rand15 expected_random(1234);
  expected_random.Next();
  auto& frame = *Battle_Ai_FrameState();
  frame.randomState = 5;
  frame.lastBudgetTick = 11;
  fixture.MakeHostile(7);
  fixture.CheckInitialReaction(busy);
  CHECK(Core_Random_State() == expected_random.State());
  CHECK(frame.randomState == 16);
  fixture.state.opaque18 = 0xA5;
  fixture.state.awarenessMarker = 0x77;
  fixture.state.awarenessFramesRemaining = 0x66;
  fixture.state.relations[7].counter = 127;
  auto expected = fixture.state;
  expected.relations[7].counter = 0;
  frame.lastBudgetTick = 100;
  fixture.MakeHostile(7);
  CHECK(StateBytes(fixture.state) == StateBytes(expected));
  CHECK(Core_Random_State() == expected_random.State());
  CHECK(frame.randomState == 16);
  CHECK(fixture.actor.commandFlags == (busy == 0 ? 2 : 0x5A));
}
