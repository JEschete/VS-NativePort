// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SAMPLES_SAMPLEBANKS_H_TYPES
#define VS_AUDIO_SAMPLES_SAMPLEBANKS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Audio_BankStreamState Audio_BankStreamState;
typedef struct Audio_Instrument Audio_Instrument;
typedef struct Audio_MusicSampleBankIds Audio_MusicSampleBankIds;

// decomp: D_80039BCC (D_80039BCC@SLUS_010.40): a view into another global
#define Audio_Samples_bankStreamDestination (*(int*)(&Audio_Samples_bankStream.spuAddress))

// decomp: D_80036770_t (src/SLUS_010.40/25AC.c:22)
typedef struct Audio_MusicSampleBankIds {
    int bankId;
    int alternateBankId;
    int reserved8;
    int reservedC;
} Audio_MusicSampleBankIds;
_Static_assert(sizeof(Audio_MusicSampleBankIds) == 0x10, "Audio_MusicSampleBankIds must keep its PS1 layout");

// decomp: D_80039BC8_t (src/SLUS_010.40/25AC.c:73)
typedef struct Audio_BankStreamState {
    int* instrumentDestination; // next g_InstrumentInfo word to fill
    int spuAddress; // next SPU transfer address; 0 = start a new stream
    u_int spuBytesRemaining;
    u_int instrumentBytesRemaining;
} Audio_BankStreamState;

// decomp: FSoundInstrumentInfo (src/include/sound.h:257)
typedef struct Audio_Instrument {
    /* 0x00 */ u_int sampleStart;
    /* 0x04 */ u_int loopStart;
    /* 0x08 */ short fineTune;
    /* 0x0A */ short rootNote;
    /* 0x0C */ u_short adsrLower;
    /* 0x0E */ u_short adsrUpper;
} Audio_Instrument;
_Static_assert(sizeof(Audio_Instrument) == 0x10, "Audio_Instrument must keep its PS1 layout");

#endif // VS_AUDIO_SAMPLES_SAMPLEBANKS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SAMPLES_SAMPLEBANKS_H_DECLS)
#define VS_AUDIO_SAMPLES_SAMPLEBANKS_H_DECLS
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"

extern Audio_MusicSampleBankIds Audio_Samples_musicSampleBankIds;
extern Audio_Instrument Audio_Samples_instruments[256];
extern int Audio_Samples_soundEffectBankIds[3];
extern int Audio_Samples_bankChunkHeader[16];
extern Audio_BankStreamState Audio_Samples_bankStream;
void Audio_Samples_LoadBank(void* bank_data, int wait_for_transfer);
int Audio_Samples_TransferStatus(void);
int Audio_Samples_ChooseMusicBankDestination(int* bank_header, int* output_first_instrument, int* output_sample_address);
int Audio_Samples_ChooseSoundEffectBankDestination(int* bank_header, int* output_first_instrument, int* output_sample_address, int bank_slot);
int Audio_Samples_SelectMusicBank(int bank_id);
int Audio_Samples_ClearMusicBankIds(void);
int Audio_Samples_BeginBankStream(void);
int Audio_Samples_UploadBankChunk(int* in_Data, u_int in_Size, int in_Slot, int in_Wait);
int Audio_Samples_LoadMusicBank(void* bank_data, int wait_for_transfer);
int Audio_Samples_LoadSoundEffectBank(void* bank_data, int bank_slot, int wait_for_transfer);
void Audio_Samples_CopyRelocatedInstruments(Audio_Instrument* in_A,
    Audio_Instrument* in_B, int in_AddrOffset, int in_Count);
int Audio_Samples_SequenceMagicDifference(int* data);
int Audio_Samples_LoadDeclaredBank(void* bank_data, int wait_for_transfer);
u_short Audio_Samples_MapInstrumentToAlternateBank(u_int in_Flags, Audio_SeqChannel* in_pChannel);
u_short Audio_Samples_MapInstrumentToBaseBank(u_int in_Flags, Audio_SeqChannel* in_Channel);
int Audio_Samples_SoundEffectBankSlot(int flags);

#endif // VS_AUDIO_SAMPLES_SAMPLEBANKS_H_DECLS
