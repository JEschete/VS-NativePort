// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "State/Progress.h"
#include "State/StateFlags.h"

// decomp: g_ActiveMusicChannels (g_ActiveMusicChannels@SLUS_010.40)
Audio_SeqChannel Audio_Seq_musicChannels[0x20];

// decomp: g_PushedMusicConfig (g_PushedMusicConfig@SLUS_010.40)
Audio_MusicConfig Audio_Seq_savedMusicConfig;

// decomp: D_800358FE (D_800358FE@SLUS_010.40)
short Audio_Seq_savedMusicId;

// decomp: g_pSecondaryMusicChannels (g_pSecondaryMusicChannels@SLUS_010.40)
Audio_SeqChannel* Audio_Seq_secondaryMusicChannels;

// decomp: g_pSavedMousicConfig (g_pSavedMousicConfig@SLUS_010.40)
Audio_MusicConfig* Audio_Seq_secondaryMusicConfig;

// decomp: D_800366F0 (D_800366F0@SLUS_010.40)
Audio_MusicConfig Audio_Seq_primaryMusicConfig;

// decomp: D_80037788 (D_80037788@SLUS_010.40)
Audio_CommandParams Audio_Seq_pendingMusicData;

// decomp: g_Music_LoopCounter (g_Music_LoopCounter@SLUS_010.40)
u_int Audio_Seq_musicLoopsRemaining;

// decomp: g_pActiveMusicConfig (g_pActiveMusicConfig@SLUS_010.40)
Audio_MusicConfig* Audio_Seq_activeMusicConfig;

// decomp: D_800378E8 (D_800378E8@SLUS_010.40)
Audio_SeqChannel Audio_Seq_savedMusicChannels[32];

// decomp: vs_main_materialModifiers (vs_main_materialModifiers@SLUS_010.40)
char Audio_Seq_unusedMaterialModifiers[8][30]; /* data: contents loaded in Step 5 */

// decomp: vs_main_scoredata (vs_main_scoredata@SLUS_010.40)
State_ScoreData Audio_Seq_unusedScoredata;

// decomp: vs_main_artsStatus (vs_main_artsStatus@SLUS_010.40)
State_ArtsStatus Audio_Seq_unusedArtsStatus;

// decomp: vs_main_inventory (vs_main_inventory@SLUS_010.40)
Items_Inventory Audio_Seq_unusedInventory;

// decomp: vs_main_stateFlags (vs_main_stateFlags@SLUS_010.40)
State_StateFlags Audio_Seq_unusedStateFlags;

// decomp: vs_main_inventoryIndices (vs_main_inventoryIndices@SLUS_010.40)
Items_InventoryIndices Audio_Seq_unusedInventoryIndices;

// decomp: SLUS_010.40 SoundVM_A3_ChannelMasterVolume (src/SLUS_010.40/25AC.c:5125)
void Audio_Seq_SetVolumeBalance(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->volumeBalance = *in_pChannel->programCounter++ << 8;
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
}

// decomp: SLUS_010.40 SoundVM_FE12_VolumeBalanceSlide (src/SLUS_010.40/25AC.c:5132)
void Audio_Seq_SlideVolumeBalance(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short VolumeBalance;
    u_short Length;
    int Target;
    int Delta;

    Length = *in_pChannel->programCounter++;
    in_pChannel->volumeBalanceSlideLength = Length;
    if (Length == 0) {
        in_pChannel->volumeBalanceSlideLength = 0x100;
    }

    Target = *in_pChannel->programCounter++ << 8;

    VolumeBalance = in_pChannel->volumeBalance & 0x7F00;
    in_pChannel->volumeBalance = VolumeBalance;
    Delta = Target - VolumeBalance;
    in_pChannel->volumeBalanceSlideStep = Delta / in_pChannel->volumeBalanceSlideLength;
}

// decomp: SLUS_010.40 SoundVM_A8_ChannelVolume (src/SLUS_010.40/25AC.c:5154)
void Audio_Seq_SetVolume(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->volume = (signed char)*in_pChannel->programCounter++ << 0x17;
    in_pChannel->volumeSlideLength = 0;
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
    in_pChannel->keyOnVolumeSlideLength = 0;
}

// decomp: SLUS_010.40 SoundVM_A9_ChannelVolumeSlide (src/SLUS_010.40/25AC.c:5163)
void Audio_Seq_SlideVolume(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int Prev;
    int Dest;
    u_short Length;
    int Delta;

    Length = *in_pChannel->programCounter++;
    in_pChannel->volumeSlideLength = Length;
    if (Length == 0) {
        in_pChannel->volumeSlideLength = 0x100;
    }
    Dest = ((signed char)*in_pChannel->programCounter++ << 0x17);

    Prev = in_pChannel->volume & 0xFFFF0000;
    in_pChannel->volume = Prev;

    Delta = Dest - Prev;
    in_pChannel->volumeSlideStep = Delta / in_pChannel->volumeSlideLength;

    in_pChannel->keyOnVolumeSlideLength = 0;
}

// decomp: SLUS_010.40 SoundVM_FE19_unk (src/SLUS_010.40/25AC.c:5187)
void Audio_Seq_SetKeyOnVolumeRamp(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short pPc1;
    int Dest;
    int Prev;
    int Delta;

    in_pChannel->keyOnVolume = ((signed char)*in_pChannel->programCounter++) << 0x17;
    pPc1 = *in_pChannel->programCounter++;
    in_pChannel->keyOnVolumeSlideLength = (short)pPc1;

    if (pPc1 == 0) {
        in_pChannel->keyOnVolumeSlideLength = 0x100;
    }

    Dest = ((signed char)*in_pChannel->programCounter++) << 0x17;
    Prev = in_pChannel->keyOnVolume;
    Delta = Dest - Prev;
    in_pChannel->keyOnVolumeSlideStep = Delta / in_pChannel->keyOnVolumeSlideLength;
}

// decomp: SLUS_010.40 SoundVM_FE1A_unk (src/SLUS_010.40/25AC.c:5209)
void Audio_Seq_EnableFullNoteLength(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->updateFlags |= SOUND_UPDATE_UNKNOWN_01;
}

// decomp: SLUS_010.40 SoundVM_FE1B_unk (src/SLUS_010.40/25AC.c:5215)
void Audio_Seq_DisableFullNoteLength(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->updateFlags &= ~SOUND_UPDATE_UNKNOWN_01;
}

// decomp: SLUS_010.40 SoundVM_unused1 (src/SLUS_010.40/25AC.c:5234)
void Audio_Seq_SetEffectVolumeScale(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->effectVolume = *in_pChannel->programCounter++ << 8;
    in_pChannel->effectVolumeSlideLength = 0;
    if (in_pChannel->updateFlags & 0x100) {
        in_pChannel->voice.dirtyFlags |= 3;
    }
}

// decomp: SLUS_010.40 SoundVM_unused2 (src/SLUS_010.40/25AC.c:5246)
void Audio_Seq_SlideEffectVolumeScale(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    short v;
    int pc;

    if ((in_pChannel->effectVolumeSlideLength = *in_pChannel->programCounter++) == 0) {
        in_pChannel->effectVolumeSlideLength = 0x100;
    }

    pc = *(in_pChannel->programCounter++) << 8;
    v = in_pChannel->effectVolume & 0xFF00;
    in_pChannel->effectVolumeSlideStep = ((pc - v) / in_pChannel->effectVolumeSlideLength);
    in_pChannel->effectVolume = v;
}

// decomp: SLUS_010.40 SoundVM_AA_ChannelPan (src/SLUS_010.40/25AC.c:5264)
void Audio_Seq_SetPan(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    // Convert signed pan (-64..+63) to unsigned 0..255, center at 0x40 and store as Q8.8
    // pan value
    in_pChannel->pan = ((*in_pChannel->programCounter++ + 0x40) & 0xFF) << 8;
    in_pChannel->panSlideLength = 0;
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_VOLUME;
}

// decomp: SLUS_010.40 SoundVM_AB_ChannelPanSlide (src/SLUS_010.40/25AC.c:5274)
void Audio_Seq_SlidePan(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short Prev;
    u_short Length;
    u_short Dest;
    int Delta;

    Length = *in_pChannel->programCounter++;
    in_pChannel->panSlideLength = Length;
    if (Length == 0) {
        in_pChannel->panSlideLength = 0x100;
    }

    Dest = *in_pChannel->programCounter++;
    Dest = ((Dest + 0x40) & 0xFF) << 8; // Center it
    Prev = in_pChannel->pan & 0xFF00;
    Delta = Dest - Prev;
    in_pChannel->panSlideStep = Delta / in_pChannel->panSlideLength;
    in_pChannel->pan = Prev;
}

// decomp: SLUS_010.40 SoundVM_A5_SetOctave (src/SLUS_010.40/25AC.c:5296)
void Audio_Seq_SetOctave(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->octave = *in_pChannel->programCounter++;
}

// decomp: SLUS_010.40 SoundVM_A6_IncreaseOctave (src/SLUS_010.40/25AC.c:5302)
void Audio_Seq_IncreaseOctave(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->octave = (in_pChannel->octave + 1) & 0xF;
}

// decomp: SLUS_010.40 SoundVM_A7_DecreaseOctave (src/SLUS_010.40/25AC.c:5308)
void Audio_Seq_DecreaseOctave(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->octave = (in_pChannel->octave - 1) & 0xF;
}

// decomp: SLUS_010.40 SoundVM_A1_LoadInstrument (src/SLUS_010.40/25AC.c:5314)
void Audio_Seq_LoadInstrument(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int pc = *in_pChannel->programCounter++;

    int var_s0 = in_pChannel->type == 0
                   ? Audio_Seq_ResolveMusicInstrument(Audio_Seq_activeMusicConfig->statusFlags, pc)
                   : Audio_Seq_ResolveSoundEffectInstrument(in_pChannel->effectVoiceMask, pc);
    Audio_Seq_ApplyInstrumentParameters(
        in_pChannel, &Audio_Samples_instruments[var_s0], Audio_Samples_instruments[var_s0].sampleStart);
    in_pChannel->instrumentIndex = var_s0;
    in_pChannel->voice.volumeScale = 0;
    in_pChannel->updateFlags &= 0xE6FFEFF7;
}

// decomp: SLUS_010.40 SoundVM_FE0A_ClearInstrument (src/SLUS_010.40/25AC.c:5329)
void Audio_Seq_LoadSilentInstrument(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short Index;

    Index = *in_pChannel->programCounter++;
    Audio_Seq_ApplyInstrumentParameters(in_pChannel, &Audio_Samples_instruments[Index], 0x1010);
    in_pChannel->instrumentIndex = Index;
    in_pChannel->voice.volumeScale = 0;
    in_pChannel->updateFlags &=
        ~(SOUND_UPDATE_DRUM_MODE | SOUND_UPDATE_UNKNOWN_12 | SOUND_UPDATE_UNKNOWN_24
            | SOUND_UPDATE_UNKNOWN_27 | SOUND_UPDATE_UNKNOWN_28);
}

// decomp: SLUS_010.40 SoundVM_FE14_ChangePatch (src/SLUS_010.40/25AC.c:5343)
void Audio_Seq_SelectPatch(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short* pPatchTable;
    u_char PatchIndex;

    PatchIndex = *in_pChannel->programCounter++;
    if (Audio_Seq_activeMusicConfig->patchTable != NULL) {
        pPatchTable = (u_short*)Audio_Seq_activeMusicConfig->patchTable;
        if (pPatchTable[PatchIndex] > 0x8000) {
            in_pChannel->voice.volumeScale = 0;
            in_pChannel->updateFlags &= ~SOUND_UPDATE_UNKNOWN_12;
            return;
        }
        in_pChannel->keymap = (char*)((uintptr_t)pPatchTable + pPatchTable[PatchIndex] + 0x20);
        in_pChannel->updateFlags &=
            ~(SOUND_UPDATE_DRUM_MODE | SOUND_UPDATE_UNKNOWN_12 | SOUND_UPDATE_UNKNOWN_24
                | SOUND_UPDATE_UNKNOWN_27 | SOUND_UPDATE_UNKNOWN_28);
        in_pChannel->updateFlags |= SOUND_UPDATE_UNKNOWN_12;
        in_pChannel->key = 0xFF;
    }
}

// decomp: SLUS_010.40 SoundVM_C0_ChannelTranspose_Absolute (src/SLUS_010.40/25AC.c:5378)
void Audio_Seq_SetTranspose(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->transpose = (signed char)*in_pChannel->programCounter++;
}

// decomp: SLUS_010.40 SoundVM_C1_ChannelTranspose_Relative (src/SLUS_010.40/25AC.c:5384)
void Audio_Seq_AddTranspose(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->transpose += (signed char)*in_pChannel->programCounter++;
}

// decomp: SLUS_010.40 SoundVM_A4_PitchBendSlide (src/SLUS_010.40/25AC.c:5390)
void Audio_Seq_SetPitchBendSlide(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int Length = *in_pChannel->programCounter++;
    in_pChannel->pitchBendSlideLength = Length;
    if (Length == 0) {
        in_pChannel->pitchBendSlideLength = 0x100;
    }

    in_pChannel->pitchBendTarget = (signed char)*in_pChannel->programCounter++;
}

// decomp: src/SLUS_010.40/25AC.c:4984
#define SOUND_DEFAULT_PORTAMENTO_STEPS (0x100) // 256 steps
// decomp: SLUS_010.40 SoundVM_DA_EnablePortamento (src/SLUS_010.40/25AC.c:5402)
void Audio_Seq_EnablePortamento(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int Steps = *in_pChannel->programCounter++;
    in_pChannel->portamentoSteps = Steps;

    if (Steps == 0) {
        in_pChannel->portamentoSteps = SOUND_DEFAULT_PORTAMENTO_STEPS;
    }

    in_pChannel->portamentoTranspose = 0;
    in_pChannel->portamentoKey = 0;
    in_pChannel->playModes = 1;
}
#undef SOUND_DEFAULT_PORTAMENTO_STEPS

// decomp: SLUS_010.40 SoundVM_DB_DisablePortamento (src/SLUS_010.40/25AC.c:5417)
void Audio_Seq_DisablePortamento(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->portamentoSteps = 0;
}

// decomp: SLUS_010.40 SoundVM_D8_ChannelFineTune_Absolute (src/SLUS_010.40/25AC.c:5423)
void Audio_Seq_SetFineTune(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_int ScaledFineTune;
    int FinePitchDelta;

    in_pChannel->fineTune = (signed char)*in_pChannel->programCounter++;
    ScaledFineTune = in_pChannel->pitchBase * (char)in_pChannel->fineTune;

    if (in_pChannel->fineTune < 0) {
        FinePitchDelta = (ScaledFineTune >> 8) - in_pChannel->pitchBase;
    } else {
        FinePitchDelta = ScaledFineTune >> 7;
    }

    in_pChannel->finePitchDelta = FinePitchDelta;
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_SAMPLE_RATE;
}

// decomp: SLUS_010.40 SoundVM_D9_ChannelFineTune_Relative (src/SLUS_010.40/25AC.c:5442)
void Audio_Seq_AddFineTune(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_int ScaledFineTune;
    int FinePitchDelta;

    in_pChannel->fineTune =
        in_pChannel->fineTune + (signed char)*in_pChannel->programCounter++;
    ScaledFineTune = in_pChannel->pitchBase * (char)in_pChannel->fineTune;

    if (in_pChannel->fineTune < 0) {
        FinePitchDelta = (ScaledFineTune >> 8) - in_pChannel->pitchBase;
    } else {
        FinePitchDelta = ScaledFineTune >> 7;
    }

    in_pChannel->finePitchDelta = FinePitchDelta;
    in_pChannel->voice.dirtyFlags |= VOICE_PARAM_SAMPLE_RATE;
}

// decomp: SLUS_010.40 SoundVM_CC_EnableLegato (src/SLUS_010.40/25AC.c:5752)
void Audio_Seq_EnableLegato(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->playModes = SOUND_SFX_LEGATO;
}

// decomp: SLUS_010.40 SoundVM_D0_EnableSustainedNote (src/SLUS_010.40/25AC.c:5763)
void Audio_Seq_EnableSustainedSoundEffect(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    if (in_pChannel->type != SOUND_CHANNEL_TYPE_MUSIC) {
        in_pChannel->playModes = SOUND_SFX_FULL_LENGTH;
    }
}

// decomp: SLUS_010.40 SoundVM_A2_OverwriteNextNoteLength (src/SLUS_010.40/25AC.c:6001)
void Audio_Seq_SetNextNoteLength(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short Length;

    Length = *in_pChannel->programCounter++;
    in_pChannel->fixedNoteLength = 0;
    in_pChannel->gateLength = Length;
    in_pChannel->noteLength = Length;
    in_pChannel->storedNoteLength = Length;
}

// decomp: SLUS_010.40 SoundVM_DC_FixNoteLength (src/SLUS_010.40/25AC.c:6015)
void Audio_Seq_SetFixedNoteLength(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int NoteLength;

    NoteLength = *(signed char*)in_pChannel->programCounter++;
    if (NoteLength != 0) {
        NoteLength += in_pChannel->storedNoteLength;
        if (NoteLength <= 0) {
            NoteLength = 1;
        } else if (NoteLength >= 256) {
            NoteLength = 255;
        }
    }
    in_pChannel->fixedNoteLength = NoteLength;
}

// decomp: SLUS_010.40 SoundVM_FE04_ClearKeymapTable (src/SLUS_010.40/25AC.c:6032)
void Audio_Seq_EnableDrumKeymap(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    if (Audio_Seq_activeMusicConfig->keymapTable != NULL) {
        in_pChannel->updateFlags &=
            ~(SOUND_UPDATE_DRUM_MODE | SOUND_UPDATE_UNKNOWN_12 | SOUND_UPDATE_UNKNOWN_24
                | SOUND_UPDATE_UNKNOWN_27 | SOUND_UPDATE_UNKNOWN_28);
        in_pChannel->updateFlags |= SOUND_UPDATE_DRUM_MODE;
    }
}

// decomp: SLUS_010.40 SoundVM_FE05_MuteVoice (src/SLUS_010.40/25AC.c:6043)
void Audio_Seq_MuteInstrument(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->voice.volumeScale = 0;
    in_pChannel->updateFlags &= ~SOUND_UPDATE_DRUM_MODE;
}

// decomp: SLUS_010.40 SoundVM_E0_unk (src/SLUS_010.40/25AC.c:6196)
void Audio_Seq_EnableLoopBoundaryStop(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->updateFlags |= SOUND_UPDATE_UNKNOWN_20;
}

