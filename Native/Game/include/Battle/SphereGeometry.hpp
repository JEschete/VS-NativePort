#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/TargetingDraw.hpp"

namespace VS::Battle {
using SphereArcBasis = std::array<TargetWorldPoint, kTargetWireAxes>;

struct SphereWireframeState {
  std::int16_t phase_a = 0;
  std::int16_t phase_b = 0;
  std::int16_t phase_c = 0;
  std::int16_t shape = 0;
};

struct SphereGeometryTables {
  std::span<const std::int16_t> arc_bases;
  std::span<const std::int16_t> mirrored_vertices;
  std::span<const std::uint8_t> shape_command_starts;
  std::span<const std::uint16_t> commands;
  std::span<const std::int16_t> sine_samples;
};

struct SphereConeAppendResult {
  std::size_t next_index = 0;
  SphereArcBasis basis{};
};

struct SphereWireframeBuildResult {
  SphereWireframeState state{};
  std::size_t vertex_count = 0;
};

[[nodiscard]] Core::Math::Mat3 BuildSphereVolumeTransform(
    const TargetVolumeExtents& extents, std::int32_t angle) noexcept;

[[nodiscard]] std::optional<std::size_t> AppendSphereArcVertices(
    const SphereArcBasis& basis, std::span<const std::int16_t> sine_samples,
    std::int32_t phase, std::int32_t segments,
    std::span<TargetWireVertex> output, std::size_t index) noexcept;

[[nodiscard]] std::optional<std::size_t> AppendSphereCircleVertices(
    const SphereArcBasis& basis, std::int32_t angle,
    std::span<TargetWireVertex> output, std::size_t index) noexcept;

[[nodiscard]] std::optional<SphereConeAppendResult> AppendSphereConeVertices(
    std::int32_t step, std::int32_t radius, std::span<TargetWireVertex> output,
    std::size_t index) noexcept;

[[nodiscard]] std::optional<SphereWireframeBuildResult> BuildSphereWireframe(
    std::int32_t type, SphereWireframeState state,
    const SphereGeometryTables& tables,
    std::span<TargetWireVertex> output) noexcept;
}  // namespace VS::Battle
