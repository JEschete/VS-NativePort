#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Unported.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorState.h"
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
    REQUIRE(Battle_Ai_PackActorAiPersistenceFlags(slot) == expected);
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
    CHECK(Battle_Ai_PackActorAiPersistenceFlags(4) == delay);
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
