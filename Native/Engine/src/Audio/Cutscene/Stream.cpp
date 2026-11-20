// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.hpp"
#include "Audio/Cutscene/Stream.hpp"
#include "Audio/StreamFormat.hpp"

// decomp: _isSpuTransfer (_isSpuTransfer@SLUS_010.40)
volatile int VS::Audio::Cutscene::legacySpuTransferBusy;

// decomp: g_Sound_Cutscene_StreamState (g_Sound_Cutscene_StreamState@SLUS_010.40)
VS::Audio::CutsceneStreamState VS::Audio::Cutscene::streamState;

