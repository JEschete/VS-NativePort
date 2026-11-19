#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/ActionFlow.h"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/CameraLiveState.hpp"
#include "Effects/Runtime.h"

namespace {
struct ImpactFixture {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, std::size_t{256} * 1024> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Battle_ActionState* previous_action = Battle_actionState;
  Actors_ActorRecord* previous_actor = Actors_actorRecords[16];
  VS::Effects::LiveEffectPackage* previous_package = Effects_package;
  VS::Effects::LiveEffectPackage* previous_saved = Effects_savedPackage;
  VS::Effects::LiveEffectPackage previous_secondary = Effects_secondaryPackage;
  VS::Effects::LiveActionEffectRequest previous_request =
      Effects_secondaryRequest;
  VS::Effects::LiveEffectInstance* previous_instances =
      Effects_secondaryInstances;
  decltype(VS::Effects::savedActionCamera) previous_camera =
      VS::Effects::savedActionCamera;
  decltype(VS::Effects::effectCamera) previous_effect_camera =
      VS::Effects::effectCamera;
  decltype(std::to_array(Effects_tokenBitmaps)) previous_tokens =
      std::to_array(Effects_tokenBitmaps);
  int previous_busy = Effects_effectBusyFlags;
  Battle_ActionState action{};
  Actors_ActorRecord actor{};
  std::array<char, 256> script{};

  ImpactFixture() {
    Battle_actionState = &action;
    Actors_actorRecords[16] = &actor;
    Effects_secondaryPackage = {};
    Effects_secondaryPackage.programs = script.data();
    Effects_secondaryInstances = nullptr;
    Effects_secondaryRequest = {};
    Effects_effectBusyFlags = 0;
    std::ranges::fill(Effects_tokenBitmaps, 0);
  }
  ImpactFixture(const ImpactFixture&) = delete;
  ImpactFixture& operator=(const ImpactFixture&) = delete;
  ~ImpactFixture() {
    Battle_actionState = previous_action;
    Actors_actorRecords[16] = previous_actor;
    Effects_package = previous_package;
    Effects_savedPackage = previous_saved;
    Effects_secondaryPackage = previous_secondary;
    Effects_secondaryRequest = previous_request;
    Effects_secondaryInstances = previous_instances;
    VS::Effects::savedActionCamera = previous_camera;
    VS::Effects::effectCamera = previous_effect_camera;
    Effects_effectBusyFlags = previous_busy;
    std::ranges::copy(previous_tokens, std::begin(Effects_tokenBitmaps));
  }
};

TEST_CASE("F00832 gated production prefix still creates actual impact effects",
          "[action-reactions][F00832][effects]") {
  auto fixture = std::make_unique<ImpactFixture>();
  fixture->actor.instance.slot = 0;
  fixture->actor.instance.movementMode = 2;
  fixture->actor.instance.jumpCountdown = 99;
  fixture->action.queueIndex = 0xFFFFFFFFU;
  auto& queued = fixture->action.queue[3];
  queued.targetCount = 1;
  queued.targets[0].targetsPosition = 1;
  auto* unused = reinterpret_cast<SVECTOR*>(std::uintptr_t{1});
  VS::Battle::PlayLiveDefenseAbilityAnimation(16, unused,
                                              std::numeric_limits<int>::min());
  CHECK(fixture->actor.instance.movementMode == 0);
  CHECK(fixture->actor.instance.jumpCountdown == 0);
  CHECK(Effects_secondaryRequest.effect_id == 0x26);
  CHECK(Effects_secondaryRequest.count == 1);
  REQUIRE(Effects_secondaryInstances != nullptr);
  CHECK(Effects_secondaryInstances->context.effect_id == 0x26);
}
}  // namespace
