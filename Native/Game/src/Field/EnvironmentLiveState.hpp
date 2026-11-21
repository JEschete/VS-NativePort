#pragma once
// Hand-maintained (tools/port/owned.csv).
// The room weather globals as the copied weather effects, scripts and
// snapshot read and write them. EnvironmentLive.cpp defines them and mirrors
// them into the typed RoomWeatherState (Field/Environment.hpp) around each
// call.

#include <stdint.h>

#include "Field/Weather.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Field {
// decomp: D_800F1D98 (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/146CPrivate.h:368): [0..2] the particle velocity the
// weather effects derive, [4] the wind strength, [5] its direction, [6] a copy
// of the intensity scale, [7..9] the direction, intensity and strength drift
// steps.
inline constexpr int kWeatherParameterWords = 632;
extern int16_t weatherParameters[kWeatherParameterWords];
// decomp: D_800F1DA4 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/2842C.c:234): the
// intensity's velocity scale; the original's D_800F1D98[6].
extern int16_t weatherIntensityScale;
// decomp: D_800F1DBC (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/146CPrivate.h:377): the ambient sound effect, -1 while
// held off and -2 for none.
extern int16_t weatherAmbientSoundId;
// decomp: D_800F1DC0 and D_800F1DC2 (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/146CPrivate.h:383): the particles drawn and allocated.
extern int16_t weatherParticleCount;
extern int16_t weatherParticleCapacity;
// decomp: D_800F1DC4 (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/146CPrivate.h:386): the weather intensity, 0..15.
extern int16_t weatherIntensity;
// decomp: D_800F1DCA (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/146CPrivate.h:389): the weather kind, 0 for none.
extern char roomWeatherKind;
// decomp: D_800F1DCB (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/146CPrivate.h:392): 1 while the wind and intensity
// drift.
extern char isWeatherVariationEnabled;
// decomp: D_800F1DD4 (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/146CPrivate.h:395): the 0x800-byte particle buffer (low
// heap), or null.
extern WeatherParticleRecord* weatherParticles;
}  // namespace VS::Field
