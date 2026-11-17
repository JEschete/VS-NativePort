// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"

// decomp: D_800EC328 (D_800EC328@BATTLE/BATTLE.PRG)
int Effects_emissionCounter; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC330 (D_800EC330@BATTLE/BATTLE.PRG)
u_char Effects_pairedParticleColorPresets[2727][2][4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC368 (D_800EC368@BATTLE/BATTLE.PRG)
u_char Effects_particleColorPresetTable[1088][5][4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F55F0 (D_800F55F0@BATTLE/BATTLE.PRG)
int Effects_particleCapacity;

// decomp: D_800F55F4 (D_800F55F4@BATTLE/BATTLE.PRG)
Effects_Particle* Effects_freeParticles;

// decomp: D_800F55F8 (D_800F55F8@BATTLE/BATTLE.PRG)
int Effects_usedParticleCount;

// decomp: D_800F55FC (D_800F55FC@BATTLE/BATTLE.PRG)
Effects_Particle* Effects_particlePool;

// decomp: D_800F5600 (D_800F5600@BATTLE/BATTLE.PRG)
int Effects_particlePoolReferences;

// decomp: D_800F5618 (D_800F5618@BATTLE/BATTLE.PRG)
int Effects_particleDrag;

// decomp: D_800F5620 (D_800F5620@BATTLE/BATTLE.PRG)
Effects_ParticleGravity Effects_particleGravity;

}  // extern "C"
