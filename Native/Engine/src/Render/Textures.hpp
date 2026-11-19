#pragma once
// The battle texture slots: each resident actor or weapon texture's page,
// palettes and reference count.
#include <cstddef>
#include <cstdint>

namespace VS::Render {
inline constexpr std::size_t kActorTextureSlotCount = 25;
inline constexpr std::size_t kWeaponTextureSlotCount = 20;

// decomp: vs_battle_texInfo_t (src/BATTLE/BATTLE.PRG/30DB0.h:50)
struct TextureSlot {
  std::int16_t texture_page;
  std::int16_t palette;
  std::int16_t alternate_palette;
  std::uint8_t reference_count;
};

// decomp: D_800F46A8 (D_800F46A8@BATTLE/BATTLE.PRG)
extern TextureSlot legacyActorTextureSlots[kActorTextureSlotCount];
// decomp: vs_battle_wepTextures (vs_battle_wepTextures@BATTLE/BATTLE.PRG)
extern TextureSlot legacyWeaponTextureSlots[kWeaponTextureSlotCount];

namespace layout {
inline constexpr std::size_t kTextureSlotBytes = 0x8;
static_assert(sizeof(TextureSlot) == kTextureSlotBytes);
}  // namespace layout
}  // namespace VS::Render
