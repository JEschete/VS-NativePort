#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "Actors/SkeletonLiveState.h"
#include "Battle/FloatingLabelQueueLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Battle/Feedback.h"
#include "Core/BiosRand.h"
#include "Field/RoomRenderingLiveState.h"
#include "Input/ControllerLiveState.h"
}

namespace VS::test_support::live_floating_labels {
using LabelWords = std::array<std::uint32_t, 6>;

inline LabelWords Words(const Battle_FloatingLabel& label) {
  return std::bit_cast<LabelWords>(label);
}

class State {
 public:
  Battle_FloatingLabel labels[1][32]{};
  Battle_FloatingLabel canary{};
  State() {
    Battle_floatingLabels = labels;
    Battle_floatingLabelCount = 0;
  }
  ~State() {
    Battle_floatingLabels = saved_owner_;
    Battle_floatingLabelCount = saved_commands_[0];
    Battle_actorMarkersHidden = saved_commands_[1];
    Battle_abilityTimingCueFrames = saved_commands_[2];
    Battle_statusLabelStepTimer = saved_commands_[3];
    Battle_ashleyStatusLabelsSuppressed = saved_commands_[4];
    Battle_placedLabelCount = saved_commands_[5];
    Battle_actorMarkerSides = saved_sides_;
    Battle_abilityTimingCuePosition = saved_cue_position_;
    std::ranges::copy(saved_markers_, Battle_actorMarkers);
    Core_Random_Seed(saved_random_state_);
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
      label.colorFlags = 128;
      label.textLength = 3;
      label.scaleX = -19;
      label.scaleY = 20;
      std::ranges::fill(label.text, static_cast<signed char>('7'));
    }
  }
  [[nodiscard]] std::span<Battle_FloatingLabel, 32> Labels() {
    return labels[0];
  }

 private:
  decltype(Battle_floatingLabels) saved_owner_ = Battle_floatingLabels;
  std::array<std::int16_t, 6> saved_commands_{
      Battle_floatingLabelCount,           Battle_actorMarkersHidden,
      Battle_abilityTimingCueFrames,       Battle_statusLabelStepTimer,
      Battle_ashleyStatusLabelsSuppressed, Battle_placedLabelCount};
  int saved_sides_ = Battle_actorMarkerSides;
  SVECTOR saved_cue_position_ = Battle_abilityTimingCuePosition;
  std::array<char, 40> saved_markers_ = std::to_array(Battle_actorMarkers);
  unsigned saved_random_state_ = Core_Random_State();
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
    Actors_ShpBone bone{};
    bone.length = 64;
    std::memcpy(model_.data() + sizeof(Actors_ShpModel), &bone, sizeof bone);
    actor.instance.model = reinterpret_cast<Actors_ShpModel*>(model_.data());
    actor.instance.boneCount = 1;
    actor.modelDataSize = static_cast<unsigned>(model_.size());
    actor.instance.position = {32, -64, 96, 0};
    actor.boneMatrices[0].m[0][0] = 4096;
    actor.boneMatrices[0].m[1][1] = 4096;
    actor.boneMatrices[0].m[2][2] = 4096;
    Actors_boneMatrixScratch = actor.boneMatrices[0];
    Actors_actorRecords[kActor] = &actor;
    Input_settings.weaponStatChange = 1;
  }
  ~Producers() {
    Actors_actorRecords[kActor] = saved_actor_;
    Actors_boneMatrixScratch = saved_matrix_;
    Input_settings = saved_settings_;
    Field_areRoomUpdatesPaused = saved_pause_;
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
        return Battle_QueueActorValueLabel(kActor, -1,
                                           static_cast<unsigned>(value), 0);
      case 2:
        return Battle_QueueAbilityTimingResult(value);
      case 3:
        return Battle_QueueChainCount(value);
      case 4:
        return Battle_QueueActorStatusLabel(kActor, 251, value % 29, 0);
      case 5:
        return Battle_QueueEquipmentStatChange(240, 0, value);
      default:
        return -1;
    }
  }

 private:
  static constexpr int kActor = 1;
  Actors_ActorRecord* saved_actor_ = Actors_actorRecords[kActor];
  MATRIX saved_matrix_ = Actors_boneMatrixScratch;
  decltype(Input_settings) saved_settings_ = Input_settings;
  decltype(Field_areRoomUpdatesPaused) saved_pause_ =
      Field_areRoomUpdatesPaused;
  alignas(Actors_ShpModel) std::array<
      std::byte, sizeof(Actors_ShpModel) + sizeof(Actors_ShpBone)> model_{};
};
}  // namespace VS::test_support::live_floating_labels
