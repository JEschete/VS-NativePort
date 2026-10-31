// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "State/Progress.h"

// decomp: vs_main_actionsLearned (vs_main_actionsLearned@SLUS_010.40)
char State_learnedActionFlags[32];

// decomp: vs_main_mapStatus (vs_main_mapStatus@SLUS_010.40)
State_RoomVisitFlags State_roomVisitProgress;

}  // extern "C"
