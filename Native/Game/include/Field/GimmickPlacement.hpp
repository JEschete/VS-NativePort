#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Field {

struct GimmickPlacementState {
  bool attached = false;
  bool falling = false;
  std::int16_t movement_mode = 0;
  std::uint8_t tile_x = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t orientation = 0;
  std::int16_t position_x = 0;
  std::int16_t position_y = 0;
  std::int16_t position_z = 0;
  std::int16_t position_tail = 0;
};

struct GimmickPlacementSnapshot {
  std::uint8_t tile_x = 0;
  std::uint8_t height_above_ground = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t orientation = 0;
  std::int32_t position_xy = 0;
  std::int32_t position_z_tail = 0;

  friend bool operator==(const GimmickPlacementSnapshot&,
                         const GimmickPlacementSnapshot&) = default;
};

class GimmickPlacementHost {
 public:
  virtual ~GimmickPlacementHost() = default;
  virtual std::size_t GimmickSlots() const = 0;
  virtual std::optional<GimmickPlacementState> Gimmick(
      std::size_t slot) const = 0;
  virtual std::int32_t SupportHeight(const GimmickPlacementState& state) = 0;
};

std::optional<GimmickPlacementSnapshot> GimmickPlacement(
    std::int32_t slot, GimmickPlacementHost& host);

}  // namespace VS::Field
