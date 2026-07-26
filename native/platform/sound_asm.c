/* Sound-driver functions that are still assembly in the decomp, reimplemented
 * in native-functional C. The PSX build keeps the assembly; only the native
 * target uses this. Same pattern as native/platform/battle_asm.c.
 *
 * Source of truth: build/src/SLUS_010.40/nonmatchings/25AC/*.s
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- sound driver state ----
 *
 * These two live here rather than in the generated stub layer because the stub
 * generator gives every unknown data symbol a flat 4096-byte blob, and the
 * init loops below stride past that: g_ActiveMusicChannels is walked 0x20 times
 * at 0x110 bytes each (0x2200 bytes) and D_80035910 0x18 times. A short blob
 * would have the loops writing into whatever symbol was emitted next. Defining
 * them here at their true extents means the generator sees them as satisfied
 * and skips them. */
unsigned char g_ActiveMusicChannels[0x2400] __attribute__((aligned(16)));

/* D_80035910 is offset 0x10 into its block on purpose. func_800147CC reads
 * `channel[-0xC]` for a stereo-linked channel -- that is the *partner* channel's
 * VoiceParams.SampleRate, since 0x110 - 0xC = 0x104. The link flag is only ever
 * set on the second channel of a pair, so the read never actually reaches
 * before channel 0, but the address is still formed, and in the retail .bss
 * there is real memory there. The padding makes that true here too. */
unsigned char _sfxChannelBlock[0x10 + 0x2000] __attribute__((aligned(16)));

/* The secondary (pushed-aside) music channel block. Same reason as the two
 * above: Sound_Cmd_40_PushMusicState and command 0x16 both memcpy 0x2200 bytes
 * into it, and a 4096-byte generated blob is not big enough to hold that. */
unsigned char D_800378E8[0x2400] __attribute__((aligned(16)));

/* ---- restoring the retail .bss layout ----
 *
 * splat starts a new dlabel at every address something references, so a struct
 * that the C code treats as ONE object arrives from the disassembly as several
 * independent symbols. The stub generator then backs each of them with its own
 * zero blob, and code that reaches a field through the struct and code that
 * reaches the same field through the split label end up in unrelated memory.
 *
 * This is silent and it is everywhere in the sound driver. The instance that
 * cost the most to find: func_80019A58 decides whether to flush pending voice
 * state to the SPU with
 *
 *     if ((D_80039B00 | ...ActiveNoteMask | D_80037898) != 0) func_80015220();
 *
 * where D_80039B00 *is* g_Sound_GlobalFlags.UpdateFlags and D_80037898 *is*
 * g_Sound_VoiceSchedulerState.KeyedFlags. The sound VM sets those fields
 * through the structs; the test read two unrelated blobs that were always
 * zero, so the flush never ran and no voice ever keyed on.
 *
 * Equating each split label to base + its true offset restores the aliasing.
 * The containers are deliberately over-allocated -- only the relative offsets
 * have to be right, and the C headers size some of these structs generously.
 */
#define ALIAS(sym, base, off)                                                            \
    __asm__(".globl " #sym "\n.set " #sym ", " #base " + " #off "\n")

/* SfxSoundChannels is the tail of D_80035910: the driver treats all twelve
 * channels as one array and depends on the adjacency -- Sound_PlaySfxProgram
 * starts at SfxSoundChannels with voice bit 0x400000 and walks *backwards* into
 * D_80035910, and func_80012358 indexes `SfxSoundChannels - 10`. */
ALIAS(D_80035910, _sfxChannelBlock, 0x10);
ALIAS(SfxSoundChannels, _sfxChannelBlock, 0x10 + 0xAA0);
extern unsigned char D_80035910[]; /* defined by the ALIAS above */

unsigned char g_Sound_VoiceSchedulerState[0x40] __attribute__((aligned(16)));
ALIAS(D_80037894, g_Sound_VoiceSchedulerState, 0x04); /* KeyOnFlags       */
ALIAS(D_80037898, g_Sound_VoiceSchedulerState, 0x08); /* KeyedFlags       */
ALIAS(D_8003789C, g_Sound_VoiceSchedulerState, 0x0C); /* KeyOffFlags      */
ALIAS(D_800378A0, g_Sound_VoiceSchedulerState, 0x10); /* unk_Flags_0x10   */
ALIAS(D_800378B8, g_Sound_VoiceSchedulerState, 0x28);

unsigned char g_Sound_GlobalFlags[0x20] __attribute__((aligned(16)));
ALIAS(D_80039AFC, g_Sound_GlobalFlags, 0x04); /* MixBehavior  */
ALIAS(D_80039B00, g_Sound_GlobalFlags, 0x08); /* UpdateFlags  */

unsigned char g_Sound_Cutscene_StreamState[0x80] __attribute__((aligned(16)));
ALIAS(D_80039B14, g_Sound_Cutscene_StreamState, 0x0C); /* VoicesInUseFlags */
ALIAS(D_80039B48, g_Sound_Cutscene_StreamState, 0x40); /* Volume           */
ALIAS(D_80039B60, g_Sound_Cutscene_StreamState, 0x58); /* VoiceSampleRate  */
ALIAS(D_80039B64, g_Sound_Cutscene_StreamState, 0x5C); /* ControlFlags     */

unsigned char D_80036770[0x20] __attribute__((aligned(16)));
ALIAS(D_80036774, D_80036770, 0x04);
ALIAS(D_80036778, D_80036770, 0x08);
ALIAS(D_8003677C, D_80036770, 0x0C);

unsigned char D_800377E0[0x20] __attribute__((aligned(16)));
ALIAS(D_800377E4, D_800377E0, 0x04);
ALIAS(D_800377E8, D_800377E0, 0x08);

unsigned char g_Sound_MasterFadeTimer[0x20] __attribute__((aligned(16)));
ALIAS(D_80037844, g_Sound_MasterFadeTimer, 0x0C);

/* The music configs are 0x80 bytes, not the 0xA0 the header declares -- the
 * driver's own memcpy of one config is 0x80, and in the retail .bss the next
 * symbol is exactly 0x80 later. */
unsigned char g_PushedMusicConfig[0x100] __attribute__((aligned(16)));
ALIAS(D_800358FE, g_PushedMusicConfig, 0x6E);

extern unsigned char D_800366F0[]; /* active music config */
extern unsigned char g_Sound_VoiceModeFlags[];

extern void* g_pActiveMusicConfig;
extern void* g_pSavedMousicConfig;
extern void* g_pSecondaryMusicChannels;
extern void* D_800377FC;
extern unsigned g_Sound_LfoPhase;
extern unsigned D_80037788;
extern unsigned g_CdVolume;
extern unsigned short D_80036780;
extern unsigned short D_80036782;
extern unsigned g_Sound_MasterPitchScaleQ16_16;
extern unsigned g_Sound_TempoMultiplier;
extern unsigned short g_Sound_CdVolumeFadeLength;
extern unsigned g_Music_LoopCounter;
extern unsigned D_800378E4;
extern unsigned D_80039B64;

extern void Sound_SetReverbMode(int mode);
extern int  SpuSetReverb(int on_off);

#define U8(base, off)  (*(unsigned char*)((unsigned char*)(base) + (off)))
#define U16(base, off) (*(unsigned short*)((unsigned char*)(base) + (off)))
#define U32(base, off) (*(unsigned*)((unsigned char*)(base) + (off)))

/* SPU hardware registers. Backed by the mapped I/O page (scratchpad.c), so
 * these stores behave exactly as the original code expects them to. */
#define SPU_MVOL_L  (*(volatile unsigned short*)0x1F801D80)
#define SPU_MVOL_R  (*(volatile unsigned short*)0x1F801D82)
#define SPU_CDVOL_L (*(volatile unsigned short*)0x1F801DB0)
#define SPU_CDVOL_R (*(volatile unsigned short*)0x1F801DB2)
#define SPUCNT      (*(volatile unsigned short*)0x1F801DAA)

/* func_8001369C: reset the whole sound driver to its power-on state. Called
 * from StartSound() during _sysReinit, so it is squarely on the boot path.
 * Transcribed from 25AC/func_8001369C.s (167 instructions). */
void func_8001369C(void)
{
    unsigned char* cfg = D_800366F0;             /* $v1 */
    unsigned char* sch = g_Sound_VoiceSchedulerState;
    unsigned char* chan;
    unsigned short spucnt;
    int i;

    g_pActiveMusicConfig     = cfg;
    g_pSavedMousicConfig     = 0;
    D_800377FC               = g_ActiveMusicChannels;
    g_pSecondaryMusicChannels = 0;
    g_Sound_LfoPhase         = 0;
    D_80037788               = 0;

    U32(g_Sound_GlobalFlags, 0) = 0;
    U32(D_80036770, 4) = 0;
    U32(D_80036770, 0) = 0;
    U32(D_800377E0, 8) = 0;
    U32(D_800377E0, 4) = 0;
    U32(D_800377E0, 0) = 0;
    U32(g_Sound_GlobalFlags, 4) = 1;
    U32(sch, 0) = 0;

    U32(cfg, 0x04) = 0;
    U32(cfg, 0x08) = 0;
    U16(cfg, 0x6E) = 0;
    U32(sch, 0x10) = 0;
    U32(cfg, 0x1C) = 0;
    U16(g_PushedMusicConfig, 0x6E) = 0;
    U32(g_PushedMusicConfig, 0x04) = 0;
    U32(cfg, 0x60) = 0x007F0000;
    g_CdVolume = 0x7FFF0000;
    D_80036780 = 0;
    g_Sound_MasterPitchScaleQ16_16 = 0;
    D_80036782 = 0;
    g_Sound_TempoMultiplier = 0;
    U16(cfg, 0x68) = 0;
    g_Sound_CdVolumeFadeLength = 0;
    U32(sch, 0x1C) = 0;
    U32(cfg, 0x4C) = 0;
    U32(sch, 0x20) = 0;
    U32(cfg, 0x50) = 0;
    U32(sch, 0x24) = 0;

    spucnt = SPUCNT;                             /* read before the volume writes */
    SPU_MVOL_L  = 0x3FFF;
    SPU_MVOL_R  = 0x3FFF;
    SPU_CDVOL_L = 0x7FFF;
    SPU_CDVOL_R = 0x7FFF;

    g_Music_LoopCounter = 0;
    D_800378E4 = 0;
    D_80039B64 = 0;

    U32(cfg, 0x54) = 0;
    U16(cfg, 0x78) = 0;
    U16(cfg, 0x76) = 0;
    U16(cfg, 0x74) = 0;
    U16(cfg, 0x7C) = 0;
    U32(g_Sound_Cutscene_StreamState, 0x40) = 0x7F00;
    U32(g_Sound_Cutscene_StreamState, 0x48) = 0;
    U32(g_Sound_VoiceModeFlags, 0x8) = 0;
    U32(g_Sound_VoiceModeFlags, 0x4) = 0;
    U32(g_Sound_VoiceModeFlags, 0x0) = 0;

    SPUCNT = (unsigned short)((spucnt & 0xFFFA) | 1);

    /* 32 music channels, stride 0x110, starting 0x58 into the block. */
    chan = g_ActiveMusicChannels + 0x58;
    for (i = 0; i < 0x20; i++) {
        U32(chan, -0x24) = 0;
        U32(chan, 0x9C)  = 0x18;
        U16(chan, 0x0C)  = 0;
        U32(chan, 0x00)  = 0;
        chan += 0x110;
    }

    /* 12 SFX voices (indices 0xC..0x17), same stride, into D_80035910+0x8A. */
    {
        unsigned char* v = D_80035910 + 0x8A;
        int id;
        for (id = 0xC; id < 0x18; id++) {
            U32(v, -0x56) = 0;
            U32(v, 0x6A)  = (unsigned)id;
            U16(v, -0x26) = 1;
            U32(v, -0x32) = 0;
            U16(v, 0x52)  = 0x7F00;
            U16(v, 0x02)  = 0;
            U16(v, -0x04) = 0;
            U32(v, -0x4A) = 0;
            U16(v, 0x00)  = 0;
            v += 0x110;
        }
    }

    U32(cfg, 0x18) = 0;
    U32(cfg, 0x14) = 0;
    U32(cfg, 0x10) = 0;
    U32(sch, 0x18) = 1;
    U32(sch, 0x14) = 0x66A80000;
    U32(sch, 0x0C) = 0;
    U32(sch, 0x08) = 0;
    U32(sch, 0x04) = 0;
    U32(cfg, 0x58) = 0x03FFF000;
    U32(cfg, 0x5C) = 0;
    U16(cfg, 0x6A) = 0;

    U32(g_Sound_GlobalFlags, 8) |= 0x80;
    Sound_SetReverbMode(4);
    SpuSetReverb(1);
}

/* func_800135D8: upload an Akao instrument bank into SPU local memory.
 * Transcribed from 25AC/func_800135D8.s.
 *
 * An Akao bank is a 0x40-byte header, then `data[7]` instrument records of 16
 * bytes each, then the ADPCM sample data. The samples go to SPU RAM at
 * in_SpuAddr, and the records are copied into the driver's own instrument
 * table with their start and loop addresses relocated by that same offset.
 *
 * Three of the callees in 25AC.c are file-static (_waitTransferAvailable,
 * _writeSpu, Sound_IsNotAkaoFile), so they are restated here. The two that
 * matter are not: Sound_CopyAndRelocateInstruments and g_InstrumentInfo are
 * both global, so the real driver code does the relocation.
 *
 * This was the last thing standing between the software SPU and audible sound.
 * Until it landed, SPU RAM held 64 bytes (StartSound's flush buffer) and no
 * voice ever keyed on, because the driver had no instruments to play. */

/* Mirrors FSoundInstrumentInfo in the game headers; the driver indexes
 * g_InstrumentInfo with a 4-bit shift, so the record is 16 bytes. */
typedef struct {
    unsigned short StartAddr;
    unsigned short LoopAddr;
    unsigned short FineTune;
    unsigned short Pad;
    unsigned short AdsrLower;
    unsigned short AdsrUpper;
    unsigned short Unk0C;
    unsigned short Unk0E;
} FSoundInstrumentInfo_native;

extern FSoundInstrumentInfo_native g_InstrumentInfo[256];
extern volatile int _isSpuTransfer;
extern void Sound_CopyAndRelocateInstruments(void* in_A, void* in_B,
    int in_AddrOffset, int in_Count);

extern unsigned SpuSetTransferStartAddr(unsigned addr);
extern unsigned SpuWrite(unsigned char* addr, unsigned size);
extern void* SpuSetTransferCallback(void (*func)(void));

static void spu_write_complete(void)
{
    SpuSetTransferCallback(0);
    _isSpuTransfer = 0;
}

int func_800135D8(void* in_File, int in_Wait, int in_InstrumentIndex, int in_SpuAddr)
{
    FSoundInstrumentInfo_native* instruments = (FSoundInstrumentInfo_native*)in_File;
    int* data = (int*)in_File;

    /* Transfers complete synchronously here, so there is never anything to
     * wait for; the flag is maintained anyway because the driver polls it. */
    if (data[0] != ('A' | ('K' << 8) | ('A' << 16) | ('O' << 24))) {
        _isSpuTransfer = -1;
        return -1;
    }

    SpuSetTransferStartAddr((unsigned)in_SpuAddr);
    instruments += 4;                       /* past the 0x40-byte header */

    _isSpuTransfer = 1;
    SpuSetTransferCallback(spu_write_complete);
    SpuWrite((unsigned char*)instruments + (data[7] << 4), (unsigned)data[5]);

    Sound_CopyAndRelocateInstruments(instruments,
        &g_InstrumentInfo[in_InstrumentIndex], in_SpuAddr, data[7]);

    (void)in_Wait;
    return 0;
}

/* ---- the Akao command layer -----------------------------------------------
 *
 * func_80018C30 is the sound driver's front door. Every request the game makes
 * -- play SFX, start/stop music, set volume, drive the cutscene stream --
 * arrives here as a numbered command, is marshalled from D_800378C0 into
 * D_80037790, and is dispatched through a 256-entry handler table.
 *
 * It used to be a no-op stub (native/platform/battle_asm.c), which is why the
 * driver could have a full instrument bank in SPU RAM and a running 240Hz
 * sequencer tick and still never key on a voice: nothing behind the front door
 * was ever called. The three functions here are the rest of the SFX path that
 * was still assembly --
 *
 *   func_80018C30   command marshalling and dispatch
 *   func_8001653C   arm one voice channel from an Akao program
 *   func_800161C4   release voices, by mask / by owner / by priority
 *
 * -- and every handler the table points at was already decompiled C, with the
 * single exception of func_8001733C (command 0x22).
 *
 * Transcribed from build/src/SLUS_010.40/nonmatchings/25AC/*.s.
 */

#define AKAO_MAGIC 0x4F414B41 /* 'A' 'K' 'A' 'O' */

extern int  _soundEvent;
extern long DisableEvent(long id);
extern long EnableEvent(long id);

extern unsigned char D_80037790[]; /* FSoundCommandParams: the dispatch args  */
extern unsigned char D_800378C0[]; /* where callers stage their arguments     */
extern void* g_Sound_VoiceChannelConfigs[];

extern void Sound_ClearVoiceFromSchedulerState(void* channel, int voiceBit);
extern void Sound_memcpy32(void* src, void* dst, unsigned size);
extern void Sound_LoadAkaoSequence(void* sequence);
extern void func_80015C9C(void* channel, char* programCounter);
extern void SetVoiceAdsrReleaseRateAndMode(unsigned voice, int rate, unsigned mode);

/* The handler table itself lives in native/platform/sound_tables.c, alongside
 * the two sound-VM opcode tables that have the same problem. */
typedef void (*SoundCmdHandler)(void* params);
extern SoundCmdHandler D_800301E0[0x100];

static void _soundDispatch(int index, void* params)
{
    D_800301E0[index & 0xFF](params);
}

/* VS_SND_TRACE=1 prints every command that reaches the driver front door. This
 * is the first thing to reach for when the game is silent: it separates "the
 * game never asked for a sound" from "the driver was asked and did nothing". */
int _sndTrace = -1;

static void _sndTraceCmd(int cmd, unsigned char* in)
{
    if (_sndTrace < 0) {
        const char* e = getenv("VS_SND_TRACE");
        _sndTrace = (e && *e != '0') ? 1 : 0;
    }
    if (_sndTrace) {
        fprintf(stderr, "[snd] cmd %02X  %08X %08X %08X %08X\n", cmd,
            U32(in, 0x00), U32(in, 0x04), U32(in, 0x08), U32(in, 0x0C));
    }
}

unsigned int func_80018C30(int in_Command)
{
    unsigned char* out = D_80037790; /* $s5 -- what the handler reads */
    unsigned char* in = D_800378C0; /* $s1/$s6 -- what the caller staged */
    int cmd = in_Command & 0xFF; /* $s0 -- also the dispatch index */
    int ret = 0; /* $s7 */

    DisableEvent(_soundEvent);
    _sndTraceCmd(cmd, in);

    switch (cmd) {
    /* Start a sequence. All four take an Akao blob in Param1 and differ only
     * in which extra argument they forward. */
    case 0x10:
    case 0x12:
    case 0x19:
    case 0x1A: {
        unsigned char* seq = (unsigned char*)U32(in, 0x00);
        unsigned short seqId;

        if (U32(seq, 0x00) != AKAO_MAGIC) {
            cmd = 0;
            ret = -1;
            break;
        }
        U32(g_Sound_GlobalFlags, 0x00) &= ~0x7700u;

        seqId = U16(seq, 0x04);
        if (U16(g_pActiveMusicConfig, 0x6E) == seqId) {
            /* Already the active sequence. Only restart it if a different one
             * is stacked underneath. */
            void* saved = g_pSavedMousicConfig;
            if (saved == 0 || U16(saved, 0x6E) == seqId) {
                cmd = 0;
                break;
            }
        }

        Sound_SetReverbMode(U16(seq, 0x08));
        U32(out, 0x00) = (unsigned)seq;
        U32(out, 0x08) = seqId;

        if (cmd == 0x12) {
            U32(out, 0x0C) = U32(in, 0x04);
        } else if (cmd == 0x19) {
            U32(out, 0x10) = U32(in, 0x04);
        } else if (cmd == 0x1A) {
            unsigned arg = U32(in, 0x04);
            if (arg == 0) {
                arg = 1;
            }
            U32(out, 0x10) = arg;
            U32(out, 0x14) = U32(in, 0x08);
        }
        ret = seqId;
        break;
    }

    /* Hand a blob to the cutscene/stream slot. Note the dispatch index stays
     * 0x14 on both paths, exactly as the assembly leaves it. */
    case 0x14: {
        unsigned char* file = (unsigned char*)U32(in, 0x00);
        ret = -1;
        if (U32(file, 0x00) == AKAO_MAGIC) {
            U32(g_pActiveMusicConfig, 0x00) |= 0x10000;
            D_80037788 = U32(in, 0x00);
            ret = U16((unsigned char*)U32(in, 0x00), 0x04);
        }
        break;
    }

    /* Load two sequences at once: Param2's becomes the active music, then it is
     * pushed aside into the secondary block so Param1's can take over. */
    case 0x16: {
        unsigned char* primary = (unsigned char*)U32(in, 0x00);
        unsigned char* secondary;

        cmd = 0;
        if (U32(primary, 0x00) != AKAO_MAGIC) {
            break;
        }
        secondary = (unsigned char*)U32(in, 0x04);
        if (U32(secondary, 0x00) != AKAO_MAGIC) {
            break;
        }

        U32(g_Sound_GlobalFlags, 0x00) &= ~0x7700u;
        Sound_SetReverbMode(U16(secondary, 0x08));

        U32(in, 0x04) = U16(secondary, 0x04);
        Sound_LoadAkaoSequence(secondary);
        U16(g_pActiveMusicConfig, 0x6E) = U16(secondary, 0x04);

        Sound_memcpy32(g_ActiveMusicChannels, D_800378E8, 0x2200);
        Sound_memcpy32(D_800366F0, g_PushedMusicConfig, 0x80);

        primary = (unsigned char*)U32(in, 0x00);
        U32(in, 0x00) = U16(primary, 0x04);
        Sound_LoadAkaoSequence(primary);

        g_pSavedMousicConfig = g_PushedMusicConfig;
        g_pSecondaryMusicChannels = D_800378E8;
        U16(g_pActiveMusicConfig, 0x6E) = U16(primary, 0x04);

        U32(g_PushedMusicConfig, 0x04) &= ~U32(g_PushedMusicConfig, 0x30);
        U32(g_Sound_GlobalFlags, 0x00) |= 0x100;
        break;
    }

    /* Pop the pushed sequence back. 0x100 means one is stacked. */
    case 0x17: {
        unsigned flags = U32(g_Sound_GlobalFlags, 0x00);
        if (flags & 0x1100) {
            U32(g_Sound_GlobalFlags, 0x00)
                = (flags & 0x100) ? (flags | 0x4000) : (flags | 0x400);
        }
        cmd = 0;
        break;
    }

    /* Cutscene stream control. These three run their handler inline and then
     * fall through to a *second*, different handler on the way out. */
    case 0xD8:
        U32(out, 0x00) = U32(in, 0x00);
        _soundDispatch(0xD0, out);
        cmd = 0xD4;
        break;

    case 0xD9:
        U32(out, 0x00) = U32(in, 0x00);
        U32(out, 0x04) = U32(in, 0x04);
        _soundDispatch(0xD1, out);
        cmd = 0xD5;
        break;

    case 0xDA:
        U32(out, 0x00) = U32(in, 0x00);
        U32(out, 0x04) = U32(in, 0x04);
        U32(out, 0x08) = U32(in, 0x08);
        _soundDispatch(0xD2, out);
        cmd = 0xD6;
        break;

    /* Compound fades: run the SFX and music variants, then the master one. */
    case 0x98:
        _soundDispatch(0x9A, out);
        _soundDispatch(0x9C, out);
        cmd = 0x9E;
        break;

    case 0x99:
        _soundDispatch(0x9B, out);
        _soundDispatch(0x9D, out);
        cmd = 0x9F;
        break;

    /* Everything else just forwards the whole parameter block. */
    default:
        U32(out, 0x00) = U32(in, 0x00);
        U32(out, 0x04) = U32(in, 0x04);
        U32(out, 0x08) = U32(in, 0x08);
        U32(out, 0x0C) = U32(in, 0x0C);
        U32(out, 0x10) = U32(in, 0x10);
        U32(out, 0x14) = U32(in, 0x14);
        break;
    }

    _soundDispatch(cmd, out);
    EnableEvent(_soundEvent);
    return (unsigned int)ret;
}

/* func_8001653C: arm one of the twelve SFX voice channels from an Akao program.
 * Transcribed from 25AC/func_8001653C.s.
 *
 * in_VoiceBit is the channel's bit in the scheduler masks: 0x1000 for
 * D_80035910[0], up to 0x800000 for the last. Field offsets are the ones in
 * FSoundChannel (src/include/sound.h); the raw form is used here because the
 * platform layer does not pull in the game headers. */
void func_8001653C(void* in_Channel, void* in_Params, unsigned in_VoiceBit,
    char* in_ProgramCounter)
{
    unsigned char* ch = (unsigned char*)in_Channel;
    unsigned char* p = (unsigned char*)in_Params;
    unsigned char* sch = g_Sound_VoiceSchedulerState;
    unsigned notBit;

    U32(ch, 0x3C) = U32(p, 0x00); /* owner id (Param1)     */
    U32(ch, 0x28) = U32(p, 0x04); /* owner mask (Param2)   */
    U16(ch, 0x6E) = 0;
    U16(ch, 0x8E) = 0x8000; /* ChannelPan, centred   */
    U16(ch, 0x90) = 0; /* ChannelPanSlideLength */
    U16(ch, 0x6C) = (unsigned short)((U8(p, 0x08) & 0xFF) << 8); /* Param3 */
    U16(ch, 0x8C) = 0;
    U16(ch, 0xDC) = (unsigned short)((U16(p, 0x0C) & 0x7F) << 8); /* Param4 */
    U16(ch, 0x66) = 2; /* Length1 */
    U16(ch, 0x68) = 1; /* Length2 */
    U16(ch, 0x64) = 1; /* Type    */
    U32(ch, 0x58) = (unsigned)-2; /* priority: lowest      */
    U32(ch, 0x40) = 0;
    U16(ch, 0x86) = 0;
    U32(ch, 0x38) = 1u << U32(p, 0x10); /* ExtParam1 */

    if (_sndTrace > 0) {
        fprintf(stderr, "[snd] arm channel +%04X bit %08X pc %p\n",
            (unsigned)(ch - D_80035910), in_VoiceBit, (void*)in_ProgramCounter);
    }

    func_80015C9C(ch, in_ProgramCounter);

    g_Sound_VoiceChannelConfigs[U32(ch, 0xF4)] = 0;
    SetVoiceAdsrReleaseRateAndMode(U32(ch, 0xF4), 5, 3);

    U32(sch, 0x00) |= in_VoiceBit; /* ActiveChannelMask */
    U32(sch, 0x0C) |= in_VoiceBit; /* KeyOffFlags       */
    notBit = ~in_VoiceBit;
    U32(sch, 0x04) &= notBit; /* KeyOnFlags        */
    U32(sch, 0x08) &= notBit; /* KeyedFlags        */
    U32(sch, 0x1C) &= notBit; /* NoiseVoiceFlags   */
    U32(sch, 0x20) &= notBit; /* ReverbVoiceFlags  */
    U32(sch, 0x24) &= notBit; /* FmVoiceFlags      */

    /* With D_80039B64 bit 1 set the driver is in its deferred mode: unowned
     * channels are parked in unk_Flags_0x10 instead of staying active. */
    if (D_80039B64 & 2) {
        unsigned char* v = D_80035910;
        unsigned bit = 0x1000;
        int i;

        for (i = 0xC; i != 0; --i, v += 0x110, bit <<= 1) {
            unsigned active = U32(sch, 0x00);
            if (!(active & bit)) {
                continue;
            }
            if (U32(v, 0x28) & 0x02000000) {
                continue;
            }
            U32(sch, 0x00) = active & ~bit;
            U32(sch, 0x10) |= bit;
        }
    }
}

/* ---- the per-tick flush to the SPU ----------------------------------------
 *
 * func_80015220 is what actually keys voices on. func_80019A58 calls it once a
 * tick when anything is pending, it walks the channels working out which voices
 * to start, hands each one's parameters to the SPU, and finishes with a single
 * SetVoiceKeyOn for the accumulated mask.
 *
 * Transcribed from 25AC/func_80015220.s together with the three helpers it
 * needs that were also still assembly: func_80015080 (reap finished voices),
 * func_80015970 (fold a channel mask down to a voice mask) and func_800147CC
 * (per-channel LFO, pan and pitch).
 */
extern unsigned char g_SpuVoiceInfo[]; /* {int unk0; short pEnvx; short pad;}[24] */
extern short D_8002F89C;
extern short g_Sound_StereoPanGainTableQ15[];

extern void SpuGetVoiceEnvelope(int vNum, short* envx);
extern void SpuSetReverbModeDepth(short l, short r);
extern long SpuSetNoiseClock(long clock);
extern void UnassignVoicesFromChannels(void* channels, int voice);
extern int func_8001503C(void* channels, int mask, int voice);
extern void ChannelMaskToVoiceMaskFiltered(void* channels, int* ioVoiceMask,
    int chanMask, int allowedVoices);
extern void func_80014D70(void* channels, unsigned flags1, unsigned flags2,
    unsigned* outKeyOn);
extern void SetVoiceParamsByFlags(unsigned voice, void* voiceParams, unsigned flags);
extern void Sound_ApplyMasterFadeToChannelVolume(void* config, void* channels);
extern void Sound_RestoreChannelVolumeFromMasterFade(void* config, void* channels);

/* SetVoiceKeyOn and the three voice-mode setters are file-static in 25AC.c, so
 * they are restated here. They are nothing but a pair of register writes; the
 * addresses are the ones in src/include/hw.h. */
#define SPU_REG16(addr) (*(volatile unsigned short*)(addr))

static void _setVoiceKeyOn(unsigned mask)
{
    SPU_REG16(0x1F801D88) = (unsigned short)mask;
    SPU_REG16(0x1F801D8A) = (unsigned short)(mask >> 16);
}

static void _setVoiceReverbMode(unsigned mask)
{
    SPU_REG16(0x1F801D98) = (unsigned short)mask;
    SPU_REG16(0x1F801D9A) = (unsigned short)(mask >> 16);
}

static void _setVoiceNoiseMode(unsigned mask)
{
    SPU_REG16(0x1F801D94) = (unsigned short)mask;
    SPU_REG16(0x1F801D96) = (unsigned short)(mask >> 16);
}

static void _setVoiceFmMode(unsigned mask)
{
    SPU_REG16(0x1F801D90) = (unsigned short)mask;
    SPU_REG16(0x1F801D92) = (unsigned short)(mask >> 16);
}

/* The retail code reads fields off g_pSavedMousicConfig without checking it for
 * NULL. On the PSX that is harmless -- address 0x4C is inside the kernel and
 * reads back as data -- but here it is a fault, so read through this instead.
 * Zero is also the semantically right answer: the values are only ever used
 * inside a branch that has already established a saved config exists. */
static unsigned _cfgWord(void* config, int offset)
{
    return config ? U32(config, offset) : 0u;
}

/* func_80015080: hand back voices whose envelope has run out.
 * Transcribed from 25AC/func_80015080.s. */
void func_80015080(int in_BusyVoices)
{
    unsigned char* cfg = (unsigned char*)g_pActiveMusicConfig;
    unsigned char* saved = (unsigned char*)g_pSavedMousicConfig;
    unsigned busy;
    int voice;

    busy = (U32(cfg, 0x04) & U32(cfg, 0x0C)) | (unsigned)in_BusyVoices;
    if (saved != 0) {
        busy |= U32(saved, 0x04) & U32(saved, 0x0C);
    }

    for (voice = 0; voice < 0x18; ++voice) {
        short* envx = (short*)(g_SpuVoiceInfo + voice * 8 + 4);

        if (busy & (1u << voice)) {
            *envx = 0x7FFF;
            continue;
        }

        SpuGetVoiceEnvelope(voice, envx);
        if (*envx == 0) {
            UnassignVoicesFromChannels(g_ActiveMusicChannels, voice);
            continue;
        }

        cfg = (unsigned char*)g_pActiveMusicConfig;
        if (U32(cfg, 0x08) == 0) {
            continue;
        }
        if (func_8001503C(g_ActiveMusicChannels, (int)U32(cfg, 0x08), voice)) {
            *envx = 0x7FFF;
        }
        saved = (unsigned char*)g_pSavedMousicConfig;
        if (saved != 0
            && func_8001503C(g_pSecondaryMusicChannels, (int)U32(saved, 0x08), voice)) {
            *envx = 0x7FFF;
        }
    }
}

/* func_80015970: collapse "which channels want this voice mode" into "which
 * voices", for one of reverb/noise/FM. Transcribed from 25AC/func_80015970.s. */
void func_80015970(unsigned* out_VoiceMask, int in_SavedChanMask,
    int in_ActiveChanMask, unsigned in_ExtraVoices)
{
    unsigned char* sch = g_Sound_VoiceSchedulerState;
    unsigned char* saved = (unsigned char*)g_pSavedMousicConfig;
    unsigned char* cfg;
    unsigned live = U32(sch, 0x00) | U32(sch, 0x10);
    unsigned allowed = ~(U32(g_Sound_Cutscene_StreamState, 0x0C) | live);
    unsigned savedSel = 0;
    unsigned activeSel;
    int voices = 0;

    if (saved != 0) {
        savedSel = U32(saved, 0x04) & (unsigned)in_SavedChanMask;
        if (savedSel & U32(saved, 0x08)) {
            ChannelMaskToVoiceMaskFiltered(g_pSecondaryMusicChannels, &voices,
                (int)(savedSel & U32(saved, 0x08)), (int)allowed);
            savedSel &= ~U32((unsigned char*)g_pSavedMousicConfig, 0x08);
        }
    }

    cfg = (unsigned char*)g_pActiveMusicConfig;
    activeSel = U32(cfg, 0x04) & (unsigned)in_ActiveChanMask;
    if (activeSel & U32(cfg, 0x08)) {
        ChannelMaskToVoiceMaskFiltered(g_ActiveMusicChannels, &voices,
            (int)(activeSel & U32(cfg, 0x08)), (int)allowed);
        activeSel &= ~U32((unsigned char*)g_pActiveMusicConfig, 0x08);
    }

    if (g_pSavedMousicConfig != 0 && savedSel != 0) {
        ChannelMaskToVoiceMaskFiltered(
            g_pSecondaryMusicChannels, &voices, (int)savedSel, (int)allowed);
    }
    if (activeSel != 0) {
        ChannelMaskToVoiceMaskFiltered(
            g_ActiveMusicChannels, &voices, (int)activeSel, (int)allowed);
    }

    *out_VoiceMask = (unsigned)voices | in_ExtraVoices;
    U32(g_Sound_GlobalFlags, 0x08) |= 0x100;
}

/* func_800147CC: advance one channel's vibrato, tremolo and auto-pan, then
 * recompute its SPU volume and sample rate. Transcribed from
 * 25AC/func_800147CC.s. Runs once per channel per tick, for every channel with
 * a keyed voice, so it is the hot one. */
void func_800147CC(void* in_Channel, unsigned in_VoiceBit)
{
    unsigned char* ch = (unsigned char*)in_Channel;
    unsigned flags = U32(ch, 0x34); /* UpdateFlags */
    int volume; /* $a3 */
    int pan = 0x80;

    (void)in_VoiceBit;

    volume = ((int)(short)U16(ch, 0x4A) * (U16(ch, 0x82) >> 8)) >> 7;

    if (flags & 1) { /* vibrato */
        if ((unsigned short)(--U16(ch, 0xA4)) == 0) {
            short* wave;
            int depth;

            U16(ch, 0xA4) = U16(ch, 0xA2);
            wave = (short*)U32(ch, 0x1C);
            if (wave[0] == 0 && wave[1] == 0) {
                U32(ch, 0x1C) = (unsigned)(wave + wave[2]);
            }
            wave = (short*)U32(ch, 0x1C);
            depth = (int)U16(ch, 0xA8) * wave[0];
            U32(ch, 0x1C) = (unsigned)(wave + 1);
            depth >>= 16;
            if (depth != (short)U16(ch, 0xEC)) {
                U16(ch, 0xEC) = (unsigned short)depth;
                U32(ch, 0xF8) |= 0x10;
                if (depth >= 0) {
                    U16(ch, 0xEC) = (unsigned short)(depth << 1);
                }
            }
        }
    }

    if (flags & 2) { /* tremolo */
        if ((unsigned short)(--U16(ch, 0xB6)) == 0) {
            short* wave;
            int scaled;

            U16(ch, 0xB6) = U16(ch, 0xB4);
            wave = (short*)U32(ch, 0x20);
            if (wave[0] == 0 && wave[1] == 0) {
                U32(ch, 0x20) = (unsigned)(wave + wave[2]);
            }
            scaled = (volume * (U16(ch, 0xBA) >> 8)) << 9;
            wave = (short*)U32(ch, 0x20);
            scaled = (scaled >> 16) * wave[0];
            U32(ch, 0x20) = (unsigned)(wave + 1);
            scaled >>= 15;
            if (scaled != (short)U16(ch, 0xEE)) {
                U16(ch, 0xEE) = (unsigned short)scaled;
                U32(ch, 0xF8) |= 3;
            }
        }
    }

    if (flags & 4) { /* auto-pan */
        if ((unsigned short)(--U16(ch, 0xC2)) == 0) {
            short* wave;
            int scaled;

            U16(ch, 0xC2) = U16(ch, 0xC0);
            wave = (short*)U32(ch, 0x24);
            if (wave[0] == 0 && wave[1] == 0) {
                U32(ch, 0x24) = (unsigned)(wave + wave[2]);
            }
            wave = (short*)U32(ch, 0x24);
            scaled = ((int)(U16(ch, 0xC6) >> 8) * wave[0]) >> 15;
            U32(ch, 0x24) = (unsigned)(wave + 1);
            if (scaled != (short)U16(ch, 0xF0)) {
                U16(ch, 0xF0) = (unsigned short)scaled;
                U32(ch, 0xF8) |= 3;
            }
        }
    }

    /* Stereo-linked channel: take the partner's already-computed sample rate as
     * the base. ch - 0xC is the previous channel's VoiceParams.SampleRate. */
    if (flags & 0x20) {
        volume = ((int)(short)(U16(ch, -0xC) << 1) * (U16(ch, 0x82) >> 8)) >> 7;
        U32(ch, 0xF8) |= 3;
    }

    if (U32(ch, 0xF8) & 3) { /* volume dirty */
        volume += (short)U16(ch, 0xEE);

        if (!(U32(ch, 0x28) & 0x02000000)) {
            int p;
            volume = (volume * (int)(signed char)(U16(ch, 0xDC) >> 8)) >> 7;
            p = ((int)(U16(ch, 0x8E) + U16(ch, 0x6C)) >> 8) + (short)U16(ch, 0xF0);
            pan = (p + 0x80) & 0xFF;
        }

        if (U32(g_Sound_GlobalFlags, 0x04) & 2) { /* forced mono */
            int v = (volume * D_8002F89C) >> 15;
            U16(ch, 0x10E) = (unsigned short)v;
            U16(ch, 0x10C) = (unsigned short)v;
        } else {
            U16(ch, 0x10C)
                = (unsigned short)((volume * g_Sound_StereoPanGainTableQ15[pan]) >> 15);
            U16(ch, 0x10E) = (unsigned short)(
                (volume * g_Sound_StereoPanGainTableQ15[pan ^ 0xFF]) >> 15);
        }
    }

    if (flags & 0x10) { /* pitch follows the linked partner */
        int rate = (int)U16(ch, -0xC) + (short)U16(ch, 0xEC) + (short)U16(ch, 0x32);

        if (!(U32(ch, 0x28) & 0x02000000)) {
            int bend = U32(ch, 0x40) & 0xFF00;
            if (bend != 0) {
                bend >>= 8;
                rate = (bend < 0x80) ? (rate + ((rate * bend) >> 7))
                                     : ((rate * bend) >> 8);
            }
        }
        U16(ch, 0x104) = (unsigned short)((U16(ch, 0x54) + rate) & 0x3FFF);
        U32(ch, 0xF8) |= 0x10;
        return;
    }

    if (U32(ch, 0xF8) & 0x10) { /* pitch dirty */
        int rate = (int)U32(ch, 0x2C) + (short)U16(ch, 0xEC) + (short)U16(ch, 0x32);

        if (!(U32(ch, 0x28) & 0x02000000)) {
            int bend = U32(ch, 0x40) & 0xFF00;
            if (bend != 0) {
                bend >>= 8;
                rate = (bend < 0x80) ? (rate + ((rate * bend) >> 7))
                                     : ((rate * bend) >> 8);
            }
        }
        U16(ch, 0x104) = (unsigned short)((U16(ch, 0x54) + rate) & 0x3FFF);
    }
}

/* func_80015220: the flush itself. Transcribed from 25AC/func_80015220.s. */
void func_80015220(void)
{
    unsigned char* sch = g_Sound_VoiceSchedulerState;
    unsigned char* cfg;
    unsigned char* saved;
    unsigned busy; /* voices already spoken for */
    unsigned keyOn = 0;

    busy = U32(sch, 0x00) | U32(sch, 0x10) | U32(g_Sound_Cutscene_StreamState, 0x0C);

    cfg = (unsigned char*)g_pActiveMusicConfig;
    saved = (unsigned char*)g_pSavedMousicConfig;

    if ((U32(cfg, 0x04) & U32(cfg, 0x10))
        || (saved != 0 && (U32(saved, 0x04) & U32(saved, 0x10)))) {
        func_80015080((int)busy);
    }

    if (saved != 0) {
        /* Two configs are live: the stacked one is serviced first, then the
         * active one, and the voices the stacked one took are removed from what
         * the active one may use. */
        unsigned savedSel, savedFree, activeSel, activeFree;

        if (U32(g_Sound_GlobalFlags, 0x04) & 0x100) {
            Sound_ApplyMasterFadeToChannelVolume(
                g_pSavedMousicConfig, g_pSecondaryMusicChannels);
        }

        saved = (unsigned char*)g_pSavedMousicConfig;
        g_pActiveMusicConfig = saved;
        savedSel = (U32(saved, 0x04) & U32(saved, 0x14)) & ~(U32(saved, 0x0C) & busy);
        savedFree = (savedSel & U32(saved, 0x0C)) & ~busy;

        if (savedSel & U32(saved, 0x08)) {
            func_80014D70(g_pSecondaryMusicChannels, savedSel & U32(saved, 0x08),
                savedFree, &keyOn);
            cfg = (unsigned char*)g_pActiveMusicConfig;
            savedSel &= ~U32(cfg, 0x08);
            U32(cfg, 0x10) &= ~U32(cfg, 0x08);
        }

        cfg = D_800366F0;
        g_pActiveMusicConfig = cfg;
        {
            unsigned taken = savedFree | busy;
            activeSel = (U32(cfg, 0x04) & U32(cfg, 0x14)) & ~(U32(cfg, 0x0C) & taken);
            activeFree = (activeSel & U32(cfg, 0x0C)) & ~taken;
        }

        if (activeSel & U32(cfg, 0x08)) {
            func_80014D70(g_ActiveMusicChannels, activeSel & U32(cfg, 0x08), activeFree,
                &keyOn);
            cfg = (unsigned char*)g_pActiveMusicConfig;
            activeSel &= ~U32(cfg, 0x08);
            U32(cfg, 0x10) &= ~U32(cfg, 0x08);
        }

        if (savedSel != 0) {
            g_pActiveMusicConfig = g_pSavedMousicConfig;
            func_80014D70(g_pSecondaryMusicChannels, savedSel, savedFree & ~activeFree,
                &keyOn);
            cfg = (unsigned char*)g_pActiveMusicConfig;
            g_pActiveMusicConfig = D_800366F0;
            U32(cfg, 0x10) = 0;
        }
        if (activeSel != 0) {
            func_80014D70(g_ActiveMusicChannels, activeSel, activeFree, &keyOn);
            U32((unsigned char*)g_pActiveMusicConfig, 0x10) = 0;
        }

        if (U32(g_Sound_GlobalFlags, 0x04) & 0x100) {
            Sound_RestoreChannelVolumeFromMasterFade(
                g_pSavedMousicConfig, g_pSecondaryMusicChannels);
        }
    } else {
        unsigned sel, free;

        cfg = (unsigned char*)g_pActiveMusicConfig;
        sel = (U32(cfg, 0x04) & U32(cfg, 0x14)) & ~(U32(cfg, 0x0C) & busy);
        free = (sel & U32(cfg, 0x0C)) & ~busy;

        if (sel & U32(cfg, 0x08)) {
            func_80014D70(g_ActiveMusicChannels, sel & U32(cfg, 0x08), free, &keyOn);
            cfg = (unsigned char*)g_pActiveMusicConfig;
            sel &= ~U32(cfg, 0x08);
            U32(cfg, 0x10) &= ~U32(cfg, 0x08);
        }
        if (sel != 0) {
            func_80014D70(g_ActiveMusicChannels, sel, free, &keyOn);
            U32((unsigned char*)g_pActiveMusicConfig, 0x10) = 0;
        }
    }

    /* SFX channels: push each keyed channel's parameters into its SPU voice. */
    {
        unsigned pending = U32(sch, 0x00) & U32(sch, 0x08);

        if (pending != 0) {
            unsigned char* ch = D_80035910;
            unsigned bit = 0x1000;
            int guard;

            keyOn |= U32(sch, 0x04);
            /* The retail loop runs `while (pending)` and relies on every set bit
             * being one of the twelve channel bits. Bounding it at 32 cannot
             * change the result for any such mask, and turns a stray high bit
             * into a dropped voice instead of a hang. */
            for (guard = 0; pending != 0 && guard < 32; ++guard) {
                if (pending & bit) {
                    func_800147CC(ch, bit);
                    if (U32(ch, 0xF8) != 0) {
                        SetVoiceParamsByFlags(
                            U32(ch, 0xF4), ch + 0xF4, U32(ch, 0x34));
                    }
                    pending &= ~bit;
                }
                bit <<= 1;
                ch += 0x110;
            }
            U32(sch, 0x04) = 0; /* KeyOnFlags */
        }
    }

    {
        unsigned update = U32(g_Sound_GlobalFlags, 0x08);

        if (update & 0x80) {
            short depth
                = (short)(((int)U32((unsigned char*)g_pActiveMusicConfig, 0x58) << 4)
                    >> 16);
            SpuSetReverbModeDepth(depth, depth);
            U32(g_Sound_GlobalFlags, 0x08) &= ~0x80u;
        }

        if (update & 0x10) {
            long clock = U32(sch, 0x00)
                ? U16(sch, 0x28)
                : U16((unsigned char*)g_pActiveMusicConfig, 0x72);
            SpuSetNoiseClock(clock);
            U32(g_Sound_GlobalFlags, 0x08) &= ~0x10u;
        }

        if (update & 0x100) {
            unsigned* modes = (unsigned*)g_Sound_VoiceModeFlags;

            saved = (unsigned char*)g_pSavedMousicConfig;
            cfg = (unsigned char*)g_pActiveMusicConfig;
            func_80015970(&modes[1], (int)_cfgWord(saved, 0x4C),
                (int)U32(cfg, 0x4C), U32(sch, 0x1C)); /* noise  */
            saved = (unsigned char*)g_pSavedMousicConfig;
            cfg = (unsigned char*)g_pActiveMusicConfig;
            func_80015970(&modes[0], (int)_cfgWord(saved, 0x50),
                (int)U32(cfg, 0x50), U32(sch, 0x20)); /* reverb */
            saved = (unsigned char*)g_pSavedMousicConfig;
            cfg = (unsigned char*)g_pActiveMusicConfig;
            func_80015970(&modes[2], (int)_cfgWord(saved, 0x54),
                (int)U32(cfg, 0x54), U32(sch, 0x24)); /* FM     */

            _setVoiceReverbMode(modes[0]);
            _setVoiceNoiseMode(modes[1]);
            _setVoiceFmMode(modes[2]);
            U32(g_Sound_GlobalFlags, 0x08) &= ~0x100u;
        }
    }

    if (keyOn != 0) {
        _setVoiceKeyOn(keyOn);
    }
}

/* func_80019FC4: look ahead in a channel's Akao program for the next note.
 * Transcribed from 25AC/func_80019FC4.s.
 *
 * func_8001A8D8 calls this after executing an opcode to decide how long the
 * note it just started should sound: it walks the bytecode forward from the
 * current PC -- following jumps, loops and calls, but executing nothing -- and
 * returns the first note-ish opcode it reaches, or 0xA0 for "the program ends".
 * The only state it touches is SfxMask, which it clears bits 1 and 2 of on the
 * paths where the lookahead crosses something that invalidates a slide.
 *
 * D_8002F5A0 and D_8002F600 are the per-opcode operand byte counts (main and
 * 0xFE-extended); a non-zero entry means "skip this many bytes and keep going",
 * and zero means the opcode needs real handling below. Both are harvested from
 * the retail .data -- see SOUND_HARVEST in Makefile.native. */
extern unsigned char D_8002F5A0[]; /* operand counts, opcodes 0xA0..0xFF   */
extern unsigned char D_8002F600[]; /* operand counts, extended opcodes     */

#define SFXMASK_CLEAR_SLIDE(ch) (U16((ch), 0x9C) &= 0xFFFA)

int func_80019FC4(void* in_Channel)
{
    unsigned char* ch = (unsigned char*)in_Channel;
    unsigned char* pc = (unsigned char*)U32(ch, 0x00); /* ProgramCounter  */
    unsigned loopTop = U16(ch, 0xD0); /* LoopStackTop    */
    unsigned char* cfg = (unsigned char*)g_pActiveMusicConfig;

    for (;;) {
        unsigned op = pc[0];
        unsigned operands;

        if (op < 0x9A) {
            if (op >= 0x8F) {
                SFXMASK_CLEAR_SLIDE(ch);
            }
            return pc[0];
        }
        if (op < 0xA0) {
            return 0xA0;
        }

        operands = D_8002F5A0[op - 0xA0];
        if (operands != 0) {
            pc += operands;
            continue;
        }

        /* Opcodes with no fixed operand length: everything below 0xC9 that
         * reaches here ends the scan. */
        if ((unsigned)(op - 0xC9) >= 0x36) {
            SFXMASK_CLEAR_SLIDE(ch);
            return 0xA0;
        }

        switch (op) {
        case 0xC9: { /* LoopN: take the back edge unless this is the last pass */
            unsigned expected = U16(ch, 0x72 + loopTop * 2) + 1;
            ++pc;
            if (pc[0] != expected) {
                pc = (unsigned char*)U32(ch, 0x04 + loopTop * 4);
                continue;
            }
            ++pc;
            loopTop = (loopTop - 1) & 3;
            continue;
        }

        case 0xCA: /* LoopInf: unless the channel is already being released */
            if (U32(ch, 0x34) & 0x200000) {
                SFXMASK_CLEAR_SLIDE(ch);
                return 0xA0;
            }
            pc = (unsigned char*)U32(ch, 0x04 + loopTop * 4);
            continue;

        case 0xCB:
        case 0xCD:
        case 0xD1:
        case 0xDB:
            SFXMASK_CLEAR_SLIDE(ch);
            ++pc;
            continue;

        case 0xFE: { /* extended opcode: one more byte selects the operation */
            unsigned ext;
            int rel;

            ++pc;
            ext = pc[0];
            operands = D_8002F600[ext];
            if (operands != 0) {
                pc += operands;
                continue;
            }
            if ((unsigned)(ext - 6) >= 0x1A) {
                continue;
            }

            switch (ext) {
            case 0x08: /* JumpOnNthLoopPass */
            case 0x09:
                ++pc;
                if (pc[0] != (unsigned)(U16(ch, 0x72 + loopTop * 2) + 1)) {
                    pc += 3;
                    continue;
                }
                ++pc;
                loopTop = (loopTop - 1) & 3;
                break; /* take the relative jump */

            case 0x06: /* JumpRelativeOffset */
            case 0x0E: /* CallRelativeOffset */
                ++pc;
                break;

            case 0x07: { /* JumpRelativeWithThreshold */
                unsigned want;
                ++pc;
                want = pc[0];
                ++pc;
                if (U16(cfg, 0x70) < want) { /* JumpThresholdValue */
                    pc += 2;
                    continue;
                }
                break;
            }

            case 0x0F: /* Return */
                pc = (unsigned char*)U32(ch, 0x14); /* ReturnProgramCounter */
                continue;

            case 0x1F:
                ++pc;
                if (U32(cfg, 0x00) & 0x10000) {
                    return 0xA0;
                }
                continue;

            default:
                continue;
            }

            /* Shared tail: a signed 16-bit displacement relative to itself. */
            rel = (short)(pc[0] | (pc[1] << 8));
            pc += rel;
            continue;
        }

        default:
            if (op >= 0xF0 && op <= 0xFB) {
                return 0x83;
            }
            if (op == 0xFC) {
                return 0x84;
            }
            if (op == 0xFD) {
                return 0x8F;
            }
            SFXMASK_CLEAR_SLIDE(ch);
            return 0xA0;
        }
    }
}

/* Stop a channel: either flag it for a deferred release (if it is mid-keyon,
 * 0x100000) or tear it down now. Shared by every branch of func_800161C4. */
static void _releaseChannel(unsigned char* ch, unsigned bit, unsigned char* sch)
{
    unsigned flags = U32(ch, 0x34); /* UpdateFlags */

    if (flags & 0x100000) {
        U32(ch, 0x34) = flags | 0x200000;
        return;
    }
    U32(sch, 0x0C) |= bit; /* KeyOffFlags */
    Sound_ClearVoiceFromSchedulerState(ch, (int)bit);
    U32(ch, 0x34) = 0;
}

/* func_800161C4: release SFX voices. Transcribed from 25AC/func_800161C4.s.
 *
 * in_Mask picks the selection rule, and it is genuinely three different
 * functions sharing an entry point:
 *   low 28 bits set   release every channel whose owner mask intersects it
 *   negative          in_Owner is a channel index; release that stereo pair
 *   bit 0x40000000    release the lowest-priority unowned channels (the voice
 *                     stealer Sound_PlaySfxProgram calls when it runs dry)
 *   zero              in_Owner selects: -1 = any with a negative owner id,
 *                     -2 = any unowned, otherwise an exact owner id match
 */
void func_800161C4(int in_Owner, int in_Mask)
{
    unsigned char* sch = g_Sound_VoiceSchedulerState;
    unsigned char* ch;
    unsigned bit;
    unsigned active;
    int i;

    /* A channel counts as live if it is active or parked. */
    active = U32(sch, 0x00) | U32(sch, 0x10);

    if (in_Mask & 0x0FFFFFFF) {
        ch = D_80035910;
        bit = 0x1000;
        for (i = 0; i < 0xC; ++i, ch += 0x110, bit <<= 1) {
            if (!(active & bit)) {
                continue;
            }
            if (!(U32(ch, 0x28) & (unsigned)in_Mask)) {
                continue;
            }
            _releaseChannel(ch, bit, sch);
        }
    } else if (in_Mask < 0) {
        /* in_Owner indexes the pair directly. This is the one path that does
         * not raise the deferred-update flags on the way out. */
        ch = D_80035910 + in_Owner * 0x110;
        bit = 0x1000u << in_Owner;
        if (active & bit) {
            func_800161C4((int)U32(ch, 0x3C), 0);
        }
        ch += 0x110;
        bit <<= 1;
        if (active & bit) {
            func_800161C4((int)U32(ch, 0x3C), 0);
        }
        return;
    } else if (in_Mask & 0x40000000) {
        int worstPriority = 0;

        /* Owned channels are off limits. */
        ch = D_80035910;
        bit = 0x1000;
        for (i = 0; i < 0xC; ++i, ch += 0x110, bit <<= 1) {
            if (U32(ch, 0x28) != 0) {
                active &= ~bit;
            }
        }

        ch = D_80035910;
        bit = 0x1000;
        for (i = 0; i < 0xC; ++i, ch += 0x110, bit <<= 1) {
            if ((active & bit) && (int)U32(ch, 0x58) > worstPriority) {
                worstPriority = (int)U32(ch, 0x58);
            }
        }

        ch = D_80035910;
        bit = 0x1000;
        for (i = 0; i < 0xC; ++i, ch += 0x110, bit <<= 1) {
            if (!(active & bit)) {
                continue;
            }
            if ((int)U32(ch, 0x58) != worstPriority) {
                continue;
            }
            _releaseChannel(ch, bit, sch);
        }
    } else {
        ch = D_80035910;
        bit = 0x1000;
        for (i = 0; i < 0xC; ++i, ch += 0x110, bit <<= 1) {
            int matches;

            if (!(active & bit)) {
                continue;
            }
            if (in_Owner == -1) {
                matches = ((int)U32(ch, 0x3C) < 0);
            } else if (in_Owner == -2) {
                matches = (U32(ch, 0x28) == 0);
            } else {
                matches = ((int)U32(ch, 0x3C) == in_Owner);
            }
            if (!matches) {
                continue;
            }
            _releaseChannel(ch, bit, sch);
        }
    }

    U32(g_Sound_GlobalFlags, 0x08) |= 0x110;
}
