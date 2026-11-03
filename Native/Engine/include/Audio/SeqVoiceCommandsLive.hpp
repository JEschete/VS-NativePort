#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct Audio_SeqChannel;

// Bindings from the sequencer's opcode handler tables
// (Audio_Seq_kOpcodeHandlers and Audio_Seq_kExtendedOpcodeHandlers, stored in
// the module image) to the typed voice commands (ChannelCommands.cpp,
// Envelope.cpp and Modulation.cpp in src\Audio\Seq). Each handler keeps the
// table's signature: it reads the operands at the channel's program counter,
// bounded by the active music layer's heap block when the counter lies in it,
// runs the command over the channel's state and advances the counter by the
// bytes it consumed. `channel_bit` is the channel's bit in the layer's voice
// mode masks.
namespace VS::Audio::Seq {
void LiveSetVolumeBalance(Audio_SeqChannel* channel, int channel_bit);
void LiveSlideVolumeBalance(Audio_SeqChannel* channel, int channel_bit);
void LiveSetVolume(Audio_SeqChannel* channel, int channel_bit);
void LiveSlideVolume(Audio_SeqChannel* channel, int channel_bit);
void LiveSetKeyOnVolumeRamp(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableFullNoteLength(Audio_SeqChannel* channel, int channel_bit);
void LiveDisableFullNoteLength(Audio_SeqChannel* channel, int channel_bit);
void LiveSetPan(Audio_SeqChannel* channel, int channel_bit);
void LiveSlidePan(Audio_SeqChannel* channel, int channel_bit);
void LiveSetOctave(Audio_SeqChannel* channel, int channel_bit);
void LiveIncreaseOctave(Audio_SeqChannel* channel, int channel_bit);
void LiveDecreaseOctave(Audio_SeqChannel* channel, int channel_bit);
void LiveLoadInstrument(Audio_SeqChannel* channel, int channel_bit);
void LiveLoadSilentInstrument(Audio_SeqChannel* channel, int channel_bit);
void LiveSelectPatch(Audio_SeqChannel* channel, int channel_bit);
void LiveSetTranspose(Audio_SeqChannel* channel, int channel_bit);
void LiveAddTranspose(Audio_SeqChannel* channel, int channel_bit);
void LiveSetPitchBendSlide(Audio_SeqChannel* channel, int channel_bit);
void LiveEnablePortamento(Audio_SeqChannel* channel, int channel_bit);
void LiveDisablePortamento(Audio_SeqChannel* channel, int channel_bit);
void LiveSetFineTune(Audio_SeqChannel* channel, int channel_bit);
void LiveAddFineTune(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableLegato(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableSustainedSoundEffect(Audio_SeqChannel* channel, int channel_bit);
void LiveSetNextNoteLength(Audio_SeqChannel* channel, int channel_bit);
void LiveSetFixedNoteLength(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableDrumKeymap(Audio_SeqChannel* channel, int channel_bit);
void LiveMuteInstrument(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableLoopBoundaryStop(Audio_SeqChannel* channel, int channel_bit);

void LiveResetEnvelope(Audio_SeqChannel* channel, int channel_bit);
void LiveSetAttackRate(Audio_SeqChannel* channel, int channel_bit);
void LiveSetDecayRate(Audio_SeqChannel* channel, int channel_bit);
void LiveSetSustainLevel(Audio_SeqChannel* channel, int channel_bit);
void LiveSetSustainRate(Audio_SeqChannel* channel, int channel_bit);
void LiveSetReleaseRate(Audio_SeqChannel* channel, int channel_bit);
void LiveSetAttackMode(Audio_SeqChannel* channel, int channel_bit);
void LiveSetSustainMode(Audio_SeqChannel* channel, int channel_bit);
void LiveSetReleaseMode(Audio_SeqChannel* channel, int channel_bit);
void LiveSetDecayAndSustain(Audio_SeqChannel* channel, int channel_bit);

void LiveEnableVibrato(Audio_SeqChannel* channel, int channel_bit);
void LiveSetVibratoDepth(Audio_SeqChannel* channel, int channel_bit);
void LiveSlideVibratoDepth(Audio_SeqChannel* channel, int channel_bit);
void LiveDisableVibrato(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableTremolo(Audio_SeqChannel* channel, int channel_bit);
void LiveSetTremoloDepth(Audio_SeqChannel* channel, int channel_bit);
void LiveSlideTremoloDepth(Audio_SeqChannel* channel, int channel_bit);
void LiveDisableTremolo(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableAutoPan(Audio_SeqChannel* channel, int channel_bit);
void LiveSetAutoPanDepth(Audio_SeqChannel* channel, int channel_bit);
void LiveSlideAutoPanDepth(Audio_SeqChannel* channel, int channel_bit);
void LiveDisableAutoPan(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableNoise(Audio_SeqChannel* channel, int channel_bit);
void LiveDisableNoise(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableFrequencyModulation(Audio_SeqChannel* channel, int channel_bit);
void LiveDisableFrequencyModulation(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableReverb(Audio_SeqChannel* channel, int channel_bit);
void LiveDisableReverb(Audio_SeqChannel* channel, int channel_bit);
void LiveSetNoiseClock(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableTimedNoise(Audio_SeqChannel* channel, int channel_bit);
void LiveSetNoiseToggleDelay(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableTimedFrequencyModulation(Audio_SeqChannel* channel,
                                        int channel_bit);
void LiveSetFrequencyModulationToggleDelay(Audio_SeqChannel* channel,
                                           int channel_bit);
void LiveResetVoiceEffects(Audio_SeqChannel* channel, int channel_bit);
void LiveEnablePitchSidechain(Audio_SeqChannel* channel, int channel_bit);
void LiveDisablePitchSidechain(Audio_SeqChannel* channel, int channel_bit);
void LiveEnableVolumeSidechain(Audio_SeqChannel* channel, int channel_bit);
void LiveDisableVolumeSidechain(Audio_SeqChannel* channel, int channel_bit);
void LiveSetRandomPitchDepth(Audio_SeqChannel* channel, int channel_bit);
void LiveResetRandomPitchDepth(Audio_SeqChannel* channel, int channel_bit);

// Commands refused since start-up: truncated operands, a missing waveform or
// instrument, a patch outside the music data, or no active music layer for a
// command that needs one. A refused command changes no channel state.
[[nodiscard]] std::uint64_t RefusedLiveVoiceCommandCount() noexcept;
}  // namespace VS::Audio::Seq
