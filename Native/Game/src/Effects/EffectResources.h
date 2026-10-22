// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_EFFECTRESOURCES_H_TYPES
#define VS_EFFECTS_EFFECTRESOURCES_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_EFFECTRESOURCES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_EFFECTRESOURCES_H_DECLS)
#define VS_EFFECTS_EFFECTRESOURCES_H_DECLS
#include "DiscQueue.h"
#include "Effects/Loading.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"

extern int Effects_textureLoadIndex;
extern Effects_TextureLoadList Effects_textureLoads;
extern void* Effects_packageBuffer;
extern u_int Effects_textureLoadState;
extern int Effects_purgeOverlayLoaded;
extern Effects_FileLoadContext Effects_packageFile;
extern Content_DiscQueueSlot* Effects_resourceReadSlot;
extern u_int Effects_pluginLoadState;
extern Effects_FileLoadContext Effects_pluginFile;
extern u_long* Effects_textureLoadBuffer;
extern int Effects_pluginEntryPointCount;
extern int Effects_loadedTextureSlots;
void Effects_InitializeEffectResources(void);
void Effects_SelectEffectResourceContext(int arg0);
void Effects_SaveEffectResourceContext(void);
void Effects_RestoreEffectResourceContext(void);
void Effects_BindParticleDefinitions(Effects_PackageBlock5* block);
void Effects_UpdateParticleGravityAndDrag(void);
void Effects_BindEffectTimelines(void* block);
void Effects_BindEffectCurves(Effects_CurveBlock* block);
void Effects_BindEffectPrograms(void* block);
void Effects_BindEffectPackageParameters(Effects_PackageBlock9* arg0);
void Effects_BindEffectTransformDefinitions(void* arg0);
void Effects_BindEffectTextureTables(Effects_PackageBlock1* block);
void Effects_BindParticleAnimationTable(Effects_PackageBlock2* block);
void* Effects_ParticleAnimationData(int arg0);
void Effects_BindEffectSoundSequences(u_char* arg0);
void Effects_BindEffectSoundBank(void* arg0);
void Effects_BindEffectAuxiliarySoundBank(void* arg0, size_t byte_count);
int Effects_PollEffectResourceRead(int lba, int size, void* buf);
int Effects_LoadEffectPurgeOverlay(void);
int Effects_LoadEffectPackage(void);
void Effects_ReleaseEffectPackageResources(void);
int Effects_UpdateEffectTextureLoading(void);
int Effects_UpdateEffectPluginLoading(void);
void Effects_ConfigureEffectPackageLoad(int lba, int size);
void Effects_ConfigureEffectPluginLoad(int lba, int size, int entryPointCount);
void Effects_SetEffectRenderer(Effects_EffectRenderer func, int index);
void Effects_SetEffectTextureAssetCount(int arg0);
void Effects_ConfigureEffectTextureAsset(int lba, int size, int slot, int isClut, int index);
void Effects_SetEffectTextureLoadState(int arg0);
u_int Effects_EffectTextureLoadState(void);
void Effects_SetEffectPluginLoadState(int arg0);
void Effects_BindEffectPackageBlocks(Effects_PackageFile* arg0);
int Effects_InvokeAllPluginUpdates(Effects_RendererSlot* arg0, u_int arg1, int arg2);
void Effects_ConfigureEffectResources(void);
void Effects_ConfigureTextureLoads(void);
void Effects_ConfigurePluginLoad(void);

#endif // VS_EFFECTS_EFFECTRESOURCES_H_DECLS
