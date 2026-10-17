// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/WeaponModel.h"
#include "DiscQueue.h"
#include "Field/Actors.h"
#include "ModelLoading.h"

// decomp: _loadShpState (_loadShpState@BATTLE/BATTLE.PRG)
u_char Content_legacyModelLoadPhase; /* data: contents loaded in Step 5 */

// decomp: D_800E8F2B (D_800E8F2B@BATTLE/BATTLE.PRG)
u_char Content_legacySequenceParsePhase; /* data: contents loaded in Step 5 */

// decomp: _loadEtmState (_loadEtmState@BATTLE/BATTLE.PRG)
u_char Content_legacyEtmLoadPhase; /* data: contents loaded in Step 5 */

// decomp: _wepFileData (_wepFileData@BATTLE/BATTLE.PRG)
void* Content_legacyWeaponFileBuffer;

// decomp: _wepFileLoaded (_wepFileLoaded@BATTLE/BATTLE.PRG)
int Content_legacyWeaponFileLoaded;

// decomp: _wepFile (_wepFile@BATTLE/BATTLE.PRG)
Content_DiscFile Content_legacyWeaponFileLocation;

// decomp: _wepCdSlot (_wepCdSlot@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Content_legacyWeaponDiscRequest;

// decomp: _wepFileEpilog (_wepFileEpilog@BATTLE/BATTLE.PRG)
void* Content_legacyWeaponParseCursor;

// decomp: _shpData (_shpData@BATTLE/BATTLE.PRG)
void* Content_legacyShpFileBuffer;

// decomp: _shpAllocated (_shpAllocated@BATTLE/BATTLE.PRG)
int Content_legacyShpAllocated;

// decomp: _shpFile (_shpFile@BATTLE/BATTLE.PRG)
Content_DiscFile Content_legacyShpFileLocation;

// decomp: _shpCdSlot (_shpCdSlot@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Content_legacyShpDiscRequest;

// decomp: D_800F22B4 (D_800F22B4@BATTLE/BATTLE.PRG)
u_char* Content_legacyShpParseCursor;

// decomp: D_800F22B8 (D_800F22B8@BATTLE/BATTLE.PRG)
u_char Content_legacyShpCopiedWordCount;

// decomp: D_800F22BC (D_800F22BC@BATTLE/BATTLE.PRG)
int Content_legacyShpHeaderWord;

// decomp: D_800F22C0 (D_800F22C0@BATTLE/BATTLE.PRG)
int Content_legacyShpTextureWidth;

// decomp: D_800F22C4 (D_800F22C4@BATTLE/BATTLE.PRG)
int Content_legacyShpTextureHeight;

// decomp: D_800F22C8 (D_800F22C8@BATTLE/BATTLE.PRG)
void* Content_legacySequenceFileBuffer;

// decomp: D_800F22D0 (D_800F22D0@BATTLE/BATTLE.PRG)
int Content_legacySequenceBufferOwned;

// decomp: D_800F22D8 (D_800F22D8@BATTLE/BATTLE.PRG)
Content_DiscFile Content_legacySequenceFileLocation;

// decomp: D_800F22E0 (D_800F22E0@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Content_legacySequenceDiscRequest;

// decomp: D_800F22E4 (D_800F22E4@BATTLE/BATTLE.PRG)
u_char* Content_legacySequenceParseData;

// decomp: _etmData (_etmData@BATTLE/BATTLE.PRG)
u_short* Content_legacyEtmFileBuffer;

// decomp: _etmFile (_etmFile@BATTLE/BATTLE.PRG)
Content_DiscFile Content_legacyEtmFileLocation;

// decomp: _etmFileCdSlot (_etmFileCdSlot@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Content_legacyEtmDiscRequest;

// decomp: vs_battle_objectDataSlots (vs_battle_objectDataSlots@BATTLE/BATTLE.PRG)
Content_ActorAssetRequest Content_legacyActorAssetRequests[16];

// decomp: vs_battle_commonWepBuffs (vs_battle_commonWepBuffs@BATTLE/BATTLE.PRG)
Actors_WeaponModelData* Content_legacyWeaponModelBuffers[84];

