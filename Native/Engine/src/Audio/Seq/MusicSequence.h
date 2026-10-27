// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SEQ_MUSICSEQUENCE_H_TYPES
#define VS_AUDIO_SEQ_MUSICSEQUENCE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_AUDIO_SEQ_MUSICSEQUENCE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SEQ_MUSICSEQUENCE_H_DECLS)
#define VS_AUDIO_SEQ_MUSICSEQUENCE_H_DECLS
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"

void Audio_Seq_ResetSoundEffectChannel(Audio_SeqChannel* channel, char* program_start);
u_int Audio_Seq_AssignedVoiceMask(Audio_SeqChannel* channel_cursor, u_int channel_mask);
void Audio_Seq_LoadMusicSequence(Audio_AkaoSequenceFile* in_Sequence);
void Audio_Seq_ReleaseMusicConfiguration(
    Audio_MusicConfig* in_Config, Audio_SeqChannel* in_pChannel, u_int music_id_filter);
void Audio_Seq_RestoreMusicSequence(Audio_AkaoSequenceFile* in_Sequence, int in_SwapWithSavedState);
void Audio_Seq_StartOrResumeMusic(Audio_CommandParams* in_Params);
void Audio_Seq_SaveCurrentMusic(void);
void Audio_Seq_StartMusicLayer(Audio_CommandParams* command);
void Audio_Seq_CrossfadeMusicLayers(Audio_CommandParams* command);
void Audio_Seq_StartMusicWithLoopCounter(Audio_CommandParams* in_Params);
void Audio_Seq_SetMusicVolume(Audio_CommandParams* command);
void Audio_Seq_FadeMusicVolume(Audio_CommandParams* command);
void Audio_Seq_FadeMusicVolumeFrom(Audio_CommandParams* command);
void Audio_Seq_SetDiscVolume(Audio_CommandParams* command);
void Audio_Seq_FadeDiscVolume(Audio_CommandParams* command);
void Audio_Seq_FadeDiscVolumeFrom(Audio_CommandParams* command);

#endif // VS_AUDIO_SEQ_MUSICSEQUENCE_H_DECLS
