// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "State/Progress.h"
#include "State/StateFlags.h"

// decomp: g_ActiveMusicChannels (g_ActiveMusicChannels@SLUS_010.40)
Audio_SeqChannel Audio_Seq_musicChannels[0x20];

// decomp: g_PushedMusicConfig (g_PushedMusicConfig@SLUS_010.40)
Audio_MusicConfig Audio_Seq_savedMusicConfig;

// decomp: g_pSecondaryMusicChannels (g_pSecondaryMusicChannels@SLUS_010.40)
Audio_SeqChannel* Audio_Seq_secondaryMusicChannels;

// decomp: g_pSavedMousicConfig (g_pSavedMousicConfig@SLUS_010.40)
Audio_MusicConfig* Audio_Seq_secondaryMusicConfig;

// decomp: D_800366F0 (D_800366F0@SLUS_010.40)
Audio_MusicConfig Audio_Seq_primaryMusicConfig;

// decomp: D_80037788 (D_80037788@SLUS_010.40)
Audio_CommandParams Audio_Seq_pendingMusicData;

// decomp: g_Music_LoopCounter (g_Music_LoopCounter@SLUS_010.40)
u_int Audio_Seq_musicLoopsRemaining;

// decomp: g_pActiveMusicConfig (g_pActiveMusicConfig@SLUS_010.40)
Audio_MusicConfig* Audio_Seq_activeMusicConfig;

// decomp: D_800378E8 (D_800378E8@SLUS_010.40)
Audio_SeqChannel Audio_Seq_savedMusicChannels[32];

// decomp: vs_main_materialModifiers (vs_main_materialModifiers@SLUS_010.40)
char Audio_Seq_unusedMaterialModifiers[8][30]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_main_scoredata (vs_main_scoredata@SLUS_010.40)
State_ScoreData Audio_Seq_unusedScoredata;

// decomp: vs_main_artsStatus (vs_main_artsStatus@SLUS_010.40)
State_ArtsStatus Audio_Seq_unusedArtsStatus;

// decomp: vs_main_inventory (vs_main_inventory@SLUS_010.40)
Items_Inventory Audio_Seq_unusedInventory;

// decomp: vs_main_stateFlags (vs_main_stateFlags@SLUS_010.40)
State_StateFlags Audio_Seq_unusedStateFlags;

// decomp: vs_main_inventoryIndices (vs_main_inventoryIndices@SLUS_010.40)
Items_InventoryIndices Audio_Seq_unusedInventoryIndices;

}  // extern "C"
