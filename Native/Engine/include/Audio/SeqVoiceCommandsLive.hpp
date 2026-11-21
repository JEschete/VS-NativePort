#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

namespace VS::Audio {
struct SeqChannel;
}  // namespace VS::Audio

// Bindings from the sequencer's opcode handler tables
// (VS::Audio::Seq::kOpcodeHandlers and VS::Audio::Seq::kExtendedOpcodeHandlers,
// typed registrations in Sequencer.cpp) to the typed voice commands
// (ChannelCommands.cpp,
// Envelope.cpp and Modulation.cpp in src\Audio\Seq). Each handler keeps the
// table's signature: it reads the operands at the channel's program counter,
// bounded by the active music layer's heap block when the counter lies in it,
// runs the command over the channel's state and advances the counter by the
// bytes it consumed. `channel_bit` is the channel's bit in the layer's voice
// mode masks.
namespace VS::Audio::Seq {
void LiveSetVolumeBalance(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSlideVolumeBalance(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetVolume(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSlideVolume(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetKeyOnVolumeRamp(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableFullNoteLength(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveDisableFullNoteLength(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetPan(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSlidePan(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetOctave(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveIncreaseOctave(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveDecreaseOctave(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveLoadInstrument(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveLoadSilentInstrument(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSelectPatch(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetTranspose(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveAddTranspose(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetPitchBendSlide(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnablePortamento(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveDisablePortamento(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetFineTune(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveAddFineTune(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableLegato(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableSustainedSoundEffect(VS::Audio::SeqChannel* channel,
                                    int channel_bit);
void LiveSetNextNoteLength(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetFixedNoteLength(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableDrumKeymap(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveMuteInstrument(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableLoopBoundaryStop(VS::Audio::SeqChannel* channel,
                                int channel_bit);

void LiveResetEnvelope(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetAttackRate(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetDecayRate(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetSustainLevel(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetSustainRate(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetReleaseRate(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetAttackMode(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetSustainMode(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetReleaseMode(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetDecayAndSustain(VS::Audio::SeqChannel* channel, int channel_bit);

void LiveEnableVibrato(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetVibratoDepth(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSlideVibratoDepth(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveDisableVibrato(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableTremolo(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetTremoloDepth(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSlideTremoloDepth(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveDisableTremolo(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableAutoPan(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetAutoPanDepth(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSlideAutoPanDepth(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveDisableAutoPan(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableNoise(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveDisableNoise(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableFrequencyModulation(VS::Audio::SeqChannel* channel,
                                   int channel_bit);
void LiveDisableFrequencyModulation(VS::Audio::SeqChannel* channel,
                                    int channel_bit);
void LiveEnableReverb(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveDisableReverb(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetNoiseClock(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableTimedNoise(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveSetNoiseToggleDelay(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableTimedFrequencyModulation(VS::Audio::SeqChannel* channel,
                                        int channel_bit);
void LiveSetFrequencyModulationToggleDelay(VS::Audio::SeqChannel* channel,
                                           int channel_bit);
void LiveResetVoiceEffects(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnablePitchSidechain(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveDisablePitchSidechain(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveEnableVolumeSidechain(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveDisableVolumeSidechain(VS::Audio::SeqChannel* channel,
                                int channel_bit);
void LiveSetRandomPitchDepth(VS::Audio::SeqChannel* channel, int channel_bit);
void LiveResetRandomPitchDepth(VS::Audio::SeqChannel* channel, int channel_bit);

// Commands refused since start-up: truncated operands, a missing waveform or
// instrument, a patch outside the music data, or no active music layer for a
// command that needs one. A refused command changes no channel state.
[[nodiscard]] std::uint64_t RefusedLiveVoiceCommandCount() noexcept;
}  // namespace VS::Audio::Seq
