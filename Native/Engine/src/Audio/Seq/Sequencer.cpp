// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/Seq/Sequencer.hpp"

// decomp: D_8002F54C (D_8002F54C@SLUS_010.40)
u_short VS::Audio::Seq::tickCounter; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F5A0 (D_8002F5A0@SLUS_010.40)
u_char VS::Audio::Seq::kOpcodeLengths[VS::Audio::Seq::kOpcodeLengthTableSize]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F600 (D_8002F600@SLUS_010.40)
u_char VS::Audio::Seq::kExtendedOpcodeLengths[VS::Audio::Seq::kExtendedOpcodeLengthTableSize]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F620 (D_8002F620@SLUS_010.40)
u_short VS::Audio::Seq::kNoteLengths[VS::Audio::Seq::kNoteLengthCount]; /* contents: module image (ModuleData.cpp) */

// decomp: g_Sound_ProgramCounter (g_Sound_ProgramCounter@SLUS_010.40)
u_int VS::Audio::Seq::kIdleProgram; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F63C (D_8002F63C@SLUS_010.40)
u_int VS::Audio::Seq::kSemitonePitches[VS::Audio::Seq::kSemitonePitchTableSize]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F66C (D_8002F66C@SLUS_010.40)
signed char VS::Audio::Seq::kNoteRemapTables[VS::Audio::Seq::kNoteRemapTableSize]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F69C (D_8002F69C@SLUS_010.40)
u_char VS::Audio::Seq::kRandomPitchOffsets[VS::Audio::Seq::kRandomPitchOffsetCount]; /* contents: module image (ModuleData.cpp) */

// decomp: g_Sound_LfoTable (g_Sound_LfoTable@SLUS_010.40)
short* VS::Audio::Seq::kLfoWaveforms[VS::Audio::Seq::kLfoWaveformTableSize]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800305E0 (D_800305E0@SLUS_010.40)
VS::Audio::SeqOpcodeHandler VS::Audio::Seq::kOpcodeHandlers[VS::Audio::Seq::kOpcodeCount]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_80030760 (D_80030760@SLUS_010.40)
VS::Audio::SeqOpcodeHandler VS::Audio::Seq::kExtendedOpcodeHandlers[VS::Audio::Seq::kExtendedOpcodeCount]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_80037800 (D_80037800@SLUS_010.40)
VS::Audio::CommandParams VS::Audio::Seq::embeddedSoundEffectParams;

// decomp: g_Sound_MasterFadeTimer (g_Sound_MasterFadeTimer@SLUS_010.40)
VS::Audio::LegacyFadeTimer VS::Audio::Seq::masterFade;

// decomp: g_Sound_LfoPhase (g_Sound_LfoPhase@SLUS_010.40)
int VS::Audio::Seq::randomPitchPhase;

