// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Hud.h"
#include "Event/Triggers.h"
#include "State/Flags.h"
#include "State/SceneTimer.h"
#include "State/Timers.h"

// decomp: D_80050468 (D_80050468@SLUS_010.40)
State_SceneTimer State_sceneTimer;

// decomp: vs_gametime_tickspeed (vs_gametime_tickspeed@SLUS_010.40)
int State_tickSpeed;

// decomp: BATTLE.PRG func_8007BBD8 (src/BATTLE/BATTLE.PRG/146C.c:8511)
void State_StartSceneTimer(int timer_mode, int reset_scope, int packed_time)
{
    State_sceneTimer.mode = timer_mode;
    State_sceneTimer.resetScope = reset_scope;
    State_sceneTimer.hours = 0;
    State_sceneTimer.minutes = 0;
    State_sceneTimer.seconds = 0;
    State_sceneTimer.frames = 0;

    if (timer_mode != 1) {
        State_sceneTimer.frames = packed_time % 256;
        State_sceneTimer.seconds = (packed_time / 256) % 256;
        State_sceneTimer.minutes = (packed_time / 65536) % 256;
        State_sceneTimer.hours = (packed_time / 16777216) % 256;
    }

    State_SetScriptFlag(0xA0, State_sceneTimer.frames);
    State_SetScriptFlag(0xA1, State_sceneTimer.seconds);
    State_SetScriptFlag(0xA2, State_sceneTimer.minutes);
    State_SetScriptFlag(0xA3, State_sceneTimer.hours);
    Battle_ShowPuzzleTimer();
}

// decomp: BATTLE.PRG func_8007BCCC (src/BATTLE/BATTLE.PRG/146C.c:8534)
void State_ResetSceneTimer(void)
{
    State_sceneTimer.mode = 0;
    State_sceneTimer.hours = 0;
    State_sceneTimer.minutes = 0;
    State_sceneTimer.seconds = 0;
    State_sceneTimer.frames = 0;
    State_SetScriptFlag(0xA0, 0);
    State_SetScriptFlag(0xA1, State_sceneTimer.seconds);
    State_SetScriptFlag(0xA2, State_sceneTimer.minutes);
    State_SetScriptFlag(0xA3, State_sceneTimer.hours);
    Battle_HidePuzzleTimer();
}

// decomp: BATTLE.PRG func_8007BD3C (src/BATTLE/BATTLE.PRG/146C.c:8548)
void State_SetSceneTimerPaused(int paused)
{
    if (paused != 0) {
        int active_mode = State_sceneTimer.mode;
        State_sceneTimer.mode = 0;
        State_sceneTimer.pausedMode = active_mode;
        Battle_HidePuzzleTimer();
        return;
    }
    State_sceneTimer.mode = State_sceneTimer.pausedMode;
    Battle_ShowPuzzleTimer();
}

// decomp: BATTLE.PRG func_8007C36C (src/BATTLE/BATTLE.PRG/146C.c:8740)
int State_SetTickSpeed(int requested_tick_speed)
{
    if ((requested_tick_speed == 2) || (requested_tick_speed == 4)) {
        State_tickSpeed = requested_tick_speed;
    }
    return State_tickSpeed;
}

// decomp: BATTLE.PRG func_800882F4 (src/BATTLE/BATTLE.PRG/146C.c:14499)
int State_UpdateEventTimer(void)
{
    int timer_event_triggered = 0;
    int var_s2 = 0;

    if (State_sceneTimer.mode == timer_event_triggered) {
        return timer_event_triggered;
    }

    if (State_sceneTimer.mode == 1) {
        if (State_sceneTimer.hours <= 0) {
            State_sceneTimer.frames += State_tickSpeed;
            if (State_sceneTimer.frames > 59) {
                State_sceneTimer.frames = 0;
                if (++State_sceneTimer.seconds > 59) {
                    State_sceneTimer.seconds = 0;
                    if (++State_sceneTimer.minutes > 59) {
                        State_sceneTimer.minutes = 0;
                        if (++State_sceneTimer.hours > 0) {
                            State_sceneTimer.hours = State_sceneTimer.mode;
                        }
                    }
                }
            }
        }
    } else if (State_sceneTimer.frames | State_sceneTimer.seconds | State_sceneTimer.minutes
               | State_sceneTimer.hours) {
        if (State_sceneTimer.frames == 0) {
            State_sceneTimer.frames = 60;
            if (--State_sceneTimer.seconds < 0) {
                State_sceneTimer.seconds = 59;
                if (--State_sceneTimer.minutes < 0) {
                    State_sceneTimer.minutes = 59;
                    if (--State_sceneTimer.hours < 0) {
                        State_sceneTimer.hours = 0;
                    }
                }
            }
        }

        State_sceneTimer.frames -= State_tickSpeed;
    }

    if (State_sceneTimer.frames != 0) {
        State_SetScriptFlag(
            0xA0, ((((State_sceneTimer.frames << 0xE) + Core_Random_Rand()) * 5u) >> 0xE) / 3);
    } else {
        State_SetScriptFlag(0xA0, State_sceneTimer.frames);
    }

    State_SetScriptFlag(0xA1, State_sceneTimer.seconds);
    State_SetScriptFlag(0xA2, State_sceneTimer.minutes);
    State_SetScriptFlag(0xA3, State_sceneTimer.hours);

    if ((var_s2 == 0) && (State_sceneTimer.frames == 0) && (Event_EvaluateRoomEventTriggers(16, 0, 0, 0) == 1)) {
        timer_event_triggered = 1;
    }

    return timer_event_triggered;
}

}  // extern "C"
