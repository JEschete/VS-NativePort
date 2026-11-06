#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>

#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/SurfaceCore.hpp"

// The grid template (decomp src/EFFECT/grids.h:23 vs_grid): a flat nine by
// nine lattice of points whose line spacing grows outwards from the centre,
// each point swaying on two sine waves, drawn as eight by eight flat-shaded
// textured quads. Its texture comes from fixed VRAM coordinates in the
// definition and scrolls by the gravity range every tick.
namespace VS::Effects::Templates {
inline constexpr std::size_t kGridLines = 9;
inline constexpr std::size_t kGridPoints = kGridLines * kGridLines;
inline constexpr std::size_t kGridPacketWords = 15;

// The persistent record (vs_gridState): the texture origin, scroll and
// VRAM words, the halfword spacing and phase accumulators and the age.
struct GridState {
  std::uint8_t lifetime{};
  std::uint8_t definition_index{};
  std::uint8_t u{};
  std::uint8_t v{};
  std::uint8_t u_scroll{};
  std::uint8_t v_scroll{};
  std::uint16_t tpage{};
  std::uint16_t clut{};
  std::int16_t x_spacing{};
  std::int16_t x_speed{};
  std::int16_t z_spacing{};
  std::int16_t z_speed{};
  std::int16_t x_phase{};
  std::int16_t z_phase{};
  std::int32_t age{};

  friend bool operator==(const GridState&, const GridState&) = default;
};

using GridHost = SurfaceRecordHost<GridState>;

// The ordering slot a visible quad moves to, from the inner-extent sample:
// sorted by depth, the farthest scene slot, three fixed slots in front of
// the scene, or 16 nearer when that stays beyond the near clip.
enum class GridDepthMode : std::int32_t {
  kSorted = 0,
  kFarthest = 1,
  kFront15 = 2,
  kFront13 = 3,
  kFront11 = 4,
  kNudged = 5,
};

// One flat-shaded textured quad of the grid.
struct GridQuad {
  std::array<std::uint32_t, kRingQuadCorners> screens{};
  RingColor color{};
  RingQuadUvs uvs{};
  std::uint16_t clut{};
  std::uint16_t tpage{};
};

// The state a create leaves for `definition`: the texture page takes the
// segments' low three bits and the transparency's low three bits as its
// blend mode, the CLUT the positions' low three bits.
[[nodiscard]] GridState CreateGridState(
    RingPlan plan, const SurfaceDefinition& definition) noexcept;

// The ordering slot of a visible quad at `depth` under `mode` (the raw
// sample; values outside the modes keep the depth).
[[nodiscard]] std::int32_t GridDepth(std::int32_t depth, std::int32_t mode,
                                     std::int32_t near_clip) noexcept;

// The fifteen words of a grid quad, linked to an empty ordering entry.
[[nodiscard]] std::array<std::uint32_t, kGridPacketWords> EncodeGridPacket(
    const GridQuad& quad) noexcept;

// One renderer command of a grid; false releases the slot.
bool Grid(SurfaceCommand command, std::int32_t packed, GridHost& host);
}  // namespace VS::Effects::Templates

// The plugin instances (Grid.cpp); SurfacesLive.hpp binds each to its
// overlay's renderer table entry.
namespace VS::Effects {
namespace Plg053 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg053
namespace Plg061 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg061
namespace Plg073 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg073
namespace Plg077 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg077
namespace Plg082 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg082
namespace Plg085 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg085
namespace Plg086 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg086
namespace Plg087 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg087
namespace Plg128 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg128
namespace Plg137 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg137
namespace Plg138 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg138
namespace Plg139 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg139
namespace Plg142 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg142
namespace Plg150 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg150
namespace Plg152 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg152
namespace Plg156 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg156
namespace Plg161 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg161
namespace Plg165 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg165
namespace Plg168 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg168
namespace Plg170 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg170
namespace Plg242 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg242
namespace Plg272 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg272
namespace Plg291 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg291
namespace Plg297 {
bool Grid(Templates::SurfaceCommand command, std::int32_t packed,
          Templates::GridHost& host);
}  // namespace Plg297
}  // namespace VS::Effects
