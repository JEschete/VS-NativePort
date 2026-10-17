// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_CUTSCENE_STREAM_H_TYPES
#define VS_AUDIO_CUTSCENE_STREAM_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_CutsceneStreamData Audio_CutsceneStreamData;
typedef struct Audio_CutsceneStreamState Audio_CutsceneStreamState;

// decomp: FSoundCutsceneStreamState (src/include/sound.h:597)
typedef struct Audio_CutsceneStreamState {
    /* 0x00 */ void* currentChunk;
    /* 0x04 */ void* auxiliaryMask;
    /* 0x08 */ int formatFlags;
    /* 0x0C */ int voiceMask;
    /* 0x10 */ int voiceIndex;
    /* 0x14 */ u_int totalPages; // Probably a bad name judged on the usage in
                                 // Sound_Cutscene_BeginPlayback
    /* 0x18 */ int currentPage;
    /* 0x1C */ int reserved1C;
    /* 0x20 */ int reserved20;
    /* 0x24 */ int reserved24;
    /* 0x28 */ int pageIndex;
    /* 0x2C */ Audio_CutsceneStreamData* streamBase;
    /* 0x30 */ int reserved30;
    /* 0x34 */ u_int streamPageIndex;
    /* 0x38 */ int reserved38;
    /* 0x3C */ u_int pageRingSize;
    /* 0x40 */ int volume;
    /* 0x44 */ int volumeFadeStep;
    /* 0x48 */ int volumeFadeTicksLeft;
    /* 0x4C */ union {
        int word;
        u_char bytes[4];
    } pan;
    /* 0x50 */ int reserved50;
    /* 0x54 */ int reserved54;
    /* 0x58 */ int sampleRate;
    /* 0x5C */ int controlFlags;
} Audio_CutsceneStreamState;

#endif // VS_AUDIO_CUTSCENE_STREAM_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_CUTSCENE_STREAM_H_DECLS)
#define VS_AUDIO_CUTSCENE_STREAM_H_DECLS
#include "Audio/Mixer/MixControl.h"

extern volatile int Audio_Cutscene_legacySpuTransferBusy;
extern Audio_CutsceneStreamState Audio_Cutscene_streamState;
extern int Audio_Cutscene_streamVoices;
extern int Audio_Cutscene_streamVolume;
extern int Audio_Cutscene_streamControlFlags;
void Audio_Cutscene_PauseStream(Audio_CommandParams* in_Params __attribute__((unused)));
void Audio_Cutscene_ResumeStream(Audio_CommandParams* in_Params __attribute__((unused)));
void Audio_Cutscene_StopStreamVoices(void);
int Audio_Cutscene_FindFreeVoicePair(void);
void Audio_Cutscene_UploadSecondMonoBuffer(void);
void Audio_Cutscene_StartStream(u_int* arg0, int arg1, int arg2);
void Audio_Cutscene_InitializeStreamVoice(
    int in_Voice, int in_PanMode, int in_StartAddr, int in_RepeatAddr);
void Audio_Cutscene_BeginStreamPlayback(
    int in_SomeIndex, int in_SampleAddr, void (*in_Callback)());
void Audio_Cutscene_BeginMonoPlayback(void);
void Audio_Cutscene_BeginStereoPlayback(void);
void Audio_Cutscene_RefillStreamBuffer(
    int in_RepeatAddressL, int in_RepeatAddressR, int in_Param3, void (*in_IrqCallback)());
void Audio_Cutscene_RefillMonoFirstBuffer(void);
void Audio_Cutscene_RefillMonoSecondBuffer(void);
void Audio_Cutscene_RefillStereoFirstBuffer(void);
void Audio_Cutscene_RefillStereoSecondBuffer(void);
void Audio_Cutscene_ReplaceStream(int* arg0);
void Audio_Cutscene_StopStream(void);
void Audio_Cutscene_SetStreamVolume(Audio_CommandParams* in_Params);
void Audio_Cutscene_SlideStreamVolume(Audio_CommandParams* in_Params);
void Audio_Cutscene_SetStreamPan(int* arg0);

#endif // VS_AUDIO_CUTSCENE_STREAM_H_DECLS
