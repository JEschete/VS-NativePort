// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MODELLOADING_H_TYPES
#define VS_MODELLOADING_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MODELLOADING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MODELLOADING_H_DECLS)
#define VS_MODELLOADING_H_DECLS
#include "ActorAssets.h"
#include "Actors/WeaponModel.h"
#include "DiscQueue.h"
#include "Field/Actors.h"

extern u_char Content_legacyModelLoadPhase;
extern u_char Content_legacySequenceParsePhase;
extern u_char Content_legacyEtmLoadPhase;
extern void* Content_legacyWeaponFileBuffer;
extern int Content_legacyWeaponFileLoaded;
extern Content_DiscFile Content_legacyWeaponFileLocation;
extern Content_DiscQueueSlot* Content_legacyWeaponDiscRequest;
extern void* Content_legacyWeaponParseCursor;
extern void* Content_legacyShpFileBuffer;
extern int Content_legacyShpAllocated;
extern Content_DiscFile Content_legacyShpFileLocation;
extern Content_DiscQueueSlot* Content_legacyShpDiscRequest;
extern u_char* Content_legacyShpParseCursor;
extern u_char Content_legacyShpCopiedWordCount;
extern int Content_legacyShpHeaderWord;
extern int Content_legacyShpTextureWidth;
extern int Content_legacyShpTextureHeight;
extern void* Content_legacySequenceFileBuffer;
extern int Content_legacySequenceBufferOwned;
extern Content_DiscFile Content_legacySequenceFileLocation;
extern Content_DiscQueueSlot* Content_legacySequenceDiscRequest;
extern u_char* Content_legacySequenceParseData;
extern u_short* Content_legacyEtmFileBuffer;
extern Content_DiscFile Content_legacyEtmFileLocation;
extern Content_DiscQueueSlot* Content_legacyEtmDiscRequest;
extern Content_ActorAssetRequest Content_legacyActorAssetRequests[16];
extern Actors_WeaponModelData* Content_legacyWeaponModelBuffers[84];

#endif // VS_MODELLOADING_H_DECLS
