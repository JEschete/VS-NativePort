#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Audio/SeqVoiceCommands.hpp"

// The channel voice's ADSR envelope commands (SLUS_010.40/25AC.c:5366 and
// 5796-6066). The rate operands are not masked: a value wider than its field
// spills into the neighbouring envelope bits as on the PS1.
namespace VS::Audio::Seq {
// B3: the envelope of the channel's instrument, dropping the overrides.
CommandStatus ResetEnvelope(ChannelState& channel, VoiceCommandHost& host);
// AD/AE/AF: the attack rate, decay rate and sustain level (lower word).
CommandResult SetAttackRate(ChannelState& channel, Operands operands);
CommandResult SetDecayRate(ChannelState& channel, Operands operands);
CommandResult SetSustainLevel(ChannelState& channel, Operands operands);
// B1/B2: the sustain and release rates (upper word).
CommandResult SetSustainRate(ChannelState& channel, Operands operands);
CommandResult SetReleaseRate(ChannelState& channel, Operands operands);
// B7/BB/BF: the attack, sustain and release modes from sequence mode
// numbers (1, 3, 5 or 7: linear or exponential, increasing or decreasing).
CommandResult SetAttackMode(ChannelState& channel, Operands operands);
CommandResult SetSustainMode(ChannelState& channel, Operands operands);
CommandResult SetReleaseMode(ChannelState& channel, Operands operands);
// B0: AE then AF.
CommandResult SetDecayAndSustain(ChannelState& channel, Operands operands);
}  // namespace VS::Audio::Seq
