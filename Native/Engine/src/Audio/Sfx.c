// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Core/Diagnostics.h"
#include "Core/Runtime.h"
#include "DiscQueue.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: D_8005FE70 (D_8005FE70@SLUS_010.40)
int Audio_nextSfxChannelGroup1;

// decomp: D_8005FE74 (D_8005FE74@SLUS_010.40)
int Audio_nextSfxChannelGroup7E;

// decomp: D_8005FE78 (D_8005FE78@SLUS_010.40)
int Audio_nextSfxChannelGroup180;

// decomp: D_8005FE7C (D_8005FE7C@SLUS_010.40)
int Audio_nextSfxChannelGroup200;

// decomp: D_8005FE80 (D_8005FE80@SLUS_010.40)
int Audio_nextSfxChannelGroupFF000;

// decomp: D_8005FE84 (D_8005FE84@SLUS_010.40)
int Audio_nextSfxChannelGroupF00000;

// decomp: SLUS_010.40 func_80045630 (src/SLUS_010.40/main.c:9785)
void Audio_PlaySfxOnPrimaryChannel(int arg0)
{
    Audio_Playback_PlaySoundEffect(arg0 & 0xFFFF, Audio_nextSfxChannelGroup1, 0x80, 0x7F);

    if (!((Audio_nextSfxChannelGroup1 *= 2) & 1)) {
        Audio_nextSfxChannelGroup1 = 1;
    }
}

// decomp: SLUS_010.40 func_80045684 (src/SLUS_010.40/main.c:9794)
void Audio_PlayBuiltInSfxChannel(int id, int arg1, int arg2, int arg3)
{
    if (arg1 == 0x200) {
        if (id != Audio_Playback_SoundEffectIdForTag(0x200)) {
            Audio_Playback_PlaySoundEffect(id, 0x200, arg2, arg3);
        }
    } else {
        Audio_Playback_PlaySoundEffect(id, arg1, arg2, arg3);
    }
}

// decomp: SLUS_010.40 func_800456EC (src/SLUS_010.40/main.c:9805)
void Audio_PlaySfxProgramChannel(void* data, int arg1, int arg2, int arg3)
{
    if (arg1 == 0x200) {
        if (data != (void*)Audio_Playback_SoundEffectIdForTag(0x200)) {
            Audio_Playback_PlaySoundEffectBlob(data, 0x200, arg2, arg3);
        }
    } else {
        Audio_Playback_PlaySoundEffectBlob(data, arg1, arg2, arg3);
    }
}

// decomp: SLUS_010.40 vs_main_playSfx (src/SLUS_010.40/main.c:9816)
void Audio_PlaySfx(int arg0, long id, int arg2, int arg3)
{
    void* var_s0_2;

    switch (arg0) {
    case 0x7E:
    case 0xFE:
        if (Audio_Playback_UsesSoundEffectTags((void*)(id & 0xFFFF)) != 0) {
            Audio_PlayBuiltInSfxChannel(id & 0xFFFF, Audio_nextSfxChannelGroup7E, arg2, arg3);
            Audio_nextSfxChannelGroup7E *= 2;
            if (!(Audio_nextSfxChannelGroup7E & 0x7E)) {
                Audio_nextSfxChannelGroup7E = 2;
            }
        } else {
            Audio_PlayBuiltInSfxChannel(id & 0xFFFF, 0, arg2, arg3);
        }
        break;

    case 0x100:
    case 0x200:
        if (Audio_Playback_UsesSoundEffectTags((void*)(id & 0xFFFF)) != 0) {
            Audio_PlayBuiltInSfxChannel(id & 0xFFFF, Audio_nextSfxChannelGroup200, arg2, arg3);
        } else {
            Audio_PlayBuiltInSfxChannel(id & 0xFFFF, 0, arg2, arg3);
        }
        break;

    case 0xFF000:
        if (Audio_soundState.currentSfxBank == 0) {
            Core_Warnf("audio.sfx", "private SFX id=%ld skipped: no selected bank", id);
            return;
        }
        if (Audio_soundState.currentSfxBank >
            sizeof Audio_soundState.sfxBankData / sizeof Audio_soundState.sfxBankData[0]) {
            Core_Errorf("audio.sfx", "invalid selected SFX bank=%u", Audio_soundState.currentSfxBank);
            return;
        }
        var_s0_2 = Audio_soundState.sfxBankData[Audio_soundState.currentSfxBank - 1];
        if (var_s0_2 == NULL || id < 0) {
            Core_Errorf("audio.sfx", "unavailable SFX bank=%u or invalid id=%ld", Audio_soundState.currentSfxBank, id);
            return;
        }
        var_s0_2 += *((int*)var_s0_2 + id + 1);
        if (Audio_Playback_UsesSoundEffectTags(var_s0_2) != 0) {
            Audio_PlaySfxProgramChannel(var_s0_2, Audio_nextSfxChannelGroupFF000, arg2, arg3);
            Audio_nextSfxChannelGroupFF000 *= 2;
            if (!(Audio_nextSfxChannelGroupFF000 & arg0)) {
                Audio_nextSfxChannelGroupFF000 = 0x1000;
            }
        } else {
            Audio_PlaySfxProgramChannel(var_s0_2, 0, arg2, arg3);
        }
        break;

    case 0xF00000:
        var_s0_2 = Audio_soundState.sfxBankData[2];
        if (var_s0_2 == NULL) {
            Core_Warnf("audio.sfx", "room SFX id=%ld skipped: no room bank", id);
            return;
        }
        if (id < 0) {
            Core_Errorf("audio.sfx", "invalid room SFX id=%ld", id);
            return;
        }
        var_s0_2 += *((int*)var_s0_2 + id + 1);
        if (Audio_Playback_UsesSoundEffectTags(var_s0_2) != 0) {
            Audio_PlaySfxProgramChannel(var_s0_2, Audio_nextSfxChannelGroupF00000, arg2, arg3);
            Audio_nextSfxChannelGroupF00000 *= 2;
            if (!(Audio_nextSfxChannelGroupF00000 & arg0)) {
                Audio_nextSfxChannelGroupF00000 = 0x100000;
            }
        } else {
            Audio_PlaySfxProgramChannel(var_s0_2, 0, arg2, arg3);
        }
        break;

    default:
        Core_IgnoreDiagnostic(0x95, 0);
        break;
    }
}

// decomp: SLUS_010.40 vs_main_playSfxDefault (src/SLUS_010.40/main.c:9877)
void Audio_PlaySfxCentered(int arg0, int id) { Audio_PlaySfx(arg0, id, 0x80, 0x7F); }

// decomp: SLUS_010.40 vs_main_computeSfxPan (src/SLUS_010.40/main.c:9879)
int Audio_SfxPanAndVolume(int coords, int z)
{
    int v0;
    int new_var2;
    int v1;

    v0 = coords;

    if (coords < 0) {
        v0 += 0xFFFF;
    }

    v0 = (v0 >> 16) << 16;
    coords = (short)(coords - v0) - 160;
    coords = (coords * 3) / 10;

    if (coords < -96) {
        coords = -96;
    }

    if (coords > 96) {
        coords = 96;
    }

    coords += 128;
    v1 = z - 768;

    if (v1 < 0) {
        v1 = 0;
    }

    if (v1 > 4095) {
        v1 = 4095;
    }

    v0 = v1;

    if (v1 < 0) {
        v0 = v1 + 31;
    }

    v0 >>= 5;
    v1 = 127;

    new_var2 = v1 - v0;
    return (coords << 16) + new_var2;
}

// decomp: SLUS_010.40 _do_panSfx (src/SLUS_010.40/main.c:9927)
void Audio_PlayProjectedSfx(int arg0, int arg1, int coords, int z)
{
    coords = Audio_SfxPanAndVolume(coords, z);
    z = coords & 0xFFFF;
    coords = coords >> 0x10;
    if (z > 0) {
        Audio_PlaySfx(arg0, arg1, coords, z);
    }
}

// decomp: SLUS_010.40 vs_main_panSfx (src/SLUS_010.40/main.c:9937)
void Audio_PlayWorldSfx(int arg0, int arg1, SVECTOR* vec)
{
    int z;
    long coords;
    long value;
    long flag;

    Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
    Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
    z = Core_Gte_RotTransPers(vec, &coords, &value, &flag);
    Audio_PlayProjectedSfx(arg0, arg1, coords, z);
}

// decomp: SLUS_010.40 func_80045B28 (src/SLUS_010.40/main.c:9950)
int Audio_ResolveSfxProgram(int arg0, int arg1)
{
    intptr_t temp_v1;

    switch (arg0) {
    case 0x1:
    case 0x7E:
    case 0xFE:
    case 0x100:
    case 0x200:
        temp_v1 = 0;
        break;
    case 0xFF000:
    case 0xF00000:
        temp_v1 = (intptr_t)Audio_soundState.sfxBankData[Audio_soundState.currentSfxBank - 1];
        temp_v1 = (int)temp_v1 + (arg1 + (int*)temp_v1)[1];
        break;
    default:
        Core_IgnoreDiagnostic(0x95, 0);
        temp_v1 = 0;
        break;
    }
    return temp_v1;
}

// decomp: SLUS_010.40 func_80045BFC (src/SLUS_010.40/main.c:9975)
void Audio_FadeSfxVolume(int arg0, int arg1, int arg2, char arg3)
{
    int temp_v0;

    if (arg1 != 0) {
        temp_v0 = Audio_ResolveSfxProgram(arg0, arg1);

        if (temp_v0 != 0) {
            Audio_Control_FadeSoundEffectVolume(temp_v0, 0, arg3, arg2);
        } else {
            do {
                Audio_Control_FadeSoundEffectVolume(arg1, 0, arg3, arg2);
            } while (0);
        }
    } else {
        Audio_Control_FadeSoundEffectVolume(0, arg0, arg3, arg2);
    }
}

// decomp: SLUS_010.40 func_80045C74 (src/SLUS_010.40/main.c:9994)
void Audio_FadeSfxPitch(int arg0, int arg1, int arg2, char arg3)
{
    int temp_v0;

    if (arg1 != 0) {
        temp_v0 = Audio_ResolveSfxProgram(arg0, arg1);
        if (temp_v0 != 0) {
            Audio_Control_FadeSoundEffectPitch(temp_v0, 0, arg3, arg2);
        } else {
            do {
                Audio_Control_FadeSoundEffectPitch(arg1, 0, arg3, arg2);
            } while (0);
        }
    } else {
        Audio_Control_FadeSoundEffectPitch(0, arg0, arg3, arg2);
    }
}

// decomp: SLUS_010.40 func_80045CEC (src/SLUS_010.40/main.c:10012)
void Audio_FadeSfxPan(int arg0, int arg1, int arg2, char arg3)
{
    if (arg1 != 0) {
        arg0 = Audio_ResolveSfxProgram(arg0, arg1);
        if (arg0 != 0) {
            Audio_Control_FadeSoundEffectPan(arg0, 0, arg3, arg2);
        } else {
            do {
                Audio_Control_FadeSoundEffectPan(arg1, 0, arg3, arg2);
            } while (0);
        }
    } else {
        Audio_Control_FadeSoundEffectPan(0, arg0, arg3, arg2);
    }
}

// decomp: SLUS_010.40 func_80045D64 (src/SLUS_010.40/main.c:10028)
void Audio_StopSfx(int arg0, int arg1)
{
    if (arg1 != 0) {
        arg0 = Audio_ResolveSfxProgram(arg0, arg1);

        if (arg0 != 0) {
            Audio_Playback_StopSoundEffects(arg0, 0);
        } else {
            Audio_Playback_StopSoundEffects(arg1, 0);
        }
    } else {
        Audio_Playback_StopSoundEffects(0, arg0);
    }
}

// decomp: SLUS_010.40 func_80045DC0 (src/SLUS_010.40/main.c:10043)
void Audio_StopAllSfxPlayback(void) { Audio_Control_StopSceneSoundEffects(); }

// decomp: SLUS_010.40 func_800461CC (src/SLUS_010.40/main.c:10178)
void Audio_PlayBankSfx(int arg0, u_int arg1[], u_int arg2, int arg3, int arg4)
{
    int var_s0;
    void* temp_s1;

    temp_s1 = arg1;

    if (arg2 < arg1[0]) {
        temp_s1 = (char*)arg1 + (arg1 + arg2)[1];
        var_s0 = 0;

        if (arg0 == 0x180) {
            var_s0 = Audio_nextSfxChannelGroup180;
            Audio_nextSfxChannelGroup180 *= 2;
            if (!(Audio_nextSfxChannelGroup180 & 0x180)) {
                Audio_nextSfxChannelGroup180 = 0x80;
            }
        } else if (arg0 == 0x200) {
            var_s0 = Audio_nextSfxChannelGroup200;
            Audio_nextSfxChannelGroup200 *= 2;
            if (!(Audio_nextSfxChannelGroup200 & 0x200)) {
                Audio_nextSfxChannelGroup200 = arg0;
            }
        } else if (arg0 == 0xF00000) {
            var_s0 = Audio_nextSfxChannelGroupF00000;
            Audio_nextSfxChannelGroupF00000 *= 2;
            if (!(Audio_nextSfxChannelGroupF00000 & arg0)) {
                Audio_nextSfxChannelGroupF00000 = 0x100000;
            }
        } else if (arg0 == 0x7E) {
        } else {
            Core_IgnoreDiagnostic(0x95, 0);
        }
        Audio_PlaySfxProgramChannel(temp_s1, var_s0, arg3, arg4);
    }
}

// decomp: SLUS_010.40 func_800462E8 (src/SLUS_010.40/main.c:10215)
void Audio_PlayBankSfxCentered(int arg0, int* arg1, u_int arg2)
{
    Audio_PlayBankSfx(arg0, arg1, arg2, 0x80, 0x7F);
}

// decomp: SLUS_010.40 func_80046310 (src/SLUS_010.40/main.c:10220)
void Audio_PlayProjectedBankSfx(int arg0, u_int arg1[], int arg2, int arg3, int arg4)
{
    int temp_t0;
    int var_v0;
    int var_v1;

    var_v1 = arg3;
    var_v0 = arg3;
    arg3 = var_v0 >> 0x10;

    if (var_v1 < 0) {
        var_v0 = var_v1 + 0xFFFF;
    }

    arg3 = var_v0 >> 0x10;
    var_v0 = arg3 << 0x10;
    arg3 = var_v1 - var_v0;

    if (arg3 < 0) {
        arg3 = 0;
    }

    if (arg3 >= 0x141) {
        arg3 = 0x140;
    }

    var_v0 = arg3 + ((u_int)arg3 >> 0x1F);
    var_v0 >>= 1;
    arg3 = var_v0 + 0x30;
    temp_t0 = arg4;

    if (temp_t0 < 0) {
        temp_t0 = 0;
    }

    if (temp_t0 >= 0x401) {
        temp_t0 = 0x400;
    }

    var_v1 = temp_t0;

    if (temp_t0 < 0) {
        var_v1 += 7;
    }

    temp_t0 = 0x7F - (var_v1 >> 3);

    if (temp_t0 > 0) {
        Audio_PlayBankSfx(arg0, arg1, arg2, arg3, temp_t0);
    }
}

// decomp: SLUS_010.40 func_800463BC (src/SLUS_010.40/main.c:10272)
void Audio_PlayWorldBankSfx(int arg0, u_int* arg1, int arg2, SVECTOR* vec)
{
    int z;
    long sxy;
    long value;
    long flag;

    Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
    Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
    z = Core_Gte_RotTransPers(vec, &sxy, &value, &flag);
    Audio_PlayProjectedBankSfx(arg0, arg1, arg2, sxy, z);
}

// decomp: SLUS_010.40 func_8004644C (src/SLUS_010.40/main.c:10285)
void Audio_StopBankSfx(int arg0, int* arg1, int arg2)
{
    int var_a1;
    char* var_a0 = (char*)arg1 + (arg1 + arg2)[1];

    if (arg2 != 0) {
        var_a1 = 0;
    } else {
        var_a0 = 0;
        var_a1 = arg0;
    }
    Audio_Playback_StopSoundEffects((intptr_t)var_a0, var_a1);
}

// decomp: SLUS_010.40 func_80046494 (src/SLUS_010.40/main.c:10299)
void Audio_FadeBankSfxVolume(int arg0, int* arg1, int arg2, int arg3, int arg4)
{
    intptr_t a0 = (intptr_t)((char*)arg1 + (arg2 + arg1)[1]);

    if (arg2 == 0) {
        Audio_Control_FadeSoundEffectVolume(0, arg0, arg4 & 0xFF, arg3);
    } else {
        Audio_Control_FadeSoundEffectVolume(a0, 0, arg4 & 0xFF, arg3);
    }
}

