/* Sound-driver functions that are still assembly in the decomp, reimplemented
 * in native-functional C. The PSX build keeps the assembly; only the native
 * target uses this. Same pattern as native/platform/battle_asm.c.
 *
 * Source of truth: build/src/SLUS_010.40/nonmatchings/25AC/*.s
 */
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
unsigned char D_80035910[0x2000] __attribute__((aligned(16)));

extern unsigned char D_800366F0[];                  /* active music config    */
extern unsigned char g_PushedMusicConfig[];
extern unsigned char g_Sound_Cutscene_StreamState[];
extern unsigned char g_Sound_VoiceModeFlags[];
extern unsigned char g_Sound_VoiceSchedulerState[];
extern unsigned char g_Sound_GlobalFlags[];
extern unsigned char D_80036770[];
extern unsigned char D_800377E0[];

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

/* func_800135D8: upload an Akao instrument bank into SPU local memory
 * (Sound_CopyAndRelocateInstruments + SpuSetTransferStartAddr + _writeSpu).
 * No-op while the SPU is silent -- _writeSpu discards its data anyway, so the
 * only thing lost is the copy. Phase 3b restores it. Ledger section B. */
void func_800135D8(void* file, int arg1, int arg2, int arg3)
{
    (void)file; (void)arg1; (void)arg2; (void)arg3;
}
