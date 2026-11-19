#pragma once
// The sequencer's tables and state: the opcode lengths and handlers, the note
// lengths, semitone pitches, note remaps, random pitch offsets and LFO
// waveforms, the idle program, the master fade and the tick counter.
#include <cstddef>
#include <cstdint>

#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SeqSequencer.hpp"

namespace VS::Audio {
// decomp: FSoundFadeTimer (src/include/sound.h:514)
struct LegacyFadeTimer {
  /* 0x00 */ std::int32_t value;
  /* 0x04 */ std::int32_t step;
  /* 0x08 */ std::int32_t ticks_remaining;
  /* 0x0C */ std::int32_t saved_value;
};

namespace layout {
inline constexpr std::size_t kLegacyFadeTimerBytes = 0x10;
static_assert(sizeof(LegacyFadeTimer) == kLegacyFadeTimerBytes);
}  // namespace layout

// An opcode handler (D_800305E0, D_80030760): the channel and its voice bit.
using SeqOpcodeHandler = void (*)(SeqChannel*, std::int32_t);
}  // namespace VS::Audio

namespace VS::Audio::Seq {
// The tables' sizes as the decomp declares them. The opcode length, semitone
// pitch and note remap tables are declared longer than the entries the
// sequencer reads and run into the tables after them; ModuleData copies each
// declared size from the module image.
inline constexpr std::size_t kOpcodeLengthTableSize = 152;
inline constexpr std::size_t kExtendedOpcodeLengthTableSize = 56;
inline constexpr std::size_t kSemitonePitchTableSize = 88;
inline constexpr std::size_t kNoteRemapTableSize = 304;
inline constexpr std::size_t kLfoWaveformTableSize = 256;

// decomp: D_8002F54C (D_8002F54C@SLUS_010.40)
extern std::uint16_t tickCounter;
// decomp: D_8002F5A0 (D_8002F5A0@SLUS_010.40)
extern std::uint8_t kOpcodeLengths[kOpcodeLengthTableSize];
// decomp: D_8002F600 (D_8002F600@SLUS_010.40)
extern std::uint8_t kExtendedOpcodeLengths[kExtendedOpcodeLengthTableSize];
// decomp: D_8002F620 (D_8002F620@SLUS_010.40)
extern std::uint16_t kNoteLengths[kNoteLengthCount];
// decomp: g_Sound_ProgramCounter (g_Sound_ProgramCounter@SLUS_010.40)
extern std::uint32_t kIdleProgram;
// decomp: D_8002F63C (D_8002F63C@SLUS_010.40)
extern std::uint32_t kSemitonePitches[kSemitonePitchTableSize];
// decomp: D_8002F66C (D_8002F66C@SLUS_010.40)
extern std::int8_t kNoteRemapTables[kNoteRemapTableSize];
// decomp: D_8002F69C (D_8002F69C@SLUS_010.40)
extern std::uint8_t kRandomPitchOffsets[kRandomPitchOffsetCount];
// decomp: g_Sound_LfoTable (g_Sound_LfoTable@SLUS_010.40)
extern std::int16_t* kLfoWaveforms[kLfoWaveformTableSize];
// decomp: D_800305E0 (D_800305E0@SLUS_010.40)
extern SeqOpcodeHandler kOpcodeHandlers[kOpcodeCount];
// decomp: D_80030760 (D_80030760@SLUS_010.40)
extern SeqOpcodeHandler kExtendedOpcodeHandlers[kExtendedOpcodeCount];
// decomp: D_80037800 (D_80037800@SLUS_010.40)
extern CommandParams embeddedSoundEffectParams;
// decomp: g_Sound_MasterFadeTimer (g_Sound_MasterFadeTimer@SLUS_010.40)
extern LegacyFadeTimer masterFade;
// decomp: g_Sound_LfoPhase (g_Sound_LfoPhase@SLUS_010.40)
extern std::int32_t randomPitchPhase;
}  // namespace VS::Audio::Seq
