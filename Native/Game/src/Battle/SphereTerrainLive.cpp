#include "Battle/SphereTerrainLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "Battle/Sphere.hpp"
#include "Battle/SphereTerrain.hpp"
#include "Battle/TargetVolume.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

using VS::Core::Gte::LoadV0;
using VS::Core::Gte::MATRIX;
using VS::Core::Gte::Rtps;
using VS::Core::Gte::StoreSxy2;
using VS::Core::Gte::StoreSz3Otz;
using VS::Core::Gte::SVECTOR;

using VS::Legacy::GpuHandle;

namespace {
constexpr int kCoordinateBits = 16;

std::uint32_t ScreenPosition(int x, int y) {
  return static_cast<std::uint16_t>(x) |
         (static_cast<std::uint32_t>(y) << kCoordinateBits);
}

// Projects through the current GTE state and links translucent lines into the
// frame ordering table at the slice depth.
class LiveCanvas final : public VS::Battle::SphereTerrainCanvas {
 public:
  [[nodiscard]] VS::Battle::SphereScreenPoint Project(int x, int y,
                                                      int z) override {
    const SVECTOR vertex{static_cast<std::int16_t>(x),
                         static_cast<std::int16_t>(y),
                         static_cast<std::int16_t>(z), 0};
    VS::Core::Gte::Word xy = 0;
    VS::Core::Gte::Word depth = 0;
    LoadV0(&vertex);
    Rtps();
    StoreSxy2(&xy);
    StoreSz3Otz(&depth);
    return {static_cast<std::int16_t>(xy),
            static_cast<std::int16_t>(xy >> kCoordinateBits),
            static_cast<int>(depth)};
  }

  void DrawLine(int y, int begin, int end, int depth) override {
    constexpr std::uint32_t kPacketSize = 4U << 24;
    constexpr std::uint32_t kAdditiveDrawMode = 0xE1000220;
    constexpr std::uint32_t kTranslucentLine = 0x42000000;
    constexpr std::uint32_t kLinkMask = 0xFFFFFF;
    constexpr int kGreenShift = 8;
    constexpr std::size_t kLinePacketWords = 5;
    auto* packet = static_cast<std::uint32_t*>(
        VS::Render::FrameScratchpad().primitive_cursor);
    auto* ordering = static_cast<std::uint32_t*>(
                         VS::Render::FrameScratchpad().ordering_table) +
                     depth;
    const std::array<std::uint32_t, kLinePacketWords> words{
        (*ordering & kLinkMask) | kPacketSize, kAdditiveDrawMode,
        kTranslucentLine | (static_cast<unsigned>(y / 2) << kGreenShift),
        ScreenPosition(begin, y), ScreenPosition(end, y)};
    std::copy(words.begin(), words.end(), packet);
    *ordering = GpuHandle(packet);
    VS::Render::FrameScratchpad().primitive_cursor = packet + words.size();
  }
};
}  // namespace

namespace VS::Battle {
void DrawLiveSphereTerrainIntersection(const SVECTOR& position,
                                       const MATRIX& shape, int scale) {
  if (VS::Battle::targetSphere == nullptr ||
      VS::Render::FrameScratchpad().primitive_cursor == nullptr ||
      VS::Render::FrameScratchpad().ordering_table == nullptr) {
    constexpr const char* kMessage =
        "sphere terrain drawing requires initialized targeting state";
    VS::Core::LogError("sphere", "{}", kMessage);
    throw std::runtime_error(kMessage);
  }
  static_assert(offsetof(VS::Battle::TargetSphereRecord, wire_edges) ==
                offsetof(VS::Battle::TargetSphereSetupView, wire_edges));
  const auto& sphere = *reinterpret_cast<const VS::Battle::TargetSphereRecord*>(
      VS::Battle::targetSphere);
  static_assert(sizeof sphere.wire_edges >=
                kSphereWireEdges * sizeof(std::uint16_t));
  std::array<std::uint16_t, kSphereWireEdges> wire_edges{};
  std::memcpy(wire_edges.data(), sphere.wire_edges, sizeof wire_edges);
  const SphereTerrainTables tables{
      std::span<const int, kSphereTerrainCells>(sphere.terrain_heights),
      wire_edges,
      std::span<const std::uint8_t, kSphereTerrainCells>(
          sphere.terrain_row_flags),
      sphere.camera_yaw};
  LiveCanvas canvas;
  DrawSphereTerrainIntersection(
      {position.vx, position.vy, position.vz},
      {shape.m[0][0], shape.m[2][0], shape.m[0][2], shape.m[2][2]}, scale,
      tables, canvas);
}
}  // namespace VS::Battle
