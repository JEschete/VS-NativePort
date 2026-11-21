#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Core::Gte {
struct SVECTOR;
}  // namespace VS::Core::Gte

// The actor markers and ability-timing cue over the battle globals, the frame
// ordering tables and primitive buffer, the actor projection services and the
// GTE (ActorMarkersLive.cpp). Each keeps the original C signature.
namespace VS::Battle {
// decomp: F00731 func_80095C18 (src/BATTLE/BATTLE.PRG/2842C.c:2399).
void LiveDrawActorMarker(int actor, int kind);
// decomp: F00733 func_8009627C (src/BATTLE/BATTLE.PRG/2842C.c:2570).
void LiveDrawActorMarkers();
// decomp: F00737 func_8009651C (src/BATTLE/BATTLE.PRG/2842C.c:2642).
void LiveDrawOffscreenActorArrow(VS::Core::Gte::SVECTOR* position,
                                 int direction);
// decomp: F00740 func_80096FF0 (src/BATTLE/BATTLE.PRG/2842C.c:2838).
void LiveShowAbilityTimingCue(int cue_duration_frames);
}  // namespace VS::Battle
