// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


#include "DiscQueue.h"
#include "Render/Images.h"

extern int Ending_countableRoomBits[16];
extern Render_TextureRegion Ending_resultAtlasRegions[69];
extern u_char Ending_rankLabelRegionIds[31][4];
extern short Ending_rankTitleThresholds[16];
extern u_int Ending_rankScoreThresholds[36];
extern Content_DiscQueueSlot* Ending_resultsDiscReadSlot;
extern void* Ending_resultsTimBuffer;
