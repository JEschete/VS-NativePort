#include <cstddef>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/ActionResults.hpp"

namespace {
namespace battle = VS::Battle;

constexpr int kTarget = 2;
constexpr int kOther = 3;
constexpr std::uint8_t kHitImpact = 0;
constexpr std::uint8_t kBlockImpact = 3;
constexpr std::uint8_t kAlternateBlockImpact = 4;
constexpr std::uint8_t kRecoveryImpact = 5;
constexpr std::uint8_t kNeutralImpact = 2;

class RecordingEffects final : public battle::ActionTargetEffects {
 public:
  std::vector<battle::ActionTargetHit> hits;
  std::vector<std::string> calls;
  bool area = false;
  bool has_record = true;

  void QueueWorldLabel() override { calls.emplace_back("label"); }
  std::size_t HitCount() override { return hits.size(); }
  battle::ActionTargetHit Hit(std::size_t index) override {
    return hits[index];
  }
  bool IsAreaAction() override {
    calls.emplace_back("area");
    return area;
  }
  void ApplyHit(std::size_t index) override {
    calls.push_back("apply " + std::to_string(index));
  }
  void PlayDamageReaction(int actor_id) override {
    calls.push_back("damage " + std::to_string(actor_id));
  }
  bool HasActorRecord(int /*actor_id*/) override { return has_record; }
  void PlayDefense(int actor_id) override {
    calls.push_back("defense " + std::to_string(actor_id));
  }
  void PlayRecoveryPose() override { calls.emplace_back("recover"); }
};

battle::ActionTargetHit HitOn(int actor, std::uint8_t impact) {
  return {
      .targets_position = false, .target_actor = actor, .impact_kind = impact};
}

using Calls = std::vector<std::string>;
}  // namespace

TEST_CASE("Typed action target effects label only tagged world targets",
          "[battle][action][typed]") {
  RecordingEffects effects;
  effects.hits = {HitOn(kTarget, kHitImpact)};
  for (const int actor : {0x80, 0x8F, 0x90, 0x180, 0x280, 0x10}) {
    battle::ApplyActionTargetEffects(actor, effects);
  }
  CHECK(effects.calls == Calls{"label", "label", "label"});
}

TEST_CASE("Typed action target effects apply only hits on the actor",
          "[battle][action][typed]") {
  RecordingEffects effects;
  effects.hits = {HitOn(kTarget, kNeutralImpact),
                  HitOn(kOther, kHitImpact),
                  {.targets_position = true, .target_actor = kTarget},
                  HitOn(kTarget, kNeutralImpact)};
  battle::ApplyActionTargetEffects(kTarget, effects);
  CHECK(effects.calls == Calls{"apply 0", "apply 3"});
}

TEST_CASE("Typed action target effects let hits and recovery override blocks",
          "[battle][action][typed]") {
  RecordingEffects effects;
  effects.hits = {HitOn(kTarget, kBlockImpact), HitOn(kTarget, kHitImpact),
                  HitOn(kTarget, kAlternateBlockImpact)};
  battle::ApplyActionTargetEffects(kTarget, effects);
  CHECK(effects.calls ==
        Calls{"area", "apply 0", "apply 1", "apply 2", "damage 2"});

  RecordingEffects recover;
  recover.hits = {HitOn(0, kHitImpact), HitOn(0, kRecoveryImpact)};
  battle::ApplyActionTargetEffects(0, recover);
  CHECK(recover.calls == Calls{"apply 0", "apply 1", "recover"});
}

TEST_CASE("Typed action target effects block only single-target actions",
          "[battle][action][typed]") {
  RecordingEffects effects;
  effects.hits = {HitOn(kTarget, kAlternateBlockImpact)};
  battle::ApplyActionTargetEffects(kTarget, effects);
  CHECK(effects.calls == Calls{"area", "apply 0", "defense 2"});

  RecordingEffects area;
  area.area = true;
  area.hits = {HitOn(kTarget, kBlockImpact)};
  battle::ApplyActionTargetEffects(kTarget, area);
  CHECK(area.calls == Calls{"area", "apply 0"});

  RecordingEffects gimmick;
  gimmick.has_record = false;
  gimmick.hits = {HitOn(kTarget, kBlockImpact)};
  battle::ApplyActionTargetEffects(kTarget, gimmick);
  CHECK(gimmick.calls == Calls{"area", "apply 0"});
}

TEST_CASE("Typed action target effects pose only Ashley on recovery",
          "[battle][action][typed]") {
  RecordingEffects effects;
  effects.hits = {HitOn(kTarget, kRecoveryImpact)};
  battle::ApplyActionTargetEffects(kTarget, effects);
  CHECK(effects.calls == Calls{"apply 0"});
}
