#include "Effects/AnalyzeGrid.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/Grid.hpp"
#include "Effects/Templates/SurfaceCore.hpp"

namespace VS::Effects {
namespace {
namespace math = Core::Math;
namespace grid = Templates;

constexpr unsigned kByteBits = 8;
constexpr unsigned kHalfwordBits = 16;
constexpr std::size_t kColorWord = 2;
constexpr std::size_t kFirstCornerWord = 3;
constexpr std::size_t kCornerWords = 2;
constexpr std::size_t kPaletteWord = 4;
constexpr std::size_t kTexturePageWord = 6;

class GridHostAdapter final : public grid::GridHost {
 public:
  explicit GridHostAdapter(AnalyzeGridHost& host) : host_(host) {}

  [[nodiscard]] bool Accepted() const { return accepted_; }

  std::optional<grid::SurfaceDefinition> CreateDefinition(
      std::uint8_t definition) override {
    auto result = host_.CreateDefinition({definition});
    accepted_ = result.has_value();
    return result;
  }
  void Create(const AnalyzeGridState& state,
              std::optional<grid::SurfaceAnimation> animation) override {
    accepted_ = !animation.has_value() && host_.Create(state);
  }
  std::optional<AnalyzeGridState> State() const override {
    return host_.State();
  }
  void StoreState(const AnalyzeGridState& state) override {
    host_.StoreState(state);
  }
  std::optional<grid::SurfaceTickInputs> BeginTick(
      std::uint8_t definition) override {
    return host_.BeginTick({definition});
  }
  std::int32_t SampleCurve(std::int32_t curve, std::int32_t age) override {
    return host_.SampleCurve(static_cast<std::uint8_t>(curve), age);
  }
  void PublishScratch(const grid::SurfaceScratch& values) override {
    host_.PublishScratch(values);
  }
  void BeginProjection(bool effect_view) override {
    host_.BeginProjection(effect_view);
  }
  grid::RingProjection Project(grid::RingPoint point) override {
    const AnalyzeGridProjection projected = host_.Project(point);
    const auto x = static_cast<std::uint16_t>(projected.screen.x);
    const auto y = static_cast<std::uint16_t>(projected.screen.y);
    return {.screen = x | (static_cast<std::uint32_t>(y) << kHalfwordBits),
            .depth = projected.depth};
  }
  void Submit(std::span<const std::uint32_t> packet,
              std::int32_t depth) override {
    if (packet.size() != grid::kGridPacketWords) {
      accepted_ = false;
      return;
    }
    const std::uint32_t color = packet[kColorWord];
    AnalyzeGridQuad quad{
        .color = {.r = static_cast<std::uint8_t>(color),
                  .g = static_cast<std::uint8_t>(color >> kByteBits),
                  .b = static_cast<std::uint8_t>(color >> kHalfwordBits)},
        .palette =
            static_cast<std::uint16_t>(packet[kPaletteWord] >> kHalfwordBits),
        .texture_page = static_cast<std::uint16_t>(packet[kTexturePageWord] >>
                                                   kHalfwordBits),
        .depth = depth};
    for (std::size_t corner = 0; corner < quad.corners.size(); ++corner) {
      const std::size_t word = kFirstCornerWord + corner * kCornerWords;
      quad.corners.at(corner) = {
          math::WrapSigned16(packet[word]),
          math::WrapSigned16(packet[word] >> kHalfwordBits)};
      quad.texture_coordinates.at(corner) = {
          static_cast<std::uint8_t>(packet[word + 1]),
          static_cast<std::uint8_t>(packet[word + 1] >> kByteBits)};
    }
    host_.Draw(quad);
  }

  // Grid never animates a texture, emits particles, or samples randomness.
  // Refuse if that shared-kernel contract changes.
  std::span<const std::uint8_t> FactorCurve(std::uint8_t) override {
    accepted_ = false;
    return {};
  }
  std::int32_t Rand() override {
    accepted_ = false;
    return 0;
  }
  std::optional<grid::RingTexture> AdvanceTexture() override {
    accepted_ = false;
    return std::nullopt;
  }
  math::Vec3 TransformByActor(grid::RingPoint) override {
    accepted_ = false;
    return {};
  }
  void EmitParticleEvent(std::uint8_t, math::Vec3) override {
    accepted_ = false;
  }

 private:
  AnalyzeGridHost& host_;
  bool accepted_ = true;
};

bool RunAnalyzeGrid(AnalyzeGridCommand command, AnalyzeGridPlan plan,
                    AnalyzeGridHost& host) {
  if (command == AnalyzeGridCommand::kExpire && !host.State().has_value()) {
    return false;
  }
  GridHostAdapter adapter(host);
  const std::int32_t packed =
      plan.lifetime |
      (static_cast<std::int32_t>(plan.definition.value) << kByteBits);
  const bool keep = grid::Grid(command, packed, adapter);
  return keep && adapter.Accepted();
}
}  // namespace

// decomp: vs_effect_analyze (src/EFFECT/PLG005.BIN/18.c:35).
// Original instructions: PLG005.BIN 800F9818-800FA368 (exclusive).
bool Plg005::UpdateAnalyzeGrid(AnalyzeGridCommand command, AnalyzeGridPlan plan,
                               AnalyzeGridHost& host) {
  return RunAnalyzeGrid(command, plan, host);
}

// decomp: func_800F9818 (src/EFFECT/PLG054.BIN/18.c:3).
// Its own 800F9818-800FA368 body is byte-identical to PLG005's.
bool Plg054::UpdateAnalyzeGrid(AnalyzeGridCommand command, AnalyzeGridPlan plan,
                               AnalyzeGridHost& host) {
  return RunAnalyzeGrid(command, plan, host);
}

// decomp: func_800F9818 (src/EFFECT/PLG055.BIN/18.c:3).
// Independently read PLG055.BIN 800F9818-800FA368, not its shared native name.
bool Plg055::UpdateAnalyzeGrid(AnalyzeGridCommand command, AnalyzeGridPlan plan,
                               AnalyzeGridHost& host) {
  return RunAnalyzeGrid(command, plan, host);
}

// decomp: func_800FE3EC (src/EFFECT/PLG074.BIN/F8.c:23).
// 800FE3EC-800FEF3C differs only by internal jumps and the table at 800F98E0.
bool Plg074::UpdateAnalyzeGrid(AnalyzeGridCommand command, AnalyzeGridPlan plan,
                               AnalyzeGridHost& host) {
  return RunAnalyzeGrid(command, plan, host);
}
}  // namespace VS::Effects
