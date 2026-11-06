#include "Field/GimmickPlacementLive.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Core/Math/ScalarMath.hpp"
#include "Field/ActorCollisionLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Field/Gimmicks.h"
}

namespace {

constexpr std::uint8_t kNoParent = 0xFF;
constexpr int kHeightUnit = 128;

std::int32_t PackPosition(std::int16_t low, std::int16_t high) {
  constexpr unsigned kHalfwordBits = 16;
  const std::uint32_t word =
      static_cast<std::uint16_t>(low) |
      (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high))
       << kHalfwordBits);
  return std::bit_cast<std::int32_t>(word);
}

}  // namespace

// decomp: BATTLE.PRG func_800A0024 (src/BATTLE/BATTLE.PRG/30DB0.c:3654).
// Original 800A0034 tests inherited s0 before the slot load at 800A004C.
int Field_GimmickPlacement(int gimmick_slot,
                           Field_GimmickSnapshot* output_snapshot) {
  if (output_snapshot == nullptr || gimmick_slot < 0 ||
      static_cast<std::size_t>(gimmick_slot) >=
          std::size(Field_gimmickInstances)) {
    Core_Warnf("field", "Gimmick snapshot refused: slot=%d output=%p",
               gimmick_slot, static_cast<void*>(output_snapshot));
    return -1;
  }
  const auto* gimmick = Field_gimmickInstances[gimmick_slot];
  if (gimmick == nullptr || gimmick->parent != kNoParent ||
      gimmick->objectInfo.isFalling != 0 || gimmick->mode != 0) {
    return -1;
  }
  const int support = VS::Field::LiveSampleSupportHeightAt(
      gimmick->positionX, gimmick->positionY, gimmick->positionZ, 0, 0, 0);
  const auto difference = VS::Core::Math::WrappingAdd(
      support, -static_cast<std::int32_t>(gimmick->positionY));
  const Field_GimmickSnapshot snapshot{
      static_cast<std::uint8_t>(gimmick->tileX),
      static_cast<std::uint8_t>(difference / kHeightUnit),
      static_cast<std::uint8_t>(gimmick->tileZ),
      static_cast<std::uint8_t>(gimmick->objectInfo.material),
      PackPosition(gimmick->positionX, gimmick->positionY),
      PackPosition(gimmick->positionZ, gimmick->reserved22)};
  *output_snapshot = snapshot;
  return 0;
}
