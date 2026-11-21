#pragma once
// Hand-maintained (tools/port/owned.csv).
// The effect loader state as the copied effect runtime reads and writes it.
// EffectResourcesLive.cpp defines it and mirrors it into the typed
// EffectResourceState (Effects/EffectResources.hpp) around each call.

#include <cstddef>
#include <cstdint>

#include "Content/DiscQueue.hpp"
#include "Effects/EffectResources.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Effects {
// decomp: D_800F5638_t (src/BATTLE/BATTLE.PRG/6E4CCShared.h:18): the texture
// load list in its PS1 layout, one array per field; TextureLoadList
// (EffectResources.hpp) is its typed value.
struct LiveTextureLoadList {
  std::int32_t asset_count;
  EffectFileExtent files[kTextureAssetCapacity];
  std::uint16_t slots[kTextureAssetCapacity];
  std::uint16_t is_clut[kTextureAssetCapacity];
};

namespace layout {
// vs_LoadFileContext (src/BATTLE/BATTLE.PRG/battleEffect.h:6): the PS1 file
// extent EffectFileExtent keeps.
inline constexpr std::size_t kFileExtentBytes = 4;
static_assert(sizeof(EffectFileExtent) == kFileExtentBytes);
inline constexpr std::size_t kFileExtentSizeOffset = 2;
static_assert(offsetof(EffectFileExtent, size) == kFileExtentSizeOffset);
inline constexpr std::size_t kTextureLoadListBytes = 0x44;
static_assert(sizeof(LiveTextureLoadList) == kTextureLoadListBytes);
inline constexpr std::size_t kTextureLoadSlotsOffset = 0x24;
static_assert(offsetof(LiveTextureLoadList, slots) == kTextureLoadSlotsOffset);
inline constexpr std::size_t kTextureLoadClutOffset = 0x34;
static_assert(offsetof(LiveTextureLoadList, is_clut) == kTextureLoadClutOffset);
}  // namespace layout

// decomp: D_800F5630 (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/6E4CCShared.h:514): the texture asset being loaded.
extern int textureLoadIndex;
// decomp: D_800F5638 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:515): the
// effect's CLUT and texture page files.
extern LiveTextureLoadList textureLoads;
// decomp: vs_battle_pfileBuf (BATTLE/BATTLE.PRG 0x800F567C): the loaded
// package (high heap).
extern void* packageBuffer;
// decomp: D_800F5680 (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/6E4CCShared.h:516): the texture load state (0 idle, 1
// loading, 2 loaded).
extern std::uint32_t textureLoadState;
// decomp: D_800F5684 (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/6E4CCPrivate.h:116): nonzero once the purge overlay's
// workspace was acquired.
extern int purgeOverlayLoaded;
// decomp: vs_battle_pFileLoadContext
// (src/BATTLE/BATTLE.PRG/battleEffect.h:421): the package file, in sectors
// past E000.P.
extern EffectFileExtent packageFile;
// decomp: D_800F568C (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/6E4CCShared.h:517): the disc request in flight, or
// null.
extern VS::Content::DiscQueueSlot* resourceReadSlot;
// decomp: _plgLoadState (BATTLE/BATTLE.PRG 0x800F5690): the plugin load
// state (0 idle, 1 acquire the overlay, 2 loading, 3 loaded).
extern std::uint32_t pluginLoadState;
// decomp: plgLoadContext (_plgLoadContext@
// src/BATTLE/BATTLE.PRG/6E4CCShared.h:538): the plugin file.
extern EffectFileExtent pluginFile;
// decomp: D_800F56A0 (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/6E4CCShared.h:518): the texture staging buffer (high
// heap).
extern std::uint32_t* textureLoadBuffer;
// decomp: _plgEntryPointCount (BATTLE/BATTLE.PRG 0x800F56A4): the plugin's
// renderers.
extern int pluginEntryPointCount;
// decomp: D_800F5874 (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/6E4CCShared.h:521): bit n set while texture page n
// holds an effect texture.
extern int loadedTextureSlots;
}  // namespace VS::Effects
