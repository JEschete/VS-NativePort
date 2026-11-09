#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Field {

inline constexpr std::size_t kGimmickReactionAxes = 3;

struct GimmickReactionState {
  std::uint8_t frame = 0;
  std::uint8_t slot = 0;
  std::uint8_t material = 0;
  std::int16_t mode = 0;
  std::int16_t orientation = 0;
  std::array<std::int16_t, kGimmickReactionAxes> position{};
  std::array<std::int16_t, kGimmickReactionAxes> rotation{};
  std::array<std::int16_t, kGimmickReactionAxes> scale{};
  std::int16_t vertical_delta = 0;
};

struct GimmickSpinFrame {
  std::int8_t height = 0;
  std::int8_t pitch = 0;
  std::int8_t yaw = 0;
  std::int8_t roll = 0;
};

struct GimmickTiltFrame {
  std::int8_t depth = 0;
  std::int8_t angle = 0;
};

struct GimmickRollFrame {
  std::int8_t height = 0;
  std::int8_t displacement = 0;
  std::uint8_t angle = 0;
};

struct GimmickSquashFrame {
  std::int8_t height = 0;
  std::array<std::int8_t, kGimmickReactionAxes> scale{};
};

struct GimmickReactionTables {
  std::span<const GimmickSpinFrame> spin;
  std::span<const GimmickTiltFrame> tilt;
  std::span<const std::int8_t> bounce;
  std::span<const GimmickRollFrame> roll;
  std::span<const GimmickSquashFrame> squash;
};

class GimmickReactionHost {
 public:
  virtual ~GimmickReactionHost() = default;
  virtual std::optional<GimmickReactionState> State() = 0;
  virtual bool Store(const GimmickReactionState& state) = 0;
  virtual bool ResetOrientation(std::uint8_t slot, std::uint8_t material) = 0;
  virtual bool BeginBreaking() = 0;
  virtual bool Release(std::uint8_t slot) = 0;
};

enum class GimmickReactionResult : std::uint8_t {
  kAdvanced,
  kComplete,
  kRefused,
};

GimmickReactionResult UpdateGimmickReactionAnimation(
    const GimmickReactionTables& tables, GimmickReactionHost& host);

}  // namespace VS::Field
