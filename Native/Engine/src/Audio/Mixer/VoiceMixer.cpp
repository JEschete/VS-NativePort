// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"

// decomp: src/SLUS_010.40/25AC.c:173
#define SPU_PAN_TABLE_SIZE (0x100)
// decomp: g_Sound_StereoPanGainTableQ15 (g_Sound_StereoPanGainTableQ15@SLUS_010.40)
short Audio_Mixer_kPanGains[SPU_PAN_TABLE_SIZE]; /* contents: module image (ModuleData.cpp) */
#undef SPU_PAN_TABLE_SIZE

// decomp: D_800301E0 (D_800301E0@SLUS_010.40)
Audio_CommandHandler Audio_Mixer_kCommandHandlers[256]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: g_SpuVoiceInfo (g_SpuVoiceInfo@SLUS_010.40)
Audio_VoiceEnvelope Audio_Mixer_voiceEnvelopes[VOICE_COUNT];

// decomp: D_80036778 (D_80036778@SLUS_010.40)
int Audio_Mixer_musicPitchScaleStep;

// decomp: D_8003677C (D_8003677C@SLUS_010.40)
int Audio_Mixer_tempoScaleStep;

// decomp: D_80036780 (D_80036780@SLUS_010.40)
short Audio_Mixer_musicPitchScaleSlideLength;

// decomp: D_80036782 (D_80036782@SLUS_010.40)
short Audio_Mixer_tempoScaleSlideLength;

// decomp: D_80037790 (D_80037790@SLUS_010.40)
Audio_CommandParams Audio_Mixer_commandParams;

// decomp: g_Sound_VoiceSchedulerState (g_Sound_VoiceSchedulerState@SLUS_010.40)
Audio_VoiceScheduler Audio_Mixer_voiceScheduler;

// decomp: D_800378E4 (D_800378E4@SLUS_010.40)
int Audio_Mixer_mutedMusicChannels;

// decomp: g_Sound_MasterPitchScaleQ16_16 (g_Sound_MasterPitchScaleQ16_16@SLUS_010.40)
int Audio_Mixer_musicPitchScale;

// decomp: g_Sound_TempoMultiplier (g_Sound_TempoMultiplier@SLUS_010.40)
int Audio_Mixer_tempoScale;

// decomp: g_Sound_VoiceChannelConfigs (g_Sound_VoiceChannelConfigs@SLUS_010.40)
Audio_MusicConfig* Audio_Mixer_voiceOwners[VOICE_COUNT];

// decomp: g_Sound_VoiceModeFlags (g_Sound_VoiceModeFlags@SLUS_010.40)
Audio_VoiceModeMasks Audio_Mixer_voiceModes;

}  // extern "C"
