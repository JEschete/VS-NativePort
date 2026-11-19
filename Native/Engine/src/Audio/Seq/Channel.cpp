// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"

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
