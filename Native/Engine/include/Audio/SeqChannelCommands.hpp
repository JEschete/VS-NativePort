#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Audio/SeqVoiceCommands.hpp"

// The channel volume, pan, pitch, instrument and note length commands
// (SLUS_010.40/25AC.c:5125-5442 and 5752-6196).
namespace VS::Audio::Seq {
// A3: the volume balance from operand 0.
CommandResult SetVolumeBalance(ChannelState& channel, Operands operands);
// FE12: slides the volume balance to operand 1 over operand 0 ticks.
CommandResult SlideVolumeBalance(ChannelState& channel, Operands operands);
// A8: the channel volume from signed operand 0; stops volume slides.
CommandResult SetVolume(ChannelState& channel, Operands operands);
// A9: slides the volume to signed operand 1 over operand 0 ticks.
CommandResult SlideVolume(ChannelState& channel, Operands operands);
// FE19: the key-on volume from signed operand 0, sliding to signed operand 2
// over operand 1 ticks.
CommandResult SetKeyOnVolumeRamp(ChannelState& channel, Operands operands);
// FE1A/FE1B: whether notes play for their full length.
void EnableFullNoteLength(ChannelState& channel);
void DisableFullNoteLength(ChannelState& channel);
// The overlay effect volume and its slide (opcodes the shipped sequences do
// not use).
CommandResult SetEffectVolumeScale(ChannelState& channel, Operands operands);
CommandResult SlideEffectVolumeScale(ChannelState& channel, Operands operands);
// AA/AB: the pan from operand 0 and its slide over operand 0 ticks.
CommandResult SetPan(ChannelState& channel, Operands operands);
CommandResult SlidePan(ChannelState& channel, Operands operands);
// A5/A6/A7: the octave, and its 4-bit wrapping step up or down.
CommandResult SetOctave(ChannelState& channel, Operands operands);
void IncreaseOctave(ChannelState& channel);
void DecreaseOctave(ChannelState& channel);
// A1: the instrument the channel kind resolves from operand 0.
CommandResult LoadInstrument(ChannelState& channel, Operands operands,
                             VoiceCommandHost& host);
// FE0A: instrument slot operand 0 with the silent sample start.
CommandResult LoadSilentInstrument(ChannelState& channel, Operands operands,
                                   VoiceCommandHost& host);
// FE14: the keymap of patch operand 0, or mutes the channel for an entry
// above kMutedPatch.
CommandResult SelectPatch(ChannelState& channel, Operands operands,
                          VoiceCommandHost& host);
// C0/C1: the transpose from signed operand 0, or adds it.
CommandResult SetTranspose(ChannelState& channel, Operands operands);
CommandResult AddTranspose(ChannelState& channel, Operands operands);
// A4: bends to signed operand 1 over operand 0 ticks.
CommandResult SetPitchBendSlide(ChannelState& channel, Operands operands);
// DA/DB: portamento over operand 0 steps, or none.
CommandResult EnablePortamento(ChannelState& channel, Operands operands);
void DisablePortamento(ChannelState& channel);
// D8/D9: the fine tune from signed operand 0, or adds it, and the pitch
// delta it gives the channel's base pitch.
CommandResult SetFineTune(ChannelState& channel, Operands operands);
CommandResult AddFineTune(ChannelState& channel, Operands operands);
// CC: legato play. D0: sound effects play their full length.
void EnableLegato(ChannelState& channel);
void EnableSustainedSoundEffect(ChannelState& channel);
// A2: the next note lengths from operand 0. DC: a fixed note length offset
// by signed operand 0 from the stored length, clamped to 1-255.
CommandResult SetNextNoteLength(ChannelState& channel, Operands operands);
CommandResult SetFixedNoteLength(ChannelState& channel, Operands operands);
// FE04: the music layer's drum keymap when it has one. FE05: mutes the
// instrument and leaves drum mode.
void EnableDrumKeymap(ChannelState& channel, VoiceCommandHost& host);
void MuteInstrument(ChannelState& channel);
// E0: stop at the next loop boundary.
void EnableLoopBoundaryStop(ChannelState& channel);
}  // namespace VS::Audio::Seq
