#include "Field/GimmickPlacement.hpp"

#include <bit>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Field {
namespace {

constexpr std::int32_t kHeightUnit = 128;
constexpr unsigned kHalfwordBits = 16;

std::int32_t PackPosition(std::int16_t low, std::int16_t high) {
  const std::uint32_t word =
      static_cast<std::uint16_t>(low) |
      (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high))
       << kHalfwordBits);
  return std::bit_cast<std::int32_t>(word);
}

}  // namespace

// decomp: BATTLE.PRG func_800A0024 (src/BATTLE/BATTLE.PRG/30DB0.c:3654).
// CPU 800A0054-800A00E8: stationary checks, wrapped height and packed stores.
std::optional<GimmickPlacementSnapshot> GimmickPlacement(
    std::int32_t slot, GimmickPlacementHost& host) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= host.GimmickSlots()) {
    return std::nullopt;
  }
  const auto state = host.Gimmick(static_cast<std::size_t>(slot));
  if (!state || state->attached || state->falling ||
      state->movement_mode != 0) {
    return std::nullopt;
  }
  const auto difference =
      Core::Math::WrappingAdd(host.SupportHeight(*state),
                              -static_cast<std::int32_t>(state->position_y));
  return GimmickPlacementSnapshot{
      state->tile_x,
      static_cast<std::uint8_t>(difference / kHeightUnit),
      state->tile_z,
      state->orientation,
      PackPosition(state->position_x, state->position_y),
      PackPosition(state->position_z, state->position_tail)};
}

}  // namespace VS::Field
