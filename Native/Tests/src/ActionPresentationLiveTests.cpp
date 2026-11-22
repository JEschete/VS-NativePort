#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorInstances.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/ShpModel.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionPresentationLive.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Content/ModuleData.hpp"
#include "Effects/Runtime.hpp"
#include "Render/ModelGeometryLive.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::ActionRequestRecord;

namespace {
namespace b = VS::Battle;
struct Model {
  VS::Actors::ShpModel header{};
  std::array<VS::Actors::ShpBone, 6> bones{};
};
struct LivePresentation {
  decltype(VS::Battle::actors) saved_battle = VS::Battle::actors;
  decltype(VS::Actors::actorRecords) saved_actors = VS::Actors::actorRecords;
  ActionDefinition saved_definition = VS::Battle::actionDefinitions[60];
  std::array<VS::Battle::Actor, 3> battle{};
  std::array<VS::Battle::ActorStats, 3> stats{};
  std::array<VS::Actors::ActorRecord, 3> actors{};
  Model model{};
  ActionRequestRecord action{};
  struct Guarded {
    std::uint32_t before = 0x12345678;
    VS::Effects::LiveActionEffectRequest request{};
    std::uint32_t after = 0x87654321;
  } output;
  LivePresentation() {
    std::ranges::fill(VS::Battle::actors, nullptr);
    VS::Actors::ResetLiveActorHandles();
    std::ranges::fill(VS::Actors::actorRecords, nullptr);
    for (std::size_t i = 0; i < model.bones.size(); ++i) {
      model.bones.at(i).limb_ids = static_cast<unsigned char>(i + 1);
    }
    VS::Render::RecordLiveModelExtent(&model, sizeof(model));
    for (std::size_t i = 0; i < battle.size(); ++i) {
      battle.at(i).stats = &stats.at(i);
      stats.at(i).default_target_limb = 4;
      actors.at(i).instance.model = &model.header;
      actors.at(i).instance.bone_count = 6;
      actors.at(i).model_data_size = sizeof(model);
      static_cast<void>(VS::Actors::InstallLiveActorInstance(
          i, &battle.at(i),
          i == 2 ? VS::Actors::ActorShape::kSecondaryBody
                 : VS::Actors::ActorShape::kEquipped));
      VS::Actors::actorRecords[i] = &actors.at(i);
    }
    action.action_id = 60;
    VS::Battle::actionDefinitions[60].effect_id = 39;
    action.focus_position = {11, 22, 33, 44};
  }
  LivePresentation(const LivePresentation&) = delete;
  LivePresentation& operator=(const LivePresentation&) = delete;
  ~LivePresentation() {
    VS::Render::ForgetLiveModelExtent(&model);
    VS::Actors::ResetLiveActorHandles();
    std::ranges::copy(saved_battle, std::begin(VS::Battle::actors));
    std::ranges::copy(saved_actors, std::begin(VS::Actors::actorRecords));
    VS::Battle::actionDefinitions[60] = saved_definition;
  }
  void ActorHit(std::size_t index, int slot, int limb) {
    auto& hit = action.targets[index];
    hit.targets_position = 0;
    hit.target.target_actor = static_cast<unsigned char>(slot);
    hit.target.target_limb = static_cast<signed char>(limb);
  }
  void WorldHit(std::size_t index) {
    auto& hit = action.targets[index];
    hit.targets_position = 1;
    constexpr std::array<std::int16_t, 4> kPoint{2, -3, 17, -99};
    hit.target = std::bit_cast<decltype(hit.target)>(kPoint);
  }
  void Build() {
    b::BuildLiveActionPresentationRequest(&action, &output.request);
  }
};
TEST_CASE("F00109 production builder excludes real targets from bystanders",
          "[action-presentation][F00109][redirect-binding]") {
  auto fixture = std::make_unique<LivePresentation>();
  fixture->action.target_count = 1;
  fixture->ActorHit(0, 1, 2);
  fixture->Build();
  const auto& request = fixture->output.request;
  CHECK(request.count == 1);
  CHECK(request.targets[0].Actor().part == 2);
  CHECK(request.other_count == 1);
  CHECK(request.others[0] == 2);
  CHECK(request.source.Actor().part == 0);
  CHECK(request.source.Actor().event_actor_slot == 0);
  CHECK(request.targets[0].Actor().event_actor_slot == 0);
  CHECK(request.position.pad == 0);
}
TEST_CASE("F00109 production bystanders follow their spawn definitions",
          "[action-presentation][F00109][redirect-binding][actor-shape]") {
  // #55: actor 2's allocation kind word says a second body (2), but only its
  // spawn definition decides; an auxiliary core or an actor the spawn host
  // did not allocate is no bystander.
  auto fixture = std::make_unique<LivePresentation>();
  fixture->action.target_count = 1;
  fixture->ActorHit(0, 1, 2);
  VS::Battle::Actor& bystander = fixture->battle.at(2);
  bystander.allocation_kind = 2;
  VS::Actors::InvalidateLiveActorHandle(2, &bystander);
  VS::Battle::actors[2] = nullptr;
  SECTION("an auxiliary core") {
    CHECK(VS::Actors::InstallLiveActorInstance(
              2, &bystander, VS::Actors::ActorShape::kAuxiliary) !=
          VS::Actors::SpawnedActor::kNone);
  }
  SECTION("no spawn definition") { VS::Battle::actors[2] = &bystander; }
  fixture->Build();
  CHECK(fixture->output.request.count == 1);
  CHECK(fixture->output.request.other_count == 0);
}
TEST_CASE("F00109 production duplicate does not change a later default limb",
          "[action-presentation][F00109][redirect-binding]") {
  auto fixture = std::make_unique<LivePresentation>();
  fixture->action.target_count = 3;
  fixture->ActorHit(0, 1, 2);
  fixture->ActorHit(1, 1, 3);
  fixture->ActorHit(2, 2, -1);
  fixture->Build();
  const auto& request = fixture->output.request;
  CHECK(request.count == 2);
  CHECK(request.targets[0].Actor().part == 0);
  CHECK(request.targets[1].Actor().actor == 2);
  CHECK(request.targets[1].Actor().part == 4);
  CHECK(request.other_count == 0);
}
TEST_CASE("F00109 production compacted world is never mistaken for an actor",
          "[action-presentation][F00109][redirect-binding]") {
  auto fixture = std::make_unique<LivePresentation>();
  fixture->action.target_count = 4;
  fixture->ActorHit(0, 1, 2);
  fixture->ActorHit(1, 1, 3);
  fixture->WorldHit(2);
  fixture->ActorHit(3, 2, 4);
  fixture->Build();
  const auto& request = fixture->output.request;
  CHECK(request.count == 3);
  CHECK(request.targets[1].type == 5);
  CHECK(
      std::bit_cast<std::array<std::int16_t, 4>>(request.targets[1].position) ==
      std::array<std::int16_t, 4>{2, -3, 17, -99});
  CHECK(request.targets[2].Actor().actor == 2);
  CHECK(request.other_count == 0);
}
TEST_CASE(
    "F00109 production sixteen-world boundary preserves neighboring storage",
    "[action-presentation][F00109][redirect-binding]") {
  auto fixture = std::make_unique<LivePresentation>();
  fixture->action.target_count = 16;
  for (std::size_t i = 0; i < 16; ++i) fixture->WorldHit(i);
  fixture->Build();
  CHECK(fixture->output.request.count == 16);
  CHECK(fixture->output.request.targets[15].type == 5);
  CHECK(fixture->output.before == 0x12345678);
  CHECK(fixture->output.after == 0x87654321);
}
TEST_CASE("F00109 production deduplication remains valid at full capacity",
          "[action-presentation][F00109][redirect-binding]") {
  auto fixture = std::make_unique<LivePresentation>();
  fixture->action.target_count = 17;
  for (std::size_t i = 0; i < 17; ++i) fixture->ActorHit(i, 1, 3);
  fixture->Build();
  CHECK(fixture->output.request.count == 1);
  CHECK(fixture->output.request.targets[0].Actor().part == 0);
  for (std::size_t i = 0; i < 15; ++i) fixture->WorldHit(i);
  fixture->Build();
  CHECK(fixture->output.request.count == 16);
  CHECK(fixture->output.request.targets[15].Actor().part == 0);
  CHECK(fixture->output.before == 0x12345678);
  CHECK(fixture->output.after == 0x87654321);
}
TEST_CASE("F00110 and F00191 forwarding aliases have no module entries",
          "[action-presentation][F00110][F00191]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800719DC) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80079030) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80070F28) ==
        reinterpret_cast<void*>(&b::CommitLiveSelectedAction));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8007138C) ==
        reinterpret_cast<void*>(&b::StartLiveActionPresentation));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80070278) ==
        reinterpret_cast<void*>(&b::StartLiveActionAnimation));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800704B0) ==
        reinterpret_cast<void*>(&b::AdvanceLiveActionEffectPhase));
}
}  // namespace
