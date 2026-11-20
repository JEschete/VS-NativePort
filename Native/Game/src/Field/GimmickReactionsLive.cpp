#include "Field/GimmickReactionsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "Core/Math/ScalarMath.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/GimmickResourcesLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/InteractionTargetLive.hpp"

namespace {
constexpr std::size_t kLongTableRows = 16;
constexpr std::size_t kTiltTableRows = 12;
constexpr std::size_t kBounceTableRows = 4;
constexpr std::uint8_t kBreakingMotionState = 12;
using Record = VS::Field::GimmickInstance;

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
      spin[frame] = {SignedByte(VS::Field::kGimmickSpinHeightSteps[frame]),
                     SignedByte(VS::Field::kGimmickSpinPitchSteps[frame]),
                     SignedByte(VS::Field::kGimmickSpinYawSteps[frame]),
                     SignedByte(VS::Field::kGimmickSpinRollSteps[frame])};
      roll[frame] = {
          SignedByte(VS::Field::kGimmickRollHeightSteps[frame]),
          SignedByte(VS::Field::kGimmickRollDisplacementSteps[frame]),
          VS::Field::kGimmickRollAngleSteps[frame]};
      squash[frame] = {SignedByte(VS::Field::kGimmickSquashHeightSteps[frame]),
                       {SignedByte(VS::Field::kGimmickSquashScaleX[frame]),
                        SignedByte(VS::Field::kGimmickSquashScaleY[frame]),
                        SignedByte(VS::Field::kGimmickSquashScaleZ[frame])}};
    }
    for (std::size_t frame = 0; frame < kTiltTableRows; ++frame) {
      tilt[frame] = {SignedByte(VS::Field::kGimmickTiltDepthSteps[frame]),
                     SignedByte(VS::Field::kGimmickTiltAngleSteps[frame])};
    }
    for (std::size_t frame = 0; frame < kBounceTableRows; ++frame) {
      bounce[frame] = SignedByte(VS::Field::kGimmickBounceHeightSteps[frame]);
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
    for (std::size_t slot = 0; slot < std::size(VS::Field::gimmickInstances);
         ++slot) {
      if (VS::Field::gimmickInstances[slot] == record_) {
        table_slot_ = slot;
        break;
      }
    }
  }
  std::optional<VS::Field::GimmickReactionState> State() override {
    if (!Valid()) return std::nullopt;
    return VS::Field::GimmickReactionState{
        record_->object_info.motion_frame,
        record_->slot,
        record_->object_info.material,
        record_->mode,
        record_->orientation,
        {record_->position_x, record_->position_y, record_->position_z},
        {record_->rotation_x, VS::Core::Math::WrapSigned16(record_->rotation_y),
         VS::Core::Math::WrapSigned16(record_->rotation_z)},
        {record_->scale_x, record_->scale_y, record_->scale_z},
        record_->velocity_y};
  }
  bool Store(const VS::Field::GimmickReactionState& state) override {
    if (!Valid()) return false;
    record_->object_info.motion_frame = state.frame;
    record_->mode = state.mode;
    record_->position_x = state.position[0];
    record_->position_y = state.position[1];
    record_->position_z = state.position[2];
    record_->rotation_x = state.rotation[0];
    record_->rotation_y = static_cast<std::uint16_t>(state.rotation[1]);
    record_->rotation_z = static_cast<std::uint16_t>(state.rotation[2]);
    record_->scale_x = state.scale[0];
    record_->scale_y = state.scale[1];
    record_->scale_z = state.scale[2];
    record_->velocity_y = state.vertical_delta;
    return true;
  }
  bool ResetOrientation(std::uint8_t slot, std::uint8_t material) override {
    if (!Valid()) return false;
    static_cast<void>(VS::Field::SetLiveGimmickOrientation(slot, material));
    return Valid();
  }
  bool BeginBreaking() override {
    if (!Valid()) return false;
    static_assert(sizeof(record_->base_palette) ==
                  sizeof(record_->pulsed_palette));
    std::memcpy(record_->pulsed_palette.data(), record_->base_palette.data(),
                sizeof(record_->pulsed_palette));
    record_->motion_state = kBreakingMotionState;
    return true;
  }
  bool Release(std::uint8_t slot) override {
    if (!Valid()) return false;
    static_cast<void>(VS::Field::ReleaseLiveGimmickInstance(slot));
    if (!SameRoom() || !VS::Field::IsLiveGimmickPrefixReadable(record_)) {
      return false;
    }
    if (table_slot_ && VS::Field::gimmickInstances[*table_slot_] != nullptr &&
        VS::Field::gimmickInstances[*table_slot_] != record_) {
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
           (!table_slot_ ||
            VS::Field::gimmickInstances[*table_slot_] == record_);
  }
  Record* record_;
  std::uint64_t generation_;
  std::optional<std::size_t> table_slot_;
};
}  // namespace

namespace VS::Field {
GimmickReactionResult UpdateLiveGimmickReactionAnimation(
    VS::Field::GimmickInstance* record) {
  LiveReactionTables tables;
  LiveReactionHost host(record);
  return UpdateGimmickReactionAnimation(tables.View(), host);
}
}  // namespace VS::Field

int VS::Field::UpdateGimmickReactionAnimationEntry(
    VS::Field::GimmickInstance* record) {
  const auto result = VS::Field::UpdateLiveGimmickReactionAnimation(record);
  return result == VS::Field::GimmickReactionResult::kComplete ? 0 : 1;
}
