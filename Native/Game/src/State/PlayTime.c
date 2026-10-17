// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "CdDrive.h"
#include "Core/Runtime.h"
#include "State/GameTime.h"
#include "State/PlayTime.h"
#include "State/Timers.h"

// decomp: vs_main_gametime (vs_main_gametime@SLUS_010.40)
State_PlayTime State_playTime;

// decomp: SLUS_010.40 vs_main_gametimeUpdate (src/SLUS_010.40/main.c:8188)
int State_UpdatePlayTimeAndLoading(int vsync)
{
    int vs = VSync(vsync);
    if (vsync != 1) {
        Core_IdleInstruction();
    }

    Content_UpdateDiscQueue();

    if (State_playTime.t.hours >= 100) {
        return vs;
    }

    State_playTime.t.frames += State_tickSpeed;

    if (State_playTime.t.frames < 60) {
        return vs;
    }

    State_playTime.t.frames = 0;

    if (++State_playTime.t.seconds < 60) {
        return vs;
    }

    State_playTime.t.seconds = 0;

    if (++State_playTime.t.minutes < 60) {
        return vs;
    }

    State_playTime.t.minutes = 0;

    if (++State_playTime.t.hours >= 100) {
        State_playTime.t.hours = 100;
    }
    return vs;
}

