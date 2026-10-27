// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SEQ_SEQUENCER_H_TYPES
#define VS_AUDIO_SEQ_SEQUENCER_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_FadeTimer Audio_FadeTimer;

// decomp: D_800378D4 (D_800378D4@SLUS_010.40): a view into another global
#define Audio_Seq_musicTransitionTimer (*(int*)(&Audio_Driver_commandArguments.extParam2))

// decomp: FSoundFadeTimer (src/include/sound.h:514)
typedef struct Audio_FadeTimer {
    /* 0x00 */ int value;
    /* 0x04 */ int step;
    /* 0x08 */ int ticksRemaining;
    /* 0x0C */ int savedValue;
} Audio_FadeTimer;
_Static_assert(sizeof(Audio_FadeTimer) == 0x10, "Audio_FadeTimer must keep its PS1 layout");

#endif // VS_AUDIO_SEQ_SEQUENCER_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SEQ_SEQUENCER_H_DECLS)
#define VS_AUDIO_SEQ_SEQUENCER_H_DECLS
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"

extern u_short Audio_Seq_tickCounter;
extern u_char Audio_Seq_kOpcodeLengths[152];
extern u_char Audio_Seq_kExtendedOpcodeLengths[56];
extern u_short Audio_Seq_kNoteLengths[12];
extern u_int Audio_Seq_kIdleProgram;
extern u_int Audio_Seq_kSemitonePitches[88];
extern char Audio_Seq_kNoteRemapTables[304];
extern char Audio_Seq_kRandomPitchOffsets[256];
extern short* Audio_Seq_kLfoWaveforms[256];
extern void (*Audio_Seq_kOpcodeHandlers[96])(Audio_SeqChannel*, int);
extern void (*Audio_Seq_kExtendedOpcodeHandlers[32])(Audio_SeqChannel*, int);
extern Audio_CommandParams Audio_Seq_embeddedSoundEffectParams;
extern Audio_FadeTimer Audio_Seq_masterFade;
extern int Audio_Seq_randomPitchPhase;
void Audio_Seq_UpdateGlobalSlides(void);
void Audio_Seq_AdvanceMusicTransition(void);
int Audio_Seq_UpdateMusicSequence(Audio_SeqChannel* channels, int secondary_music);
void Audio_Seq_UpdateSavedMusicFade(Audio_MusicConfig* music_config, Audio_SeqChannel* channels);
long Audio_Seq_TickPlayback(void);
char Audio_Seq_PeekNextNote(Audio_SeqChannel* channel);
int Audio_Seq_ResolveSoundEffectInstrument(int sample_bank_flags, int instrument_index);
int Audio_Seq_ResolveMusicInstrument(int music_status_flags, u_int instrument_index);
void Audio_Seq_SelectNotePatch(Audio_SeqChannel* channel, u_int note_key);
int Audio_Seq_CalculateNotePitch(Audio_Instrument* instrument, int note_cursor, u_int fine_tune, int* output_fine_pitch_delta);
int Audio_Seq_StartDrumNote(Audio_SeqChannel* channel, int channel_bit, int drum_key);
void Audio_Seq_ExecuteChannelStep(Audio_SeqChannel* channel, int channel_bit);
void Audio_Seq_ApplyInstrumentParameters(
    Audio_SeqChannel* channel, Audio_Instrument* instrument, int sample_start_address);
void Audio_Seq_SelectChannelInstrument(Audio_SeqChannel* channel, int instrument_index);
void Audio_Seq_ReleaseSoundEffectVoice(Audio_SeqChannel* channel, int voice_bit);

#endif // VS_AUDIO_SEQ_SEQUENCER_H_DECLS
