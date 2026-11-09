#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Field/GimmickPlacement.hpp"
#include "Field/GimmickReactions.hpp"

namespace VS::Field {
struct GimmickDynamicsState {
  GimmickReactionState reaction;
  std::uint8_t delay = 0;
  std::int16_t rotation_ticks = 0;
  std::array<std::int16_t, kGimmickReactionAxes> rotation_remaining{};
  std::array<std::int16_t, 2> slide{};
  std::uint8_t tile_x = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t parent = kUnattachedGimmick;
  std::uint8_t carrier = kUnattachedGimmick;
  std::uint8_t kind = 0;
  std::uint8_t durability = 0;
  std::uint8_t interaction = 0;
  bool skipped = false;
};

class GimmickDynamicsHost {
 public:
  virtual ~GimmickDynamicsHost() = default;
  virtual std::optional<GimmickDynamicsState> State() = 0;
  virtual bool Store(const GimmickDynamicsState& state) = 0;
  virtual GimmickReactionResult React() = 0;
  virtual bool PointAllowed(const GimmickPlacementPoint& point) = 0;
  virtual std::optional<std::int32_t> SupportHeight(
      const std::array<std::int16_t, kGimmickReactionAxes>& position,
      std::int32_t mode) = 0;
  virtual std::optional<std::uint8_t> TopKind(const GimmickTileQuery& tile) = 0;
  virtual bool AddOccupant() = 0;
  virtual bool ReleaseTile(const GimmickTileQuery& tile) = 0;
  virtual bool PlaySound(
      std::uint16_t effect,
      const std::array<std::int16_t, kGimmickReactionAxes>& position) = 0;
  virtual std::optional<GimmickDynamicsState> Child(std::size_t slot) = 0;
  virtual bool StoreChild(std::size_t slot, std::int16_t height,
                          std::uint8_t carrier) = 0;
};

bool UpdateGimmickMotion(GimmickDynamicsHost& host);
}  // namespace VS::Field
