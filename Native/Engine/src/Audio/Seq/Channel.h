// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SEQ_CHANNEL_H_TYPES
#define VS_AUDIO_SEQ_CHANNEL_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_AkaoSequenceFile Audio_AkaoSequenceFile;
typedef struct Audio_KeymapEntry Audio_KeymapEntry;
typedef struct Audio_MusicConfig Audio_MusicConfig;
typedef struct Audio_SeqChannel Audio_SeqChannel;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_AUDIO_SEQ_CHANNEL_H_SET_TYPES_ONLY
#endif
#include "Audio/Mixer/VoiceMixer.h"
#ifdef VS_AUDIO_SEQ_CHANNEL_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_AUDIO_SEQ_CHANNEL_H_SET_TYPES_ONLY
#endif

// decomp: src/include/sound.h:42
#define SOUND_CHANNEL_TYPE_MUSIC 0x0

// decomp: src/include/sound.h:50
#define SOUND_SFX_LEGATO 0x1

// decomp: src/include/sound.h:51
#define SOUND_SFX_FULL_LENGTH 0x4

// decomp: src/include/sound.h:69
#define SOUND_UPDATE_DRUM_MODE (1 << 3)

// decomp: src/include/sound.h:72
#define SOUND_UPDATE_UNKNOWN_01 (1 << 6)

// decomp: src/include/sound.h:75
#define SOUND_UPDATE_UNKNOWN_12 (1 << 12)

// decomp: src/include/sound.h:78
#define SOUND_UPDATE_UNKNOWN_20 (1 << 20)

// decomp: src/include/sound.h:79
#define SOUND_UPDATE_UNKNOWN_24 (1 << 24)

// decomp: src/include/sound.h:80
#define SOUND_UPDATE_UNKNOWN_27 (1 << 27)

// decomp: src/include/sound.h:81
#define SOUND_UPDATE_UNKNOWN_28 (1 << 28)

// decomp: FSoundChannel (src/include/sound.h:410)
typedef struct Audio_SeqChannel {
    char* programCounter;
    char* loopStarts[(4)];
    char* returnAddress;
    char* keymap;
    short* vibratoWave;
    short* tremoloWave;
    short* autoPanWave;
    int effectTags;
    int pitchBase;
    int pitchSlide;
    u_int updateFlags;
    int effectVoiceMask;
    int effectId;
    int effectPitchScale;
    u_int effectPitchScaleSlideStep;
    int volume;
    int volumeSlideStep;
    int pitchSlideStep;
    int finePitchDelta;
    int priority;
    int keyOnVolume;
    int keyOnVolumeSlideStep;
    u_short type;
    u_short noteLength;
    u_short gateLength;
    u_short instrumentIndex;
    u_short effectPan;
    u_short effectPanSlideLength;
    u_short opcodeStepCounter;
    u_short loopIterations[(4)];
    short loopStepSnapshots[(4)];
    u_short volumeBalance;
    u_short volumeBalanceSlideLength;
    u_short effectPitchScaleSlideLength;
    u_short volumeSlideLength;
    u_short keyOnVolumeSlideLength;
    u_short effectVolumeSlideLength;
    u_short pan;
    u_short panSlideLength;
    u_short pitchSlideTicksLeft;
    u_short octave;
    u_short pitchBendSlideLength;
    u_short portamentoKey;
    u_short portamentoSteps;
    u_short playModes;
    short vibratoDelay;
    u_short vibratoDelayLeft;
    u_short vibratoRate;
    u_short vibratoRateCounter;
    u_short vibratoShape;
    u_short vibratoBase;
    u_short vibratoDepth;
    u_short vibratoDepthSlideLength;
    u_short vibratoDepthSlideStep;
    short tremoloDelay;
    u_short tremoloDelayLeft;
    u_short tremoloRate;
    short tremoloRateCounter;
    u_short tremoloShape;
    u_short tremoloDepth;
    u_short tremoloDepthSlideLength;
    short tremoloDepthSlideStep;
    u_short autoPanRate;
    u_short autoPanRateCounter;
    u_short autoPanShape;
    u_short autoPanDepth;
    u_short autoPanDepthSlideLength;
    short autoPanDepthSlideStep;
    u_short noiseTimer;
    u_short fmTimer;
    u_short loopDepth;
    u_short randomPitchDepth;
    short storedNoteLength;
    short fixedNoteLength;
    short volumeBalanceSlideStep;
    short effectPanSlideStep;
    short effectVolume;
    short effectVolumeSlideStep;
    short panSlideStep;
    short transpose;
    short fineTune;
    short key;
    short pitchBendTarget;
    short portamentoTranspose;
    short vibratoPitch;
    short tremoloVolume;
    short autoPanOffset;
    Audio_VoiceParams voice;
} Audio_SeqChannel;

// decomp: FSoundChannelConfig (src/include/sound.h:492)
typedef struct Audio_MusicConfig {
    int statusFlags;
    int activeChannels;
    u_int keyedVoices;
    int allocatedVoices;
    int pendingKeyOn;
    u_int activeNotes;
    int pendingKeyOff;
    u_int pausedChannels;
    int tempo;
    int tempoSlideStep;
    u_int tickAccumulator;
    u_int resumeKeyOnMask;
    int pushReleaseMask;
    int reserved34;
    uintptr_t sequenceBase;
    Audio_AkaoSequenceFile* patchTable;
    u_short* keymapTable;
    Audio_KeymapEntry* noteRemapTable;
    int voiceSearchStart;
    u_int noiseChannels;
    u_int reverbChannels;
    u_int fmChannels;
    int reverbDepth;
    int reverbDepthSlideStep;
    int masterVolume;
    int masterVolumeStep;
    short masterVolumeSlideLength;
    short reverbDepthSlideLength;
    u_short tempoSlideLength;
    u_short musicId;
    u_short jumpThreshold;
    u_short noiseClock;
    u_short timerUpper;
    u_short timerUpperCount;
    u_short timerLower;
    u_short timerLowerCount;
    u_short timerTop;
    int reserved80;
    int reserved84;
    int reserved88;
    int reserved8C;
    int reserved90;
    int reserved94;
    int reserved98;
    int reserved9C;
} Audio_MusicConfig;

#endif // VS_AUDIO_SEQ_CHANNEL_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SEQ_CHANNEL_H_DECLS)
#define VS_AUDIO_SEQ_CHANNEL_H_DECLS
#include "Audio/Mixer/MixControl.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "State/Progress.h"
#include "State/StateFlags.h"

extern Audio_SeqChannel Audio_Seq_musicChannels[0x20];
extern Audio_MusicConfig Audio_Seq_savedMusicConfig;
extern short Audio_Seq_savedMusicId;
extern Audio_SeqChannel* Audio_Seq_secondaryMusicChannels;
extern Audio_MusicConfig* Audio_Seq_secondaryMusicConfig;
extern Audio_MusicConfig Audio_Seq_primaryMusicConfig;
extern Audio_CommandParams Audio_Seq_pendingMusicData;
extern u_int Audio_Seq_musicLoopsRemaining;
extern Audio_MusicConfig* Audio_Seq_activeMusicConfig;
extern Audio_SeqChannel Audio_Seq_savedMusicChannels[32];
extern char Audio_Seq_unusedMaterialModifiers[8][30];
extern State_ScoreData Audio_Seq_unusedScoredata;
extern State_ArtsStatus Audio_Seq_unusedArtsStatus;
extern Items_Inventory Audio_Seq_unusedInventory;
extern State_StateFlags Audio_Seq_unusedStateFlags;
extern Items_InventoryIndices Audio_Seq_unusedInventoryIndices;
void Audio_Seq_SetVolumeBalance(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SlideVolumeBalance(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetVolume(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SlideVolume(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetKeyOnVolumeRamp(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableFullNoteLength(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_DisableFullNoteLength(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetEffectVolumeScale(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SlideEffectVolumeScale(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetPan(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SlidePan(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetOctave(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_IncreaseOctave(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_DecreaseOctave(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_LoadInstrument(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_LoadSilentInstrument(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SelectPatch(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetTranspose(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_AddTranspose(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetPitchBendSlide(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnablePortamento(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_DisablePortamento(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetFineTune(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_AddFineTune(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableLegato(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableSustainedSoundEffect(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetNextNoteLength(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_SetFixedNoteLength(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableDrumKeymap(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_MuteInstrument(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));
void Audio_Seq_EnableLoopBoundaryStop(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)));

#endif // VS_AUDIO_SEQ_CHANNEL_H_DECLS
