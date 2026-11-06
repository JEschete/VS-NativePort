#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binds the typed room lighting sampler to the loaded room and the generated
// callers' colour codes.

#include <cstddef>
#include <cstdint>
#include <span>

#include "Render/RoomLighting.hpp"

struct Render_ColorCode;

namespace VS::Field {

// The loaded room's lighting section, or none when no room or section is
// loaded.
[[nodiscard]] std::span<const std::byte> LiveRoomLightingSection();

// Samples the loaded room's `layer` lighting for a packed (z << 16 | x)
// position and writes the colour code; a null colour is ignored.
void SampleLiveRoomLighting(std::uint32_t packed_position,
                            Render::RoomLightLayer layer,
                            Render_ColorCode* color);

// The generated callers' shape: layer 0 is the floor and any other the
// ceiling.
void SampleLiveRoomLighting(int packed_position, int layer,
                            Render_ColorCode* color);

}  // namespace VS::Field
