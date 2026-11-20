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

#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorSettingsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Battle/Damage.h"
#include "Battle/Feedback.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Random.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Unported.hpp"
#include "Field/Gimmicks.h"
#include "Legacy/Psx.h"

static_assert(
    std::is_same_v<decltype(&VS::Battle::Ai::ClearLiveActorAiNavigationReset),
                   void (*)(int)>);

namespace {
struct AiLifecycleFixture {
  decltype(VS::Battle::Ai::actorAiStates) saved = VS::Battle::Ai::actorAiStates;
  VS::Battle::Ai::ActorAiState state{};

  AiLifecycleFixture() {
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), saved.size(), nullptr);
  }

  ~AiLifecycleFixture() {
    std::copy(saved.begin(), saved.end(),
              VS::Battle::Ai::actorAiStates.begin());
  }
};

struct HostilityFixture {
  unsigned saved_random = VS::Core::BiosRand().State();
  VS::Battle::Ai::FrameContext saved_frame = VS::Battle::Ai::LiveFrameContext();
  Battle_Actor* saved_actor = Battle_actors[3];
  VS::Battle::Ai::ActorAiState state{};
  Battle_Actor actor{};

  HostilityFixture() {
    state.actor_id = 3;
    Battle_actors[3] = &actor;
  }

  ~HostilityFixture() {
    VS::Core::SeedBiosRand(saved_random);
    VS::Battle::Ai::LiveFrameContext() = saved_frame;
    Battle_actors[3] = saved_actor;
  }

  void SetRelation(int target, std::uint32_t word) {
    std::memcpy(&state.relations[target], &word, sizeof(word));
  }

  void MakeHostile(int target) {
    VS::Battle::Ai::MakeLiveTargetHostile(
        reinterpret_cast<VS::Battle::Ai::ReactionView*>(&state), target);
  }

  void CheckInitialReaction(unsigned busy) const {
    REQUIRE(state.relations[7].classification == 3);
    CHECK(state.relations[7].counter == 0);
    CHECK(state.opaque18 == 1);
    CHECK(state.awareness_marker == 6);
    CHECK(state.awareness_frames_remaining == 40);
    CHECK(state.decision_delay == (busy == 0 ? 30 : (busy & 1)));
    CHECK(actor.commandFlags == (busy == 0 ? 2 : 0x5A));
  }
};

int AshleyAllianceTestCallback(VS::Battle::Ai::ActorAiState*) { return 0x5A; }

struct AshleyAllianceFixture {
  decltype(VS::Battle::Ai::actorAiStates) saved_ai_states =
      VS::Battle::Ai::actorAiStates;
  VS::Battle::Ai::FrameContext saved_frame = VS::Battle::Ai::LiveFrameContext();
  unsigned saved_random = VS::Core::BiosRand().State();
  Battle_Actor* saved_actor0 = Battle_actors[0];
  Battle_Actor* saved_actor1 = Battle_actors[1];
  char saved_marker1 = VS::Battle::actorMarkers[1];
  VS::Battle::Ai::ActorAiState state{};
  VS::Battle::Ai::ActorAiState other_state{};
  Battle_Actor actors[2]{};

  AshleyAllianceFixture() {
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), saved_ai_states.size(),
                nullptr);
    state.actor_id = 1;
    other_state.actor_id = 0;
    actors[0].id = 0;
    actors[0].next = &actors[1];
    actors[1].id = 1;
    VS::Battle::Ai::actorAiStates[0] = &other_state;
    VS::Battle::Ai::actorAiStates[1] = &state;
    Battle_actors[0] = &actors[0];
    Battle_actors[1] = &actors[1];
  }

  ~AshleyAllianceFixture() {
    std::copy(saved_ai_states.begin(), saved_ai_states.end(),
              VS::Battle::Ai::actorAiStates.begin());
    VS::Battle::Ai::LiveFrameContext() = saved_frame;
    VS::Core::SeedBiosRand(saved_random);
    Battle_actors[0] = saved_actor0;
    Battle_actors[1] = saved_actor1;
    VS::Battle::actorMarkers[1] = saved_marker1;
  }

  VS::Battle::Ai::ReactionView* Reaction() {
    return reinterpret_cast<VS::Battle::Ai::ReactionView*>(&state);
  }

  VS::Battle::Ai::SettingsView* Settings() {
    return reinterpret_cast<VS::Battle::Ai::SettingsView*>(&state);
  }

  void CheckRestored(int (*callback)(VS::Battle::Ai::ActorAiState*)) {
    CHECK(Reaction()->temporary_ashley_alliance == 0);
    CHECK(Reaction()->alliance_reaction_count == 0);
    CHECK(Settings()->saved_decision_callback == nullptr);
    CHECK(Settings()->decision_callback == callback);
  }

  void CheckEntered(unsigned count,
                    int (*saved_callback)(VS::Battle::Ai::ActorAiState*)) {
    CHECK(Reaction()->alliance_reaction_count == count);
    CHECK(Reaction()->temporary_ashley_alliance == 1);
    CHECK(Settings()->saved_decision_callback == saved_callback);
    CHECK(Settings()->decision_callback ==
          VS::Battle::Ai::UpdateLiveEngagementApproachBehavior);
    CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[1]) == 0x85);
  }

  void CheckRecovery(int settings_bit, int busy, unsigned random) {
    CHECK(state.awareness_marker == (settings_bit ? 0x71 : 6));
    CHECK(state.awareness_frames_remaining == (settings_bit ? 0x72 : 40));
    CHECK(VS::Battle::Ai::LiveFrameContext().randomState ==
          (settings_bit ? 0x2000 : 0x2002));
    CHECK(VS::Core::BiosRand().State() == random);
    CHECK(state.decision_delay ==
          (busy == 1 ? 3 : (!settings_bit && busy == 0 ? 30 : 0)));
    CHECK(actors[1].commandFlags == (!settings_bit && busy == 0 ? 2 : 0x5A));
  }

  void CheckDeferredRecovery() {
    CHECK(VS::Battle::actorMarkers[1] == 0x05);
    CHECK(state.relations[0].classification == 3);
    CHECK(state.awareness_marker == 6);
    CHECK(state.awareness_frames_remaining == 0x28);
    CHECK(state.decision_delay == 1);
    CHECK(VS::Battle::Ai::LiveFrameContext().randomState == 0x2002);
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
    state.settings_bit27 = 1;
    state.decision_delay = 1;
    state.saved_relation_options = -1;
    other_state.saved_relation_options = -1;
    Settings()->decision_callback = AshleyAllianceTestCallback;
    request.actionId = 42;
    request.primaryTarget.target.target_actor = 0;
    request.targetCount = 1;
    request.targets[0].target.target_actor = 1;
  }

  ~AshleyActionFixture() { Battle_actionDefinitions[42] = saved_action; }

  void Apply(std::uint16_t flags) {
    Battle_actionDefinitions[42].aiFlags =
        std::bit_cast<Battle_ActionAiFlags>(flags);
    VS::Battle::Ai::ApplyLiveActorActionReaction(Reaction(), &request);
  }
};

static_assert(offsetof(VS::Battle::Ai::SettingsView, saved_decision_callback) ==
              offsetof(VS::Battle::Ai::ActorAiState, reserved_60_6c));
static_assert(sizeof(VS::Battle::Ai::SettingsView::saved_decision_callback) <=
              sizeof(VS::Battle::Ai::ActorAiState::reserved_60_6c));
static_assert(offsetof(VS::Battle::Ai::ReactionView, alliance_reaction_count) ==
              offsetof(VS::Battle::Ai::ActorAiState, reserved120) + 6);

auto StateBytes(const VS::Battle::Ai::ActorAiState& state) {
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
  VS::Battle::Ai::actorAiStates[slot] = &fixture.state;
  fixture.state.actor_id = 7;
  fixture.state.opaque_1a = 0xA5;
  for (unsigned expected = 0; expected < 8192; ++expected) {
    const std::uint32_t relation = noise | (((expected >> 1) & 3) << 6) |
                                   (((expected >> 3) & 3) << 28) |
                                   (((expected >> 6) & 127) << 20);
    std::memcpy(&fixture.state.relations[0], &relation, sizeof(relation));
    fixture.state.uses_room_action_delay = static_cast<u_char>(expected & 1);
    fixture.state.behavior_mode = (expected & 32) ? 0 : 0xA5;
    const auto before = StateBytes(fixture.state);
    REQUIRE(VS::Battle::Ai::PackLiveActorAiPersistenceFlags(slot) ==
            static_cast<int>(expected));
    CHECK(StateBytes(fixture.state) == before);
  }
}

TEST_CASE("AI persistence preserves the complete original delay byte",
          "[ai][lifecycle-leaves]") {
  AiLifecycleFixture fixture;
  VS::Battle::Ai::actorAiStates[4] = &fixture.state;
  fixture.state.behavior_mode = 1;
  for (unsigned delay = 0; delay < 256; ++delay) {
    fixture.state.uses_room_action_delay = static_cast<u_char>(delay);
    CHECK(VS::Battle::Ai::PackLiveActorAiPersistenceFlags(4) ==
          static_cast<int>(delay));
  }
}

TEST_CASE(
    "The original empty AI extension hook preserves state without reporting a "
    "gap",
    "[ai][lifecycle-leaves]") {
  AiLifecycleFixture fixture;
  fixture.state.opaque_1a = 0xA5;
  VS::Battle::Ai::actorAiStates[3] = &fixture.state;
  const auto state = StateBytes(fixture.state);
  const auto hits = ExtensionHookHits();
  VS::Battle::Ai::InitializeLiveActorAiExtensionHook();
  VS::Battle::Ai::InitializeLiveActorAiExtensionHook();
  CHECK(StateBytes(fixture.state) == state);
  CHECK(VS::Battle::Ai::actorAiStates[3] == &fixture.state);
  CHECK(ExtensionHookHits() == hits);
}

TEST_CASE("AI navigation reset clears only the original hold byte",
          "[ai][lifecycle-leaves]") {
  const bool present = GENERATE(false, true);
  AiLifecycleFixture fixture;
  for (int slot = 0; slot < 16; ++slot) {
    fixture.state.opaque_1a = 0xA5;
    fixture.state.actor_id = 7;
    fixture.state.behavior_mode = 0x5A;
    VS::Battle::Ai::actorAiStates[slot] = present ? &fixture.state : nullptr;
    auto expected = StateBytes(fixture.state);
    if (present) {
      expected[offsetof(VS::Battle::Ai::ActorAiState, opaque_1a)] =
          std::byte{0};
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
    fixture.state.actor_id = 3;
    fixture.state.opaque18 = 0xA5;
    fixture.state.awareness_marker = 0x77;
    fixture.state.awareness_frames_remaining = 0x66;
    for (int id = 0; id < 16; ++id) {
      fixture.SetRelation(id, 0xA5C10000u | static_cast<unsigned>(id));
    }
    const auto word = noise | (classification << 6) | (counter << 8);
    fixture.SetRelation(target, word);
    fixture.actor.commandFlags = 0x5A;
    auto expected = fixture.state;
    const std::uint32_t hostile = (word & 0xFFFF803Fu) | 0xC0u;
    std::memcpy(&expected.relations[target], &hostile, sizeof(hostile));
    VS::Core::SeedBiosRand(1234);
    VS::Core::Rand15 expected_random(1234);
    auto& frame = VS::Battle::Ai::LiveFrameContext();
    frame.randomState = 0xFFFF8001u;
    frame.lastBudgetTick = 7;
    if (classification != 3) {
      expected_random.Next();
      expected.opaque18 = 1;
      expected.awareness_marker = 6;
      expected.awareness_frames_remaining = 40;
      expected.decision_delay = 30;
    }
    fixture.MakeHostile(target);
    REQUIRE(StateBytes(fixture.state) == StateBytes(expected));
    CHECK(VS::Core::BiosRand().State() == expected_random.State());
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
  fixture.state.decision_delay = static_cast<u_char>(busy & 1);
  fixture.state.native_pointer_18c = (busy & 2) ? &fixture.actor : nullptr;
  fixture.SetRelation(7, 0xABCD7F7Fu);
  fixture.actor.commandFlags = 0x5A;
  VS::Core::SeedBiosRand(1234);
  VS::Core::Rand15 expected_random(1234);
  expected_random.Next();
  auto& frame = VS::Battle::Ai::LiveFrameContext();
  frame.randomState = 5;
  frame.lastBudgetTick = 11;
  fixture.MakeHostile(7);
  fixture.CheckInitialReaction(busy);
  CHECK(VS::Core::BiosRand().State() == expected_random.State());
  CHECK(frame.randomState == 16);
  fixture.state.opaque18 = 0xA5;
  fixture.state.awareness_marker = 0x77;
  fixture.state.awareness_frames_remaining = 0x66;
  fixture.state.relations[7].counter = 127;
  auto expected = fixture.state;
  expected.relations[7].counter = 0;
  frame.lastBudgetTick = 100;
  fixture.MakeHostile(7);
  CHECK(StateBytes(fixture.state) == StateBytes(expected));
  CHECK(VS::Core::BiosRand().State() == expected_random.State());
  CHECK(frame.randomState == 16);
  CHECK(fixture.actor.commandFlags == (busy == 0 ? 2 : 0x5A));
}

TEST_CASE(
    "AI Ashley alliance begins after two favorable reactions and restores "
    "behavior",
    "[ai][ashley-alliance]") {
  AshleyAllianceFixture fixture;
  auto* settings = fixture.Settings();
  settings->decision_callback = AshleyAllianceTestCallback;
  fixture.state.settings_bit25 = 0;
  fixture.state.decision_delay = 1;
  fixture.state.relations[0].classification = 1;
  VS::Battle::actorMarkers[1] = 0x05;

  const auto [seed, after_first, after_second] = TwoFavorableAllianceRolls();
  VS::Core::SeedBiosRand(seed);

  VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 0);
  CHECK(fixture.Reaction()->alliance_reaction_count == 1);
  CHECK(fixture.Reaction()->temporary_ashley_alliance == 0);
  CHECK(settings->decision_callback == AshleyAllianceTestCallback);
  CHECK(VS::Core::BiosRand().State() == after_first);

  VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 0);
  fixture.CheckEntered(2, AshleyAllianceTestCallback);
  CHECK(VS::Core::BiosRand().State() == after_second);

  auto& frame = VS::Battle::Ai::LiveFrameContext();
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
  settings->decision_callback = AshleyAllianceTestCallback;
  fixture.Reaction()->alliance_reaction_count = 1;
  VS::Battle::actorMarkers[1] = 0x05;

  unsigned seed = 0;
  unsigned after_roll = 0;
  for (unsigned candidate = 1; candidate < 256; ++candidate) {
    VS::Core::SeedBiosRand(candidate);
    const bool succeeds = (VS::Core::NextBiosRand() & 0x7F) < 0x4C;
    if (!succeeds) {
      seed = candidate;
      after_roll = VS::Core::BiosRand().State();
      break;
    }
  }
  REQUIRE(seed != 0);
  VS::Core::SeedBiosRand(seed);

  VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 0);

  CHECK(fixture.Reaction()->alliance_reaction_count == 1);
  CHECK(fixture.Reaction()->temporary_ashley_alliance == 0);
  CHECK(settings->decision_callback == AshleyAllianceTestCallback);
  CHECK(settings->saved_decision_callback == nullptr);
  CHECK(VS::Battle::actorMarkers[1] == 0x05);
  CHECK(VS::Core::BiosRand().State() == after_roll);
}

TEST_CASE(
    "AI Ashley alliance forced entry skips BIOS RNG and handles a null saved "
    "callback",
    "[ai][ashley-alliance]") {
  AshleyAllianceFixture fixture;
  fixture.Reaction()->alliance_reaction_count = 9;
  VS::Battle::actorMarkers[1] = 0x05;

  VS::Battle::Ai::EndLiveAshleyAlliance(fixture.Reaction());
  fixture.CheckRestored(nullptr);
  CHECK(VS::Battle::actorMarkers[1] == 0x05);

  fixture.state.settings_bit25 = 1;
  const unsigned random_before = VS::Core::BiosRand().State();
  VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 1);
  CHECK(VS::Core::BiosRand().State() == random_before);
  fixture.CheckEntered(0, nullptr);

  VS::Battle::Ai::EndLiveAshleyAlliance(fixture.Reaction());
  fixture.CheckRestored(VS::Battle::Ai::UpdateLiveEngagementApproachBehavior);
  CHECK(VS::Battle::actorMarkers[1] == 0x05);
}

TEST_CASE("AI Ashley alliance compares before truncating the reaction count",
          "[ai][ashley-alliance][alliance-wrap]") {
  AshleyAllianceFixture fixture;
  const auto seeds = AshleyAllianceRollSeeds();
  VS::Core::SeedBiosRand(seeds[0]);
  fixture.Reaction()->alliance_reaction_count = 255;
  fixture.Settings()->decision_callback = AshleyAllianceTestCallback;
  VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 0);
  CHECK(fixture.Reaction()->alliance_reaction_count == 0);
  CHECK(fixture.Reaction()->temporary_ashley_alliance == 1);
  CHECK(fixture.Settings()->saved_decision_callback ==
        AshleyAllianceTestCallback);
  CHECK(fixture.Settings()->decision_callback ==
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
      settings->decision_callback = AshleyAllianceTestCallback;
      fixture.Reaction()->alliance_reaction_count =
          static_cast<u_char>(counter);
      fixture.state.actor_id = 1;
      VS::Battle::actorMarkers[1] = 0x05;
      VS::Core::SeedBiosRand(seeds[roll]);
      VS::Core::Rand15 expected_random(seeds[roll]);
      REQUIRE((expected_random.Next() & 0x7F) == static_cast<int>(roll));
      VS::Battle::Ai::TryBeginLiveAshleyAlliance(fixture.Reaction(), 0);
      const bool favorable = roll < 0x4C;
      const bool entered = favorable && counter + 1 >= 2;
      CHECK(fixture.Reaction()->alliance_reaction_count ==
            static_cast<u_char>(counter + (favorable ? 1 : 0)));
      CHECK(fixture.Reaction()->temporary_ashley_alliance == entered);
      CHECK(settings->decision_callback ==
            (entered ? VS::Battle::Ai::UpdateLiveEngagementApproachBehavior
                     : AshleyAllianceTestCallback));
      CHECK(settings->saved_decision_callback ==
            (entered ? AshleyAllianceTestCallback : nullptr));
      CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[1]) ==
            (entered ? 0x85 : 0x05));
      CHECK(VS::Core::BiosRand().State() == expected_random.State());
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
    VS::Core::SeedBiosRand(seeds[0]);
    fixture.Apply(1u << 14);
  }
  REQUIRE(fixture.Reaction()->temporary_ashley_alliance == 1);
  CHECK(fixture.Settings()->saved_decision_callback ==
        AshleyAllianceTestCallback);
  CHECK(fixture.state.relations[0].timer == 28);
  fixture.Apply(1u << 15);
  CHECK(fixture.Reaction()->temporary_ashley_alliance == 0);
  CHECK(fixture.Reaction()->alliance_reaction_count == 0);
  CHECK(fixture.Settings()->decision_callback == AshleyAllianceTestCallback);
  CHECK(fixture.state.relations[0].classification == 3);
  CHECK(fixture.state.awareness_marker == 6);
}

TEST_CASE("AI saved relations force Ashley alliance without consuming BIOS RNG",
          "[ai][ashley-alliance][alliance-persistence]") {
  AshleyActionFixture fixture;
  const auto random = VS::Core::BiosRand().State();
  for (int cycle = 0; cycle < 2; ++cycle) {
    fixture.state.saved_relation_options = 0x1235;
    VS::Battle::Ai::ApplyLiveSavedActorAiRelations();
    REQUIRE(fixture.Reaction()->temporary_ashley_alliance == 1);
    CHECK(fixture.Reaction()->alliance_reaction_count == 0);
    CHECK(fixture.Settings()->saved_decision_callback ==
          AshleyAllianceTestCallback);
    CHECK(fixture.state.relations[0].classification == ((0x1235 >> 1) & 3));
    CHECK(fixture.state.relations[0].classification_timer ==
          ((0x1235 >> 6) & 127));
    CHECK(VS::Core::BiosRand().State() == random);
    VS::Battle::Ai::EndLiveAshleyAlliance(fixture.Reaction());
    CHECK(fixture.Settings()->decision_callback == AshleyAllianceTestCallback);
  }
}

TEST_CASE("Inactive Ashley alliance exit only resets the favorable counter",
          "[ai][ashley-alliance][alliance-inactive]") {
  AshleyAllianceFixture fixture;
  fixture.Settings()->decision_callback = AshleyAllianceTestCallback;
  fixture.Settings()->saved_decision_callback =
      VS::Battle::Ai::UpdateLiveEngagementApproachBehavior;
  VS::Battle::actorMarkers[1] = static_cast<char>(0xA5);
  const auto random = VS::Core::BiosRand().State();
  const auto frame_random = VS::Battle::Ai::LiveFrameContext().randomState;
  for (unsigned count = 0; count < 256; ++count) {
    fixture.Reaction()->alliance_reaction_count = static_cast<u_char>(count);
    auto expected = fixture.state;
    reinterpret_cast<VS::Battle::Ai::ReactionView*>(&expected)
        ->alliance_reaction_count = 0;
    VS::Battle::Ai::EndLiveAshleyAlliance(fixture.Reaction());
    CHECK(StateBytes(fixture.state) == StateBytes(expected));
    CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[1]) == 0xA5);
    CHECK(VS::Core::BiosRand().State() == random);
    CHECK(VS::Battle::Ai::LiveFrameContext().randomState == frame_random);
  }
}

TEST_CASE("Ashley alliance recovery preserves callback and native busy gates",
          "[ai][ashley-alliance][alliance-recovery]") {
  const auto settings_bit = GENERATE(0, 1);
  const auto busy = GENERATE(0, 1, 2);
  const bool saved_callback = GENERATE(false, true);
  AshleyAllianceFixture fixture;
  auto* settings = fixture.Settings();
  settings->decision_callback =
      VS::Battle::Ai::UpdateLiveEngagementApproachBehavior;
  settings->saved_decision_callback =
      saved_callback ? AshleyAllianceTestCallback : nullptr;
  fixture.state.settings_bit25 = static_cast<u_char>(settings_bit);
  fixture.state.decision_delay = busy == 1 ? 3 : 0;
  fixture.state.native_pointer_18c = busy == 2 ? &fixture : nullptr;
  fixture.state.awareness_marker = 0x71;
  fixture.state.awareness_frames_remaining = 0x72;
  fixture.Reaction()->temporary_ashley_alliance = 1;
  fixture.Reaction()->alliance_reaction_count = 9;
  VS::Battle::actorMarkers[1] = static_cast<char>(0xA5);
  fixture.actors[1].commandFlags = 0x5A;
  const auto random = VS::Core::BiosRand().State();
  auto& frame = VS::Battle::Ai::LiveFrameContext();
  frame.randomState = 0x2000;
  frame.lastBudgetTick = 2;
  VS::Battle::Ai::EndLiveAshleyAlliance(fixture.Reaction());
  fixture.CheckRestored(
      saved_callback ? AshleyAllianceTestCallback
                     : VS::Battle::Ai::UpdateLiveEngagementApproachBehavior);
  CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[1]) == 0x25);
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
  std::array<VS::Battle::Ai::ActorAiState, kIds.size()> states{};
  for (std::size_t index = 0; index < kIds.size(); ++index) {
    std::memset(&states[index], 0xA5, sizeof(states[index]));
    states[index].linked_actor = 16;
    actors[index].id = kIds[index];
    actors[index].next = index + 1 < kIds.size() ? &actors[index + 1] : nullptr;
    VS::Battle::Ai::actorAiStates[kIds[index]] = &states[index];
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
  states[3].linked_actor = 7;
  auto expected = states;
  expected[1].linked_actor = 5;
  expected[2].linked_actor = 2;
  Battle_actors[0] = actors.data();
  VS::Battle::Ai::ResolveLiveAiLinkedActors();
  for (std::size_t index = 0; index < kIds.size(); ++index) {
    CHECK(StateBytes(states[index]) == StateBytes(expected[index]));
  }
  CHECK(states[0].linked_actor == 16);
  CHECK(states[1].linked_actor == 5);
  CHECK(states[2].linked_actor == 2);
  CHECK(states[3].linked_actor == 7);
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
  auto* view = reinterpret_cast<VS::Battle::Ai::AiActorView*>(&actor);
  REQUIRE((view->saved_ai_settings_flags & 1u) != 0);
  int packed = view->ai_settings;
  VS::Battle::Ai::ApplyLiveSavedAiSettingsFlags(
      &packed, reinterpret_cast<const VS::Battle::Ai::SavedSettingsPair*>(
                   &view->saved_ai_settings));
  CHECK(std::bit_cast<std::uint32_t>(packed) == 0xACF45678u);
  CHECK(bytes(actor) == before);
}

namespace {
// Ashley heads the list with the state under test; actors 2 and 5 follow in
// linked groups so the trailing link resolution has work to do.
struct ActorSettingsFixture {
  AiLifecycleFixture table;
  Battle_Actor* saved_head = Battle_actors[0];
  void* saved_has_waypoints = VS::Battle::Ai::roomHasWaypoints;
  std::uint16_t* saved_waypoints = VS::Battle::Ai::roomWaypoints;
  void* saved_section6 = Field_roomData.section6;
  u_int saved_section6_length = Field_roomData.header.section6Length;
  std::array<Battle_Actor, 3> actors{};
  std::array<VS::Battle::Ai::ActorAiState, 3> states{};
  alignas(std::uint32_t) std::array<unsigned char, 0x450> auxiliary{};

  ActorSettingsFixture() {
    constexpr std::array<int, 3> kIds{0, 2, 5};
    for (std::size_t index = 0; index < kIds.size(); ++index) {
      actors[index].id = kIds[index];
      actors[index].next =
          index + 1 < actors.size() ? &actors[index + 1] : nullptr;
      VS::Battle::Ai::actorAiStates[kIds[index]] = &states[index];
    }
    actors[1].aiGroup = 3;
    actors[1].aiLinkedGroup = 4;
    actors[2].aiGroup = 4;
    actors[2].aiLinkedGroup = 3;
    states[1].linked_actor = 16;
    states[2].linked_actor = 16;
    Battle_actors[0] = actors.data();
    VS::Battle::Ai::roomHasWaypoints = nullptr;
    Field_roomData.section6 = auxiliary.data();
    Field_roomData.header.section6Length = auxiliary.size();
  }

  ~ActorSettingsFixture() {
    Battle_actors[0] = saved_head;
    VS::Battle::Ai::roomHasWaypoints = saved_has_waypoints;
    VS::Battle::Ai::roomWaypoints = saved_waypoints;
    Field_roomData.section6 = saved_section6;
    Field_roomData.header.section6Length = saved_section6_length;
  }

  // Waypoint 5 of the room's auxiliary section, active or not.
  void UseRoomWaypoint(std::uint16_t waypoint) {
    std::memcpy(auxiliary.data() + 0x440 + (5 * sizeof(waypoint)), &waypoint,
                sizeof(waypoint));
    VS::Battle::Ai::roomHasWaypoints = auxiliary.data();
    VS::Battle::Ai::roomWaypoints =
        reinterpret_cast<std::uint16_t*>(auxiliary.data() + 0x440);
  }

  // Ashley's spawn words and a patterned state viewed through the settings
  // view, with the given behaviour index and movement flags.
  VS::Battle::Ai::SettingsView& PrepareLeader(unsigned behaviour,
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
    VS::Battle::Ai::ActorAiState& state = states[0];
    std::memset(&state, 0xA5, sizeof(state));
    state.actor_id = 0;
    state.linked_actor = 7;
    auto& view = reinterpret_cast<VS::Battle::Ai::SettingsView&>(state);
    view.actor = reinterpret_cast<VS::Battle::Ai::SettingsActorView*>(&actor);
    view.collision_radius = 129;
    view.movement_flags = movement_flags;
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
void ExpectLiveSettings(VS::Battle::Ai::ActorAiState& expected, int waypoints,
                        std::uint16_t movement_flags, unsigned behaviour) {
  auto& want = reinterpret_cast<VS::Battle::Ai::SettingsView&>(expected);
  const unsigned far = VS::Battle::Ai::kPerceptionRangesSquared[6];
  want.far_distance_squared = far;
  want.near_distance_squared = far > 0x23FFFF ? 0x240000 : far;
  want.angle = VS::Battle::Ai::viewAngles[1][0];
  want.view_cosine = VS::Core::Math::Cosine((want.angle / 2) * 4096 / 360);
  want.settings_bit15 = 0;
  want.settings_bit14 = 1;
  want.settings_bit13 = 0;
  want.opaque_2d = waypoints == 2 ? 1 : 0;
  want.opaque_b2 = 3;
  want.opaque_b0 = VS::Battle::Ai::settingsBits8Values[3][0];
  want.facing_delay = static_cast<std::uint16_t>(
      (32 * VS::Battle::Ai::facingDelayScales[2]) >> 4);
  want.settings_constant23 = 1;
  want.settings_byte3_bit0 = 0;
  want.settings_bit23 = 1;
  want.settings_bits21 = 2;
  want.settings_bits28 = 1;
  want.settings_bits30 = 3;
  want.settings_bit26 = 0;
  want.move_period = VS::Battle::Ai::kWanderTimingPresets[5].move_period;
  want.rest_period = VS::Battle::Ai::kWanderTimingPresets[5].rest_period;
  want.wander_rate = VS::Battle::Ai::kWanderTimingPresets[5].rate;
  want.settings_bit27 = 1;
  want.settings_bit25 = 1;
  if ((movement_flags & 0xC0) == 0) {
    want.settings_bits30_value = VS::Battle::Ai::kSettingsBits30Values[3];
  }
  want.settings_byte0_bit7 = 1;
  want.allow_fallback_action = 1;
  want.settings_mode_low = 1;
  want.opaque102 = VS::Battle::Ai::kFlags34Bit14Values[1];
  want.opaque101 = 2;
  want.opaque_c8 = 0x2A;
  want.opaque_cc = 0x15;
  want.opaque_ce = 0x1B;
  want.decision_callback =
      behaviour < 9 ? VS::Battle::Ai::decisionCallbacks[behaviour] : nullptr;
  want.linked_actor = 16;
  // Ashley's action metadata only sets her action cost threshold.
  expected.action_cost_enabled = 1;
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
  VS::Battle::Ai::SettingsView& view =
      fixture.PrepareLeader(behaviour, movement_flags);
  const VS::Battle::Ai::ActorAiState& state = fixture.states[0];
  VS::Battle::Ai::ActorAiState expected;
  std::memcpy(&expected, &state, sizeof(state));
  ExpectLiveSettings(expected, waypoints, movement_flags, behaviour);
  const auto& want =
      reinterpret_cast<const VS::Battle::Ai::SettingsView&>(expected);
  VS::Battle::Ai::SettingsWord word{};
  word.word = ActorSettingsFixture::kLeaderSettings;

  VS::Battle::Ai::ApplyLiveActorAiSettings(&view, &word);
  CHECK(state.linked_actor == 16);
  CHECK(view.decision_callback == want.decision_callback);
  CHECK(view.settings_bits30_value == want.settings_bits30_value);
  CHECK(view.opaque_2d == want.opaque_2d);
  CHECK(StateBytes(state) == StateBytes(expected));
  CHECK(fixture.states[1].linked_actor == 5);
  CHECK(fixture.states[2].linked_actor == 2);
}

TEST_CASE("AI settings retain their original module address",
          "[ai][actor-settings][settings-live]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x800E5A9C) ==
      reinterpret_cast<const void*>(&VS::Battle::Ai::ApplyLiveActorAiSettings));
}
