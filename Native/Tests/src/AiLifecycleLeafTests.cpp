#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.h"
#include "Core/BiosRand.h"
#include "Core/Random.hpp"
#include "Core/Unported.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/ActionTable.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/DecisionCallbacks.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Ai/Relations.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
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

int AshleyAllianceTestCallback(Battle_Ai_ActorAiState*) { return 0x5A; }

struct AshleyAllianceFixture {
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved_ai_states =
      std::to_array(Battle_Ai_actorAiStates);
  Battle_Ai_FrameContext saved_frame = *Battle_Ai_FrameState();
  unsigned saved_random = Core_Random_State();
  Battle_Actor* saved_actor0 = Battle_actors[0];
  Battle_Actor* saved_actor1 = Battle_actors[1];
  char saved_marker1 = Battle_actorMarkers[1];
  Battle_Ai_ActorAiState state{};
  Battle_Ai_ActorAiState other_state{};
  Battle_Actor actors[2]{};

  AshleyAllianceFixture() {
    std::fill_n(Battle_Ai_actorAiStates, saved_ai_states.size(), nullptr);
    state.actorId = 1;
    other_state.actorId = 0;
    actors[0].id = 0;
    actors[0].next = &actors[1];
    actors[1].id = 1;
    Battle_Ai_actorAiStates[0] = &other_state;
    Battle_Ai_actorAiStates[1] = &state;
    Battle_actors[0] = &actors[0];
    Battle_actors[1] = &actors[1];
  }

  ~AshleyAllianceFixture() {
    std::copy(saved_ai_states.begin(), saved_ai_states.end(),
              Battle_Ai_actorAiStates);
    *Battle_Ai_FrameState() = saved_frame;
    Core_Random_Seed(saved_random);
    Battle_actors[0] = saved_actor0;
    Battle_actors[1] = saved_actor1;
    Battle_actorMarkers[1] = saved_marker1;
  }

  Battle_Ai_ReactionView* Reaction() {
    return reinterpret_cast<Battle_Ai_ReactionView*>(&state);
  }

  Battle_Ai_SettingsView* Settings() {
    return reinterpret_cast<Battle_Ai_SettingsView*>(&state);
  }

  void CheckRestored(int (*callback)(Battle_Ai_ActorAiState*)) {
    CHECK(Reaction()->temporaryAshleyAlliance == 0);
    CHECK(Reaction()->allianceReactionCount == 0);
    CHECK(Settings()->savedDecisionCallback == nullptr);
    CHECK(Settings()->decisionCallback == callback);
  }

  void CheckEntered(unsigned count,
                    int (*saved_callback)(Battle_Ai_ActorAiState*)) {
    CHECK(Reaction()->allianceReactionCount == count);
    CHECK(Reaction()->temporaryAshleyAlliance == 1);
    CHECK(Settings()->savedDecisionCallback == saved_callback);
    CHECK(Settings()->decisionCallback ==
          Battle_Ai_UpdateEngagementApproachBehavior);
    CHECK(static_cast<unsigned char>(Battle_actorMarkers[1]) == 0x85);
  }

  void CheckRecovery(int settings_bit, int busy, unsigned random) {
    CHECK(state.awarenessMarker == (settings_bit ? 0x71 : 6));
    CHECK(state.awarenessFramesRemaining == (settings_bit ? 0x72 : 40));
    CHECK(Battle_Ai_FrameState()->randomState ==
          (settings_bit ? 0x2000 : 0x2002));
    CHECK(Core_Random_State() == random);
    CHECK(state.decisionDelay ==
          (busy == 1 ? 3 : (!settings_bit && busy == 0 ? 30 : 0)));
    CHECK(actors[1].commandFlags == (!settings_bit && busy == 0 ? 2 : 0x5A));
  }

  void CheckDeferredRecovery() {
    CHECK(Battle_actorMarkers[1] == 0x05);
    CHECK(state.relations[0].classification == 3);
    CHECK(state.awarenessMarker == 6);
    CHECK(state.awarenessFramesRemaining == 0x28);
    CHECK(state.decisionDelay == 1);
    CHECK(Battle_Ai_FrameState()->randomState == 0x2002);
  }
};

std::array<unsigned, 128> AshleyAllianceRollSeeds() {
  std::array<unsigned, 128> seeds{};
  unsigned found = 0;
  for (unsigned candidate = 1; candidate < 65536 && found < seeds.size();
       ++candidate) {
    VS::Core::Rand15 random(candidate);
    const auto roll = static_cast<unsigned>(random.Next()) & 0x7F;
    if (seeds[roll] == 0) {
      seeds[roll] = candidate;
      ++found;
    }
  }
  REQUIRE(found == seeds.size());
  return seeds;
}

std::array<unsigned, 3> TwoFavorableAllianceRolls() {
  for (unsigned candidate = 1; candidate < 256; ++candidate) {
    VS::Core::Rand15 random(candidate);
    const bool first = (random.Next() & 0x7F) < 0x4C;
    const auto first_state = random.State();
    const bool second = (random.Next() & 0x7F) < 0x4C;
    if (first && second) return {candidate, first_state, random.State()};
  }
  FAIL("No seed for two favorable alliance reactions");
  return {};
}

struct AshleyActionFixture : AshleyAllianceFixture {
  Battle_ActionDefinition saved_action = Battle_actionDefinitions[42];
  Battle_ActorStats stats{};
  Battle_Ai_StatApplicationView application{};

  AshleyActionFixture() {
    Battle_actionDefinitions[42] = {};
    actors[1].stats = &stats;
    state.settingsBit27 = 1;
    state.decisionDelay = 1;
    state.savedRelationOptions = -1;
    other_state.savedRelationOptions = -1;
    Settings()->decisionCallback = AshleyAllianceTestCallback;
    application.action = 42;
    application.first.actor = 0;
    application.count = 1;
    application.entries[0].actor = 1;
  }

  ~AshleyActionFixture() { Battle_actionDefinitions[42] = saved_action; }

  void Apply(std::uint16_t flags) {
    Battle_actionDefinitions[42].aiFlags =
        std::bit_cast<Battle_ActionAiFlags>(flags);
    Battle_Ai_ApplyActorActionReaction(Reaction(), &application);
  }
};

static_assert(offsetof(Battle_Ai_SettingsView, savedDecisionCallback) ==
              offsetof(Battle_Ai_ActorAiState, reserved60_6C));
static_assert(sizeof(Battle_Ai_SettingsView::savedDecisionCallback) <=
              sizeof(Battle_Ai_ActorAiState::reserved60_6C));
static_assert(offsetof(Battle_Ai_ReactionView, allianceReactionCount) ==
              offsetof(Battle_Ai_ActorAiState, reserved120) + 6);

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

TEST_CASE(
    "AI Ashley alliance begins after two favorable reactions and restores "
    "behavior",
    "[ai][ashley-alliance]") {
  AshleyAllianceFixture fixture;
  auto* settings = fixture.Settings();
  settings->decisionCallback = AshleyAllianceTestCallback;
  fixture.state.settingsBit25 = 0;
  fixture.state.decisionDelay = 1;
  fixture.state.relations[0].classification = 1;
  Battle_actorMarkers[1] = 0x05;

  const auto [seed, after_first, after_second] = TwoFavorableAllianceRolls();
  Core_Random_Seed(seed);

  Battle_Ai_TryBeginAshleyAlliance(fixture.Reaction(), 0);
  CHECK(fixture.Reaction()->allianceReactionCount == 1);
  CHECK(fixture.Reaction()->temporaryAshleyAlliance == 0);
  CHECK(settings->decisionCallback == AshleyAllianceTestCallback);
  CHECK(Core_Random_State() == after_first);

  Battle_Ai_TryBeginAshleyAlliance(fixture.Reaction(), 0);
  fixture.CheckEntered(2, AshleyAllianceTestCallback);
  CHECK(Core_Random_State() == after_second);

  auto& frame = *Battle_Ai_FrameState();
  frame.randomState = 0x2000;
  frame.lastBudgetTick = 2;
  Battle_Ai_EndAshleyAlliance(fixture.Reaction());

  fixture.CheckRestored(AshleyAllianceTestCallback);
  fixture.CheckDeferredRecovery();
}

TEST_CASE(
    "AI Ashley alliance failed reactions leave progress and callback unchanged",
    "[ai][ashley-alliance]") {
  AshleyAllianceFixture fixture;
  auto* settings = fixture.Settings();
  settings->decisionCallback = AshleyAllianceTestCallback;
  fixture.Reaction()->allianceReactionCount = 1;
  Battle_actorMarkers[1] = 0x05;

  unsigned seed = 0;
  unsigned after_roll = 0;
  for (unsigned candidate = 1; candidate < 256; ++candidate) {
    Core_Random_Seed(candidate);
    const bool succeeds = (Core_Random_Rand() & 0x7F) < 0x4C;
    if (!succeeds) {
      seed = candidate;
      after_roll = Core_Random_State();
      break;
    }
  }
  REQUIRE(seed != 0);
  Core_Random_Seed(seed);

  Battle_Ai_TryBeginAshleyAlliance(fixture.Reaction(), 0);

  CHECK(fixture.Reaction()->allianceReactionCount == 1);
  CHECK(fixture.Reaction()->temporaryAshleyAlliance == 0);
  CHECK(settings->decisionCallback == AshleyAllianceTestCallback);
  CHECK(settings->savedDecisionCallback == nullptr);
  CHECK(Battle_actorMarkers[1] == 0x05);
  CHECK(Core_Random_State() == after_roll);
}

TEST_CASE(
    "AI Ashley alliance forced entry skips BIOS RNG and handles a null saved "
    "callback",
    "[ai][ashley-alliance]") {
  AshleyAllianceFixture fixture;
  fixture.Reaction()->allianceReactionCount = 9;
  Battle_actorMarkers[1] = 0x05;

  Battle_Ai_EndAshleyAlliance(fixture.Reaction());
  fixture.CheckRestored(nullptr);
  CHECK(Battle_actorMarkers[1] == 0x05);

  fixture.state.settingsBit25 = 1;
  const unsigned random_before = Core_Random_State();
  Battle_Ai_TryBeginAshleyAlliance(fixture.Reaction(), 1);
  CHECK(Core_Random_State() == random_before);
  fixture.CheckEntered(0, nullptr);

  Battle_Ai_EndAshleyAlliance(fixture.Reaction());
  fixture.CheckRestored(Battle_Ai_UpdateEngagementApproachBehavior);
  CHECK(Battle_actorMarkers[1] == 0x05);
}

TEST_CASE("AI Ashley alliance compares before truncating the reaction count",
          "[ai][ashley-alliance][alliance-wrap]") {
  AshleyAllianceFixture fixture;
  const auto seeds = AshleyAllianceRollSeeds();
  Core_Random_Seed(seeds[0]);
  fixture.Reaction()->allianceReactionCount = 255;
  fixture.Settings()->decisionCallback = AshleyAllianceTestCallback;
  Battle_Ai_TryBeginAshleyAlliance(fixture.Reaction(), 0);
  CHECK(fixture.Reaction()->allianceReactionCount == 0);
  CHECK(fixture.Reaction()->temporaryAshleyAlliance == 1);
  CHECK(fixture.Settings()->savedDecisionCallback ==
        AshleyAllianceTestCallback);
  CHECK(fixture.Settings()->decisionCallback ==
        Battle_Ai_UpdateEngagementApproachBehavior);
}

TEST_CASE("AI Ashley alliance covers every masked roll and byte counter",
          "[ai][ashley-alliance][alliance-boundaries]") {
  AshleyAllianceFixture fixture;
  const auto seeds = AshleyAllianceRollSeeds();
  for (unsigned roll = 0; roll < seeds.size(); ++roll) {
    for (unsigned counter = 0; counter < 256; ++counter) {
      fixture.state = {};
      auto* settings = fixture.Settings();
      settings->decisionCallback = AshleyAllianceTestCallback;
      fixture.Reaction()->allianceReactionCount = static_cast<u_char>(counter);
      fixture.state.actorId = 1;
      Battle_actorMarkers[1] = 0x05;
      Core_Random_Seed(seeds[roll]);
      VS::Core::Rand15 expected_random(seeds[roll]);
      REQUIRE((expected_random.Next() & 0x7F) == static_cast<int>(roll));
      Battle_Ai_TryBeginAshleyAlliance(fixture.Reaction(), 0);
      const bool favorable = roll < 0x4C;
      const bool entered = favorable && counter + 1 >= 2;
      CHECK(fixture.Reaction()->allianceReactionCount ==
            static_cast<u_char>(counter + (favorable ? 1 : 0)));
      CHECK(fixture.Reaction()->temporaryAshleyAlliance == entered);
      CHECK(settings->decisionCallback ==
            (entered ? Battle_Ai_UpdateEngagementApproachBehavior
                     : AshleyAllianceTestCallback));
      CHECK(settings->savedDecisionCallback ==
            (entered ? AshleyAllianceTestCallback : nullptr));
      CHECK(static_cast<unsigned char>(Battle_actorMarkers[1]) ==
            (entered ? 0x85 : 0x05));
      CHECK(Core_Random_State() == expected_random.State());
    }
  }
}

TEST_CASE(
    "AI Ashley alliance follows actual helpful and harmful action reactions",
    "[ai][ashley-alliance][alliance-action]") {
  AshleyActionFixture fixture;
  const auto seeds = AshleyAllianceRollSeeds();
  fixture.state.relations[0].classification = 1;
  for (int reaction = 0; reaction < 2; ++reaction) {
    Core_Random_Seed(seeds[0]);
    fixture.Apply(1u << 14);
  }
  REQUIRE(fixture.Reaction()->temporaryAshleyAlliance == 1);
  CHECK(fixture.Settings()->savedDecisionCallback ==
        AshleyAllianceTestCallback);
  CHECK(fixture.state.relations[0].timer == 28);
  fixture.Apply(1u << 15);
  CHECK(fixture.Reaction()->temporaryAshleyAlliance == 0);
  CHECK(fixture.Reaction()->allianceReactionCount == 0);
  CHECK(fixture.Settings()->decisionCallback == AshleyAllianceTestCallback);
  CHECK(fixture.state.relations[0].classification == 3);
  CHECK(fixture.state.awarenessMarker == 6);
}

TEST_CASE("AI saved relations force Ashley alliance without consuming BIOS RNG",
          "[ai][ashley-alliance][alliance-persistence]") {
  AshleyActionFixture fixture;
  const auto random = Core_Random_State();
  for (int cycle = 0; cycle < 2; ++cycle) {
    fixture.state.savedRelationOptions = 0x1235;
    Battle_Ai_ApplySavedActorAiRelations();
    REQUIRE(fixture.Reaction()->temporaryAshleyAlliance == 1);
    CHECK(fixture.Reaction()->allianceReactionCount == 0);
    CHECK(fixture.Settings()->savedDecisionCallback ==
          AshleyAllianceTestCallback);
    CHECK(fixture.state.relations[0].classification == ((0x1235 >> 1) & 3));
    CHECK(fixture.state.relations[0].classificationTimer ==
          ((0x1235 >> 6) & 127));
    CHECK(Core_Random_State() == random);
    Battle_Ai_EndAshleyAlliance(fixture.Reaction());
    CHECK(fixture.Settings()->decisionCallback == AshleyAllianceTestCallback);
  }
}

TEST_CASE(
    "AI Ashley alliance original entries resolve to typed native callbacks",
    "[ai][ashley-alliance][alliance-binding]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DC19C) ==
        reinterpret_cast<void*>(&Battle_Ai_EndAshleyAlliance));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DC210) ==
        reinterpret_cast<void*>(&Battle_Ai_TryBeginAshleyAlliance));
}

TEST_CASE("Inactive Ashley alliance exit only resets the favorable counter",
          "[ai][ashley-alliance][alliance-inactive]") {
  AshleyAllianceFixture fixture;
  fixture.Settings()->decisionCallback = AshleyAllianceTestCallback;
  fixture.Settings()->savedDecisionCallback =
      Battle_Ai_UpdateEngagementApproachBehavior;
  Battle_actorMarkers[1] = static_cast<char>(0xA5);
  const auto random = Core_Random_State();
  const auto frame_random = Battle_Ai_FrameState()->randomState;
  for (unsigned count = 0; count < 256; ++count) {
    fixture.Reaction()->allianceReactionCount = static_cast<u_char>(count);
    auto expected = fixture.state;
    reinterpret_cast<Battle_Ai_ReactionView*>(&expected)
        ->allianceReactionCount = 0;
    Battle_Ai_EndAshleyAlliance(fixture.Reaction());
    CHECK(StateBytes(fixture.state) == StateBytes(expected));
    CHECK(static_cast<unsigned char>(Battle_actorMarkers[1]) == 0xA5);
    CHECK(Core_Random_State() == random);
    CHECK(Battle_Ai_FrameState()->randomState == frame_random);
  }
}

TEST_CASE("Ashley alliance recovery preserves callback and native busy gates",
          "[ai][ashley-alliance][alliance-recovery]") {
  const auto settings_bit = GENERATE(0, 1);
  const auto busy = GENERATE(0, 1, 2);
  const bool saved_callback = GENERATE(false, true);
  AshleyAllianceFixture fixture;
  auto* settings = fixture.Settings();
  settings->decisionCallback = Battle_Ai_UpdateEngagementApproachBehavior;
  settings->savedDecisionCallback =
      saved_callback ? AshleyAllianceTestCallback : nullptr;
  fixture.state.settingsBit25 = static_cast<u_char>(settings_bit);
  fixture.state.decisionDelay = busy == 1 ? 3 : 0;
  fixture.state.nativePointer18C = busy == 2 ? &fixture : nullptr;
  fixture.state.awarenessMarker = 0x71;
  fixture.state.awarenessFramesRemaining = 0x72;
  fixture.Reaction()->temporaryAshleyAlliance = 1;
  fixture.Reaction()->allianceReactionCount = 9;
  Battle_actorMarkers[1] = static_cast<char>(0xA5);
  fixture.actors[1].commandFlags = 0x5A;
  const auto random = Core_Random_State();
  auto& frame = *Battle_Ai_FrameState();
  frame.randomState = 0x2000;
  frame.lastBudgetTick = 2;
  Battle_Ai_EndAshleyAlliance(fixture.Reaction());
  fixture.CheckRestored(saved_callback
                            ? AshleyAllianceTestCallback
                            : Battle_Ai_UpdateEngagementApproachBehavior);
  CHECK(static_cast<unsigned char>(Battle_actorMarkers[1]) == 0x25);
  CHECK(fixture.state.relations[0].classification == 3);
  fixture.CheckRecovery(settings_bit, busy, random);
}
