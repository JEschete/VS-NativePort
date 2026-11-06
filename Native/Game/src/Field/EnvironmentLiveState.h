#pragma once
// Hand-maintained (tools/port/owned.csv).
// The room weather globals as the copied weather effects, scripts and
// snapshot read and write them. EnvironmentLive.cpp defines them and mirrors
// them into the typed RoomWeatherState (Field/Environment.hpp) around each
// call.

#include <stdint.h>

#include "Field/Weather.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_800F1D98 (BATTLE/BATTLE.PRG 146C.c:710): [0..2] the particle
// velocity the weather effects derive, [4] the wind strength, [5] its
// direction, [6] a copy of the intensity scale, [7..9] the direction,
// intensity and strength drift steps.
enum { kFieldWeatherParameterWords = 632 };
extern int16_t Field_weatherParameters[kFieldWeatherParameterWords];
// decomp: D_800F1DA4 (BATTLE/BATTLE.PRG 2842C.c:239): the intensity's
// velocity scale; the original's D_800F1D98[6].
extern int16_t Field_weatherIntensityScale;
// decomp: D_800F1DBC (BATTLE/BATTLE.PRG 146C.c:717): the ambient sound
// effect, -1 while held off and -2 for none.
extern int16_t Field_weatherAmbientSoundId;
// decomp: D_800F1DC0 and D_800F1DC2 (BATTLE/BATTLE.PRG 146C.c:719): the
// particles drawn and allocated.
extern int16_t Field_weatherParticleCount;
extern int16_t Field_weatherParticleCapacity;
// decomp: D_800F1DC4 (BATTLE/BATTLE.PRG 146C.c:721): the weather intensity,
// 0..15.
extern int16_t Field_weatherIntensity;
// decomp: D_800F1DCA (BATTLE/BATTLE.PRG 146C.c:725): the weather kind, 0 for
// none.
extern char Field_roomWeatherKind;
// decomp: D_800F1DCB (BATTLE/BATTLE.PRG 146C.c:726): 1 while the wind and
// intensity drift.
extern char Field_isWeatherVariationEnabled;
// decomp: D_800F1DD4 (BATTLE/BATTLE.PRG 146C.c:729): the 0x800-byte particle
// buffer (low heap), or null.
extern Field_WeatherParticle* Field_weatherParticles;

#ifdef __cplusplus
}
#endif
