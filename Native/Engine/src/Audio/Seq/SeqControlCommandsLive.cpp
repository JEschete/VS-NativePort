// Hand-maintained (tools/port/owned.csv).
// Live binding for SeqControlCommandsLive.hpp: the channel record, the active
// music layer, the sound-effect voice scheduler, the driver's update requests
// and the sound-effect programs FE0B starts.
#include "Audio/SeqControlCommandsLive.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundEffectProgramsLive.hpp"
#include "Audio/VoiceOutput.hpp"

extern "C" {
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SoundEffects/SoundEffects.h"
}

namespace VS::Audio::Seq {
namespace {
static_assert(std::is_convertible_v<
              decltype(&LiveSetTempo),
              std::remove_all_extents_t<decltype(Audio_Seq_kOpcodeHandlers)>>);
static_assert(
    std::is_convertible_v<decltype(&LiveSetTempo),
                          std::remove_all_extents_t<
                              decltype(Audio_Seq_kExtendedOpcodeHandlers)>>);

// FE0B's two program offsets.
constexpr std::size_t kEmbeddedOperandBytes = 4;

// The music layer a command needs: none, the active layer for music channels
// only (sound effects use the voice scheduler), or the active layer for any
// channel.
enum class LayerUse : std::uint8_t { kNone, kMusicChannels, kAlways };

bool NeedsLayer(const Audio_SeqChannel& channel, LayerUse use) {
  if (use == LayerUse::kAlways) return true;
  return use == LayerUse::kMusicChannels &&
         LiveChannelKind(channel) == Driver::ChannelKind::Music;
}

const char* RefusalReason(ControlStatus status) {
  switch (status) {
    case ControlStatus::kTruncated:
      return "the program memory ends inside its operands";
    case ControlStatus::kTargetOutside:
      return "the target lies outside the program memory";
    case ControlStatus::kApplied:
      break;
  }
  return "applied";
}

ControlState LoadControlState(const Audio_SeqChannel& channel,
                              const LiveProgramMemory& memory) {
  ControlState state;
  state.program = LoadProgram(channel, memory);
  state.kind = LiveChannelKind(channel);
  state.update_flags = channel.updateFlags;
  state.volume = channel.volume;
  state.pan = channel.pan;
  state.effect = {channel.effectTags, channel.effectId != 0};
  if (const Audio_MusicConfig* layer = Audio_Seq_activeMusicConfig) {
    state.layer = LoadMusicLayer(*layer);
  }
  state.scheduler = LoadVoiceScheduler(Audio_Mixer_voiceScheduler);
  state.driver_update_flags = Audio_Driver_controlLatches.updateFlags;
  state.music_loops_remaining = Audio_Seq_musicLoopsRemaining;
  return state;
}

void StoreControlState(Audio_SeqChannel& channel, const ControlState& state,
                       const LiveProgramMemory& memory) {
  StoreProgram(channel, state.program, memory);
  channel.updateFlags = state.update_flags;
  channel.effectTags = state.effect.tags;
  if (!state.effect.has_effect) channel.effectId = 0;
  if (Audio_MusicConfig* layer = Audio_Seq_activeMusicConfig) {
    StoreMusicLayer(*layer, state.layer);
  }
  StoreVoiceScheduler(Audio_Mixer_voiceScheduler, state.scheduler);
  Audio_Driver_controlLatches.updateFlags = state.driver_update_flags;
  Audio_Seq_musicLoopsRemaining = state.music_loops_remaining;
}

// The legacy pointer to `program`, or none.
char* ProgramPointer(const LiveProgramMemory& memory,
                     const std::optional<std::uint32_t>& program) {
  if (!program || *program >= memory.size) return nullptr;
  return memory.base + *program;
}

// Sound_PlaySfxProgram with FE0B's parameters (25AC.c:6162). The original
// then steps past the operands from wherever the channel's counter is after
// the call, which has restarted the channel if it took the channel's slot.
void StartEmbeddedSoundEffect(Audio_SeqChannel& channel,
                              const LiveProgramMemory& memory,
                              const EmbeddedSoundEffect& effect) {
  Audio_CommandParams& params = Audio_Seq_embeddedSoundEffectParams;
  params.param1 = 0;
  params.param2 = 0;
  params.param3 = effect.pan;
  params.param4 = effect.volume;
  SoundEffects::LivePlayProgramPair(
      &params, ProgramPointer(memory, effect.first_program),
      ProgramPointer(memory, effect.second_program), 0);
  const std::optional<LiveProgramMemory> rest = LiveChannelProgram(channel);
  if (!rest ||
      rest->From(channel.programCounter).size < kEmbeddedOperandBytes) {
    ReportRefusal("FE0B PlayEmbeddedSoundEffect",
                  "the restarted program ends inside the operand step");
    return;
  }
  channel.programCounter += kEmbeddedOperandBytes;
}

// Runs `command` over the channel's program and state. A refused command
// changes none of them.
void Run(Audio_SeqChannel* channel, int channel_bit, const char* name,
         LayerUse use, ControlCommand command) {
  if (channel == nullptr) {
    ReportRefusal(name, "no channel");
    return;
  }
  if (Audio_Seq_activeMusicConfig == nullptr && NeedsLayer(*channel, use)) {
    ReportRefusal(name, "no active music layer");
    return;
  }
  const LiveProgramMemory memory =
      LiveChannelProgram(*channel).value_or(LiveProgramMemory{});
  ControlState state = LoadControlState(*channel, memory);
  const ControlResult result =
      command(state, memory.View(), static_cast<std::uint32_t>(channel_bit));
  if (result.status != ControlStatus::kApplied) {
    ReportRefusal(name, RefusalReason(result.status));
    return;
  }
  StoreControlState(*channel, state, memory);
  if (result.sound_effect) {
    StartEmbeddedSoundEffect(*channel, memory, *result.sound_effect);
  }
}
}  // namespace

// decomp: SLUS_010.40 SoundVM_A0_FinishChannel (src/SLUS_010.40/25AC.c:4989)
void LiveFinishChannel(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "A0 FinishChannel", LayerUse::kMusicChannels,
      FinishChannel);
}

// decomp: SLUS_010.40 SoundVM_FE1F_unk (src/SLUS_010.40/25AC.c:5011)
void LiveFinishChannelOnReplacement(Audio_SeqChannel* channel,
                                    int channel_bit) {
  Run(channel, channel_bit, "FE1F FinishChannelOnReplacement",
      LayerUse::kAlways, FinishChannelOnReplacement);
}

// decomp: SLUS_010.40 SoundVM_XX_Unimplemented (src/SLUS_010.40/25AC.c:6233)
void LiveStopOnInvalidInstruction(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "StopOnInvalidInstruction",
      LayerUse::kMusicChannels, StopOnInvalidInstruction);
}

// decomp: SLUS_010.40 SoundVM_CD_DEBUG (src/SLUS_010.40/25AC.c:5758)
// decomp: SLUS_010.40 SoundVM_D1_DEBUG (src/SLUS_010.40/25AC.c:5771)
void LiveIgnoreInstruction(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "IgnoreInstruction", LayerUse::kNone,
      IgnoreInstruction);
}

// decomp: SLUS_010.40 SoundVM_FE00_SetTempo (src/SLUS_010.40/25AC.c:5018)
void LiveSetTempo(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE00 SetTempo", LayerUse::kAlways, SetTempo);
}

// decomp: SLUS_010.40 SoundVM_FE01_SetTempoSlide (src/SLUS_010.40/25AC.c:5027)
void LiveSlideTempo(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE01 SlideTempo", LayerUse::kAlways, SlideTempo);
}

// decomp: SLUS_010.40 SoundVM_FE02_SetMasterReverbDepth
// (src/SLUS_010.40/25AC.c:5048)
void LiveSetReverbDepth(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE02 SetReverbDepth", LayerUse::kAlways,
      SetReverbDepth);
}

// decomp: SLUS_010.40 SoundVM_FE03_SetMasterReverbSlide
// (src/SLUS_010.40/25AC.c:5063)
void LiveSlideReverbDepth(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE03 SlideReverbDepth", LayerUse::kAlways,
      SlideReverbDepth);
}

// decomp: SLUS_010.40 SoundVM_FE06_JumpRelativeOffset
// (src/SLUS_010.40/25AC.c:5085)
void LiveJumpRelative(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE06 JumpRelative", LayerUse::kNone, JumpRelative);
}

// decomp: SLUS_010.40 SoundVM_FE07_JumpRelativeWithThreshold
// (src/SLUS_010.40/25AC.c:5092)
void LiveJumpAtThreshold(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE07 JumpAtThreshold", LayerUse::kAlways,
      JumpAtThreshold);
}

// decomp: SLUS_010.40 SoundVM_FE0E_CallRelativeOffset
// (src/SLUS_010.40/25AC.c:5111)
void LiveCallRelative(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE0E CallRelative", LayerUse::kNone, CallRelative);
}

// decomp: SLUS_010.40 SoundVM_FE0F_Return (src/SLUS_010.40/25AC.c:5119)
void LiveReturnFromCall(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE0F ReturnFromCall", LayerUse::kNone,
      ReturnFromCall);
}

// decomp: SLUS_010.40 SoundVM_FE0C_unk (src/SLUS_010.40/25AC.c:5221)
void LiveSetSequenceControlValue(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE0C SetSequenceControlValue", LayerUse::kAlways,
      SetSequenceControlValue);
}

// decomp: SLUS_010.40 SoundVM_FE10_unk (src/SLUS_010.40/25AC.c:5892)
void LiveSetVoiceAllocationStart(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE10 SetVoiceAllocationStart", LayerUse::kAlways,
      SetVoiceAllocationStart);
}

// decomp: SLUS_010.40 SoundVM_FE11_unk (src/SLUS_010.40/25AC.c:5898)
void LiveResetVoiceAllocationStart(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE11 ResetVoiceAllocationStart", LayerUse::kAlways,
      ResetVoiceAllocationStart);
}

// decomp: SLUS_010.40 SoundVM_C8_LoopPoint (src/SLUS_010.40/25AC.c:5904)
void LivePushLoop(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "C8 PushLoop", LayerUse::kNone, PushLoop);
}

// decomp: SLUS_010.40 SoundVM_C9_LoopN (src/SLUS_010.40/25AC.c:5914)
void LiveRepeatLoop(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "C9 RepeatLoop", LayerUse::kNone, RepeatLoop);
}

// decomp: SLUS_010.40 SoundVM_FE08_JumpOnNthLoopPass
// (src/SLUS_010.40/25AC.c:5941)
void LiveJumpOnLoopPass(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE08 JumpOnLoopPass", LayerUse::kNone,
      JumpOnLoopPass);
}

// decomp: SLUS_010.40 SoundVM_FE09_JumpAndPopStackOnNthLoopPass
// (src/SLUS_010.40/25AC.c:5969)
void LiveExitLoopOnPass(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE09 ExitLoopOnPass", LayerUse::kNone,
      ExitLoopOnPass);
}

// decomp: SLUS_010.40 SoundVM_CA_LoopInf (src/SLUS_010.40/25AC.c:5992)
void LiveRepeatLoopForever(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "CA RepeatLoopForever", LayerUse::kNone,
      RepeatLoopForever);
}

// decomp: SLUS_010.40 SoundVM_FE15_unk (src/SLUS_010.40/25AC.c:6050)
void LiveSetTimerDivisors(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE15 SetTimerDivisors", LayerUse::kAlways,
      SetTimerDivisors);
}

// decomp: SLUS_010.40 SoundVM_FE16_unk (src/SLUS_010.40/25AC.c:6059)
void LiveSetTimerCount(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE16 SetTimerCount", LayerUse::kAlways,
      SetTimerCount);
}

// decomp: SLUS_010.40 SoundVM_FE0B_unk (src/SLUS_010.40/25AC.c:6162)
void LivePlayEmbeddedSoundEffect(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE0B PlayEmbeddedSoundEffect", LayerUse::kNone,
      PlayEmbeddedSoundEffect);
}

// decomp: SLUS_010.40 SoundVM_FE1C_unk (src/SLUS_010.40/25AC.c:6202)
void LiveSelectNoteRemapTable(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE1C SelectNoteRemapTable", LayerUse::kAlways,
      SelectNoteRemapTable);
}

// decomp: SLUS_010.40 SoundVM_FE1D_MarkVoicesKeyed
// (src/SLUS_010.40/25AC.c:6209)
void LiveMarkChannelKeyed(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE1D MarkChannelKeyed", LayerUse::kAlways,
      MarkChannelKeyed);
}

// decomp: SLUS_010.40 SoundVM_FE1E_ClearVoicesKeyed
// (src/SLUS_010.40/25AC.c:6215)
void LiveClearChannelKeyed(Audio_SeqChannel* channel, int channel_bit) {
  Run(channel, channel_bit, "FE1E ClearChannelKeyed", LayerUse::kAlways,
      ClearChannelKeyed);
}
}  // namespace VS::Audio::Seq
