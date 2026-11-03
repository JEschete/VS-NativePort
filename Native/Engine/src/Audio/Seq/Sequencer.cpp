// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"

// decomp: D_8002F54C (D_8002F54C@SLUS_010.40)
u_short Audio_Seq_tickCounter; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F5A0 (D_8002F5A0@SLUS_010.40)
u_char Audio_Seq_kOpcodeLengths[152]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F600 (D_8002F600@SLUS_010.40)
u_char Audio_Seq_kExtendedOpcodeLengths[56]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F620 (D_8002F620@SLUS_010.40)
u_short Audio_Seq_kNoteLengths[12]; /* contents: module image (ModuleData.cpp) */

// decomp: g_Sound_ProgramCounter (g_Sound_ProgramCounter@SLUS_010.40)
u_int Audio_Seq_kIdleProgram; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F63C (D_8002F63C@SLUS_010.40)
u_int Audio_Seq_kSemitonePitches[88]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F66C (D_8002F66C@SLUS_010.40)
char Audio_Seq_kNoteRemapTables[304]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8002F69C (D_8002F69C@SLUS_010.40)
char Audio_Seq_kRandomPitchOffsets[256]; /* contents: module image (ModuleData.cpp) */

// decomp: g_Sound_LfoTable (g_Sound_LfoTable@SLUS_010.40)
short* Audio_Seq_kLfoWaveforms[256]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800305E0 (D_800305E0@SLUS_010.40)
void (*Audio_Seq_kOpcodeHandlers[96])(Audio_SeqChannel*, int); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_80030760 (D_80030760@SLUS_010.40)
void (*Audio_Seq_kExtendedOpcodeHandlers[32])(Audio_SeqChannel*, int); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_80037800 (D_80037800@SLUS_010.40)
Audio_CommandParams Audio_Seq_embeddedSoundEffectParams;

// decomp: g_Sound_MasterFadeTimer (g_Sound_MasterFadeTimer@SLUS_010.40)
Audio_FadeTimer Audio_Seq_masterFade;

// decomp: g_Sound_LfoPhase (g_Sound_LfoPhase@SLUS_010.40)
int Audio_Seq_randomPitchPhase;

}  // extern "C"
