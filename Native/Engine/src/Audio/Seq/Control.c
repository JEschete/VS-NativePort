// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/AkaoFormat.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Control.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SoundEffects/SoundEffects.h"

// decomp: SLUS_010.40 SoundVM_A0_FinishChannel (src/SLUS_010.40/25AC.c:4989)
void Audio_Seq_FinishChannel(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags)
{
    if (in_pChannel->type == SOUND_CHANNEL_TYPE_MUSIC) {
        Audio_Seq_activeMusicConfig->activeChannels &= ~in_VoiceFlags;
        if (Audio_Seq_activeMusicConfig->activeChannels == 0) {
            Audio_Seq_musicLoopsRemaining = 0;
            Audio_Seq_activeMusicConfig->musicId = 0;
            Audio_Seq_activeMusicConfig->statusFlags = 0;
        }
        Audio_Seq_activeMusicConfig->activeNotes &= ~in_VoiceFlags;
        Audio_Seq_activeMusicConfig->keyedVoices &= ~in_VoiceFlags;
        Audio_Seq_activeMusicConfig->allocatedVoices &= ~in_VoiceFlags;
        Audio_Seq_activeMusicConfig->noiseChannels &= ~in_VoiceFlags;
        Audio_Seq_activeMusicConfig->reverbChannels &= ~in_VoiceFlags;
        Audio_Seq_activeMusicConfig->fmChannels &= ~in_VoiceFlags;
    } else {
        Audio_Seq_ReleaseSoundEffectVoice(in_pChannel, in_VoiceFlags);
    }
    in_pChannel->updateFlags = 0;
    Audio_Driver_controlLatches.updateFlags |= SOUND_GLOBAL_UPDATE_04 | SOUND_GLOBAL_UPDATE_08;
}

// decomp: SLUS_010.40 SoundVM_FE1F_unk (src/SLUS_010.40/25AC.c:5011)
void Audio_Seq_FinishChannelOnReplacement(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags)
{
    if (Audio_Seq_activeMusicConfig->statusFlags & 0x10000) {
        Audio_Seq_FinishChannel(in_pChannel, in_VoiceFlags);
    }
}

// decomp: SLUS_010.40 SoundVM_FE00_SetTempo (src/SLUS_010.40/25AC.c:5018)
void Audio_Seq_SetTempo(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    Audio_Seq_activeMusicConfig->tempo = in_pChannel->programCounter[0] << 0x10;
    Audio_Seq_activeMusicConfig->tempo |= in_pChannel->programCounter[1] << 0x18;
    in_pChannel->programCounter += sizeof(short);
    Audio_Seq_activeMusicConfig->tempoSlideLength = 0;
}

// decomp: SLUS_010.40 SoundVM_FE01_SetTempoSlide (src/SLUS_010.40/25AC.c:5027)
void Audio_Seq_SlideTempo(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    char* pc;
    int Dest;
    int Prev;
    int Delta;

    if ((Audio_Seq_activeMusicConfig->tempoSlideLength = *in_pChannel->programCounter++) == 0) {
        Audio_Seq_activeMusicConfig->tempoSlideLength = 0x100;
    }
    pc = in_pChannel->programCounter;
    Dest = pc[0] << 0x10;
    Dest |= pc[1] << 0x18;
    in_pChannel->programCounter += 2;
    Prev = Audio_Seq_activeMusicConfig->tempo & 0xFFFF0000;
    Delta = Dest - Prev;
    Audio_Seq_activeMusicConfig->tempoSlideStep = Delta / Audio_Seq_activeMusicConfig->tempoSlideLength;
    Audio_Seq_activeMusicConfig->tempo = Prev;
}

// decomp: SLUS_010.40 SoundVM_FE02_SetMasterReverbDepth (src/SLUS_010.40/25AC.c:5048)
void Audio_Seq_SetReverbDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    char* pc;
    u_int Depth;

    pc = in_pChannel->programCounter;
    Depth = (signed char)pc[1] << 0x14;
    Depth |= pc[0] << 0xC;
    in_pChannel->programCounter += sizeof(short);
    Audio_Seq_activeMusicConfig->reverbDepthSlideLength = 0;
    Audio_Driver_controlLatches.updateFlags |= SOUND_GLOBAL_UPDATE_07;
    Audio_Seq_activeMusicConfig->reverbDepth = Depth;
}

// decomp: SLUS_010.40 SoundVM_FE03_SetMasterReverbSlide (src/SLUS_010.40/25AC.c:5063)
void Audio_Seq_SlideReverbDepth(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int Prev;
    int Dest;
    int Delta;
    char* pc;

    Audio_Seq_activeMusicConfig->reverbDepthSlideLength = *in_pChannel->programCounter++;
    if (Audio_Seq_activeMusicConfig->reverbDepthSlideLength == 0) {
        Audio_Seq_activeMusicConfig->reverbDepthSlideLength = 0x100;
    }
    pc = in_pChannel->programCounter;
    Dest = (signed char)pc[1] << 0x14;
    Dest |= (char)pc[0] << 0xC;
    in_pChannel->programCounter += 2;
    Prev = Audio_Seq_activeMusicConfig->reverbDepth & ~0xFFF;
    Audio_Seq_activeMusicConfig->reverbDepth = Prev;
    Delta = Dest - Prev;
    Audio_Seq_activeMusicConfig->reverbDepthSlideStep = Delta / Audio_Seq_activeMusicConfig->reverbDepthSlideLength;
}

// decomp: src/SLUS_010.40/25AC.c:4987
#define READ_16LE_PC(pc) ((pc[0]) | (pc[1] << 8))
// decomp: SLUS_010.40 SoundVM_FE06_JumpRelativeOffset (src/SLUS_010.40/25AC.c:5085)
void Audio_Seq_JumpRelative(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    short Offset = READ_16LE_PC(in_pChannel->programCounter);
    in_pChannel->programCounter += Offset;
}
#undef READ_16LE_PC

// decomp: src/SLUS_010.40/25AC.c:4987
#define READ_16LE_PC(pc) ((pc[0]) | (pc[1] << 8))
// decomp: SLUS_010.40 SoundVM_FE07_JumpRelativeWithThreshold (src/SLUS_010.40/25AC.c:5092)
void Audio_Seq_JumpAtThreshold(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    short Threshold;
    short Offset;

    Threshold = *in_pChannel->programCounter++;

    if (Audio_Seq_activeMusicConfig->jumpThreshold >= Threshold) {
        // Read signed 16-bit LE offset at current pc, jump relative to pc
        Offset = READ_16LE_PC(in_pChannel->programCounter);
        in_pChannel->programCounter += Offset;
        return;
    }

    // Skip over the 16-bit offset operand
    in_pChannel->programCounter += sizeof(short);
}
#undef READ_16LE_PC

// decomp: src/SLUS_010.40/25AC.c:4987
#define READ_16LE_PC(pc) ((pc[0]) | (pc[1] << 8))
// decomp: SLUS_010.40 SoundVM_FE0E_CallRelativeOffset (src/SLUS_010.40/25AC.c:5111)
void Audio_Seq_CallRelative(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    short Offset = READ_16LE_PC(in_pChannel->programCounter);
    in_pChannel->returnAddress = in_pChannel->programCounter + sizeof(short);
    in_pChannel->programCounter += Offset;
}
#undef READ_16LE_PC

// decomp: SLUS_010.40 SoundVM_FE0F_Return (src/SLUS_010.40/25AC.c:5119)
void Audio_Seq_ReturnFromCall(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->programCounter = in_pChannel->returnAddress;
}

// decomp: SLUS_010.40 SoundVM_FE0C_unk (src/SLUS_010.40/25AC.c:5221)
void Audio_Seq_SetSequenceControlValue(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    Audio_Seq_activeMusicConfig->reserved34 = *in_pChannel->programCounter++;
}

// decomp: SLUS_010.40 SoundVM_nop (src/SLUS_010.40/25AC.c:5227)
void Audio_Seq_IgnoreInstruction(Audio_SeqChannel* in_pChannels __attribute__((unused)),
    u_int in_VoiceFlags __attribute__((unused)))
{
}

// decomp: SLUS_010.40 SoundVM_FE10_unk (src/SLUS_010.40/25AC.c:5892)
void Audio_Seq_SetVoiceAllocationStart(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    Audio_Seq_activeMusicConfig->voiceSearchStart = *in_pChannel->programCounter++;
}

// decomp: SLUS_010.40 SoundVM_FE11_unk (src/SLUS_010.40/25AC.c:5898)
void Audio_Seq_ResetVoiceAllocationStart(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    Audio_Seq_activeMusicConfig->voiceSearchStart = 0;
}

// decomp: SLUS_010.40 SoundVM_C8_LoopPoint (src/SLUS_010.40/25AC.c:5904)
void Audio_Seq_PushLoop(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->loopDepth = (in_pChannel->loopDepth + 1) & 3;
    in_pChannel->loopStarts[in_pChannel->loopDepth] = in_pChannel->programCounter;
    in_pChannel->loopIterations[in_pChannel->loopDepth] = 0;
    in_pChannel->loopStepSnapshots[in_pChannel->loopDepth] =
        in_pChannel->opcodeStepCounter;
}

// decomp: SLUS_010.40 SoundVM_C9_LoopN (src/SLUS_010.40/25AC.c:5914)
void Audio_Seq_RepeatLoop(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short NewCount;
    u_short TopIndex;
    u_short DesiredLoopCount;

    DesiredLoopCount = *in_pChannel->programCounter++;
    if (DesiredLoopCount == 0) {
        DesiredLoopCount = 0x100;
    }

    TopIndex = in_pChannel->loopDepth;
    NewCount = in_pChannel->loopIterations[TopIndex] + 1;
    in_pChannel->loopIterations[TopIndex] = NewCount;

    if (NewCount != DesiredLoopCount) {
        in_pChannel->programCounter = in_pChannel->loopStarts[in_pChannel->loopDepth];
        in_pChannel->opcodeStepCounter =
            in_pChannel->loopStepSnapshots[in_pChannel->loopDepth];
        return;
    }

    in_pChannel->loopDepth =
        (in_pChannel->loopDepth - 1) & SOUND_LOOP_STACK_MAX_INDEX;
}

// decomp: src/SLUS_010.40/25AC.c:4987
#define READ_16LE_PC(pc) ((pc[0]) | (pc[1] << 8))
// decomp: SLUS_010.40 SoundVM_FE08_JumpOnNthLoopPass (src/SLUS_010.40/25AC.c:5941)
void Audio_Seq_JumpOnLoopPass(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short N;
    short JumpOffset;

    // pLoopCount   pOffset
    // 0x01         0x02          0x03         0x04

    N = *in_pChannel->programCounter++;
    if (N == 0) {
        N = 0x100;
    }

    // If current loop's iteration + 1 != N
    if ((in_pChannel->loopIterations[in_pChannel->loopDepth] + 1) != N) {
        // Jumps to next program instruction
        in_pChannel->programCounter = in_pChannel->programCounter + sizeof(u_short);
        return;
    }

    // Read the relative offset
    JumpOffset = READ_16LE_PC(in_pChannel->programCounter);

    // Jump from pOffset to the relative offset
    in_pChannel->programCounter += JumpOffset;
}
#undef READ_16LE_PC

// decomp: src/SLUS_010.40/25AC.c:4987
#define READ_16LE_PC(pc) ((pc[0]) | (pc[1] << 8))
// decomp: SLUS_010.40 SoundVM_FE09_JumpAndPopStackOnNthLoopPass (src/SLUS_010.40/25AC.c:5969)
void Audio_Seq_ExitLoopOnPass(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    u_short N;
    short JumpOffset;

    N = *in_pChannel->programCounter++;
    if (N == 0) {
        N = 0x100;
    }

    if ((in_pChannel->loopIterations[in_pChannel->loopDepth] + 1) != N) {
        in_pChannel->programCounter += sizeof(u_short);
        return;
    }

    JumpOffset = READ_16LE_PC(in_pChannel->programCounter);

    in_pChannel->programCounter += JumpOffset;
    in_pChannel->loopDepth =
        (in_pChannel->loopDepth - 1) & SOUND_LOOP_STACK_MAX_INDEX;
}
#undef READ_16LE_PC

// decomp: SLUS_010.40 SoundVM_CA_LoopInf (src/SLUS_010.40/25AC.c:5992)
void Audio_Seq_RepeatLoopForever(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    in_pChannel->loopIterations[in_pChannel->loopDepth] += 1;
    in_pChannel->programCounter = in_pChannel->loopStarts[in_pChannel->loopDepth];
    in_pChannel->opcodeStepCounter =
        in_pChannel->loopStepSnapshots[in_pChannel->loopDepth];
}

// decomp: SLUS_010.40 SoundVM_FE15_unk (src/SLUS_010.40/25AC.c:6050)
void Audio_Seq_SetTimerDivisors(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    Audio_Seq_activeMusicConfig->timerLower = *in_pChannel->programCounter++;
    Audio_Seq_activeMusicConfig->timerUpper = *in_pChannel->programCounter++;
    Audio_Seq_activeMusicConfig->timerLowerCount = 0;
    Audio_Seq_activeMusicConfig->timerUpperCount = 0;
}

// decomp: SLUS_010.40 SoundVM_FE16_unk (src/SLUS_010.40/25AC.c:6059)
void Audio_Seq_SetTimerCount(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    Audio_Seq_activeMusicConfig->timerTop = *in_pChannel->programCounter++;
    Audio_Seq_activeMusicConfig->timerTop |= *in_pChannel->programCounter++ << 8;
}

// decomp: SLUS_010.40 SoundVM_FE0B_unk (src/SLUS_010.40/25AC.c:6162)
void Audio_Seq_PlayEmbeddedSoundEffect(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int temp_v0;
    char* pc;
    char* var_a1;
    char* var_a2;

    pc = in_pChannel->programCounter;
    temp_v0 = (pc[1] << 8) | pc[0];

    if (temp_v0 != 0) {
        var_a1 = &pc[temp_v0] + 2;
    } else {
        var_a1 = NULL;
    }

    pc += 2;
    temp_v0 = (pc[1] << 8) | pc[0];

    if (temp_v0 != 0) {
        var_a2 = &pc[temp_v0] + 2;
    } else {
        var_a2 = NULL;
    }

    Audio_Seq_embeddedSoundEffectParams.param1 = 0;
    Audio_Seq_embeddedSoundEffectParams.param2 = 0;
    Audio_Seq_embeddedSoundEffectParams.param3 = in_pChannel->pan >> 8;
    Audio_Seq_embeddedSoundEffectParams.param4 = in_pChannel->volume >> 0x17;
    Audio_SoundEffects_PlayProgramPair(&Audio_Seq_embeddedSoundEffectParams, var_a1, var_a2, 0);
    in_pChannel->programCounter += 4;
}

// decomp: SLUS_010.40 SoundVM_FE1C_unk (src/SLUS_010.40/25AC.c:6202)
void Audio_Seq_SelectNoteRemapTable(
    Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags __attribute__((unused)))
{
    int temp_v0 = *in_pChannel->programCounter++;
    Audio_Seq_activeMusicConfig->noteRemapTable = (Audio_KeymapEntry*)&Audio_Seq_kNoteRemapTables[temp_v0];
}

// decomp: SLUS_010.40 SoundVM_FE1D_MarkVoicesKeyed (src/SLUS_010.40/25AC.c:6209)
void Audio_Seq_MarkChannelKeyed(
    Audio_SeqChannel* in_pChannel __attribute__((unused)), u_int in_VoiceFlags)
{
    Audio_Seq_activeMusicConfig->keyedVoices |= in_VoiceFlags;
}

// decomp: SLUS_010.40 SoundVM_FE1E_ClearVoicesKeyed (src/SLUS_010.40/25AC.c:6215)
void Audio_Seq_ClearChannelKeyed(
    Audio_SeqChannel* in_pChannel __attribute__((unused)), u_int in_VoiceFlags)
{
    Audio_Seq_activeMusicConfig->keyedVoices &= ~in_VoiceFlags;
}

// decomp: SLUS_010.40 SoundVM_XX_Unimplemented (src/SLUS_010.40/25AC.c:6233)
void Audio_Seq_StopOnInvalidInstruction(Audio_SeqChannel* in_pChannel, u_int in_VoiceFlags)
{
    Audio_Seq_FinishChannel(in_pChannel, in_VoiceFlags);
}

