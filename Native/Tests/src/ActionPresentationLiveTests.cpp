#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Battle/ActionPresentationLive.hpp"
#include "Content/ModuleData.h"
#include "Render/ModelGeometryLive.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Effects/Runtime.h"
}

namespace {
namespace b = VS::Battle;
struct Model {
  Actors_ShpModel header{};
  std::array<Actors_ShpBone, 6> bones{};
};
struct LivePresentation {
  decltype(std::to_array(Battle_actors)) saved_battle =
      std::to_array(Battle_actors);
  decltype(std::to_array(Actors_actorRecords)) saved_actors =
      std::to_array(Actors_actorRecords);
  Battle_ActionDefinition saved_definition = Battle_actionDefinitions[60];
  std::array<Battle_Actor, 3> battle{};
  std::array<Battle_ActorStats, 3> stats{};
  std::array<Actors_ActorRecord, 3> actors{};
  Model model{};
  Battle_ActionRequest action{};
  struct Guarded {
    std::uint32_t before = 0x12345678;
    Effects_ActionEffectRequest request{};
    std::uint32_t after = 0x87654321;
  } output;
  LivePresentation() {
    std::ranges::fill(Battle_actors, nullptr);
    std::ranges::fill(Actors_actorRecords, nullptr);
    for (std::size_t i = 0; i < model.bones.size(); ++i) {
      model.bones.at(i).limbIds = static_cast<unsigned char>(i + 1);
    }
    VS::Render::RecordLiveModelExtent(&model, sizeof(model));
    for (std::size_t i = 0; i < battle.size(); ++i) {
      battle.at(i).allocationKind = i == 2 ? 2 : 1;
      battle.at(i).stats = &stats.at(i);
      stats.at(i).defaultTargetLimb = 4;
      actors.at(i).instance.model = &model.header;
      actors.at(i).instance.boneCount = 6;
      actors.at(i).modelDataSize = sizeof(model);
      Battle_actors[i] = &battle.at(i);
      Actors_actorRecords[i] = &actors.at(i);
    }
    action.actionId = 60;
    Battle_actionDefinitions[60].effectId = 39;
    action.focusPosition = {11, 22, 33, 44};
  }
  LivePresentation(const LivePresentation&) = delete;
  LivePresentation& operator=(const LivePresentation&) = delete;
  ~LivePresentation() {
    VS::Render::ForgetLiveModelExtent(&model);
    std::ranges::copy(saved_battle, std::begin(Battle_actors));
    std::ranges::copy(saved_actors, std::begin(Actors_actorRecords));
    Battle_actionDefinitions[60] = saved_definition;
  }
  void ActorHit(std::size_t index, int slot, int limb) {
    auto& hit = action.targets[index];
    hit.targetsPosition = 0;
    hit.target.targetActor = static_cast<unsigned char>(slot);
    hit.target.targetLimb = static_cast<signed char>(limb);
  }
  void WorldHit(std::size_t index) {
    auto& hit = action.targets[index];
    hit.targetsPosition = 1;
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
  fixture->action.targetCount = 1;
  fixture->ActorHit(0, 1, 2);
  fixture->Build();
  const auto& request = fixture->output.request;
  CHECK(request.count == 1);
  CHECK(request.targets[0].value.actor.part == 2);
  CHECK(request.otherCount == 1);
  CHECK(request.others[0] == 2);
  CHECK(request.source.value.actor.part == 0);
  CHECK(request.source.value.actor.eventActorSlot == 0);
  CHECK(request.targets[0].value.actor.eventActorSlot == 0);
  CHECK(request.position.pad == 0);
}
TEST_CASE("F00109 production duplicate does not change a later default limb",
          "[action-presentation][F00109][redirect-binding]") {
  auto fixture = std::make_unique<LivePresentation>();
  fixture->action.targetCount = 3;
  fixture->ActorHit(0, 1, 2);
  fixture->ActorHit(1, 1, 3);
  fixture->ActorHit(2, 2, -1);
  fixture->Build();
  const auto& request = fixture->output.request;
  CHECK(request.count == 2);
  CHECK(request.targets[0].value.actor.part == 0);
  CHECK(request.targets[1].value.actor.actor == 2);
  CHECK(request.targets[1].value.actor.part == 4);
  CHECK(request.otherCount == 0);
}
TEST_CASE("F00109 production compacted world is never mistaken for an actor",
          "[action-presentation][F00109][redirect-binding]") {
  auto fixture = std::make_unique<LivePresentation>();
  fixture->action.targetCount = 4;
  fixture->ActorHit(0, 1, 2);
  fixture->ActorHit(1, 1, 3);
  fixture->WorldHit(2);
  fixture->ActorHit(3, 2, 4);
  fixture->Build();
  const auto& request = fixture->output.request;
  CHECK(request.count == 3);
  CHECK(request.targets[1].type == 5);
  CHECK(std::bit_cast<std::array<std::int16_t, 4>>(
            request.targets[1].value.position) ==
        std::array<std::int16_t, 4>{2, -3, 17, -99});
  CHECK(request.targets[2].value.actor.actor == 2);
  CHECK(request.otherCount == 0);
}
TEST_CASE(
    "F00109 production sixteen-world boundary preserves neighboring storage",
    "[action-presentation][F00109][redirect-binding]") {
  auto fixture = std::make_unique<LivePresentation>();
  fixture->action.targetCount = 16;
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
  fixture->action.targetCount = 17;
  for (std::size_t i = 0; i < 17; ++i) fixture->ActorHit(i, 1, 3);
  fixture->Build();
  CHECK(fixture->output.request.count == 1);
  CHECK(fixture->output.request.targets[0].value.actor.part == 0);
  for (std::size_t i = 0; i < 15; ++i) fixture->WorldHit(i);
  fixture->Build();
  CHECK(fixture->output.request.count == 16);
  CHECK(fixture->output.request.targets[15].value.actor.part == 0);
  CHECK(fixture->output.before == 0x12345678);
  CHECK(fixture->output.after == 0x87654321);
}
TEST_CASE("F00110 and F00191 forwarding aliases have no module entries",
          "[action-presentation][F00110][F00191]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
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
