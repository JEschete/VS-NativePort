#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/ActionPreview.hpp"
#include "Battle/ActionPreviewLive.hpp"
#include "Battle/HitModifiers.hpp"
#include "Battle/HitModifiersLive.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/HitChance.h"
}

namespace {
namespace b = VS::Battle;
struct Actor final : b::ResolutionActor {
  std::uint8_t slot{}, limb = 2, damage = 3;
  bool cancels{}, blade{};
  std::array<std::int16_t, 7> affinities{};
  std::uint8_t Slot() override { return slot; }
  std::uint8_t DefaultLimb() override { return limb; }
  bool Cancels() override { return cancels; }
  bool HasBlade() override { return blade; }
  std::uint8_t DamageType() override { return damage; }
  std::array<std::int16_t, 7> Affinities() override { return affinities; }
};
struct Definition final : b::ResolutionDefinition {
  std::array<b::ResolutionDescriptor, 2> hits{};
  std::uint8_t category{};
  bool redirects{};
  b::PreviewVolume range{7, 0, 0, {255, 128, 64}, 0, {}};
  b::PreviewVolume area{255, 0, 0, {7, 8, 9}, 255, {}};
  b::ResolutionDescriptor Hit(int index) override {
    return hits.at(static_cast<std::size_t>(index));
  }
  bool Redirects() override { return redirects; }
  std::uint8_t Category() override { return category; }
  b::PreviewVolume Range() override { return range; }
  b::PreviewVolume Area() override { return area; }
};
struct World : b::ActionPreviewWorld {
  ::Definition definition;
  std::array<::Actor, 3> actors{};
  b::ActionRequest result{}, source{}, captured{};
  std::vector<int> events;
  std::vector<int> slots;
  std::function<void(int)> callback;
  std::optional<b::ActionPoint> origin = b::ActionPoint{1, 2, 3, 4};
  std::optional<b::ActionPoint> from = b::ActionPoint{0, 0, 0, 23};
  std::optional<b::ActionPoint> to = b::ActionPoint{64, 64, 0, 29};
  std::optional<b::ActionPoint> impact;
  std::optional<std::uint32_t> queue = 1;
  b::PreviewVolume volume{};
  b::ResolutionActor* paid{};
  std::uint32_t drawn{}, previous = 123, prerequisite = 456;
  std::uint32_t check = 1, roll = 1, cost = 0xAB123456;
  std::uint8_t spawn{};
  bool capture{}, swap{}, no_actor{}, null_stats{};
  int position_reads{}, contains{}, recovery{};
  World() {
    source.count = 1;
    source.targets[0].actor = 1;
    source.targets[0].limb = 3;
    for (std::size_t i = 0; i < actors.size(); ++i) {
      actors.at(i).slot = static_cast<std::uint8_t>(i);
    }
  }
  void Event(int id) {
    events.push_back(id);
    if (callback) callback(id);
  }
  b::ResolutionDefinition* Definition(std::uint32_t action) override {
    return action < 256 ? &definition : nullptr;
  }
  std::optional<b::ResolutionActor*> Actor(int slot) override {
    slots.push_back(slot);
    if (no_actor || slot < 0 || slot >= 3) return std::nullopt;
    if (null_stats) return nullptr;
    return &actors.at(swap ? 2 : static_cast<std::size_t>(slot));
  }
  std::optional<std::uint32_t> WeaponDrawn(int slot) override {
    slots.push_back(slot);
    return slot < 3 ? std::optional{drawn} : std::nullopt;
  }
  std::optional<std::uint8_t> SpawnId(int slot) override {
    slots.push_back(slot);
    return slot < 3 ? std::optional{spawn} : std::nullopt;
  }
  std::optional<b::ActionPoint> SourceOrigin(int) override {
    Event(1);
    return origin;
  }
  std::optional<b::ActionPoint> TargetPosition(int, int) override { return to; }
  std::optional<b::ActionPoint> LimbPosition(int slot, int limb) override {
    slots.push_back(slot);
    Event(20 + limb);
    return limb % 2 == 0 ? std::optional{b::ActionPoint{
                               static_cast<std::int16_t>(limb), 0, 0, 0}}
                         : std::nullopt;
  }
  std::optional<b::ActionPoint> PreviewPosition(bool target) override {
    ++position_reads;
    Event(target ? 31 : 30);
    return target ? to : from;
  }
  bool Contains(const b::PreviewVolume& value, b::ActionPoint point) override {
    volume = value;
    ++contains;
    return point.x != 2;
  }
  std::optional<b::ActionPoint> Trace(b::ActionPoint, b::ActionPoint,
                                      int) override {
    return impact;
  }
  void ResetHitState() override { previous = prerequisite = 0; }
  void Prerequisite(std::uint32_t value) override { prerequisite = value; }
  void PreviousHit(std::uint32_t value) override { previous = value; }
  std::optional<std::uint32_t> QueueIndex() override { return queue; }
  std::uint32_t Check(b::ResolutionDefinition&, b::ActionHit&,
                      int hit) override {
    Event(40 + hit);
    return check;
  }
  std::uint32_t Roll(b::ResolutionDefinition&, b::ActionHit&, b::ActionHit&,
                     int hit, int) override {
    Event(50 + hit);
    return roll;
  }
  void Effect(b::ResolutionDefinition&, b::ActionHit&, b::ActionHit& target,
              int, int hit) override {
    target.hp = 10;
    target.effects |= 1;
    Event(60 + hit);
  }
  void Injuries(b::ActionHit&) override { Event(70); }
  bool Recovery(std::uint32_t, b::ResolutionActor& actor) override {
    paid = &actor;
    ++recovery;
    Event(80);
    return true;
  }
  std::optional<std::uint32_t> Cost(std::uint32_t, b::ResolutionActor* actor,
                                    int update) override {
    paid = actor;
    Event(90 + update);
    return cost;
  }
  bool Resolve(std::uint32_t action, b::ActionRequest& output,
               b::ActionRequest& input, int execute) override {
    captured = input;
    if (capture) {
      Event(100);
      return true;
    }
    return ActionPreviewWorld::Resolve(action, output, input, execute);
  }
  bool Run(int execute = 0) {
    return b::ResolveActionTargets(23, result, source, execute, *this);
  }
};
}  // namespace

TEST_CASE("Request reset clears the complete expanded native record",
          "[action-preview][F00508]") {
  std::array<std::uint8_t, sizeof(b::ActionRequest)> poison;
  poison.fill(0xA5);
  auto request = std::bit_cast<b::ActionRequest>(poison);
  b::ClearActionRequest(request);
  CHECK(request == b::ActionRequest{});
  auto live = std::bit_cast<Battle_ActionRequest>(poison);
  b::ClearLiveActionRequest(&live);
  CHECK(std::bit_cast<b::ActionRequest>(live) == request);
}
TEST_CASE("Hit initialization clears all effects and sets neutral impact",
          "[action-preview][F00509]") {
  std::array<std::uint8_t, sizeof(b::ActionHit)> poison;
  poison.fill(0xFF);
  auto hit = std::bit_cast<b::ActionHit>(poison);
  b::InitializeActionHit(hit);
  b::ActionHit expected{};
  expected.impact = 6;
  CHECK(hit == expected);
  auto live = std::bit_cast<Battle_HitResult>(poison);
  b::InitializeLiveActionHit(&live);
  CHECK(std::bit_cast<b::ActionHit>(live) == expected);
}
TEST_CASE("Hit finalization scans nine lanes in order without upper fallback",
          "[action-preview][F00513]") {
  const auto lane = GENERATE(0, 1, 8);
  const auto flag = GENERATE(1U, 2U, 3U);
  b::ActionHit hit{};
  hit.impact = 6;
  hit.threshold = 255;
  hit.effects = flag << (lane * 2) | 3U << 18;
  b::FinalizeActionHit(hit);
  CHECK(hit.impact == (flag == 2 ? 5 : flag == 1 ? 0 : 1));
  CHECK(hit.threshold == 0);
}
TEST_CASE("Finalization leaves positions and prior impacts and statuses alone",
          "[action-preview][F00513]") {
  b::ActionHit hit{};
  hit.impact = 6;
  hit.effects = 1U << 31;
  b::FinalizeActionHit(hit);
  CHECK(hit.impact == 6);
  hit.effects = 0;
  hit.remove_statuses = 1;
  b::FinalizeActionHit(hit);
  CHECK(hit.impact == 6);
  hit.remove_statuses = 0;
  b::FinalizeActionHit(hit);
  CHECK(hit.impact == 4);
  hit.threshold = 255;
  hit.position_kind = -1;
  b::FinalizeActionHit(hit);
  CHECK(hit.threshold == 255);
}
TEST_CASE("Resolution initializes every target and preserves native capacity",
          "[action-preview][F00514]") {
  World w;
  w.source.count = 255;
  w.source.focus = {4, 5, 6, 7};
  w.source.targets.back().actor = 2;
  w.source.targets[1].position_kind = 5;
  w.source.targets[1].SetPosition({-1, -2, -3, -4});
  w.source.targets[2].position_kind = 6;
  w.source.targets[2].hp = 99;
  w.queue = 0;
  REQUIRE(w.Run(1));
  CHECK(w.result == w.source);
  CHECK(w.result.targets.back().actor == 2);
  CHECK(w.result.targets[1].Position() == b::ActionPoint{-1, -2, -3, -4});
  CHECK(w.result.targets[2].hp == 0);
  CHECK(w.result.focus == b::ActionPoint{4, 5, 6, 7});
}
TEST_CASE("Resolution keeps clear before source reads for same request",
          "[action-preview][F00514]") {
  World w;
  w.source.count = 255;
  REQUIRE(b::ResolveActionTargets(23, w.source, w.source, 0, w));
  CHECK(w.source.count == 0);
  CHECK(w.source.primary.actor == 0);
  CHECK(w.source.primary.impact == 4);
}
TEST_CASE("Resolution caches source owner but refreshes counts and descriptors",
          "[action-preview][F00514]") {
  World w;
  w.definition.hits[0].prerequisite = 1;
  w.queue = 0;
  w.callback = [&](int event) {
    if (event == 1) w.source.count = 2;
    if (event == 60) {
      w.result.count = 1;
      w.definition.hits[1].prerequisite = 3;
      w.swap = true;
    }
  };
  REQUIRE(w.Run(1));
  CHECK(w.result.count == 1);
  CHECK(w.paid == &w.actors[0]);
  CHECK(w.source == w.result);
  CHECK(w.events == std::vector<int>{1, 40, 50, 60, 70, 51, 61, 70, 80, 91});
}
TEST_CASE("Cancellation reads cached stats and fresh target identity",
          "[action-preview][F00514]") {
  World w;
  w.definition.hits[0].prerequisite = 1;
  w.spawn = 128;
  w.actors[1].cancels = true;
  w.callback = [&](int event) {
    if (event == 40) w.swap = true;
    if (event == 60) {
      w.result.targets[0].actor = 2;
      w.result.targets[0].limb = 5;
    }
  };
  REQUIRE(w.Run());
  CHECK(w.result.targets[0].actor == 2);
  CHECK(w.result.targets[0].limb == 5);
  CHECK(w.result.targets[0].hp == 0);
  CHECK(w.result.targets[0].impact == 4);
}
TEST_CASE("Self cancellation erases identity while direct effects ignore count",
          "[action-preview][F00514]") {
  World w;
  w.source.primary.actor = 1;
  w.source.primary.limb = 5;
  w.source.count = 0;
  w.actors[1].cancels = true;
  w.definition.hits[0].prerequisite = 3;
  w.definition.hits[1].prerequisite = 6;
  REQUIRE(w.Run(1));
  CHECK(w.result.primary.actor == 0);
  CHECK(w.result.primary.limb == 0);
  CHECK(w.result.primary.impact == 4);
  CHECK(w.result.targets[0].hp == 10);
}
TEST_CASE("Prerequisite failure and preview misses retain the original order",
          "[action-preview][F00514]") {
  World w;
  w.definition.hits[0].prerequisite = 1;
  w.definition.hits[1].prerequisite = 1;
  w.check = 0;
  REQUIRE(w.Run());
  CHECK(w.events == std::vector<int>{1, 40, 41});
  CHECK(w.result.targets[0].threshold == 0);
  w.check = 1;
  w.roll = 0;
  w.events.clear();
  REQUIRE(w.Run());
  CHECK(w.result.targets[0].hp == 10);
  CHECK(w.previous == 0);
}
TEST_CASE(
    "Damage selection uses owner slot first positive maximum and zero type",
    "[action-preview][F00514]") {
  World w;
  w.actors[0].slot = 2;
  w.actors[0].blade = true;
  w.actors[0].damage = 0;
  w.actors[0].affinities = {-1, 0, 9, 9, 4, -2, 8};
  w.drawn = 1;
  w.definition.hits[0].stat = 25;
  REQUIRE(w.Run());
  CHECK(w.result.affinity == 2);
  CHECK(w.result.damage_type == 0);
  CHECK(w.slots == std::vector<int>{0, 2});
  w.definition.hits[0].affinity = 7;
  REQUIRE(w.Run());
  CHECK(w.result.affinity == 6);
}
TEST_CASE("World traps and unavailable unused origin remain supported",
          "[action-preview][F00514]") {
  World w;
  w.origin.reset();
  REQUIRE(w.Run());
  w.source.primary.position_kind = 3;
  w.source.primary.hp = 99;
  w.no_actor = true;
  REQUIRE(w.Run(1));
  CHECK(w.result.primary.hp == 0);
  CHECK(w.recovery == 0);
  w.definition.hits[0].stat = 2;
  CHECK_FALSE(w.Run());
  w.definition.hits[0].stat = 0;
  w.definition.hits[0].prerequisite = 1;
  w.definition.redirects = true;
  CHECK_FALSE(w.Run(1));
}
TEST_CASE("Redirect terrain skips effects and actor tags restore normal flow",
          "[action-preview][F00514]") {
  World w;
  w.definition.hits[0].prerequisite = 1;
  w.definition.redirects = true;
  const auto tag = GENERATE(0, 2, 3, 17, -1);
  w.impact = b::ActionPoint{11, 22, 33, static_cast<std::int16_t>(tag)};
  REQUIRE(w.Run(1));
  const auto& target = w.result.targets[0];
  CHECK(target.position_kind == (tag == 0 ? 1 : 0));
  CHECK(target.mp == (tag == 0 ? 128 : 0));
  CHECK(target.actor == (tag == 0 ? 11 : tag == 3 ? 2 : 1));
  CHECK(target.limb == (tag == 0 ? 0 : tag == 3 ? 2 : 3));
}
TEST_CASE("Resolver refuses used out of range requests without relaxing checks",
          "[action-preview][F00514]") {
  World w;
  w.source.count = 256;
  CHECK_FALSE(w.Run());
  w.source.count = 0;
  w.source.primary.actor = 255;
  CHECK_FALSE(w.Run());
  w.source.primary.actor = 0;
  w.queue.reset();
  CHECK_FALSE(w.Run());
}
TEST_CASE("Limb preview creates a complete fresh source and narrows inputs",
          "[action-preview][F00515]") {
  World w;
  w.capture = true;
  REQUIRE(b::PreviewActionAgainstLimb(23, 0, 257, -1, w.result, w) ==
          0xAB000000U);
  CHECK(w.captured.primary.limb == 0);
  CHECK(w.captured.focus == b::ActionPoint{});
  CHECK(w.captured.targets[0].actor == 1);
  CHECK(w.captured.targets[0].limb == 255);
  CHECK(w.captured.count == 1);
  CHECK(w.captured.targets.back() == b::ActionHit{});
}
TEST_CASE("Both preview costs reacquire the full source slot after resolution",
          "[action-preview][F00515][F00516]") {
  World w;
  w.capture = true;
  w.callback = [&](int event) {
    if (event == 100) w.swap = true;
  };
  const bool actor = GENERATE(false, true);
  const auto preview =
      actor ? b::PreviewActionAgainstActor : b::PreviewActionAgainstLimb;
  REQUIRE(preview(23, 1, 1, 3, w.result, w).has_value());
  CHECK(w.paid == &w.actors[2]);
  CHECK_FALSE(preview(23, 257, 1, 3, w.result, w));
  CHECK(w.captured.primary.actor == 1);
}
TEST_CASE("Missing unused stats are distinct from a missing actor record",
          "[action-preview][F00514][F00515][F00516]") {
  World w;
  w.null_stats = true;
  REQUIRE(w.Run());
  CHECK(b::PreviewActionAgainstLimb(23, 0, 1, 0, w.result, w));
  CHECK(b::PreviewActionAgainstActor(23, 0, -1, -1, w.result, w));
  CHECK(w.paid == nullptr);
  w.definition.hits[0].stat = 2;
  CHECK_FALSE(w.Run());
}
TEST_CASE("Actor preview categories gate unused positions target and limb",
          "[action-preview][F00516]") {
  World w;
  w.capture = true;
  w.from.reset();
  w.to.reset();
  const auto category = GENERATE(0, 9, 10, 13, 14, 15);
  w.definition.category = static_cast<std::uint8_t>(category);
  REQUIRE(b::PreviewActionAgainstActor(23, 0, -1, -1, w.result, w));
  CHECK(w.captured.count == 0);
  CHECK(w.position_reads == 0);
  CHECK(w.captured.primary.limb == 0);
  CHECK(w.captured.focus == b::ActionPoint{});
}
TEST_CASE(
    "Actor preview explicit limb narrows but default lookup uses full slot",
    "[action-preview][F00516]") {
  World w;
  w.capture = true;
  w.definition.category = 5;
  REQUIRE(b::PreviewActionAgainstActor(23, 0, 257, 259, w.result, w));
  CHECK(w.captured.targets[0].actor == 1);
  CHECK(w.captured.targets[0].limb == 3);
  w.definition.category = 1;
  CHECK_FALSE(b::PreviewActionAgainstActor(23, 0, 257, -1, w.result, w));
  REQUIRE(b::PreviewActionAgainstActor(23, 0, 1, -1, w.result, w));
  CHECK(w.captured.targets[0].limb == 2);
}
TEST_CASE("Area categories read only target position even when volume unused",
          "[action-preview][F00516]") {
  World w;
  w.capture = true;
  w.from.reset();
  const auto category = GENERATE(2, 4, 6, 8);
  w.definition.category = static_cast<std::uint8_t>(category);
  REQUIRE(b::PreviewActionAgainstActor(23, 0, 1, -1, w.result, w));
  CHECK(w.position_reads == 1);
  CHECK(w.captured.count == (category < 6 ? 1 : 2));
  w.to.reset();
  CHECK_FALSE(b::PreviewActionAgainstActor(23, 0, 1, -1, w.result, w));
}
TEST_CASE("Directional volumes use exact angles and skip unavailable limbs",
          "[action-preview][F00516]") {
  World w;
  w.capture = true;
  w.definition.category = 11;
  REQUIRE(b::PreviewActionAgainstActor(23, 0, 1, -1, w.result, w));
  CHECK(w.volume.yaw == 1024);
  CHECK(w.volume.pitch == 95);
  CHECK(w.volume.origin == w.from);
  CHECK(w.contains == 3);
  CHECK(w.captured.count == 2);
  CHECK(w.captured.targets[0].limb == 0);
  CHECK(w.captured.targets[1].limb == 4);
}
TEST_CASE("Preview category is reread after building the cached volume",
          "[action-preview][F00516]") {
  World w;
  w.capture = true;
  w.definition.category = 2;
  w.callback = [&](int event) {
    if (event == 31) w.definition.category = 6;
  };
  REQUIRE(b::PreviewActionAgainstActor(23, 0, 1, -1, w.result, w));
  CHECK(w.captured.count == 2);
  CHECK(w.volume.shape == 7);
  CHECK(w.volume.pitch == 248);
}

namespace {
struct PreviewFixture {
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Battle_actionDefinitions)) saved_definitions =
      std::to_array(Battle_actionDefinitions);
  decltype(std::to_array(Battle_hitFunctions)) saved_hits =
      std::to_array(Battle_hitFunctions);
  decltype(std::to_array(Battle_statCalculators)) saved_stats =
      std::to_array(Battle_statCalculators);
  decltype(std::to_array(Battle_actionEffectMediators)) saved_effects =
      std::to_array(Battle_actionEffectMediators);
  Battle_ActionState* saved_state = Battle_actionState;
  std::uint32_t saved_last = Battle_lastActionStatValue;
  std::uint32_t saved_prerequisite = Battle_hitPrerequisiteResult;
  std::uint32_t saved_previous = Battle_previousHitResult;
  Battle_ActionState state{};
  std::array<Battle_Actor, 2> actors{};
  std::array<Battle_ActorStats, 2> stats{};
  PreviewFixture() {
    std::fill(std::begin(Battle_actors), std::end(Battle_actors), nullptr);
    Battle_actionState = &state;
    state.chainStep = 1;
    for (std::size_t i = 0; i < actors.size(); ++i) {
      Battle_actors[i] = &actors.at(i);
      actors.at(i).stats = &stats.at(i);
      stats.at(i).actorId = static_cast<std::uint8_t>(i);
      stats.at(i).maxHp = stats.at(i).currentHp = 100;
      stats.at(i).maxMp = stats.at(i).currentMp = 100;
      for (auto& limb : stats.at(i).limbs) limb.hp = 100;
    }
  }
  ~PreviewFixture() {
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_definitions.begin(), saved_definitions.end(),
              Battle_actionDefinitions);
    std::copy(saved_hits.begin(), saved_hits.end(), Battle_hitFunctions);
    std::copy(saved_stats.begin(), saved_stats.end(), Battle_statCalculators);
    std::copy(saved_effects.begin(), saved_effects.end(),
              Battle_actionEffectMediators);
    Battle_actionState = saved_state;
    Battle_lastActionStatValue = saved_last;
    Battle_hitPrerequisiteResult = saved_prerequisite;
    Battle_previousHitResult = saved_previous;
  }
  static std::vector<std::uint8_t> Module(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) SKIP("Extracted original modules unavailable");
    return {std::istreambuf_iterator<char>(stream), {}};
  }
  static void Relocate(void** target, std::span<const std::uint8_t> image) {
    const auto tables =
        std::span(Content_modulePointers, Content_modulePointerCount);
    const auto found =
        std::ranges::find(tables, target, &Content_ModulePointerTable::target);
    REQUIRE(found != tables.end());
    REQUIRE(VS::Content::RelocateModulePointerTable(
        *found, image, {Content_moduleSymbols, Content_moduleSymbolCount}));
  }
  void OriginalDefinitions() {
    const auto main = Module("Native\\data\\disc\\SLUS_010.40");
    constexpr auto kActionOffset = 0x3C1DC;
    REQUIRE(main.size() >= kActionOffset + sizeof(Battle_actionDefinitions));
    std::memcpy(Battle_actionDefinitions, main.data() + kActionOffset,
                sizeof(Battle_actionDefinitions));
    const auto battle = Module("Native\\data\\disc\\BATTLE\\BATTLE.PRG");
    Relocate(reinterpret_cast<void**>(Battle_hitFunctions), battle);
    Relocate(reinterpret_cast<void**>(Battle_statCalculators), battle);
    Relocate(reinterpret_cast<void**>(Battle_actionEffectMediators), battle);
  }
  void SelfRoute(int action) {
    stats[0].limbs[0].chainEvasion = 51;
    stats[0].defaultTargetLimb = 4;
    REQUIRE(Battle_actionDefinitions[action].hits[0].prerequisiteFunctionId ==
            3);
    REQUIRE(Battle_hitFunctions[7] == &b::LiveChainHitChance);
  }
};
}  // namespace
TEST_CASE(
    "Original self route previews use limb zero through real chain helper",
    "[action-preview][F00515][F00516][issue-353]") {
  PreviewFixture f;
  f.OriginalDefinitions();
  const int action = GENERATE(23, 25);
  const bool actor = GENERATE(false, true);
  const auto poison = GENERATE(0U, 127U, 255U);
  std::array<std::uint8_t, sizeof(Battle_ActionRequest)> bytes;
  bytes.fill(static_cast<std::uint8_t>(poison));
  auto output = std::bit_cast<Battle_ActionRequest>(bytes);
  f.SelfRoute(action);
  const auto affordable =
      actor ? b::PreviewLiveActionAgainstActor(action, 0, 1, 4, nullptr,
                                               nullptr, &output)
            : b::PreviewLiveActionAgainstLimb(action, 0, 1, 4, &output);
  CHECK(affordable == 0x01000000);
  CHECK(output.primaryTarget.target.targetLimb == 0);
  CHECK(output.primaryTarget.target.hitThreshold == 80);
  CHECK(std::bit_cast<b::ActionPoint>(output.focusPosition) ==
        b::ActionPoint{});
  CHECK(output.targetCount == (actor ? 0 : 1));
  Battle_ActionRequest fresh{};
  if (actor) {
    b::PreviewLiveActionAgainstActor(action, 0, 1, 4, nullptr, nullptr, &fresh);
  } else {
    b::PreviewLiveActionAgainstLimb(action, 0, 1, 4, &fresh);
  }
  CHECK(std::bit_cast<b::ActionRequest>(output) ==
        std::bit_cast<b::ActionRequest>(fresh));
}
TEST_CASE("Actual trap definitions support world sources without origin union",
          "[action-preview][F00514]") {
  PreviewFixture f;
  f.OriginalDefinitions();
  const int action =
      GENERATE(6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21);
  Battle_ActionRequest source{}, result{};
  source.primaryTarget.targetsPosition = 3;
  source.primaryTarget.target.hp = 123;
  source.targetCount = 1;
  source.targets[0].target.targetActor = 1;
  b::ResolveLiveActionTargets(action, &result, &source, 0);
  CHECK(result.primaryTarget.targetsPosition == 3);
  CHECK(result.primaryTarget.target.hp == 0);
  CHECK(result.targetCount == 1);
}
TEST_CASE(
    "Live resolution refreshes callback records and preserves cached stats",
    "[action-preview][F00514]") {
  PreviewFixture f;
  constexpr int kAction = 200;
  Battle_actionDefinitions[kAction] = {};
  auto& definition = Battle_actionDefinitions[kAction];
  definition.hits[0].prerequisiteFunctionId = 1;
  definition.hits[0].hitFunctionId = 7;
  definition.hits[0].effectId = 1;
  Battle_hitFunctions[7] = &b::LiveChainHitChance;
  Battle_actionEffectMediators[1] =
      +[](Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult* target,
          int, int) {
        target->target.targetActor = 0;
        target->target.targetLimb = 5;
        target->target.hp = 10;
        Battle_actors[1]->stats = Battle_actors[0]->stats;
        Battle_actionState->queueIndex = 0;
        Battle_actionState->queue[0].targetCount = 1;
        Battle_actionState->queue[0].targets[254].target.hp = 321;
      };
  f.stats[1].cancelsHitResults = 1;
  f.actors[0].roomSpawnIndex = 128;
  auto& source = f.state.selection;
  auto& result = f.state.queue[0];
  source.targetCount = 2;
  source.targets[0].target.targetActor = 1;
  b::ResolveLiveActionTargets(kAction, &result, &source, 1);
  CHECK(result.targetCount == 1);
  CHECK(result.targets[0].target.targetActor == 0);
  CHECK(result.targets[0].target.targetLimb == 5);
  CHECK(result.targets[0].target.hp == 0);
  CHECK(result.targets[0].target.impactKind == 4);
  CHECK(result.targets[254].target.hp == 321);
  CHECK(std::bit_cast<b::ActionRequest>(source) ==
        std::bit_cast<b::ActionRequest>(result));
}
TEST_CASE("Live previews preserve unused null stats and real no-resource cost",
          "[action-preview][F00515][F00516]") {
  PreviewFixture f;
  Battle_actionDefinitions[0] = {};
  f.actors[0].stats = nullptr;
  Battle_ActionRequest result{};
  CHECK(b::PreviewLiveActionAgainstLimb(0, 0, 1, 0, &result) == 0x01000000);
  CHECK(b::PreviewLiveActionAgainstActor(0, 0, -1, -1, nullptr, nullptr,
                                         &result) == 0x01000000);
  CHECK(result.targetCount == 0);
}
