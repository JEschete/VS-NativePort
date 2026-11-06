// Hand-maintained (tools/port/owned.csv).
#include "Audio/Ambience.hpp"

namespace VS::Audio {
// decomp: BATTLE.PRG func_8008E480 (src/BATTLE/BATTLE.PRG/146C.c:17323)
void SetRoomAmbienceDucked(AmbienceHost& host, bool ducked) {
  host.DuckRoomEffectSound(ducked);
  host.DuckWeatherAmbientSound(ducked);
}
}  // namespace VS::Audio
