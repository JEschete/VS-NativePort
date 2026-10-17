// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Ambience.h"
#include "Field/Environment.h"
#include "Field/RoomEffects.h"

// decomp: BATTLE.PRG func_8008E480 (src/BATTLE/BATTLE.PRG/146C.c:17323)
void Audio_SetRoomAmbienceDucked(int arg0)
{
    Field_DuckRoomEffectSound(arg0);
    Field_DuckWeatherAmbientSound(arg0);
}

