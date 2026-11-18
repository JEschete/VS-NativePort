#pragma once
// Hand-maintained (tools/port/owned.csv).
// The screen transition globals as the copied room loading, interactions,
// scripts, encounter loop and screen overlay draw read and write them.
// TransitionsLive.cpp defines them and mirrors them into the typed
// RoomTransitionState (Field/Transitions.hpp) around each call.

namespace VS::Field {

// decomp: vs_battle_screenTransitionStep (BATTLE/BATTLE.PRG 146C.h): the
// Field::TransitionStep.
extern unsigned int screenTransitionStep;
// decomp: vs_battle_screenTransitionEffect (BATTLE/BATTLE.PRG 146C.h): the
// fade or wipe style (Render::ScreenTransitionStyleOf).
extern int screenTransitionEffect;
// decomp: vs_battle_screenTransitionAlpha (BATTLE/BATTLE.PRG 146C.h): 0
// clear to 255 covered.
extern int screenTransitionAlpha;
// decomp: vs_battle_screenTransitionWipeAngle (BATTLE/BATTLE.PRG 146C.h).
extern int screenTransitionWipeAngle;
// decomp: vs_battle_screenTransitionSpeed (BATTLE/BATTLE.PRG 146C.h): the
// alpha change per frame.
extern int screenTransitionSpeed;

}  // namespace VS::Field
