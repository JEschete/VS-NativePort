#pragma once
// Hand-maintained (tools/port/owned.csv).
// The effect loader state as the copied effect runtime reads and writes it.
// EffectResourcesLive.cpp defines it and mirrors it into the typed
// EffectResourceState (Effects/EffectResources.hpp) around each call.

#include "DiscQueue.h"
#include "Effects/Loading.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_800F5630 (BATTLE/BATTLE.PRG 6F014.c:612): the texture asset
// being loaded.
extern int Effects_textureLoadIndex;
// decomp: D_800F5638 (BATTLE/BATTLE.PRG 6F014.c:56): the effect's CLUT and
// texture page files.
extern Effects_TextureLoadList Effects_textureLoads;
// decomp: vs_battle_pfileBuf (BATTLE/BATTLE.PRG 0x800F567C): the loaded
// package (high heap).
extern void* Effects_packageBuffer;
// decomp: D_800F5680 (BATTLE/BATTLE.PRG 6F014.c:612): the texture load
// state (0 idle, 1 loading, 2 loaded).
extern u_int Effects_textureLoadState;
// decomp: D_800F5684 (BATTLE/BATTLE.PRG 6F014.c:570): nonzero once the
// purge overlay's workspace was acquired.
extern int Effects_purgeOverlayLoaded;
// decomp: vs_battle_pFileLoadContext (BATTLE/BATTLE.PRG 0x800F5688): the
// package file, in sectors past E000.P.
extern Effects_FileLoadContext Effects_packageFile;
// decomp: D_800F568C (BATTLE/BATTLE.PRG 6F014.c:549): the disc request in
// flight, or null.
extern Content_DiscQueueSlot* Effects_resourceReadSlot;
// decomp: _plgLoadState (BATTLE/BATTLE.PRG 0x800F5690): the plugin load
// state (0 idle, 1 acquire the overlay, 2 loading, 3 loaded).
extern u_int Effects_pluginLoadState;
// decomp: _plgLoadContext (BATTLE/BATTLE.PRG 0x800F5694): the plugin file.
extern Effects_FileLoadContext Effects_pluginFile;
// decomp: D_800F56A0 (BATTLE/BATTLE.PRG 6F014.c:612): the texture staging
// buffer (high heap).
extern u_long* Effects_textureLoadBuffer;
// decomp: _plgEntryPointCount (BATTLE/BATTLE.PRG 0x800F56A4): the plugin's
// renderers.
extern int Effects_pluginEntryPointCount;
// decomp: D_800F5874 (BATTLE/BATTLE.PRG 6F014.c:592): bit n set while
// texture page n holds an effect texture.
extern int Effects_loadedTextureSlots;

#ifdef __cplusplus
}
#endif
