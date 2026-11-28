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
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionPreview.hpp"
#include "Battle/ActionPreviewLive.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/CombatAction.hpp"
#include "Battle/Damage.hpp"
#include "Battle/HitChance.hpp"
#include "Battle/HitModifiers.hpp"
#include "Battle/HitModifiersLive.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Random.hpp"
#include "Core/RandomLive.hpp"
#include "TestSupport/BattleCatalogRoot.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::ActionRequestRecord;
using VS::Battle::ActionState;

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
  auto live = std::bit_cast<ActionRequestRecord>(poison);
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
  auto live = std::bit_cast<VS::Battle::HitResult>(poison);
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
  VS::test_support::BattleCatalogRoot catalogs;
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  decltype(VS::Battle::actionDefinitions) saved_definitions =
      VS::Battle::actionDefinitions;
  decltype(VS::Battle::hitFunctions) saved_hits = VS::Battle::hitFunctions;
  decltype(VS::Battle::statCalculators) saved_stats =
      VS::Battle::statCalculators;
  decltype(VS::Battle::actionEffectMediators) saved_effects =
      VS::Battle::actionEffectMediators;
  ActionState* saved_state = VS::Battle::actionState;
  std::uint32_t saved_last = VS::Battle::lastActionStatValue;
  std::uint32_t saved_prerequisite = VS::Battle::hitPrerequisiteResult;
  std::uint32_t saved_previous = VS::Battle::previousHitResult;
  ActionState state{};
  std::array<VS::Battle::Actor, 2> actors{};
  std::array<VS::Battle::ActorStats, 2> stats{};
  PreviewFixture() {
    std::fill(std::begin(VS::Battle::actors), std::end(VS::Battle::actors),
              nullptr);
    VS::Battle::actionState = &state;
    state.chain_step = 1;
    for (std::size_t i = 0; i < actors.size(); ++i) {
      VS::Battle::actors[i] = &actors.at(i);
      actors.at(i).stats = &stats.at(i);
      stats.at(i).actor_id = static_cast<std::uint8_t>(i);
      stats.at(i).max_hp = stats.at(i).current_hp = 100;
      stats.at(i).max_mp = stats.at(i).current_mp = 100;
      for (auto& limb : stats.at(i).limbs) limb.hp = 100;
    }
  }
  ~PreviewFixture() {
    std::copy(saved_actors.begin(), saved_actors.end(),
              VS::Battle::actors.begin());
    std::copy(saved_definitions.begin(), saved_definitions.end(),
              VS::Battle::actionDefinitions.data());
    std::copy(saved_hits.begin(), saved_hits.end(),
              VS::Battle::hitFunctions.begin());
    std::copy(saved_stats.begin(), saved_stats.end(),
              VS::Battle::statCalculators.begin());
    std::copy(saved_effects.begin(), saved_effects.end(),
              VS::Battle::actionEffectMediators.begin());
    VS::Battle::actionState = saved_state;
    VS::Battle::lastActionStatValue = saved_last;
    VS::Battle::hitPrerequisiteResult = saved_prerequisite;
    VS::Battle::previousHitResult = saved_previous;
  }
  static std::vector<std::uint8_t> Module(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) SKIP("Extracted original modules unavailable");
    return {std::istreambuf_iterator<char>(stream), {}};
  }
  static void Relocate(void** target, std::span<const std::uint8_t> image) {
    const auto tables = VS::Content::ModulePointerTables();
    const auto found = std::ranges::find(
        tables, target, &VS::Content::ModulePointerTable::target);
    REQUIRE(found != tables.end());
    REQUIRE(VS::Content::RelocateModulePointerTable(
        *found, image, VS::Content::ModuleSymbols()));
  }
  void OriginalDefinitions() {
    const auto main = Module("Native\\data\\disc\\SLUS_010.40");
    constexpr auto kActionOffset = 0x3C1DC;
    REQUIRE(main.size() >=
            kActionOffset + sizeof(VS::Battle::actionDefinitions));
    std::memcpy(VS::Battle::actionDefinitions.data(),
                main.data() + kActionOffset,
                sizeof(VS::Battle::actionDefinitions));
    const auto battle = Module("Native\\data\\disc\\BATTLE\\BATTLE.PRG");
    Relocate(reinterpret_cast<void**>(VS::Battle::hitFunctions.data()), battle);
    Relocate(reinterpret_cast<void**>(VS::Battle::statCalculators.data()),
             battle);
    Relocate(reinterpret_cast<void**>(VS::Battle::actionEffectMediators.data()),
             battle);
  }
  void SelfRoute(int action) {
    stats[0].limbs[0].chain_evasion = 51;
    stats[0].default_target_limb = 4;
    REQUIRE(VS::Battle::actionDefinitions[action]
                .hits[0]
                .prerequisite_function_id == 3);
    REQUIRE(VS::Battle::hitFunctions[7] == &b::LiveChainHitChance);
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
  std::array<std::uint8_t, sizeof(ActionRequestRecord)> bytes;
  bytes.fill(static_cast<std::uint8_t>(poison));
  auto output = std::bit_cast<ActionRequestRecord>(bytes);
  f.SelfRoute(action);
  const auto affordable =
      actor ? b::PreviewLiveActionAgainstActor(action, 0, 1, 4, nullptr,
                                               nullptr, &output)
            : b::PreviewLiveActionAgainstLimb(action, 0, 1, 4, &output);
  CHECK(affordable == 0x01000000);
  CHECK(output.primary_target.target.target_limb == 0);
  CHECK(output.primary_target.target.hit_threshold == 80);
  CHECK(std::bit_cast<b::ActionPoint>(output.focus_position) ==
        b::ActionPoint{});
  CHECK(output.target_count == (actor ? 0 : 1));
  ActionRequestRecord fresh{};
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
  ActionRequestRecord source{}, result{};
  source.primary_target.targets_position = 3;
  source.primary_target.target.hp = 123;
  source.target_count = 1;
  source.targets[0].target.target_actor = 1;
  b::ResolveLiveActionTargets(action, &result, &source, 0);
  CHECK(result.primary_target.targets_position == 3);
  CHECK(result.primary_target.target.hp == 0);
  CHECK(result.target_count == 1);
}
TEST_CASE(
    "Live resolution refreshes callback records and preserves cached stats",
    "[action-preview][F00514]") {
  PreviewFixture f;
  constexpr int kAction = 200;
  VS::Battle::actionDefinitions[kAction] = {};
  auto& definition = VS::Battle::actionDefinitions[kAction];
  definition.hits[0].prerequisite_function_id = 1;
  definition.hits[0].hit_function_id = 7;
  definition.hits[0].effect_id = 1;
  VS::Battle::hitFunctions[7] = &b::LiveChainHitChance;
  VS::Battle::actionEffectMediators[1] =
      +[](ActionDefinition*, VS::Battle::HitResult*,
          VS::Battle::HitResult* target, int, int) {
        target->target.target_actor = 0;
        target->target.target_limb = 5;
        target->target.hp = 10;
        VS::Battle::actors[1]->stats = VS::Battle::actors[0]->stats;
        VS::Battle::actionState->queue_index = 0;
        VS::Battle::actionState->queue[0].target_count = 1;
        VS::Battle::actionState->queue[0].targets[254].target.hp = 321;
      };
  f.stats[1].cancels_hit_results = 1;
  f.actors[0].room_spawn_index = 128;
  auto& source = f.state.selection;
  auto& result = f.state.queue[0];
  source.target_count = 2;
  source.targets[0].target.target_actor = 1;
  b::ResolveLiveActionTargets(kAction, &result, &source, 1);
  CHECK(result.target_count == 1);
  CHECK(result.targets[0].target.target_actor == 0);
  CHECK(result.targets[0].target.target_limb == 5);
  CHECK(result.targets[0].target.hp == 0);
  CHECK(result.targets[0].target.impact_kind == 4);
  CHECK(result.targets[254].target.hp == 321);
  CHECK(std::bit_cast<b::ActionRequest>(source) ==
        std::bit_cast<b::ActionRequest>(result));
}
TEST_CASE("Live previews preserve unused null stats and real no-resource cost",
          "[action-preview][F00515][F00516]") {
  PreviewFixture f;
  VS::Battle::actionDefinitions[0] = {};
  f.actors[0].stats = nullptr;
  ActionRequestRecord result{};
  CHECK(b::PreviewLiveActionAgainstLimb(0, 0, 1, 0, &result) == 0x01000000);
  CHECK(b::PreviewLiveActionAgainstActor(0, 0, -1, -1, nullptr, nullptr,
                                         &result) == 0x01000000);
  CHECK(result.target_count == 0);
}

namespace {
class CombatRandom final : public VS::Items::ItemRandom {
 public:
  VS::Core::Random random;
  std::vector<int> draws;
  std::function<void()> interleave;
  bool lowest_draws{};
  std::int32_t NextBounded(std::int32_t bound) override {
    Interleave();
    draws.push_back(bound);
    return lowest_draws ? 0 : random.Next(bound);
  }
  std::int32_t NextAveraged(std::int32_t bound) override {
    Interleave();
    draws.push_back(-bound);
    return lowest_draws ? 0 : random.NextSmoothed(bound);
  }

 private:
  void Interleave() {
    if (!interleave) return;
    auto callback = std::move(interleave);
    interleave = {};
    callback();
  }
};

struct PhysicalFixture {
  PreviewFixture legacy;
  decltype(b::limbArmorDpLoss) saved_loss = b::limbArmorDpLoss;
  decltype(b::criticalHitOccurred) saved_critical = b::criticalHitOccurred;
  VS::Core::Rand15 saved_random = VS::Core::BiosRand();
  std::array<std::int32_t, VS::Core::Random::TableSize> saved_shuffle =
      std::to_array(VS::Core::randomShuffleTable);
  std::int32_t saved_index = VS::Core::randomShuffleIndex;
  VS::Content::ActionRule rule;
  VS::Actors::ActorInstanceTable identities;
  std::array<b::CombatActorState, 2> actors{};
  decltype(b::bladePpDamageBonuses) bonuses = b::bladePpDamageBonuses;
  decltype(b::limbInjuryStatusIds) injuries = b::limbInjuryStatusIds;
  b::CombatActionIntent intent{};

  PhysicalFixture() {
    legacy.OriginalDefinitions();
    const auto& original = b::actionDefinitions[1];
    rule.kind = original.type;
    rule.category = original.category;
    rule.cost = original.cost;
    rule.recovery_delay = original.recovery_delay;
    rule.behavior = VS::Content::kBehaviorHitsFirstObstruction |
                    VS::Content::kBehaviorStartsUnlocked;
    const auto& hit = original.hits[0];
    rule.hits.front() = VS::Content::ActionHit{
        static_cast<std::uint8_t>(hit.effect_id),
        static_cast<std::uint8_t>(hit.prerequisite_function_id),
        static_cast<std::uint8_t>(hit.hit_function_id),
        static_cast<std::uint8_t>(hit.stat_calculator_id),
        static_cast<std::uint8_t>(hit.stat_factor),
        static_cast<std::uint8_t>(hit.damage_type),
        static_cast<std::uint8_t>(hit.affinity)};
    for (std::size_t i = 0; i < actors.size(); ++i) InitializeActor(i);
    Select(0);
  }
  void InitializeActor(std::size_t i) {
    auto& actor = actors.at(i);
    actor.identity = identities.Begin(i, i + 1, {});
    if (!identities.Publish(i, actor.identity)) FAIL("publish combat fixture");
    actor.slot = actor.damage.actor_id = static_cast<std::uint8_t>(i);
    actor.hp = actor.max_hp = 4000;
    actor.mp = actor.max_mp = 100;
    actor.agility = 100;
    actor.damage.strength = static_cast<std::uint16_t>(100 - i * 80);
    actor.weapon_drawn = 1;
    actor.damage.weapon.current_dp = actor.damage.weapon.max_dp = 400;
    actor.damage.weapon.current_pp = 20;
    actor.damage.weapon.max_pp = 100;
    actor.damage.weapon.action_type = 2;
    actor.damage.weapon.risk = 10;
    actor.damage.weapon.recovery_delay = 21;
    for (std::size_t limb = 0; limb < actor.limbs.size(); ++limb) {
      actor.limbs.at(limb).hp = {100, 100};
      actor.limbs.at(limb).damage.armor.current_dp =
          actor.limbs.at(limb).damage.armor.max_dp = 300;
      actor.armor_splash.at(limb).at(limb) = 100;
    }
  }
  ~PhysicalFixture() {
    b::limbArmorDpLoss = saved_loss;
    b::criticalHitOccurred = saved_critical;
    VS::Core::BiosRand() = saved_random;
    std::copy(saved_shuffle.begin(), saved_shuffle.end(),
              std::begin(VS::Core::randomShuffleTable));
    VS::Core::randomShuffleIndex = saved_index;
  }
  void Select(std::size_t source) {
    intent.control =
        source == 0 ? b::CombatControl::kPlayer : b::CombatControl::kAi;
    intent.source = actors.at(source).identity;
    intent.action = 1;
    intent.targets = {b::CombatActorTarget{actors.at(1 - source).identity, 3}};
  }
  static void DetachLiveState() {
    b::actors.fill(nullptr);
    b::actionState = nullptr;
    b::statCalculators.fill(nullptr);
    b::hitFunctions.fill(nullptr);
    b::actionEffectMediators.fill(nullptr);
    b::lastActionStatValue = 0xABCDEF01U;
    b::hitPrerequisiteResult = 31;
    b::previousHitResult = 91;
    b::criticalHitOccurred = 97;
    b::limbArmorDpLoss.fill(79);
  }
  b::CombatRuleInputs Inputs(CombatRandom& random) {
    return {rule,     actors, random, bonuses, legacy.catalogs.status.rules,
            injuries, {},     1};
  }
  void RestoreLegacyActors() {
    for (std::size_t i = 0; i < actors.size(); ++i) {
      const auto& input = actors.at(i);
      auto& actor = legacy.actors.at(i);
      auto& stats = legacy.stats.at(i);
      actor.weapon_drawn = static_cast<std::int32_t>(input.weapon_drawn);
      actor.room_spawn_index = input.spawn;
      stats.current_hp = input.hp;
      stats.max_hp = input.max_hp;
      stats.current_mp = input.mp;
      stats.max_mp = input.max_mp;
      stats.agility = input.agility;
      stats.strength = input.damage.strength;
      stats.intelligence = input.damage.intelligence;
      stats.risk = input.damage.risk;
      stats.weapon = input.damage.weapon;
      stats.shield = input.damage.shield;
      stats.accessory = input.damage.accessory;
      stats.statuses = std::bit_cast<std::int32_t>(input.statuses);
      stats.cancels_hit_results = input.cancels_results;
      for (std::size_t limb = 0; limb < input.limbs.size(); ++limb) {
        stats.limbs.at(limb).hp = input.limbs.at(limb).hp.current;
        stats.limbs.at(limb).max_hp = input.limbs.at(limb).hp.maximum;
        stats.limbs.at(limb).armor = input.limbs.at(limb).damage.armor;
        for (std::size_t other = 0; other < input.limbs.size(); ++other) {
          stats.dp_splash.at(limb).at(other) =
              std::bit_cast<std::int8_t>(input.armor_splash.at(limb).at(other));
        }
      }
    }
  }
  b::ActionRequest LegacyResult(std::size_t source, bool execute) {
    RestoreLegacyActors();
    // Both callers receive an already resolved trajectory in this comparison.
    b::actionDefinitions[1].hits_first_obstruction = 0;
    ActionRequestRecord request{}, output{};
    request.primary_target.target.target_actor =
        static_cast<std::uint8_t>(source);
    request.target_count = 1;
    request.targets[0].target.target_actor =
        static_cast<std::uint8_t>(1 - source);
    request.targets[0].target.target_limb = 3;
    b::ResolveLiveActionTargets(1, &output, &request,
                                static_cast<int>(execute));
    return std::bit_cast<b::ActionRequest>(output);
  }
  b::CombatActionResult Run(CombatRandom& random, bool execute = true) {
    auto result = b::ResolvePhysicalAction(intent, Inputs(random), execute);
    if (!result) FAIL(result.error());
    return std::move(*result);
  }
};
void CheckPhysicalActor(const b::CombatActorState& actual,
                        const b::ActorStats& expected) {
  CHECK(actual.damage.risk == expected.risk);
  CHECK(actual.damage.weapon.current_dp == expected.weapon.current_dp);
  CHECK(actual.damage.weapon.current_pp == expected.weapon.current_pp);
  CHECK(actual.damage.shield.current_dp == expected.shield.current_dp);
  CHECK(actual.damage.shield.current_pp == expected.shield.current_pp);
  CHECK(actual.recovery_timer == expected.action_recovery_timer);
  for (std::size_t i = 0; i < actual.limbs.size(); ++i) {
    CHECK(actual.limbs.at(i).damage.armor.current_dp ==
          expected.limbs.at(i).armor.current_dp);
  }
}
}  // namespace

TEST_CASE("Physical rules preserve original player and AI damage and RNG",
          "[combat-rules][action-preview][issue-56]") {
  PhysicalFixture f;
  const auto source = GENERATE(std::size_t{0}, std::size_t{1});
  const bool execute = GENERATE(false, true);
  const bool cancelled = GENERATE(false, true);
  const int advance = GENERATE(0, 9, 31);
  f.Select(source);
  f.actors.at(1 - source).cancels_results = cancelled;
  CombatRandom random;
  VS::Core::InitializeLiveRandomShuffleTable();
  for (int i = 0; i < advance; ++i) {
    random.random.Next(100);
    VS::Core::NextLiveRandomBounded(100);
  }
  const auto expected = f.LegacyResult(source, execute);
  const auto result = f.Run(random, execute);
  REQUIRE(result.hits.size() == 1);
  CHECK(result.hits.front().changes ==
        b::ReadActionHitResult(expected.targets.front()));
  CHECK(result.hits.front().threshold == expected.targets.front().threshold);
  CheckPhysicalActor(result.actors.at(source), f.legacy.stats.at(source));
  CheckPhysicalActor(result.actors.at(1 - source),
                     f.legacy.stats.at(1 - source));
  CHECK(random.random.Next(100000) == VS::Core::NextLiveRandomBounded(100000));
}

TEST_CASE("Physical rules execute with no live actors state or calculators",
          "[combat-rules][issue-56]") {
  PhysicalFixture f;
  PhysicalFixture::DetachLiveState();
  CombatRandom random;
  const auto result = f.Run(random);
  REQUIRE(result.hits.size() == 1);
  CHECK(result.hits.front().changes.changes.front().amount > 0);
  CHECK(result.actors.front().damage.risk == 10);
  CHECK(f.actors.front().damage.risk == 0);
  CHECK(b::lastActionStatValue == 0xABCDEF01U);
  CHECK(b::hitPrerequisiteResult == 31);
  CHECK(b::previousHitResult == 91);
  CHECK(b::criticalHitOccurred == 97);
  CHECK(b::limbArmorDpLoss.front() == 79);
}

TEST_CASE("Nested physical simulations retain their own pending hit state",
          "[combat-rules][issue-56]") {
  PhysicalFixture f;
  CombatRandom alone, interleaved, nested;
  const auto expected = f.Run(alone);
  std::optional<b::CombatActionResult> inner;
  interleaved.interleave = [&] { inner = f.Run(nested); };
  const auto actual = f.Run(interleaved);
  REQUIRE(inner.has_value());
  if (!inner) return;
  CHECK(actual.hits.front().changes == expected.hits.front().changes);
  CHECK(inner->hits.front().changes == expected.hits.front().changes);
  CHECK(actual.actors.front().damage.risk ==
        expected.actors.front().damage.risk);
  CHECK(actual.actors.front().damage.weapon.current_dp ==
        expected.actors.front().damage.weapon.current_dp);
  CHECK(interleaved.draws == alone.draws);
  CHECK(nested.draws == alone.draws);
  CHECK(interleaved.random.Next(999) == alone.random.Next(999));
}

TEST_CASE("Physical rules refuse stale identities before consuming random",
          "[combat-rules][issue-56]") {
  PhysicalFixture f;
  const bool source = GENERATE(false, true);
  auto& actor = f.actors.at(source ? 0 : 1);
  REQUIRE(f.identities.Remove(actor.slot, actor.identity));
  actor.identity = f.identities.Begin(actor.slot, actor.slot + 1, {});
  REQUIRE(f.identities.Publish(actor.slot, actor.identity));
  CombatRandom random;
  const auto result =
      b::ResolvePhysicalAction(f.intent, f.Inputs(random), true);
  CHECK_FALSE(result.has_value());
  CHECK(random.draws.empty());
  CHECK(f.actors.front().damage.risk == 0);
  CHECK(f.actors.back().hp == 4000);
}

TEST_CASE("Physical equipment growth produces owned actor-qualified notices",
          "[combat-rules][issue-56]") {
  PhysicalFixture f;
  f.actors.front().damage.weapon.blade.id = 1;
  CombatRandom random;
  random.lowest_draws = true;
  const auto result = f.Run(random);
  REQUIRE_FALSE(result.equipment_notices.empty());
  CHECK(std::ranges::all_of(result.equipment_notices, [&](const auto& notice) {
    return notice.actor == f.intent.source;
  }));
  CHECK(result.equipment_notices.front().label ==
        VS::Items::kWeaponNoticeLabel);
  CHECK(f.actors.front().damage.weapon.blade.classes.front() == 0);
}
