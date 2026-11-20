#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorRecord.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/ActionFlow.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/CameraLiveState.hpp"
#include "Effects/Runtime.hpp"

using VS::Battle::ActionState;

namespace {
struct ImpactFixture {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, std::size_t{256} * 1024> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  ActionState* previous_action = VS::Battle::actionState;
  VS::Actors::ActorRecord* previous_actor = VS::Actors::actorRecords[16];
  VS::Effects::LiveEffectPackage* previous_package = VS::Effects::package;
  VS::Effects::LiveEffectPackage* previous_saved = VS::Effects::savedPackage;
  VS::Effects::LiveEffectPackage previous_secondary =
      VS::Effects::secondaryPackage;
  VS::Effects::LiveActionEffectRequest previous_request =
      VS::Effects::secondaryRequest;
  VS::Effects::LiveEffectInstance* previous_instances =
      VS::Effects::secondaryInstances;
  decltype(VS::Effects::savedActionCamera) previous_camera =
      VS::Effects::savedActionCamera;
  decltype(VS::Effects::effectCamera) previous_effect_camera =
      VS::Effects::effectCamera;
  decltype(std::to_array(VS::Effects::tokenBitmaps)) previous_tokens =
      std::to_array(VS::Effects::tokenBitmaps);
  int previous_busy = VS::Effects::effectBusyFlags;
  ActionState action{};
  VS::Actors::ActorRecord actor{};
  std::array<char, 256> script{};

  ImpactFixture() {
    VS::Battle::actionState = &action;
    VS::Actors::actorRecords[16] = &actor;
    VS::Effects::secondaryPackage = {};
    VS::Effects::secondaryPackage.programs = script.data();
    VS::Effects::secondaryInstances = nullptr;
    VS::Effects::secondaryRequest = {};
    VS::Effects::effectBusyFlags = 0;
    std::ranges::fill(VS::Effects::tokenBitmaps, 0);
  }
  ImpactFixture(const ImpactFixture&) = delete;
  ImpactFixture& operator=(const ImpactFixture&) = delete;
  ~ImpactFixture() {
    VS::Battle::actionState = previous_action;
    VS::Actors::actorRecords[16] = previous_actor;
    VS::Effects::package = previous_package;
    VS::Effects::savedPackage = previous_saved;
    VS::Effects::secondaryPackage = previous_secondary;
    VS::Effects::secondaryRequest = previous_request;
    VS::Effects::secondaryInstances = previous_instances;
    VS::Effects::savedActionCamera = previous_camera;
    VS::Effects::effectCamera = previous_effect_camera;
    VS::Effects::effectBusyFlags = previous_busy;
    std::ranges::copy(previous_tokens, std::begin(VS::Effects::tokenBitmaps));
  }
};

TEST_CASE("F00832 gated production prefix still creates actual impact effects",
          "[action-reactions][F00832][effects]") {
  auto fixture = std::make_unique<ImpactFixture>();
  fixture->actor.instance.slot = 0;
  fixture->actor.instance.movement_mode = 2;
  fixture->actor.instance.jump_countdown = 99;
  fixture->action.queue_index = 0xFFFFFFFFU;
  auto& queued = fixture->action.queue[3];
  queued.target_count = 1;
  queued.targets[0].targets_position = 1;
  auto* unused = reinterpret_cast<SVECTOR*>(std::uintptr_t{1});
  VS::Battle::PlayLiveDefenseAbilityAnimation(16, unused,
                                              std::numeric_limits<int>::min());
  CHECK(fixture->actor.instance.movement_mode == 0);
  CHECK(fixture->actor.instance.jump_countdown == 0);
  CHECK(VS::Effects::secondaryRequest.effect_id == 0x26);
  CHECK(VS::Effects::secondaryRequest.count == 1);
  REQUIRE(VS::Effects::secondaryInstances != nullptr);
  CHECK(VS::Effects::secondaryInstances->context.effect_id == 0x26);
}
}  // namespace
