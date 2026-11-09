#include "Field/GimmickReactionsLive.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "Core/Math/ScalarMath.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/GimmickReactionsLive.hpp"
#include "Field/GimmickResourcesLive.hpp"
#include "Field/InteractionTargetLive.hpp"

extern "C" {
#include "Field/GimmickMotion.h"
#include "Field/Gimmicks.h"
}

namespace {
constexpr std::size_t kLongTableRows = 16;
constexpr std::size_t kTiltTableRows = 12;
constexpr std::size_t kBounceTableRows = 4;
constexpr std::uint8_t kBreakingMotionState = 12;
using Record = Field_GimmickInstance;
using MotionView = Field_GimmickMotionView;
static_assert(offsetof(MotionView, objectId) == offsetof(Record, slot));
static_assert(offsetof(MotionView, mode) == offsetof(Record, mode));
static_assert(offsetof(MotionView, x) == offsetof(Record, positionX));
static_assert(offsetof(MotionView, rotationX) == offsetof(Record, rotationX));
static_assert(offsetof(MotionView, savedState) ==
              offsetof(Record, basePalette));
static_assert(offsetof(MotionView, state) == offsetof(Record, pulsedPalette));
static_assert(offsetof(MotionView, frame) ==
              offsetof(Record, objectInfo) +
                  offsetof(Field_GimmickObjectInfo, supportIndex));

std::int8_t SignedByte(std::uint8_t value) {
  return std::bit_cast<std::int8_t>(value);
}

struct LiveReactionTables {
  std::array<VS::Field::GimmickSpinFrame, kLongTableRows> spin{};
  std::array<VS::Field::GimmickTiltFrame, kTiltTableRows> tilt{};
  std::array<std::int8_t, kBounceTableRows> bounce{};
  std::array<VS::Field::GimmickRollFrame, kLongTableRows> roll{};
  std::array<VS::Field::GimmickSquashFrame, kLongTableRows> squash{};
  LiveReactionTables() {
    for (std::size_t frame = 0; frame < kLongTableRows; ++frame) {
      spin[frame] = {SignedByte(Field_kGimmickSpinHeightSteps[frame]),
                     SignedByte(Field_kGimmickSpinPitchSteps[frame]),
                     SignedByte(Field_kGimmickSpinYawSteps[frame]),
                     SignedByte(Field_kGimmickSpinRollSteps[frame])};
      roll[frame] = {SignedByte(Field_kGimmickRollHeightSteps[frame]),
                     SignedByte(Field_kGimmickRollDisplacementSteps[frame]),
                     Field_kGimmickRollAngleSteps[frame]};
      squash[frame] = {SignedByte(Field_kGimmickSquashHeightSteps[frame]),
                       {SignedByte(Field_kGimmickSquashScaleX[frame]),
                        SignedByte(Field_kGimmickSquashScaleY[frame]),
                        SignedByte(Field_kGimmickSquashScaleZ[frame])}};
    }
    for (std::size_t frame = 0; frame < kTiltTableRows; ++frame) {
      tilt[frame] = {SignedByte(Field_kGimmickTiltDepthSteps[frame]),
                     SignedByte(Field_kGimmickTiltAngleSteps[frame])};
    }
    for (std::size_t frame = 0; frame < kBounceTableRows; ++frame) {
      bounce[frame] = SignedByte(Field_kGimmickBounceHeightSteps[frame]);
    }
  }
  VS::Field::GimmickReactionTables View() const {
    return {spin, tilt, bounce, roll, squash};
  }
};

class LiveReactionHost final : public VS::Field::GimmickReactionHost {
 public:
  explicit LiveReactionHost(Record* record)
      : record_(record),
        generation_(VS::Field::LiveInteractionTargetDomain().generation) {
    for (std::size_t slot = 0; slot < std::size(Field_gimmickInstances);
         ++slot) {
      if (Field_gimmickInstances[slot] == record_) {
        table_slot_ = slot;
        break;
      }
    }
  }
  std::optional<VS::Field::GimmickReactionState> State() override {
    if (!Valid()) return std::nullopt;
    return VS::Field::GimmickReactionState{
        record_->objectInfo.supportIndex,
        record_->slot,
        record_->objectInfo.material,
        record_->mode,
        record_->orientation,
        {record_->positionX, record_->positionY, record_->positionZ},
        {record_->rotationX, VS::Core::Math::WrapSigned16(record_->rotationY),
         VS::Core::Math::WrapSigned16(record_->rotationZ)},
        {record_->scaleX, record_->scaleY, record_->scaleZ},
        record_->velocityY};
  }
  bool Store(const VS::Field::GimmickReactionState& state) override {
    if (!Valid()) return false;
    record_->objectInfo.supportIndex = state.frame;
    record_->mode = state.mode;
    record_->positionX = state.position[0];
    record_->positionY = state.position[1];
    record_->positionZ = state.position[2];
    record_->rotationX = state.rotation[0];
    record_->rotationY = static_cast<std::uint16_t>(state.rotation[1]);
    record_->rotationZ = static_cast<std::uint16_t>(state.rotation[2]);
    record_->scaleX = state.scale[0];
    record_->scaleY = state.scale[1];
    record_->scaleZ = state.scale[2];
    record_->velocityY = state.vertical_delta;
    return true;
  }
  bool ResetOrientation(std::uint8_t slot, std::uint8_t material) override {
    if (!Valid()) return false;
    static_cast<void>(VS::Field::SetLiveGimmickOrientation(slot, material));
    return Valid();
  }
  bool BeginBreaking() override {
    if (!Valid()) return false;
    static_assert(sizeof(record_->basePalette) ==
                  sizeof(record_->pulsedPalette));
    std::memcpy(record_->pulsedPalette, record_->basePalette,
                sizeof(record_->pulsedPalette));
    record_->motionState = kBreakingMotionState;
    return true;
  }
  bool Release(std::uint8_t slot) override {
    if (!Valid()) return false;
    static_cast<void>(VS::Field::ReleaseLiveGimmickInstance(slot));
    if (!SameRoom() || !VS::Field::IsLiveGimmickPrefixReadable(record_)) {
      return false;
    }
    if (table_slot_ && Field_gimmickInstances[*table_slot_] != nullptr &&
        Field_gimmickInstances[*table_slot_] != record_) {
      return false;
    }
    record_->mode = 0;
    return true;
  }

 private:
  bool SameRoom() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation;
  }
  bool Valid() const {
    return SameRoom() && VS::Field::IsLiveGimmickPrefixReadable(record_) &&
           (!table_slot_ || Field_gimmickInstances[*table_slot_] == record_);
  }
  Record* record_;
  std::uint64_t generation_;
  std::optional<std::size_t> table_slot_;
};
}  // namespace

namespace VS::Field {
GimmickReactionResult UpdateLiveGimmickReactionAnimation(
    Field_GimmickInstance* record) {
  LiveReactionTables tables;
  LiveReactionHost host(record);
  return UpdateGimmickReactionAnimation(tables.View(), host);
}
}  // namespace VS::Field

int Field_UpdateGimmickReactionAnimationEntry(Field_GimmickMotionView* record) {
  const auto result = VS::Field::UpdateLiveGimmickReactionAnimation(
      reinterpret_cast<Field_GimmickInstance*>(record));
  return result == VS::Field::GimmickReactionResult::kComplete ? 0 : 1;
}
