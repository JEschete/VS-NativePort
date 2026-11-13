#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "Battle/ActionAnimationsLive.hpp"
#include "Core/Memory/HeapLive.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Effects/CameraLiveState.h"
#include "Effects/Runtime.h"
}

namespace {
struct ImpactFixture {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, std::size_t{256} * 1024> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Battle_ActionState* previous_action = Battle_actionState;
  Actors_ActorRecord* previous_actor = Actors_actorRecords[16];
  Effects_EffectPackage* previous_package = Effects_package;
  Effects_EffectPackage* previous_saved = Effects_savedPackage;
  Effects_EffectPackage previous_secondary = Effects_secondaryPackage;
  Effects_ActionEffectRequest previous_request = Effects_secondaryRequest;
  Effects_EffectInstance* previous_instances = Effects_secondaryInstances;
  decltype(Effects_savedActionCamera) previous_camera =
      Effects_savedActionCamera;
  decltype(Effects_effectCamera) previous_effect_camera = Effects_effectCamera;
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
    Effects_secondaryPackage.block8Data = script.data();
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
    Effects_savedActionCamera = previous_camera;
    Effects_effectCamera = previous_effect_camera;
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
  CHECK(Effects_secondaryRequest.effectId == 0x26);
  CHECK(Effects_secondaryRequest.count == 1);
  REQUIRE(Effects_secondaryInstances != nullptr);
  CHECK(Effects_secondaryInstances->context.effectId == 0x26);
}
}  // namespace
