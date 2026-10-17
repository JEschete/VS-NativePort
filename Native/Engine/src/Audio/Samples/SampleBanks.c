// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Core/Memory/Copy.h"

// decomp: D_80036770 (D_80036770@SLUS_010.40)
Audio_MusicSampleBankIds Audio_Samples_musicSampleBankIds;

// decomp: g_InstrumentInfo (g_InstrumentInfo@SLUS_010.40)
Audio_Instrument Audio_Samples_instruments[256];

// decomp: D_800377E0 (D_800377E0@SLUS_010.40)
int Audio_Samples_soundEffectBankIds[3];

// decomp: D_80037850 (D_80037850@SLUS_010.40)
int Audio_Samples_bankChunkHeader[16];

// decomp: D_80039BC8 (D_80039BC8@SLUS_010.40)
Audio_BankStreamState Audio_Samples_bankStream;

// decomp: D_80039BCC (D_80039BCC@SLUS_010.40)
int Audio_Samples_bankStreamDestination;

// decomp: SLUS_010.40 func_80012BB8 (src/SLUS_010.40/25AC.c:719)
void Audio_Samples_LoadBank(void* arg0, int arg1)
{
    while (Audio_Samples_LoadDeclaredBank(arg0, arg1) == 1)
        ;
}

// decomp: SLUS_010.40 vs_sound_spuTransferring (src/SLUS_010.40/25AC.c:725)
int Audio_Samples_TransferStatus(void) { return Audio_Cutscene_legacySpuTransferBusy; }

// decomp: SLUS_010.40 func_80012C14 (src/SLUS_010.40/25AC.c:727)
int Audio_Samples_ChooseMusicBankDestination(int* arg0, int* arg1, int* arg2)
{
    int var_a3;

    if (Audio_Seq_activeMusicConfig->activeChannels == 0) {
        Audio_Seq_activeMusicConfig->statusFlags &= ~0x60;
    }

    var_a3 = 0;

    if ((arg0[7] >= 0x21U) || (arg0[5] > 0x20000U)) {
        Audio_Driver_controlLatches.mixBehavior &= 0xFFFEFFFF;
        Audio_Samples_musicSampleBankIds.alternateBankId = 0;
        Audio_Samples_musicSampleBankIds.bankId = 0;
    } else {
        if ((Audio_Seq_activeMusicConfig->statusFlags & 0x20)
            || (!(Audio_Seq_activeMusicConfig->statusFlags & 0x40) && (Audio_Driver_mixBehavior & 0x10000))) {
            var_a3 = 1;
            Audio_Driver_controlLatches.mixBehavior &= 0xFFFEFFFF;
        } else {
            Audio_Driver_controlLatches.mixBehavior |= 0x10000;
        }
    }
    if (var_a3 == 0) {
        *arg1 = 0x40;
        *arg2 = 0x35100;
        Audio_Samples_musicSampleBankIds.bankId = arg0[1];
        if (Audio_Samples_musicSampleBankIds.alternateBankId == arg0[1]) {
            Audio_Samples_musicSampleBankIds.alternateBankId = 0;
        }
    } else {
        *arg1 = 0x60;
        *arg2 = 0x55100;
        Audio_Samples_musicSampleBankIds.alternateBankId = arg0[1];
        if (Audio_Samples_musicSampleBankIds.bankId == arg0[1]) {
            Audio_Samples_musicSampleBankIds.bankId = 0;
        }
    }
    return var_a3;
}

// decomp: SLUS_010.40 func_80012D9C (src/SLUS_010.40/25AC.c:768)
int Audio_Samples_ChooseSoundEffectBankDestination(int* arg0, int* arg1, int* arg2, int arg3)
{
    u_int i;

    for (i = 0; i < 3; ++i) {
        if (Audio_Samples_soundEffectBankIds[i] == arg0[1]) {
            Audio_Samples_soundEffectBankIds[i] = 0;
        }
    }
    switch (arg3) {
    case 1:
        *arg1 = 0xA0;
        *arg2 = 0x13100;
        Audio_Samples_soundEffectBankIds[1] = arg0[1];
        break;
    case 2:
        *arg1 = 0xC0;
        *arg2 = 0x1B100;
        Audio_Samples_soundEffectBankIds[2] = arg0[1];
        break;
    default:
        *arg1 = 0x80;
        *arg2 = 0xB100;
        Audio_Samples_soundEffectBankIds[0] = arg0[1];
        break;
    }
    return arg3;
}

// decomp: SLUS_010.40 func_80012E50 (src/SLUS_010.40/25AC.c:797)
int Audio_Samples_SelectMusicBank(int arg0)
{
    if (Audio_Samples_musicSampleBankIds.bankId == arg0) {
        Audio_Driver_controlLatches.mixBehavior |= 0x10000;
    } else if (Audio_Samples_musicSampleBankIds.alternateBankId == arg0) {
        Audio_Driver_controlLatches.mixBehavior &= 0xFFFEFFFF;
    }
    return Audio_Driver_mixBehavior & 0x10000;
}

// decomp: SLUS_010.40 func_80012EBC (src/SLUS_010.40/25AC.c:807)
int Audio_Samples_ClearMusicBankIds(void)
{
    Audio_Driver_controlLatches.mixBehavior &= ~0x10000;
    Audio_Samples_musicSampleBankIds.alternateBankId = 0;
    Audio_Samples_musicSampleBankIds.bankId = 0;
    return 0;
}

// decomp: SLUS_010.40 func_80012EF0 (src/SLUS_010.40/25AC.c:815)
int Audio_Samples_BeginBankStream(void)
{
    Audio_Samples_bankStreamDestination = 0;
    Audio_Driver_controlLatches.controlLatches |= 1;
    return 0;
}

// decomp: SLUS_010.40 func_80012F10 (src/SLUS_010.40/25AC.c:822)
int Audio_Samples_UploadBankChunk(int* in_Data, u_int in_Size, int in_Slot, int in_Wait)
{
    int instrumentIndex;
    int spuAddr;
    u_int chunk;
    u_int spuChunk;

    if (Audio_Driver_controlLatches.controlLatches & 1) {
        if (Audio_Samples_bankStream.spuAddress == 0) {
            if (Audio_Samples_SequenceMagicDifference(in_Data) == 0) {
                if (in_Data[4] == 0x35100) {
                    Audio_Samples_ChooseMusicBankDestination(in_Data, &instrumentIndex, &spuAddr);
                } else if (in_Data[4] == 0xB100) {
                    Audio_Samples_ChooseSoundEffectBankDestination(in_Data, &instrumentIndex, &spuAddr, in_Slot);
                } else {
                    spuAddr = in_Data[4];
                    instrumentIndex = in_Data[6];
                }
                Core_Memory_CopyWords(in_Data, Audio_Samples_bankChunkHeader, 0x40);
                in_Data += 0x10;
                in_Size -= 0x40;
                Audio_Samples_bankChunkHeader[4] = spuAddr;
                Audio_Samples_bankStream.spuAddress = spuAddr;
                Audio_Samples_bankChunkHeader[6] = instrumentIndex;
                Audio_Samples_bankStream.spuBytesRemaining = Audio_Samples_bankChunkHeader[5];
                Audio_Samples_bankStream.instrumentDestination = (int*)&Audio_Samples_instruments[instrumentIndex];
                Audio_Samples_bankStream.instrumentBytesRemaining =
                    Audio_Samples_bankChunkHeader[7] * sizeof(Audio_Instrument);
            } else {
                in_Size = 0;
                Audio_Samples_bankStream.spuBytesRemaining = 0;
                Audio_Samples_bankStream.instrumentBytesRemaining = 0;
            }
        }
        if ((Audio_Samples_bankStream.instrumentBytesRemaining != 0) && (in_Size != 0)) {
            chunk = Audio_Samples_bankStream.instrumentBytesRemaining;
            if (chunk >= in_Size) {
                chunk = in_Size;
            }
            // The original reuses the spuAddr stack slot for the chunk size.
            spuAddr = chunk;
            Core_Memory_CopyWords(in_Data, Audio_Samples_bankStream.instrumentDestination, spuAddr);
            in_Data += (u_int)spuAddr / 4;
            in_Size -= spuAddr;
            Audio_Samples_bankStream.instrumentDestination += (u_int)spuAddr / 4;
            Audio_Samples_bankStream.instrumentBytesRemaining -= spuAddr;
            if (Audio_Samples_bankStream.instrumentBytesRemaining == 0) {
                Audio_Samples_CopyRelocatedInstruments(&Audio_Samples_instruments[Audio_Samples_bankChunkHeader[6]],
                    &Audio_Samples_instruments[Audio_Samples_bankChunkHeader[6]], Audio_Samples_bankChunkHeader[4], Audio_Samples_bankChunkHeader[7]);
            }
        }
        if ((in_Size != 0) && (Audio_Samples_bankStream.spuBytesRemaining != 0)) {
            spuChunk = Audio_Samples_bankStream.spuBytesRemaining;
            if (spuChunk >= in_Size) {
                spuChunk = in_Size;
            }
            in_Size = spuChunk;
            Audio_Driver_WriteSampleMemory((char*)in_Data, in_Size);
            Audio_Samples_bankStream.spuAddress += in_Size;
            Audio_Samples_bankStream.spuBytesRemaining -= in_Size;
            if (in_Wait != 0) {
                Audio_Driver_WaitForSampleTransfer();
            }
        }
        if (Audio_Samples_bankStream.spuBytesRemaining == 0) {
            Audio_Driver_controlLatches.controlLatches &= ~1;
        }
    }
    return Audio_Samples_bankStream.spuBytesRemaining;
}

// decomp: SLUS_010.40 func_80013188 (src/SLUS_010.40/25AC.c:894)
int Audio_Samples_LoadMusicBank(void* arg0, int arg1)
{
    int sp10;
    int sp14;
    int temp_s0 = Audio_Samples_ChooseMusicBankDestination(arg0, &sp10, &sp14);
    Audio_Samples_UploadBank(arg0, arg1, sp10, sp14);
    return temp_s0;
}

// decomp: SLUS_010.40 func_800131DC (src/SLUS_010.40/25AC.c:903)
int Audio_Samples_LoadSoundEffectBank(void* arg0, int arg1, int arg2)
{
    int sp10;
    int sp14;

    Audio_Samples_ChooseSoundEffectBankDestination((int*)arg0, &sp10, &sp14, arg1);
    Audio_Samples_UploadBank(arg0, arg2, sp10, sp14);
    return 0;
}

// decomp: SLUS_010.40 Sound_CopyAndRelocateInstruments (src/SLUS_010.40/25AC.c:970)
void Audio_Samples_CopyRelocatedInstruments(Audio_Instrument* in_A,
    Audio_Instrument* in_B, int in_AddrOffset, int in_Count)
{
    do {
        in_B->sampleStart = in_A->sampleStart + in_AddrOffset;
        in_B->loopStart = in_A->loopStart + in_AddrOffset;
        *(int*)&in_B->fineTune = *(int*)&in_A->fineTune;
        *(int*)&in_B->adsrLower = *(int*)&in_A->adsrLower;
        in_A++;
        in_B++;
        in_Count--;
    } while (in_Count != 0);
}

// decomp: SLUS_010.40 Sound_IsNotAkaoFile (src/SLUS_010.40/25AC.c:984)
int Audio_Samples_SequenceMagicDifference(int* data)
{
    return data[0] - ('A' | ('K' << 8) | ('A' << 16) | ('O' << 24));
}

// decomp: SLUS_010.40 func_80013588 (src/SLUS_010.40/25AC.c:1023)
int Audio_Samples_LoadDeclaredBank(void* arg0, int arg1)
{
    if (Audio_Samples_SequenceMagicDifference(arg0) == 0) {
        Audio_Samples_UploadBank(arg0, arg1, ((int*)arg0)[6], ((int*)arg0)[4]);
        return 0;
    }
    return -1;
}

// decomp: SLUS_010.40 func_800135D8 (src/SLUS_010.40/25AC.c:1033)
int Audio_Samples_UploadBank(void* in_Data, int in_Wait, int in_FirstInstrument, int in_SpuAddr)
{
    Audio_AkaoSequenceFile* header;
    Audio_Instrument* instruments;

    Audio_Driver_WaitForSampleTransfer();
    if (Audio_Samples_SequenceMagicDifference(in_Data) == 0) {
        header = in_Data;
        in_Data = header->payload;
        instruments = in_Data;
        in_Data = instruments + header->instrumentDataOffset;
        Audio_Driver_WriteSampleMemory(in_Data, header->sampleBankId);
        Audio_Samples_CopyRelocatedInstruments(instruments,
            &Audio_Samples_instruments[in_FirstInstrument], in_SpuAddr, header->instrumentDataOffset);
        if (in_Wait != 0) {
            Audio_Driver_WaitForSampleTransfer();
        }
        return 0;
    }
    Audio_Cutscene_legacySpuTransferBusy = -1;
    return -1;
}

// decomp: src/SLUS_010.40/25AC.c:2291
#define SOUND_BANK_FLAG_ALT_SAMPLE_BANK (1u << 6) // 0x40
// decomp: src/SLUS_010.40/25AC.c:2294
#define SOUND_BANK_REMAP_BASE_INDEX 0x20u // first remappable instrument
// decomp: src/SLUS_010.40/25AC.c:2295
#define SOUND_BANK_REMAP_COUNT 0x60u // 64 instruments (0x20..0x5F)
// decomp: src/SLUS_010.40/25AC.c:2298
#define SOUND_BANK_SPU_ADDR_OFFSET 0x20000u
// decomp: SLUS_010.40 Sound_MapInstrumentToAltSampleBank (src/SLUS_010.40/25AC.c:2303)
u_short Audio_Samples_MapInstrumentToAlternateBank(u_int in_Flags, Audio_SeqChannel* in_pChannel)
{
    if (in_Flags & SOUND_BANK_FLAG_ALT_SAMPLE_BANK && 0x40 <= in_pChannel->instrumentIndex
        && in_pChannel->instrumentIndex < SOUND_BANK_REMAP_COUNT) {
        in_pChannel->voice.sampleStart += SOUND_BANK_SPU_ADDR_OFFSET;
        in_pChannel->voice.loopStart += SOUND_BANK_SPU_ADDR_OFFSET;
        in_pChannel->instrumentIndex +=
            SOUND_BANK_REMAP_BASE_INDEX; // mirror into alt-bank instrument table
    }
    return in_pChannel->instrumentIndex;
}
#undef SOUND_BANK_SPU_ADDR_OFFSET
#undef SOUND_BANK_REMAP_COUNT
#undef SOUND_BANK_REMAP_BASE_INDEX
#undef SOUND_BANK_FLAG_ALT_SAMPLE_BANK

// decomp: src/SLUS_010.40/25AC.c:2291
#define SOUND_BANK_FLAG_ALT_SAMPLE_BANK (1u << 6) // 0x40
// decomp: src/SLUS_010.40/25AC.c:2294
#define SOUND_BANK_REMAP_BASE_INDEX 0x20u // first remappable instrument
// decomp: src/SLUS_010.40/25AC.c:2298
#define SOUND_BANK_SPU_ADDR_OFFSET 0x20000u
// decomp: SLUS_010.40 Sound_MapInstrumentToBaseSampleBank (src/SLUS_010.40/25AC.c:2315)
u_short Audio_Samples_MapInstrumentToBaseBank(u_int in_Flags, Audio_SeqChannel* in_Channel)
{
    if ((in_Flags & SOUND_BANK_FLAG_ALT_SAMPLE_BANK)
        && 0x60 <= in_Channel->instrumentIndex && in_Channel->instrumentIndex < 0x80) {
        in_Channel->voice.sampleStart -= SOUND_BANK_SPU_ADDR_OFFSET;
        in_Channel->voice.loopStart -= SOUND_BANK_SPU_ADDR_OFFSET;
        in_Channel->instrumentIndex -= SOUND_BANK_REMAP_BASE_INDEX;
    }
    return in_Channel->instrumentIndex;
}
#undef SOUND_BANK_SPU_ADDR_OFFSET
#undef SOUND_BANK_REMAP_BASE_INDEX
#undef SOUND_BANK_FLAG_ALT_SAMPLE_BANK

// decomp: SLUS_010.40 func_80016DA8 (src/SLUS_010.40/25AC.c:2941)
int Audio_Samples_SoundEffectBankSlot(int flags)
{
    int var_v1 = 0;

    flags &= 0x7FFF;

    if (flags == 0) {
        return 0;
    }
    if (flags == Audio_Samples_soundEffectBankIds[1]) {
        var_v1 = 1;
    } else if (flags == Audio_Samples_soundEffectBankIds[2]) {
        var_v1 = 2;
    }
    return var_v1;
}

