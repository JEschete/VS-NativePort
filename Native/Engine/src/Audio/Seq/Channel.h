// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Audio_AkaoSequenceFile;
struct Audio_KeymapEntry;
struct Audio_MusicConfig;
struct Audio_SeqChannel;

#include "Audio/Mixer/VoiceMixer.h"

// decomp: D_800358FE (D_800358FE@SLUS_010.40): a view into another global
#define Audio_Seq_savedMusicId (*(short*)(&Audio_Seq_savedMusicConfig.musicId))

// decomp: FSoundChannel (src/include/sound.h:410)
struct Audio_SeqChannel {
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
    uintptr_t effectId;
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
};

// decomp: FSoundChannelConfig (src/include/sound.h:492)
struct Audio_MusicConfig {
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
};

#include "Audio/Mixer/MixControl.h"

extern Audio_SeqChannel Audio_Seq_musicChannels[0x20];
extern Audio_MusicConfig Audio_Seq_savedMusicConfig;
extern Audio_SeqChannel* Audio_Seq_secondaryMusicChannels;
extern Audio_MusicConfig* Audio_Seq_secondaryMusicConfig;
extern Audio_MusicConfig Audio_Seq_primaryMusicConfig;
extern Audio_CommandParams Audio_Seq_pendingMusicData;
extern u_int Audio_Seq_musicLoopsRemaining;
extern Audio_MusicConfig* Audio_Seq_activeMusicConfig;
extern Audio_SeqChannel Audio_Seq_savedMusicChannels[32];
