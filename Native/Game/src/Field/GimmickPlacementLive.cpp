#include "Field/GimmickPlacementLive.h"

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Field/ActorCollisionLive.hpp"
#include "Field/GimmickPlacement.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Field/Gimmicks.h"
}

namespace {

constexpr std::uint8_t kNoParent = 0xFF;

class LiveGimmickPlacementHost final : public VS::Field::GimmickPlacementHost {
 public:
  std::size_t GimmickSlots() const override {
    return std::size(Field_gimmickInstances);
  }

  std::optional<VS::Field::GimmickPlacementState> Gimmick(
      std::size_t slot) const override {
    if (slot >= GimmickSlots() || Field_gimmickInstances[slot] == nullptr) {
      return std::nullopt;
    }
    const auto& gimmick = *Field_gimmickInstances[slot];
    if (gimmick.parent != kNoParent) {
      return VS::Field::GimmickPlacementState{.attached = true};
    }
    if (gimmick.objectInfo.isFalling != 0) {
      return VS::Field::GimmickPlacementState{.falling = true};
    }
    if (gimmick.mode != 0) {
      return VS::Field::GimmickPlacementState{.movement_mode = gimmick.mode};
    }
    return VS::Field::GimmickPlacementState{
        .tile_x = static_cast<std::uint8_t>(gimmick.tileX),
        .tile_z = static_cast<std::uint8_t>(gimmick.tileZ),
        .orientation = static_cast<std::uint8_t>(gimmick.objectInfo.material),
        .position_x = gimmick.positionX,
        .position_y = gimmick.positionY,
        .position_z = gimmick.positionZ,
        .position_tail = gimmick.reserved22};
  }

  std::int32_t SupportHeight(
      const VS::Field::GimmickPlacementState& state) override {
    return VS::Field::LiveSampleSupportHeightAt(
        state.position_x, state.position_y, state.position_z, 0, 0, 0);
  }
};

}  // namespace

// decomp: BATTLE.PRG func_800A0024 (src/BATTLE/BATTLE.PRG/30DB0.c:3654).
// Original 800A0034 tests inherited s0 before the slot load at 800A004C.
int Field_GimmickPlacementEntry(int gimmick_slot,
                                Field_GimmickSnapshot* output_snapshot) {
  if (output_snapshot == nullptr || gimmick_slot < 0 ||
      static_cast<std::size_t>(gimmick_slot) >=
          std::size(Field_gimmickInstances)) {
    Core_Warnf("field", "Gimmick snapshot refused: slot=%d output=%p",
               gimmick_slot, static_cast<void*>(output_snapshot));
    return -1;
  }
  LiveGimmickPlacementHost host;
  const auto snapshot = VS::Field::GimmickPlacement(gimmick_slot, host);
  if (!snapshot) {
    return -1;
  }
  *output_snapshot = {snapshot->tile_x,      snapshot->height_above_ground,
                      snapshot->tile_z,      snapshot->orientation,
                      snapshot->position_xy, snapshot->position_z_tail};
  return 0;
}
