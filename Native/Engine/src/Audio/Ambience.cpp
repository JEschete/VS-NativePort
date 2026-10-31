// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Ambience.h"
#include "Field/Environment.h"
#include "Field/RoomEffects.h"

// decomp: BATTLE.PRG func_8008E480 (src/BATTLE/BATTLE.PRG/146C.c:17323)
void Audio_SetRoomAmbienceDucked(int duck_enabled)
{
    Field_DuckRoomEffectSound(duck_enabled);
    Field_DuckWeatherAmbientSound(duck_enabled);
}

}  // extern "C"
