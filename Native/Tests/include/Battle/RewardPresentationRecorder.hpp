#pragma once

#include <cstdint>

namespace reward_tests {
template <class Interface, class Operation>
class RewardPresentationRecorder : public Interface {
 public:
  void EnableClutAnimation(bool enabled) override {
    RecordPresentation(Operation::kClut, enabled ? 1U : 0U);
  }
  void FreezeActorAnimations() override {
    RecordPresentation(Operation::kFreeze, 0);
  }
  void UnfreezeActorAnimations() override {
    RecordPresentation(Operation::kUnfreeze, 0);
  }
  void PauseRoomAnimations(bool paused) override {
    RecordPresentation(Operation::kRoom, paused ? 1U : 0U);
  }
  void ShowMinimap(bool visible) override {
    RecordPresentation(Operation::kMinimap, visible ? 1U : 0U);
  }
  void DuckAmbience(bool ducked) override {
    RecordPresentation(Operation::kAmbience, ducked ? 1U : 0U);
  }
  void RestoreIdlePose() override { RecordPresentation(Operation::kIdle, 0); }
  void PlayLearnedPose() override { RecordPresentation(Operation::kPose, 0); }
  void SpawnLearnedEffect() override {
    RecordPresentation(Operation::kEffect, 0);
  }
  void PauseMovingPlatforms() override {
    RecordPresentation(Operation::kPlatforms, 0);
  }

 protected:
  virtual void RecordPresentation(Operation operation, std::uint32_t value) = 0;
};
}  // namespace reward_tests
