// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Stream.h"

// decomp: _isSpuTransfer (_isSpuTransfer@SLUS_010.40)
volatile int Audio_Cutscene_legacySpuTransferBusy;

// decomp: g_Sound_Cutscene_StreamState (g_Sound_Cutscene_StreamState@SLUS_010.40)
Audio_CutsceneStreamState Audio_Cutscene_streamState;

}  // extern "C"
