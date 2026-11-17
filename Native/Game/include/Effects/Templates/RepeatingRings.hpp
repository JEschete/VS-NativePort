#pragma once

#include <cstdint>
#include <span>

#include "Effects/Templates/RingCore.hpp"

namespace VS::Effects::Templates {
enum class RepeatingRingStyle : std::uint8_t {
  kRepeating,
  kTransposed,
  kForeground,
};

// The foreground ring still clips using its average projected depth, but
// links every surviving quad fifteen entries before the scene table.
inline constexpr std::int32_t kForegroundRingDepth = -15;

// A tick requires all eight palette entries. Other commands do not sample it.
[[nodiscard]] bool UpdateRepeatingRing(
    RepeatingRingStyle style, RingCommand command, std::int32_t packed,
    std::span<const RingPaletteEntry> palette, RingHost& host);
}  // namespace VS::Effects::Templates

namespace VS::Effects {
namespace Plg052 {
bool RepeatingRing(Templates::RingCommand command, std::int32_t packed,
                   std::span<const Templates::RingPaletteEntry> palette,
                   Templates::RingHost& host);
}
namespace Plg077 {
bool RepeatingRing(Templates::RingCommand command, std::int32_t packed,
                   std::span<const Templates::RingPaletteEntry> palette,
                   Templates::RingHost& host);
}
namespace Plg162 {
bool TransposedRing(Templates::RingCommand command, std::int32_t packed,
                    std::span<const Templates::RingPaletteEntry> palette,
                    Templates::RingHost& host);
}
namespace Plg236 {
bool TransposedRing(Templates::RingCommand command, std::int32_t packed,
                    std::span<const Templates::RingPaletteEntry> palette,
                    Templates::RingHost& host);
}
namespace Plg263 {
bool TransposedRing(Templates::RingCommand command, std::int32_t packed,
                    std::span<const Templates::RingPaletteEntry> palette,
                    Templates::RingHost& host);
}
namespace Plg300 {
bool RepeatingRing(Templates::RingCommand command, std::int32_t packed,
                   std::span<const Templates::RingPaletteEntry> palette,
                   Templates::RingHost& host);
bool ForegroundRepeatingRing(
    Templates::RingCommand command, std::int32_t packed,
    std::span<const Templates::RingPaletteEntry> palette,
    Templates::RingHost& host);
}  // namespace Plg300
}  // namespace VS::Effects
