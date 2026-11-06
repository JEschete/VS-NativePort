#pragma once
// Hand-maintained (tools/port/owned.csv).
// The screen transition globals as the copied room loading, interactions,
// scripts, encounter loop and screen overlay draw read and write them.
// TransitionsLive.cpp defines them and mirrors them into the typed
// RoomTransitionState (Field/Transitions.hpp) around each call.

#ifdef __cplusplus
extern "C" {
#endif

// decomp: vs_battle_screenTransitionStep (BATTLE/BATTLE.PRG 146C.h): the
// Field::TransitionStep.
extern unsigned int Field_screenTransitionStep;
// decomp: vs_battle_screenTransitionEffect (BATTLE/BATTLE.PRG 146C.h): the
// fade or wipe style (Render::ScreenTransitionStyleOf).
extern int Field_screenTransitionEffect;
// decomp: vs_battle_screenTransitionAlpha (BATTLE/BATTLE.PRG 146C.h): 0
// clear to 255 covered.
extern int Field_screenTransitionAlpha;
// decomp: vs_battle_screenTransitionWipeAngle (BATTLE/BATTLE.PRG 146C.h).
extern int Field_screenTransitionWipeAngle;
// decomp: vs_battle_screenTransitionSpeed (BATTLE/BATTLE.PRG 146C.h): the
// alpha change per frame.
extern int Field_screenTransitionSpeed;

#ifdef __cplusplus
}
#endif
