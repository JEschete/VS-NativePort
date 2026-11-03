#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Audio/SeqVoiceCommands.hpp"

// The channel's vibrato, tremolo and auto-pan oscillators, its noise,
// frequency modulation and reverb voice modes, sidechains and random pitch
// (SLUS_010.40/25AC.c:5462-5776 and 6072-6227). `channel_bits` is the
// channel's bit in the music layer's or sound-effect voices' mode masks.
namespace VS::Audio::Seq {
// B4: vibrato with delay operand 0 (a depth for sound effects), rate operand
// 1 and waveform operand 2; refused when the waveform is missing.
CommandResult EnableVibrato(ChannelState& channel, Operands operands,
                            VoiceCommandHost& host);
// B5/DD: the vibrato depth and its slide over operand 0 ticks.
CommandResult SetVibratoDepth(ChannelState& channel, Operands operands);
CommandResult SlideVibratoDepth(ChannelState& channel, Operands operands);
void DisableVibrato(ChannelState& channel);
// B8: tremolo as B4 (a 7-bit depth for sound effects).
CommandResult EnableTremolo(ChannelState& channel, Operands operands,
                            VoiceCommandHost& host);
// B9/DE: the 7-bit tremolo depth and its slide over operand 0 ticks.
CommandResult SetTremoloDepth(ChannelState& channel, Operands operands);
CommandResult SlideTremoloDepth(ChannelState& channel, Operands operands);
void DisableTremolo(ChannelState& channel);
// BC: auto-pan with rate operand 0 and waveform operand 1.
CommandResult EnableAutoPan(ChannelState& channel, Operands operands,
                            VoiceCommandHost& host);
// BD/DF: the auto-pan depth and its slide over operand 0 ticks.
CommandResult SetAutoPanDepth(ChannelState& channel, Operands operands);
CommandResult SlideAutoPanDepth(ChannelState& channel, Operands operands);
void DisableAutoPan(ChannelState& channel);
// C4/C5, C6/C7 and C2/C3: the voice modes of the channel's bits. Sound
// effects take frequency modulation only with kFrequencyModulationAllowed.
void EnableNoise(ChannelState& channel, VoiceCommandHost& host,
                 std::uint32_t channel_bits);
void DisableNoise(ChannelState& channel, VoiceCommandHost& host,
                  std::uint32_t channel_bits);
void EnableFrequencyModulation(ChannelState& channel, VoiceCommandHost& host,
                               std::uint32_t channel_bits);
void DisableFrequencyModulation(ChannelState& channel, VoiceCommandHost& host,
                                std::uint32_t channel_bits);
void EnableReverb(ChannelState& channel, VoiceCommandHost& host,
                  std::uint32_t channel_bits);
void DisableReverb(ChannelState& channel, VoiceCommandHost& host,
                   std::uint32_t channel_bits);
// AC: the noise clock from operand 0, or advanced by its low 6 bits when
// either of its top bits is set.
CommandResult SetNoiseClock(ChannelState& channel, Operands operands,
                            VoiceCommandHost& host);
// CE/D2: noise or frequency modulation that toggles after operand 0 + 1
// ticks (257 for zero). CF/D3 set only the toggle delay.
CommandResult EnableTimedNoise(ChannelState& channel, Operands operands,
                               VoiceCommandHost& host,
                               std::uint32_t channel_bits);
CommandResult SetNoiseToggleDelay(ChannelState& channel, Operands operands);
CommandResult EnableTimedFrequencyModulation(ChannelState& channel,
                                             Operands operands,
                                             VoiceCommandHost& host,
                                             std::uint32_t channel_bits);
CommandResult SetFrequencyModulationToggleDelay(ChannelState& channel,
                                                Operands operands);
// CB: stops the oscillators, sidechains and voice modes, and legato and
// full-length play.
void ResetVoiceEffects(ChannelState& channel, VoiceCommandHost& host,
                       std::uint32_t channel_bits);
// D4-D7: the pitch and volume sidechains.
void EnablePitchSidechain(ChannelState& channel);
void DisablePitchSidechain(ChannelState& channel);
void EnableVolumeSidechain(ChannelState& channel);
void DisableVolumeSidechain(ChannelState& channel);
// E1/E2: the random pitch depth.
CommandResult SetRandomPitchDepth(ChannelState& channel, Operands operands);
void ResetRandomPitchDepth(ChannelState& channel);
}  // namespace VS::Audio::Seq
