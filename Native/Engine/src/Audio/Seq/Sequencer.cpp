// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Audio/Seq/Sequencer.hpp"

#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Legacy/Psx.hpp"

// decomp: D_8002F54C (D_8002F54C@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint16_t VS::Audio::Seq::tickCounter;

// decomp: D_8002F5A0 (D_8002F5A0@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint8_t
    VS::Audio::Seq::kOpcodeLengths[VS::Audio::Seq::kOpcodeLengthTableSize];

// decomp: D_8002F600 (D_8002F600@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint8_t VS::Audio::Seq::kExtendedOpcodeLengths
    [VS::Audio::Seq::kExtendedOpcodeLengthTableSize];

// decomp: D_8002F620 (D_8002F620@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint16_t VS::Audio::Seq::kNoteLengths[VS::Audio::Seq::kNoteLengthCount];

// decomp: g_Sound_ProgramCounter (g_Sound_ProgramCounter@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint32_t VS::Audio::Seq::kIdleProgram;

// decomp: D_8002F63C (D_8002F63C@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint32_t
    VS::Audio::Seq::kSemitonePitches[VS::Audio::Seq::kSemitonePitchTableSize];

// decomp: D_8002F66C (D_8002F66C@SLUS_010.40)
// contents: module image (ModuleData.cpp)
signed char
    VS::Audio::Seq::kNoteRemapTables[VS::Audio::Seq::kNoteRemapTableSize];

// decomp: D_8002F69C (D_8002F69C@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint8_t VS::Audio::Seq::kRandomPitchOffsets
    [VS::Audio::Seq::kRandomPitchOffsetCount];

// decomp: g_Sound_LfoTable (g_Sound_LfoTable@SLUS_010.40)
// contents: module image, relocated (ModulePointers.cpp)
std::int16_t*
    VS::Audio::Seq::kLfoWaveforms[VS::Audio::Seq::kLfoWaveformTableSize];

// decomp: D_800305E0 (D_800305E0@SLUS_010.40)
// contents: module image, relocated (ModulePointers.cpp)
VS::Audio::SeqOpcodeHandler
    VS::Audio::Seq::kOpcodeHandlers[VS::Audio::Seq::kOpcodeCount];

// decomp: D_80030760 (D_80030760@SLUS_010.40)
// contents: module image, relocated (ModulePointers.cpp)
VS::Audio::SeqOpcodeHandler VS::Audio::Seq::kExtendedOpcodeHandlers
    [VS::Audio::Seq::kExtendedOpcodeCount];

// decomp: D_80037800 (D_80037800@SLUS_010.40)
VS::Audio::CommandParams VS::Audio::Seq::embeddedSoundEffectParams;

// decomp: g_Sound_MasterFadeTimer (g_Sound_MasterFadeTimer@SLUS_010.40)
VS::Audio::LegacyFadeTimer VS::Audio::Seq::masterFade;

// decomp: g_Sound_LfoPhase (g_Sound_LfoPhase@SLUS_010.40)
int VS::Audio::Seq::randomPitchPhase;
