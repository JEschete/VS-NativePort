// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SOUNDBANKS_H_TYPES
#define VS_AUDIO_SOUNDBANKS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_SoundReleaseView Audio_SoundReleaseView;
typedef struct Audio_SoundState Audio_SoundState;
typedef struct Content_DiscQueueSlot Content_DiscQueueSlot;

// decomp: func_8004644C_t (src/BATTLE/BATTLE.PRG/30DB0.c:29)
typedef struct Audio_SoundReleaseView {
    u_char reserved0[0x30];
    int* reserved30;
} Audio_SoundReleaseView;

// decomp: soundData_t (src/SLUS_010.40/main.h:135)
typedef struct Audio_SoundState {
    int currentMusicSlot;
    u_int playingMusicHandle;
    u_int currentMusicId;
    void* currentMusicData;
    char musicIds[4];
    u_int musicHandles[4];
    void* musicData[4];
    Content_DiscQueueSlot* musicRequests[4];
    u_int currentSfxBank;
    char sfxBankIds[4];
    void* sfxBankData[3];
    Content_DiscQueueSlot* sfxBankRequests[3];
    void* sfxStreamData[3];
    int* reserved70[3];
    int waveBankId;
    void* waveBankData;
    Content_DiscQueueSlot* waveBankRequest;
} Audio_SoundState;

#endif // VS_AUDIO_SOUNDBANKS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SOUNDBANKS_H_DECLS)
#define VS_AUDIO_SOUNDBANKS_H_DECLS

extern u_short Audio_menuSoundEffects[11264];
extern Audio_SoundState Audio_soundState;
extern char Audio_waveBankFileMap[];

#endif // VS_AUDIO_SOUNDBANKS_H_DECLS
