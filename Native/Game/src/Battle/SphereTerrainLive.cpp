#include "Battle/SphereTerrainLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "Battle/SphereTerrain.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Sphere.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

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
    Core_GteWord xy = 0;
    Core_GteWord depth = 0;
    Core_Gte_LoadV0(&vertex);
    Core_Gte_Rtps();
    Core_Gte_StoreSxy2(&xy);
    Core_Gte_StoreSz3Otz(&depth);
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
    auto* packet = static_cast<u_long*>(Render_frameScratch.primitiveCursor);
    auto* ordering =
        static_cast<u_long*>(Render_frameScratch.orderingTable) + depth;
    const std::array<u_long, kLinePacketWords> words{
        (*ordering & kLinkMask) | kPacketSize, kAdditiveDrawMode,
        kTranslucentLine | (static_cast<unsigned>(y / 2) << kGreenShift),
        ScreenPosition(begin, y), ScreenPosition(end, y)};
    std::copy(words.begin(), words.end(), packet);
    *ordering = LegacyGpu_Handle(packet);
    Render_frameScratch.primitiveCursor = packet + words.size();
  }
};
}  // namespace

namespace VS::Battle {
void DrawLiveSphereTerrainIntersection(const SVECTOR& position,
                                       const MATRIX& shape, int scale) {
  if (Battle_targetSphere == nullptr ||
      Render_frameScratch.primitiveCursor == nullptr ||
      Render_frameScratch.orderingTable == nullptr) {
    constexpr const char* kMessage =
        "sphere terrain drawing requires initialized targeting state";
    VS_LOG_ERROR("sphere", "{}", kMessage);
    throw std::runtime_error(kMessage);
  }
  static_assert(offsetof(Battle_TargetSphere, wireEdges) ==
                offsetof(Battle_TargetSphereSetupView, wireEdges));
  const auto& sphere =
      *reinterpret_cast<const Battle_TargetSphere*>(Battle_targetSphere);
  static_assert(sizeof sphere.wireEdges >=
                kSphereWireEdges * sizeof(std::uint16_t));
  std::array<std::uint16_t, kSphereWireEdges> wire_edges{};
  std::memcpy(wire_edges.data(), sphere.wireEdges, sizeof wire_edges);
  const SphereTerrainTables tables{
      std::span<const int, kSphereTerrainCells>(sphere.terrainHeights),
      wire_edges,
      std::span<const std::uint8_t, kSphereTerrainCells>(
          sphere.terrainRowFlags),
      sphere.cameraYaw};
  LiveCanvas canvas;
  DrawSphereTerrainIntersection(
      {position.vx, position.vy, position.vz},
      {shape.m[0][0], shape.m[2][0], shape.m[0][2], shape.m[2][2]}, scale,
      tables, canvas);
}
}  // namespace VS::Battle
