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
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Field/Gimmicks.h"
}

#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorSettingsLive.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Core/Math/ScalarMath.hpp"

static_assert(
    std::is_same_v<decltype(&VS::Battle::Ai::ClearLiveActorAiNavigationReset),
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
    VS::Battle::Ai::MakeLiveTargetHostile(
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
          VS::Battle::Ai::UpdateLiveEngagementApproachBehavior);
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
  Battle_ActionRequest request{};

  AshleyActionFixture() {
    Battle_actionDefinitions[42] = {};
    actors[1].stats = &stats;
    state.settingsBit27 = 1;
    state.decisionDelay = 1;
    state.savedRelationOptions = -1;
    other_state.savedRelationOptions = -1;
    Settings()->decisionCallback = AshleyAllianceTestCallback;
    request.actionId = 42;
    request.primaryTarget.target.targetActor = 0;
    request.targetCount = 1;
    request.targets[0].target.targetActor = 1;
  }

  ~AshleyActionFixture() { Battle_actionDefinitions[42] = saved_action; }

  void Apply(std::uint16_t flags) {
    Battle_actionDefinitions[42].aiFlags =
        std::bit_cast<Battle_ActionAiFlags>(flags);
    VS::Battle::Ai::ApplyLiveActorActionReaction(Reaction(), &request);
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
    CHECK(VS::Battle::Ai::PackLiveActorAiPersistenceFlags(slot) == -1);
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
    REQUIRE(VS::Battle::Ai::PackLiveActorAiPersistenceFlags(slot) ==
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
    CHECK(VS::Battle::Ai::PackLiveActorAiPersistenceFlags(4) ==
          static_cast<int>(delay));
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
  VS::Battle::Ai::InitializeLiveActorAiExtensionHook();
  VS::Battle::Ai::InitializeLiveActorAiExtensionHook();
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
    VS::Battle::Ai::ClearLiveActorAiNavigationReset(slot);
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

  VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 0);
  CHECK(fixture.Reaction()->allianceReactionCount == 1);
  CHECK(fixture.Reaction()->temporaryAshleyAlliance == 0);
  CHECK(settings->decisionCallback == AshleyAllianceTestCallback);
  CHECK(Core_Random_State() == after_first);

  VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 0);
  fixture.CheckEntered(2, AshleyAllianceTestCallback);
  CHECK(Core_Random_State() == after_second);

  auto& frame = *Battle_Ai_FrameState();
  frame.randomState = 0x2000;
  frame.lastBudgetTick = 2;
  VS::Battle::Ai::EndLiveAshleyAlliance(fixture.Reaction());

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

  VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 0);

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

  VS::Battle::Ai::EndLiveAshleyAlliance(fixture.Reaction());
  fixture.CheckRestored(nullptr);
  CHECK(Battle_actorMarkers[1] == 0x05);

  fixture.state.settingsBit25 = 1;
  const unsigned random_before = Core_Random_State();
  VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 1);
  CHECK(Core_Random_State() == random_before);
  fixture.CheckEntered(0, nullptr);

  VS::Battle::Ai::EndLiveAshleyAlliance(fixture.Reaction());
  fixture.CheckRestored(VS::Battle::Ai::UpdateLiveEngagementApproachBehavior);
  CHECK(Battle_actorMarkers[1] == 0x05);
}

TEST_CASE("AI Ashley alliance compares before truncating the reaction count",
          "[ai][ashley-alliance][alliance-wrap]") {
  AshleyAllianceFixture fixture;
  const auto seeds = AshleyAllianceRollSeeds();
  Core_Random_Seed(seeds[0]);
  fixture.Reaction()->allianceReactionCount = 255;
  fixture.Settings()->decisionCallback = AshleyAllianceTestCallback;
  VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 0);
  CHECK(fixture.Reaction()->allianceReactionCount == 0);
  CHECK(fixture.Reaction()->temporaryAshleyAlliance == 1);
  CHECK(fixture.Settings()->savedDecisionCallback ==
        AshleyAllianceTestCallback);
  CHECK(fixture.Settings()->decisionCallback ==
        VS::Battle::Ai::UpdateLiveEngagementApproachBehavior);
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
      VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 0);
      const bool favorable = roll < 0x4C;
      const bool entered = favorable && counter + 1 >= 2;
      CHECK(fixture.Reaction()->allianceReactionCount ==
            static_cast<u_char>(counter + (favorable ? 1 : 0)));
      CHECK(fixture.Reaction()->temporaryAshleyAlliance == entered);
      CHECK(settings->decisionCallback ==
            (entered ? VS::Battle::Ai::UpdateLiveEngagementApproachBehavior
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
    VS::Battle::Ai::ApplyLiveSavedActorAiRelations();
    REQUIRE(fixture.Reaction()->temporaryAshleyAlliance == 1);
    CHECK(fixture.Reaction()->allianceReactionCount == 0);
    CHECK(fixture.Settings()->savedDecisionCallback ==
          AshleyAllianceTestCallback);
    CHECK(fixture.state.relations[0].classification == ((0x1235 >> 1) & 3));
    CHECK(fixture.state.relations[0].classificationTimer ==
          ((0x1235 >> 6) & 127));
    CHECK(Core_Random_State() == random);
    VS::Battle::Ai::EndLiveAshleyAlliance(fixture.Reaction());
    CHECK(fixture.Settings()->decisionCallback == AshleyAllianceTestCallback);
  }
}

TEST_CASE("Inactive Ashley alliance exit only resets the favorable counter",
          "[ai][ashley-alliance][alliance-inactive]") {
  AshleyAllianceFixture fixture;
  fixture.Settings()->decisionCallback = AshleyAllianceTestCallback;
  fixture.Settings()->savedDecisionCallback =
      VS::Battle::Ai::UpdateLiveEngagementApproachBehavior;
  Battle_actorMarkers[1] = static_cast<char>(0xA5);
  const auto random = Core_Random_State();
  const auto frame_random = Battle_Ai_FrameState()->randomState;
  for (unsigned count = 0; count < 256; ++count) {
    fixture.Reaction()->allianceReactionCount = static_cast<u_char>(count);
    auto expected = fixture.state;
    reinterpret_cast<Battle_Ai_ReactionView*>(&expected)
        ->allianceReactionCount = 0;
    VS::Battle::Ai::EndLiveAshleyAlliance(fixture.Reaction());
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
  settings->decisionCallback =
      VS::Battle::Ai::UpdateLiveEngagementApproachBehavior;
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
  VS::Battle::Ai::EndLiveAshleyAlliance(fixture.Reaction());
  fixture.CheckRestored(
      saved_callback ? AshleyAllianceTestCallback
                     : VS::Battle::Ai::UpdateLiveEngagementApproachBehavior);
  CHECK(static_cast<unsigned char>(Battle_actorMarkers[1]) == 0x25);
  CHECK(fixture.state.relations[0].classification == 3);
  fixture.CheckRecovery(settings_bit, busy, random);
}

TEST_CASE("AI linked actors resolve through the live battle actor list",
          "[ai][relations][linked-actors]") {
  struct ListFixture {
    AiLifecycleFixture table;
    Battle_Actor* saved_head = Battle_actors[0];
    ~ListFixture() { Battle_actors[0] = saved_head; }
  } fixture;
  constexpr std::array<int, 4> kIds{0, 2, 5, 9};
  std::array<Battle_Actor, kIds.size()> actors{};
  std::array<Battle_Ai_ActorAiState, kIds.size()> states{};
  for (std::size_t index = 0; index < kIds.size(); ++index) {
    std::memset(&states[index], 0xA5, sizeof(states[index]));
    states[index].linkedActor = 16;
    actors[index].id = kIds[index];
    actors[index].next = index + 1 < kIds.size() ? &actors[index + 1] : nullptr;
    Battle_Ai_actorAiStates[kIds[index]] = &states[index];
  }
  // Ashley heads the list: it shares group 3 and is never examined or matched.
  actors[0].aiGroup = 3;
  actors[0].aiLinkedGroup = 3;
  actors[1].aiGroup = 3;
  actors[1].aiLinkedGroup = 4;
  actors[2].aiGroup = 4;
  actors[2].aiLinkedGroup = 3;
  actors[3].aiGroup = 4;
  actors[3].aiLinkedGroup = 4;
  actors[3].reserved30Bit4 = 0x1F;
  states[3].linkedActor = 7;
  auto expected = states;
  expected[1].linkedActor = 5;
  expected[2].linkedActor = 2;
  Battle_actors[0] = actors.data();
  VS::Battle::Ai::ResolveLiveAiLinkedActors();
  for (std::size_t index = 0; index < kIds.size(); ++index) {
    CHECK(StateBytes(states[index]) == StateBytes(expected[index]));
  }
  CHECK(states[0].linkedActor == 16);
  CHECK(states[1].linkedActor == 5);
  CHECK(states[2].linkedActor == 2);
  CHECK(states[3].linkedActor == 7);
}

TEST_CASE("AI saved settings flags apply through the actor-state creation view",
          "[ai][actor-settings][saved-settings]") {
  // The creation view's saved pair starts at the actor's group word, so its
  // second word is the room spawn's saved flags. Only that word's low half is
  // read and only the settings word is written.
  const auto bytes = [](const Battle_Actor& actor) {
    std::array<std::byte, sizeof(Battle_Actor)> image{};
    std::memcpy(image.data(), &actor, image.size());
    return image;
  };
  Battle_Actor actor{};
  std::memset(&actor, 0xA5, sizeof(actor));
  actor.reserved34 = static_cast<int>(0xBEEF0ACFu);
  actor.templateWord2C = 0x12345678;
  const auto before = bytes(actor);
  auto* view = reinterpret_cast<Battle_Ai_AiActorView*>(&actor);
  REQUIRE((view->savedAiSettingsFlags & 1u) != 0);
  int packed = view->aiSettings;
  VS::Battle::Ai::ApplyLiveSavedAiSettingsFlags(
      &packed, reinterpret_cast<const Battle_Ai_SavedSettingsPair*>(
                   &view->savedAiSettings));
  CHECK(std::bit_cast<std::uint32_t>(packed) == 0xACF45678u);
  CHECK(bytes(actor) == before);
}

namespace {
// Ashley heads the list with the state under test; actors 2 and 5 follow in
// linked groups so the trailing link resolution has work to do.
struct ActorSettingsFixture {
  AiLifecycleFixture table;
  Battle_Actor* saved_head = Battle_actors[0];
  void* saved_has_waypoints = Battle_Ai_roomHasWaypoints;
  std::uint16_t* saved_waypoints = Battle_Ai_roomWaypoints;
  void* saved_section6 = Field_roomData.section6;
  u_int saved_section6_length = Field_roomData.header.section6Length;
  std::array<Battle_Actor, 3> actors{};
  std::array<Battle_Ai_ActorAiState, 3> states{};
  alignas(std::uint32_t) std::array<unsigned char, 0x450> auxiliary{};

  ActorSettingsFixture() {
    constexpr std::array<int, 3> kIds{0, 2, 5};
    for (std::size_t index = 0; index < kIds.size(); ++index) {
      actors[index].id = kIds[index];
      actors[index].next =
          index + 1 < actors.size() ? &actors[index + 1] : nullptr;
      Battle_Ai_actorAiStates[kIds[index]] = &states[index];
    }
    actors[1].aiGroup = 3;
    actors[1].aiLinkedGroup = 4;
    actors[2].aiGroup = 4;
    actors[2].aiLinkedGroup = 3;
    states[1].linkedActor = 16;
    states[2].linkedActor = 16;
    Battle_actors[0] = actors.data();
    Battle_Ai_roomHasWaypoints = nullptr;
    Field_roomData.section6 = auxiliary.data();
    Field_roomData.header.section6Length = auxiliary.size();
  }

  ~ActorSettingsFixture() {
    Battle_actors[0] = saved_head;
    Battle_Ai_roomHasWaypoints = saved_has_waypoints;
    Battle_Ai_roomWaypoints = saved_waypoints;
    Field_roomData.section6 = saved_section6;
    Field_roomData.header.section6Length = saved_section6_length;
  }

  // Waypoint 5 of the room's auxiliary section, active or not.
  void UseRoomWaypoint(std::uint16_t waypoint) {
    std::memcpy(auxiliary.data() + 0x440 + (5 * sizeof(waypoint)), &waypoint,
                sizeof(waypoint));
    Battle_Ai_roomHasWaypoints = auxiliary.data();
    Battle_Ai_roomWaypoints =
        reinterpret_cast<std::uint16_t*>(auxiliary.data() + 0x440);
  }

  // Ashley's spawn words and a patterned state viewed through the settings
  // view, with the given behaviour index and movement flags.
  Battle_Ai_SettingsView& PrepareLeader(unsigned behaviour,
                                        std::uint16_t movement_flags) {
    Battle_Actor& actor = actors[0];
    actor.subType = 0xAB;
    // Spawn group: AI group 0xF, behaviour bits 4-8, linked group 0x1F,
    // patrol row 2, wander radius 0x2A and wander tile (0x15, 0x1B).
    actor.aiGroup = 0xF;
    actor.reserved30Bit4 = behaviour;
    actor.aiLinkedGroup = 0x1F;
    actor.reserved31Bit6 = 2u | (0x2Au << 2) | (0x15u << 8) | (0x1Bu << 13);
    // Spawn flags: mode bits 12-13 both set and patrol rate bit 14.
    actor.reserved34 = 0x7FFF;
    Battle_Ai_ActorAiState& state = states[0];
    std::memset(&state, 0xA5, sizeof(state));
    state.actorId = 0;
    state.linkedActor = 7;
    auto& view = reinterpret_cast<Battle_Ai_SettingsView&>(state);
    view.actor = reinterpret_cast<Battle_Ai_SettingsActorView*>(&actor);
    view.collisionRadius = 129;
    view.movementFlags = movement_flags;
    return view;
  }

  // Every multi-bit field nonzero and neighbouring single-bit flags differ.
  static constexpr std::uint32_t kLeaderSettings =
      3u | (2u << 2) | (5u << 4) | (1u << 7) | (3u << 8) | (2u << 10) |
      (1u << 12) | (1u << 14) | (1u << 16) | (6u << 18) | (2u << 21) |
      (1u << 23) | (1u << 25) | (1u << 27) | (1u << 28) | (3u << 30);
};

// The state ApplyLiveActorAiSettings leaves from the fixture leader's
// settings and spawn words.
void ExpectLiveSettings(Battle_Ai_ActorAiState& expected, int waypoints,
                        std::uint16_t movement_flags, unsigned behaviour) {
  auto& want = reinterpret_cast<Battle_Ai_SettingsView&>(expected);
  const unsigned far = Battle_Ai_kPerceptionRangesSquared[6];
  want.farDistanceSquared = far;
  want.nearDistanceSquared = far > 0x23FFFF ? 0x240000 : far;
  want.angle = Battle_Ai_viewAngles[1][0];
  want.viewCosine = VS::Core::Math::Cosine((want.angle / 2) * 4096 / 360);
  want.settingsBit15 = 0;
  want.settingsBit14 = 1;
  want.settingsBit13 = 0;
  want.opaque2D = waypoints == 2 ? 1 : 0;
  want.opaqueB2 = 3;
  want.opaqueB0 = Battle_Ai_settingsBits8Values[3][0];
  want.facingDelay =
      static_cast<std::uint16_t>((32 * Battle_Ai_facingDelayScales[2]) >> 4);
  want.settingsConstant23 = 1;
  want.settingsByte3Bit0 = 0;
  want.settingsBit23 = 1;
  want.settingsBits21 = 2;
  want.settingsBits28 = 1;
  want.settingsBits30 = 3;
  want.settingsBit26 = 0;
  want.movePeriod = Battle_Ai_kWanderTimingPresets[5].movePeriod;
  want.restPeriod = Battle_Ai_kWanderTimingPresets[5].restPeriod;
  want.wanderRate = Battle_Ai_kWanderTimingPresets[5].rate;
  want.settingsBit27 = 1;
  want.settingsBit25 = 1;
  if ((movement_flags & 0xC0) == 0) {
    want.settingsBits30Value = Battle_Ai_kSettingsBits30Values[3];
  }
  want.settingsByte0Bit7 = 1;
  want.allowFallbackAction = 1;
  want.settingsModeLow = 1;
  want.opaque102 = Battle_Ai_kFlags34Bit14Values[1];
  want.opaque101 = 2;
  want.opaqueC8 = 0x2A;
  want.opaqueCC = 0x15;
  want.opaqueCE = 0x1B;
  want.decisionCallback =
      behaviour < 9 ? Battle_Ai_decisionCallbacks[behaviour] : nullptr;
  want.linkedActor = 16;
  // Ashley's action metadata only sets her action cost threshold.
  expected.actionCostEnabled = 1;
}
}  // namespace

TEST_CASE("AI settings apply every decoded field through the live view",
          "[ai][actor-settings][settings-live]") {
  // 0: no auxiliary section; 1: inactive waypoint; 2: active waypoint.
  const int waypoints = GENERATE(0, 1, 2);
  const std::uint16_t movement_flags = GENERATE(0x0000, 0x0040, 0xFF3F);
  const unsigned behaviour = GENERATE(0u, 4u, 8u, 9u, 31u);
  ActorSettingsFixture fixture;
  if (waypoints != 0) fixture.UseRoomWaypoint(waypoints == 2 ? 0x8001 : 0x7FFF);
  Battle_Ai_SettingsView& view =
      fixture.PrepareLeader(behaviour, movement_flags);
  const Battle_Ai_ActorAiState& state = fixture.states[0];
  Battle_Ai_ActorAiState expected;
  std::memcpy(&expected, &state, sizeof(state));
  ExpectLiveSettings(expected, waypoints, movement_flags, behaviour);
  const auto& want = reinterpret_cast<const Battle_Ai_SettingsView&>(expected);
  Battle_Ai_SettingsWord word{};
  word.word = ActorSettingsFixture::kLeaderSettings;

  VS::Battle::Ai::ApplyLiveActorAiSettings(&view, &word);
  CHECK(state.linkedActor == 16);
  CHECK(view.decisionCallback == want.decisionCallback);
  CHECK(view.settingsBits30Value == want.settingsBits30Value);
  CHECK(view.opaque2D == want.opaque2D);
  CHECK(StateBytes(state) == StateBytes(expected));
  CHECK(fixture.states[1].linkedActor == 5);
  CHECK(fixture.states[2].linkedActor == 2);
}

TEST_CASE("AI settings retain their original module address",
          "[ai][actor-settings][settings-live]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x800E5A9C) ==
      reinterpret_cast<const void*>(&VS::Battle::Ai::ApplyLiveActorAiSettings));
}
