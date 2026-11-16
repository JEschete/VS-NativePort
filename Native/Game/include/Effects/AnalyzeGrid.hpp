#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/Grid.hpp"

namespace VS::Effects {
using AnalyzeGridCommand = Templates::SurfaceCommand;
using AnalyzeGridState = Templates::GridState;

struct AnalyzeGridDefinitionId {
  std::uint8_t value{};
};

struct AnalyzeGridPlan {
  std::uint8_t lifetime{};
  AnalyzeGridDefinitionId definition{};
};

struct AnalyzeGridProjection {
  Core::Math::Vec2 screen{};
  std::int32_t depth{};
};

struct AnalyzeGridQuad {
  std::array<Core::Math::Vec2, Templates::kRingQuadCorners> corners{};
  Templates::RingColor color{};
  Templates::RingQuadUvs texture_coordinates{};
  std::uint16_t palette{};
  std::uint16_t texture_page{};
  std::int32_t depth{};
};

// The owned grid kernel supplies the lattice, sampling and lifetime rules.
// Its packet representation stays private; callers supply points and receive
// quads, without scratch addresses, ordering-table links or register state.
class AnalyzeGridHost {
 public:
  virtual ~AnalyzeGridHost() = default;
  [[nodiscard]] virtual std::optional<Templates::SurfaceDefinition>
  CreateDefinition(AnalyzeGridDefinitionId definition) = 0;
  [[nodiscard]] virtual bool Create(const AnalyzeGridState& state) = 0;
  [[nodiscard]] virtual std::optional<AnalyzeGridState> State() const = 0;
  virtual void StoreState(const AnalyzeGridState& state) = 0;
  [[nodiscard]] virtual std::optional<Templates::SurfaceTickInputs> BeginTick(
      AnalyzeGridDefinitionId definition) = 0;
  [[nodiscard]] virtual std::int32_t SampleCurve(std::uint8_t curve,
                                                 std::int32_t age) = 0;
  virtual void PublishScratch(const Templates::SurfaceScratch& values) = 0;
  virtual void BeginProjection(bool effect_view) = 0;
  [[nodiscard]] virtual AnalyzeGridProjection Project(
      Templates::RingPoint point) = 0;
  virtual void Draw(const AnalyzeGridQuad& quad) = 0;
};

namespace Plg005 {
bool UpdateAnalyzeGrid(AnalyzeGridCommand command, AnalyzeGridPlan plan,
                       AnalyzeGridHost& host);
}
namespace Plg054 {
bool UpdateAnalyzeGrid(AnalyzeGridCommand command, AnalyzeGridPlan plan,
                       AnalyzeGridHost& host);
}
namespace Plg055 {
bool UpdateAnalyzeGrid(AnalyzeGridCommand command, AnalyzeGridPlan plan,
                       AnalyzeGridHost& host);
}
namespace Plg074 {
bool UpdateAnalyzeGrid(AnalyzeGridCommand command, AnalyzeGridPlan plan,
                       AnalyzeGridHost& host);
}
}  // namespace VS::Effects
