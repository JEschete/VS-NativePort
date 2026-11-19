// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

#include "Effects/ActionRequest.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/Instance.hpp"
#include "Effects/Package.hpp"

#include "DiscQueue.h"

extern u_char Effects_effectBufferIndex;
extern VS::Effects::EffectRenderer Effects_scriptRenderers[2];
extern char Effects_actionVariantValues[21824];
extern int (*Effects_scriptOpcodeHandlers[49])(void*);
extern u_char Effects_lastReactionSoundVariant;
extern u_int Effects_emptyPackage[0x5200 / sizeof(u_int)];
extern int Effects_tintedActor;
extern int Effects_effectBusyFlags;
extern Content_DiscQueueSlot* Effects_menuReloadSlot;
extern u_int Effects_pendingTracks;
extern int Effects_trackCount;
extern int Effects_startedTrackCount;
extern u_char Effects_actionVariant;
extern VS::Effects::LiveActionEffectRequest Effects_actionRequest;
extern char Effects_primaryToken;
extern u_int Effects_primaryPhase;
extern int Effects_tokenBitmaps[215];
extern int Effects_curveSamples[211];
extern int Effects_unusedPrimaryWord;
extern VS::Effects::LiveEffectInstance* Effects_primaryInstances;
extern VS::Effects::LiveEffectInstance* Effects_currentInstance;
extern VS::Effects::LiveActionEffectRequest Effects_secondaryRequest;
extern char Effects_primaryLoaded;
extern VS::Effects::LiveEffectInstance* Effects_secondaryInstances;
extern int Effects_scriptSelector;
extern VS::Effects::LiveEffectPackage* Effects_savedPackage;
extern VS::Effects::LiveEffectPackage* Effects_package;
extern VS::Effects::EffectRenderer Effects_rendererTableAlias[];
extern VS::Effects::EffectRenderer Effects_effectRenderers[8];
extern VS::Effects::LiveEffectPackage Effects_actionPackage;
extern VS::Effects::LiveEffectPackage Effects_secondaryPackage;
