#pragma once
// Hand-maintained (tools/port/owned.csv).
// Binds the typed room lighting sampler to the loaded room and the generated
// callers' colour codes.

struct Render_ColorCode;

namespace VS::Field {

// Samples the loaded room's lighting for a packed (z << 16 | x) position and
// writes the colour code; a null colour is ignored.
void SampleLiveRoomLighting(int packed_position, int layer,
                            Render_ColorCode* color);

}  // namespace VS::Field
