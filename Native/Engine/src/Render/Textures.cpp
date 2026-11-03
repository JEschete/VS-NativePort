// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Render/Textures.h"

// decomp: D_800F46A8 (D_800F46A8@BATTLE/BATTLE.PRG)
Render_TextureSlot Render_legacyActorTextureSlots[25];

// decomp: vs_battle_wepTextures (vs_battle_wepTextures@BATTLE/BATTLE.PRG)
Render_TextureSlot Render_legacyWeaponTextureSlots[20];

}  // extern "C"
