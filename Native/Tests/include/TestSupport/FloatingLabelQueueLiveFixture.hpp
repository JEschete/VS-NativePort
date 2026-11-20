#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/ShpModel.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/FloatingLabelPlacementLive.hpp"
#include "Battle/FloatingLabelProducersLive.hpp"
#include "Battle/FloatingLabelQueueLive.hpp"
#include "Core/RandomLive.hpp"
#include "Field/RoomRenderingLiveState.hpp"
#include "Input/ControllerLiveState.hpp"

namespace VS::test_support::live_floating_labels {
using LabelWords = std::array<std::uint32_t, 6>;

inline LabelWords Words(const VS::Battle::FloatingLabel& label) {
  return std::bit_cast<LabelWords>(label);
}

class State {
 public:
  std::array<VS::Battle::FloatingLabelSlots, 1> labels{};
  VS::Battle::FloatingLabel canary{};
  State() {
    VS::Battle::floatingLabels = labels.data();
    VS::Battle::floatingLabelCount = 0;
  }
  ~State() {
    VS::Battle::floatingLabels = saved_owner_;
    VS::Battle::floatingLabelCount = saved_commands_[0];
    VS::Battle::actorMarkersHidden = saved_commands_[1];
    VS::Battle::abilityTimingCueFrames = saved_commands_[2];
    VS::Battle::statusLabelStepTimer = saved_commands_[3];
    VS::Battle::ashleyStatusLabelsSuppressed = saved_commands_[4];
    VS::Battle::placedLabelCount = saved_commands_[5];
    VS::Battle::actorMarkerSides = saved_sides_;
    VS::Battle::abilityTimingCuePosition = saved_cue_position_;
    std::ranges::copy(saved_markers_, VS::Battle::actorMarkers.begin());
    VS::Core::SeedBiosRand(saved_random_state_);
  }
  State(const State&) = delete;
  State& operator=(const State&) = delete;
  void Populate() {
    for (std::size_t index = 0; index < std::size(labels[0]); ++index) {
      auto& label = labels[0][index];
      label.position = {11, -22, 33, -3};
      label.key.bytes[0] = 1;
      label.key.bytes[1] = static_cast<char>(index);
      label.limb = 251;
      label.frame = 17;
      label.value = static_cast<std::uint16_t>(index + 500);
      label.color_flags = 128;
      label.text_length = 3;
      label.scale_x = -19;
      label.scale_y = 20;
      std::ranges::fill(label.text, static_cast<signed char>('7'));
    }
  }
  [[nodiscard]] std::span<VS::Battle::FloatingLabel, 32> Labels() {
    return labels[0];
  }

 private:
  decltype(VS::Battle::floatingLabels) saved_owner_ =
      VS::Battle::floatingLabels;
  std::array<std::int16_t, 6> saved_commands_{
      VS::Battle::floatingLabelCount,
      VS::Battle::actorMarkersHidden,
      VS::Battle::abilityTimingCueFrames,
      VS::Battle::statusLabelStepTimer,
      VS::Battle::ashleyStatusLabelsSuppressed,
      VS::Battle::placedLabelCount};
  int saved_sides_ = VS::Battle::actorMarkerSides;
  SVECTOR saved_cue_position_ = VS::Battle::abilityTimingCuePosition;
  decltype(VS::Battle::actorMarkers) saved_markers_ = VS::Battle::actorMarkers;
  unsigned saved_random_state_ = VS::Core::BiosRand().State();
};

inline std::array<LabelWords, 32> Snapshot(const State& state) {
  std::array<LabelWords, 32> result{};
  for (std::size_t index = 0; index < result.size(); ++index) {
    result[index] = Words(state.labels[0][index]);
  }
  return result;
}

class Producers {
 public:
  Actors_ActorRecord actor{};
  Producers() {
    VS::Actors::ShpBone bone{};
    bone.length = 64;
    std::memcpy(model_.data() + sizeof(VS::Actors::ShpModel), &bone,
                sizeof bone);
    actor.instance.model =
        reinterpret_cast<VS::Actors::ShpModel*>(model_.data());
    actor.instance.boneCount = 1;
    actor.modelDataSize = static_cast<unsigned>(model_.size());
    actor.instance.position = {32, -64, 96, 0};
    actor.boneMatrices[0].m[0][0] = 4096;
    actor.boneMatrices[0].m[1][1] = 4096;
    actor.boneMatrices[0].m[2][2] = 4096;
    VS::Actors::boneMatrixScratch = actor.boneMatrices[0];
    Actors_actorRecords[kActor] = &actor;
    VS::Input::settings.weapon_stat_change = 1;
  }
  ~Producers() {
    Actors_actorRecords[kActor] = saved_actor_;
    VS::Actors::boneMatrixScratch = saved_matrix_;
    VS::Input::settings = saved_settings_;
    VS::Field::areRoomUpdatesPaused = saved_pause_;
  }
  Producers(const Producers&) = delete;
  Producers& operator=(const Producers&) = delete;
  static int Queue(int producer, int value) {
    switch (producer) {
      case 0: {
        const std::array<std::uint16_t, 3> position{
            static_cast<std::uint16_t>(value), 22, 33};
        return Battle::QueueLiveWorldLockedLabel(position);
      }
      case 1:
        return Battle::LiveQueueActorValueLabel(
            kActor, -1, static_cast<unsigned>(value), 0);
      case 2:
        return Battle::LiveQueueAbilityTimingResult(value);
      case 3:
        return Battle::LiveQueueChainCount(value);
      case 4:
        return Battle::LiveQueueActorStatusLabel(kActor, 251, value % 29, 0);
      case 5:
        return Battle::LiveQueueEquipmentStatChange(240, 0, value);
      default:
        return -1;
    }
  }

 private:
  static constexpr int kActor = 1;
  Actors_ActorRecord* saved_actor_ = Actors_actorRecords[kActor];
  MATRIX saved_matrix_ = VS::Actors::boneMatrixScratch;
  decltype(VS::Input::settings) saved_settings_ = VS::Input::settings;
  decltype(VS::Field::areRoomUpdatesPaused) saved_pause_ =
      VS::Field::areRoomUpdatesPaused;
  alignas(VS::Actors::ShpModel)
      std::array<std::byte, sizeof(VS::Actors::ShpModel) +
                                sizeof(VS::Actors::ShpBone)> model_{};
};
}  // namespace VS::test_support::live_floating_labels
