#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for Ambience.hpp over the room effect and weather ambient
// sound controls; the copied callers keep the original int flag (nonzero
// ducks).
namespace VS::Audio {
void LiveSetRoomAmbienceDucked(int duck_enabled);
}  // namespace VS::Audio
