#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

// The radial screen warp scales every polygon vertex queued for the frame
// toward or away from the screen centre by a factor that depends on its
// distance from the centre. Packets are reached through a cursor and the root
// lookup is passed in, so the warp reads no ordering table, overlay controller
// or PS1 address.
namespace VS::Effects::ScreenDistortion {
// The overlay's two warp words: the starting scale (0x1000 is 1.0) and the
// per-radius growth added to 1.0 for each step outward.
struct RadialWarpSettings {
  std::uint16_t scale{};
  std::int16_t growth{};
};

// The resident arctangent/root lookup image the original indexed for radii.
inline constexpr std::size_t kRadialWarpRootTableBytes = 1296;

// The frame's primitive packets in drawing order. Words are a packet's payload
// after its ordering tag; the warp rewrites only polygon coordinate words.
class PrimitivePacketCursor {
 public:
  PrimitivePacketCursor() = default;
  PrimitivePacketCursor(const PrimitivePacketCursor&) = delete;
  PrimitivePacketCursor& operator=(const PrimitivePacketCursor&) = delete;
  PrimitivePacketCursor(PrimitivePacketCursor&&) = delete;
  PrimitivePacketCursor& operator=(PrimitivePacketCursor&&) = delete;
  virtual ~PrimitivePacketCursor() = default;

  // Moves to the next packet (the first on the first call); false at the end.
  [[nodiscard]] virtual bool Advance() = 0;
  [[nodiscard]] virtual std::size_t WordCount() const = 0;
  [[nodiscard]] virtual std::uint32_t Word(std::size_t index) const = 0;
  virtual void SetWord(std::size_t index, std::uint32_t value) = 0;
};

// Warps the first polygon command of every packet (func_800F9BD8). Aborts
// with a diagnostic when a polygon's vertices run past its packet or the
// radius lookup leaves the root table.
void WarpPolygonVertices(RadialWarpSettings settings,
                         std::span<const std::uint8_t> root_table,
                         PrimitivePacketCursor& packets);
}  // namespace VS::Effects::ScreenDistortion
