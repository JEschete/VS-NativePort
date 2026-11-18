// Hand-maintained (tools/port/owned.csv).
#include "Battle/FloatingLabelProducersLive.hpp"

#include <bit>
#include <cstdint>
#include <optional>
#include <type_traits>

#include "Actors/SkeletonLive.hpp"
#include "Battle/Feedback.h"
#include "Battle/FloatingLabelProducers.hpp"
#include "Battle/FloatingLabelQueue.hpp"
#include "Battle/FloatingLabelQueueLive.hpp"
#include "Core/BiosRand.h"
#include "Core/Log.hpp"

namespace VS::Battle {
namespace {
static_assert(sizeof(FloatingLabelRecord) == sizeof(Battle_FloatingLabel));
static_assert(std::is_trivially_copyable_v<Battle_FloatingLabel>);
static_assert(sizeof(LabelPoint) == sizeof(SVECTOR));

std::uint64_t refusals = 0;

void Refuse(const char* what) {
  if (std::has_single_bit(++refusals)) {
    VS_LOG_WARN("battle.labels",
                "Floating label producer refused: {} ({} "
                "refusals)",
                what, refusals);
  }
}

class LiveProducerWorld final : public FloatingLabelProducerWorld {
 public:
  std::int16_t LabelCount() override { return LiveFloatingLabelCount(); }
  std::optional<FloatingLabelRecord> Label(std::int32_t index) override {
    if (Battle_floatingLabels == nullptr || index < 0 ||
        index >= kFloatingLabelCapacity) {
      Refuse("label outside the 32-record owner");
      return std::nullopt;
    }
    return std::bit_cast<FloatingLabelRecord>((*Battle_floatingLabels)[index]);
  }
  bool RemoveLabel(std::int32_t index) override {
    RemoveLiveFloatingLabel(index);
    return true;
  }
  std::optional<FloatingLabelRecord> ReserveLabel() override {
    reserved_ = ReserveLiveFloatingLabel();
    return std::bit_cast<FloatingLabelRecord>(*reserved_);
  }
  // The reserved record stays the one reserve returned, as the original's
  // pointer does, even if the owner is replaced in between.
  bool WriteReservedLabel(const FloatingLabelRecord& record) override {
    if (reserved_ == nullptr) {
      Refuse("write without a reserved label");
      return false;
    }
    *reserved_ = std::bit_cast<Battle_FloatingLabel>(record);
    return true;
  }
  bool WriteReservedTextLength(std::uint8_t text_length) override {
    if (reserved_ == nullptr) {
      Refuse("text length without a reserved label");
      return false;
    }
    reserved_->textLength = text_length;
    return true;
  }
  bool CommitReservedLabel() override {
    CommitLiveFloatingLabel(reserved_);
    return true;
  }
  std::int32_t FindBoneTag(std::int32_t actor, std::int32_t tag,
                           std::int32_t kind) override {
    return Actors::FindLiveActorBoneTag(actor, tag, kind);
  }
  std::int32_t BonePosition(std::int32_t actor, std::int32_t bone,
                            std::int32_t endpoint, LabelPoint& point) override {
    auto output = std::bit_cast<SVECTOR>(point);
    const int status =
        Actors::LiveActorBonePosition(actor, bone, &output, endpoint);
    point = std::bit_cast<LabelPoint>(output);
    return status;
  }
  std::int32_t TaggedBonePosition(std::int32_t actor, std::int32_t tag,
                                  std::int32_t endpoint,
                                  LabelPoint& point) override {
    auto output = std::bit_cast<SVECTOR>(point);
    const int status =
        Actors::LiveActorTaggedBonePosition(actor, tag, &output, endpoint);
    point = std::bit_cast<LabelPoint>(output);
    return status;
  }
  std::int32_t ProjectTaggedBone(std::int32_t actor, std::int32_t tag,
                                 std::int32_t endpoint,
                                 LabelPoint& point) override {
    auto output = std::bit_cast<SVECTOR>(point);
    const int status =
        Actors::ProjectLiveActorTaggedBone(actor, tag, &output, endpoint);
    point = std::bit_cast<LabelPoint>(output);
    return status;
  }
  std::int32_t Random() override { return Core_Random_Rand(); }

 private:
  Battle_FloatingLabel* reserved_ = nullptr;
};

int Result(const std::optional<std::int32_t>& result, const char* what) {
  if (!result) {
    Refuse(what);
    return 0;
  }
  return *result;
}
}  // namespace

// decomp: func_80093B68 (src/BATTLE/BATTLE.PRG/2842C.c:1544)
int LiveQueueActorValueLabel(int actor_slot, int limb_tag,
                             unsigned int encoded_amount, int label_flags) {
  LiveProducerWorld world;
  return Result(QueueActorValueLabel(actor_slot, limb_tag, encoded_amount,
                                     label_flags, world),
                "value label");
}

// decomp: vs_battle_renderBattleAbilityTimingResult
// (src/BATTLE/BATTLE.PRG/2842C.c:1643)
int LiveQueueAbilityTimingResult(int timing_result) {
  LiveProducerWorld world;
  return Result(QueueAbilityTimingResult(timing_result, world),
                "ability timing result");
}

// decomp: func_80093F24 (src/BATTLE/BATTLE.PRG/2842C.c:1666)
int LiveQueueChainCount(int chain_count) {
  LiveProducerWorld world;
  return Result(QueueChainCount(chain_count, world), "chain count");
}

// decomp: func_80093FEC (src/BATTLE/BATTLE.PRG/2842C.c:1695)
int LiveQueueActorStatusLabels(int actor_slot, int /*arg1*/, int status_mask,
                               int label_flags) {
  LiveProducerWorld world;
  return Result(
      QueueActorStatusLabels(actor_slot, status_mask, label_flags, world),
      "status labels");
}

// decomp: func_8009406C (src/BATTLE/BATTLE.PRG/2842C.c:1708)
int LiveQueueActorStatusLabel(int actor_slot, int bone_tag, int status_index,
                              int label_flags) {
  LiveProducerWorld world;
  return Result(QueueActorStatusLabel(actor_slot, bone_tag, status_index,
                                      label_flags, world),
                "status label");
}
}  // namespace VS::Battle
