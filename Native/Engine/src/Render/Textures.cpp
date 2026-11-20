// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.hpp"
#include "Render/Textures.hpp"

// decomp: D_800F46A8 (D_800F46A8@BATTLE/BATTLE.PRG)
VS::Render::TextureSlot VS::Render::legacyActorTextureSlots[VS::Render::kActorTextureSlotCount];

// decomp: vs_battle_wepTextures (vs_battle_wepTextures@BATTLE/BATTLE.PRG)
VS::Render::TextureSlot VS::Render::legacyWeaponTextureSlots[VS::Render::kWeaponTextureSlotCount];

