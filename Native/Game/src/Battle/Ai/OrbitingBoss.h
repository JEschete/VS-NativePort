// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_ORBITINGBOSS_H_TYPES
#define VS_BATTLE_AI_ORBITINGBOSS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_AI_ORBITINGBOSS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_AI_ORBITINGBOSS_H_DECLS)
#define VS_BATTLE_AI_ORBITINGBOSS_H_DECLS
#include "Battle/Ai/Movement.h"

extern short Battle_Ai_radialRetreatPath[36][2];
extern int Battle_Ai_orbitAngularVelocity;
extern int Battle_Ai_orbitHeightOffset;
extern Battle_Ai_ArenaOrbitState* Battle_Ai_arenaOrbit;

#endif // VS_BATTLE_AI_ORBITINGBOSS_H_DECLS
