// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
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
void Audio_PlaySfxOnPrimaryChannel(int effect_id)
{
    Audio_Playback_PlaySoundEffect(effect_id & 0xFFFF, Audio_nextSfxChannelGroup1, 0x80, 0x7F);

    if (!((Audio_nextSfxChannelGroup1 *= 2) & 1)) {
        Audio_nextSfxChannelGroup1 = 1;
    }
}

// decomp: SLUS_010.40 func_80045684 (src/SLUS_010.40/main.c:9794)
void Audio_PlayBuiltInSfxChannel(int id, int effect_tag, int pan, int volume)
{
    if (effect_tag == 0x200) {
        if (id != Audio_Playback_SoundEffectIdForTag(0x200)) {
            Audio_Playback_PlaySoundEffect(id, 0x200, pan, volume);
        }
    } else {
        Audio_Playback_PlaySoundEffect(id, effect_tag, pan, volume);
    }
}

// decomp: SLUS_010.40 func_800456EC (src/SLUS_010.40/main.c:9805)
void Audio_PlaySfxProgramChannel(void* data, int effect_tag, int pan, int volume)
{
    if (effect_tag == 0x200) {
        if (data != (void*)Audio_Playback_SoundEffectIdForTag(0x200)) {
            Audio_Playback_PlaySoundEffectBlob(data, 0x200, pan, volume);
        }
    } else {
        Audio_Playback_PlaySoundEffectBlob(data, effect_tag, pan, volume);
    }
}

// decomp: SLUS_010.40 vs_main_playSfx (src/SLUS_010.40/main.c:9816)
void Audio_PlaySfx(int channel_group, long id, int pan, int volume)
{
    void* bank_program;

    switch (channel_group) {
    case 0x7E:
    case 0xFE:
        if (Audio_Playback_UsesSoundEffectTags((Audio_SfxProgramHeader *)((void*)(id & 0xFFFF))) != 0) {
            Audio_PlayBuiltInSfxChannel(id & 0xFFFF, Audio_nextSfxChannelGroup7E, pan, volume);
            Audio_nextSfxChannelGroup7E *= 2;
            if (!(Audio_nextSfxChannelGroup7E & 0x7E)) {
                Audio_nextSfxChannelGroup7E = 2;
            }
        } else {
            Audio_PlayBuiltInSfxChannel(id & 0xFFFF, 0, pan, volume);
        }
        break;

    case 0x100:
    case 0x200:
        if (Audio_Playback_UsesSoundEffectTags((Audio_SfxProgramHeader *)((void*)(id & 0xFFFF))) != 0) {
            Audio_PlayBuiltInSfxChannel(id & 0xFFFF, Audio_nextSfxChannelGroup200, pan, volume);
        } else {
            Audio_PlayBuiltInSfxChannel(id & 0xFFFF, 0, pan, volume);
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
        bank_program = Audio_soundState.sfxBankData[Audio_soundState.currentSfxBank - 1];
        if (bank_program == NULL || id < 0) {
            Core_Errorf("audio.sfx", "unavailable SFX bank=%u or invalid id=%ld", Audio_soundState.currentSfxBank, id);
            return;
        }
        bank_program = (u_char*)(bank_program) + (*((int*)bank_program + id + 1));
        if (Audio_Playback_UsesSoundEffectTags((Audio_SfxProgramHeader *)(bank_program)) != 0) {
            Audio_PlaySfxProgramChannel(bank_program, Audio_nextSfxChannelGroupFF000, pan, volume);
            Audio_nextSfxChannelGroupFF000 *= 2;
            if (!(Audio_nextSfxChannelGroupFF000 & channel_group)) {
                Audio_nextSfxChannelGroupFF000 = 0x1000;
            }
        } else {
            Audio_PlaySfxProgramChannel(bank_program, 0, pan, volume);
        }
        break;

    case 0xF00000:
        bank_program = Audio_soundState.sfxBankData[2];
        if (bank_program == NULL) {
            Core_Warnf("audio.sfx", "room SFX id=%ld skipped: no room bank", id);
            return;
        }
        if (id < 0) {
            Core_Errorf("audio.sfx", "invalid room SFX id=%ld", id);
            return;
        }
        bank_program = (u_char*)(bank_program) + (*((int*)bank_program + id + 1));
        if (Audio_Playback_UsesSoundEffectTags((Audio_SfxProgramHeader *)(bank_program)) != 0) {
            Audio_PlaySfxProgramChannel(bank_program, Audio_nextSfxChannelGroupF00000, pan, volume);
            Audio_nextSfxChannelGroupF00000 *= 2;
            if (!(Audio_nextSfxChannelGroupF00000 & channel_group)) {
                Audio_nextSfxChannelGroupF00000 = 0x100000;
            }
        } else {
            Audio_PlaySfxProgramChannel(bank_program, 0, pan, volume);
        }
        break;

    default:
        Core_IgnoreDiagnostic(0x95, 0);
        break;
    }
}

// decomp: SLUS_010.40 vs_main_playSfxDefault (src/SLUS_010.40/main.c:9877)
void Audio_PlaySfxCentered(int channel_group, int id) { Audio_PlaySfx(channel_group, id, 0x80, 0x7F); }

// decomp: SLUS_010.40 vs_main_computeSfxPan (src/SLUS_010.40/main.c:9879)
int Audio_SfxPanAndVolume(int coords, int z)
{
    int coordinate_or_attenuation_work;
    int attenuated_volume;
    int depth_or_volume_limit;

    coordinate_or_attenuation_work = coords;

    if (coords < 0) {
        coordinate_or_attenuation_work += 0xFFFF;
    }

    coordinate_or_attenuation_work = (coordinate_or_attenuation_work >> 16) << 16;
    coords = (short)(coords - coordinate_or_attenuation_work) - 160;
    coords = (coords * 3) / 10;

    if (coords < -96) {
        coords = -96;
    }

    if (coords > 96) {
        coords = 96;
    }

    coords += 128;
    depth_or_volume_limit = z - 768;

    if (depth_or_volume_limit < 0) {
        depth_or_volume_limit = 0;
    }

    if (depth_or_volume_limit > 4095) {
        depth_or_volume_limit = 4095;
    }

    coordinate_or_attenuation_work = depth_or_volume_limit;

    if (depth_or_volume_limit < 0) {
        coordinate_or_attenuation_work = depth_or_volume_limit + 31;
    }

    coordinate_or_attenuation_work >>= 5;
    depth_or_volume_limit = 127;

    attenuated_volume = depth_or_volume_limit - coordinate_or_attenuation_work;
    return (coords << 16) + attenuated_volume;
}

// decomp: SLUS_010.40 _do_panSfx (src/SLUS_010.40/main.c:9927)
void Audio_PlayProjectedSfx(int channel_group, int effect_id, int coords, int z)
{
    coords = Audio_SfxPanAndVolume(coords, z);
    z = coords & 0xFFFF;
    coords = coords >> 0x10;
    if (z > 0) {
        Audio_PlaySfx(channel_group, effect_id, coords, z);
    }
}

// decomp: SLUS_010.40 vs_main_panSfx (src/SLUS_010.40/main.c:9937)
void Audio_PlayWorldSfx(int channel_group, int effect_id, SVECTOR* vec)
{
    int z;
    long coords;
    long value;
    long flag;

    Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
    Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
    z = Core_Gte_RotTransPers(vec, &coords, &value, &flag);
    Audio_PlayProjectedSfx(channel_group, effect_id, coords, z);
}

// decomp: SLUS_010.40 func_80045B28 (src/SLUS_010.40/main.c:9950)
intptr_t Audio_ResolveSfxProgram(int channel_group, int effect_id)
{
    intptr_t program_address;

    switch (channel_group) {
    case 0x1:
    case 0x7E:
    case 0xFE:
    case 0x100:
    case 0x200:
        program_address = 0;
        break;
    case 0xFF000:
    case 0xF00000:
        if (Audio_soundState.currentSfxBank == 0 ||
            Audio_soundState.currentSfxBank >
                sizeof Audio_soundState.sfxBankData / sizeof Audio_soundState.sfxBankData[0]) {
            Core_Errorf("audio.sfx", "cannot resolve banked SFX group=0x%X id=%d bank=%u", channel_group, effect_id, Audio_soundState.currentSfxBank);
            return -1;
        }
        program_address = (intptr_t)Audio_soundState.sfxBankData[Audio_soundState.currentSfxBank - 1];
        if (program_address == 0 || effect_id < 0 || (u_int)effect_id >= ((u_int*)program_address)[0]) {
            Core_Errorf("audio.sfx", "unavailable bank or invalid SFX id=%d bank=%u", effect_id, Audio_soundState.currentSfxBank);
            return -1;
        }
        {
            int program_offset = ((int*)program_address)[(size_t)effect_id + 1];
            if (program_offset < 0 ||
                (size_t)program_offset < ((size_t)((u_int*)program_address)[0] + 1) * sizeof(u_int)) {
                Core_Errorf("audio.sfx", "invalid banked SFX offset=%d id=%d", program_offset, effect_id);
                return -1;
            }
            program_address += program_offset;
        }
        break;
    default:
        Core_IgnoreDiagnostic(0x95, 0);
        program_address = 0;
        break;
    }
    return program_address;
}

// decomp: SLUS_010.40 func_80045BFC (src/SLUS_010.40/main.c:9975)
void Audio_FadeSfxVolume(int channel_group, int effect_id, int target_volume, char fade_ticks)
{
    intptr_t program_address;

    if (effect_id != 0) {
        program_address = Audio_ResolveSfxProgram(channel_group, effect_id);
        if (program_address == -1) return;

        if (program_address != 0) {
            Audio_Control_FadeSoundEffectVolume(program_address, 0, fade_ticks, target_volume);
        } else {
            do {
                Audio_Control_FadeSoundEffectVolume(effect_id, 0, fade_ticks, target_volume);
            } while (0);
        }
    } else {
        Audio_Control_FadeSoundEffectVolume(0, channel_group, fade_ticks, target_volume);
    }
}

// decomp: SLUS_010.40 func_80045C74 (src/SLUS_010.40/main.c:9994)
void Audio_FadeSfxPitch(int channel_group, int effect_id, int target_pitch_scale, char fade_ticks)
{
    intptr_t program_address;

    if (effect_id != 0) {
        program_address = Audio_ResolveSfxProgram(channel_group, effect_id);
        if (program_address == -1) return;
        if (program_address != 0) {
            Audio_Control_FadeSoundEffectPitch(program_address, 0, fade_ticks, target_pitch_scale);
        } else {
            do {
                Audio_Control_FadeSoundEffectPitch(effect_id, 0, fade_ticks, target_pitch_scale);
            } while (0);
        }
    } else {
        Audio_Control_FadeSoundEffectPitch(0, channel_group, fade_ticks, target_pitch_scale);
    }
}

// decomp: SLUS_010.40 func_80045CEC (src/SLUS_010.40/main.c:10012)
void Audio_FadeSfxPan(int channel_group, int effect_id, int target_pan, char fade_ticks)
{
    if (effect_id != 0) {
        intptr_t program_address = Audio_ResolveSfxProgram(channel_group, effect_id);
        if (program_address == -1) return;
        if (program_address != 0) {
            Audio_Control_FadeSoundEffectPan(program_address, 0, fade_ticks, target_pan);
        } else {
            do {
                Audio_Control_FadeSoundEffectPan(effect_id, 0, fade_ticks, target_pan);
            } while (0);
        }
    } else {
        Audio_Control_FadeSoundEffectPan(0, channel_group, fade_ticks, target_pan);
    }
}

// decomp: SLUS_010.40 func_80045D64 (src/SLUS_010.40/main.c:10028)
void Audio_StopSfx(int channel_group, int effect_id)
{
    if (effect_id != 0) {
        intptr_t program_address = Audio_ResolveSfxProgram(channel_group, effect_id);
        if (program_address == -1) return;

        if (program_address != 0) {
            Audio_Playback_StopSoundEffects(program_address, 0);
        } else {
            Audio_Playback_StopSoundEffects(effect_id, 0);
        }
    } else {
        Audio_Playback_StopSoundEffects(0, channel_group);
    }
}

// decomp: SLUS_010.40 func_80045DC0 (src/SLUS_010.40/main.c:10043)
void Audio_StopAllSfxPlayback(void) { Audio_Control_StopSceneSoundEffects(); }

// decomp: SLUS_010.40 func_800461CC (src/SLUS_010.40/main.c:10178)
void Audio_PlayBankSfx(int channel_group, u_int sound_bank[], u_int sound_index, int pan, int volume)
{
    int effect_tag;
    void* bank_program;

    bank_program = sound_bank;

    if (sound_index < sound_bank[0]) {
        bank_program = (char*)sound_bank + (sound_bank + sound_index)[1];
        effect_tag = 0;

        if (channel_group == 0x180) {
            effect_tag = Audio_nextSfxChannelGroup180;
            Audio_nextSfxChannelGroup180 *= 2;
            if (!(Audio_nextSfxChannelGroup180 & 0x180)) {
                Audio_nextSfxChannelGroup180 = 0x80;
            }
        } else if (channel_group == 0x200) {
            effect_tag = Audio_nextSfxChannelGroup200;
            Audio_nextSfxChannelGroup200 *= 2;
            if (!(Audio_nextSfxChannelGroup200 & 0x200)) {
                Audio_nextSfxChannelGroup200 = channel_group;
            }
        } else if (channel_group == 0xF00000) {
            effect_tag = Audio_nextSfxChannelGroupF00000;
            Audio_nextSfxChannelGroupF00000 *= 2;
            if (!(Audio_nextSfxChannelGroupF00000 & channel_group)) {
                Audio_nextSfxChannelGroupF00000 = 0x100000;
            }
        } else if (channel_group == 0x7E) {
        } else {
            Core_IgnoreDiagnostic(0x95, 0);
        }
        Audio_PlaySfxProgramChannel(bank_program, effect_tag, pan, volume);
    }
}

// decomp: SLUS_010.40 func_800462E8 (src/SLUS_010.40/main.c:10215)
void Audio_PlayBankSfxCentered(int channel_group, int* sound_bank, u_int sound_index)
{
    Audio_PlayBankSfx(channel_group, (u_int *)(sound_bank), sound_index, 0x80, 0x7F);
}

// decomp: SLUS_010.40 func_80046310 (src/SLUS_010.40/main.c:10220)
void Audio_PlayProjectedBankSfx(int channel_group, u_int sound_bank[], int sound_index, int projection_or_pan, int projected_depth)
{
    int depth_or_volume;
    int pan_work;
    int coordinate_or_depth_work;

    coordinate_or_depth_work = projection_or_pan;
    pan_work = projection_or_pan;
    projection_or_pan = pan_work >> 0x10;

    if (coordinate_or_depth_work < 0) {
        pan_work = coordinate_or_depth_work + 0xFFFF;
    }

    projection_or_pan = pan_work >> 0x10;
    pan_work = projection_or_pan << 0x10;
    projection_or_pan = coordinate_or_depth_work - pan_work;

    if (projection_or_pan < 0) {
        projection_or_pan = 0;
    }

    if (projection_or_pan >= 0x141) {
        projection_or_pan = 0x140;
    }

    pan_work = projection_or_pan + ((u_int)projection_or_pan >> 0x1F);
    pan_work >>= 1;
    projection_or_pan = pan_work + 0x30;
    depth_or_volume = projected_depth;

    if (depth_or_volume < 0) {
        depth_or_volume = 0;
    }

    if (depth_or_volume >= 0x401) {
        depth_or_volume = 0x400;
    }

    coordinate_or_depth_work = depth_or_volume;

    if (depth_or_volume < 0) {
        coordinate_or_depth_work += 7;
    }

    depth_or_volume = 0x7F - (coordinate_or_depth_work >> 3);

    if (depth_or_volume > 0) {
        Audio_PlayBankSfx(channel_group, sound_bank, sound_index, projection_or_pan, depth_or_volume);
    }
}

// decomp: SLUS_010.40 func_800463BC (src/SLUS_010.40/main.c:10272)
void Audio_PlayWorldBankSfx(int channel_group, u_int* sound_bank, int sound_index, SVECTOR* vec)
{
    int z;
    long sxy;
    long value;
    long flag;

    Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
    Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
    z = Core_Gte_RotTransPers(vec, &sxy, &value, &flag);
    Audio_PlayProjectedBankSfx(channel_group, sound_bank, sound_index, sxy, z);
}

// decomp: SLUS_010.40 func_8004644C (src/SLUS_010.40/main.c:10285)
void Audio_StopBankSfx(int channel_group, int* sound_bank, int sound_index)
{
    int effect_tag_mask;
    char* program_address = (char*)sound_bank + (sound_bank + sound_index)[1];

    if (sound_index != 0) {
        effect_tag_mask = 0;
    } else {
        program_address = 0;
        effect_tag_mask = channel_group;
    }
    Audio_Playback_StopSoundEffects((intptr_t)program_address, effect_tag_mask);
}

// decomp: SLUS_010.40 func_80046494 (src/SLUS_010.40/main.c:10299)
void Audio_FadeBankSfxVolume(int channel_group, int* sound_bank, int sound_index, int target_volume, int fade_ticks)
{
    intptr_t program_address = (intptr_t)((char*)sound_bank + (sound_index + sound_bank)[1]);

    if (sound_index == 0) {
        Audio_Control_FadeSoundEffectVolume(0, channel_group, fade_ticks & 0xFF, target_volume);
    } else {
        Audio_Control_FadeSoundEffectVolume(program_address, 0, fade_ticks & 0xFF, target_volume);
    }
}

}  // extern "C"
