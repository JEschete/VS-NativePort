// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Render/Loading.h"

// decomp: _nowLoading_data (_nowLoading_data@SLUS_010.40)
u_long Render_loadingScreenPixels[1260]; /* contents: module image (ModuleData.cpp) */

// decomp: _nowLoading_header (_nowLoading_header@SLUS_010.40)
Render_LegacyImageSize Render_legacyLoadingImageHeader; /* contents: module image (ModuleData.cpp) */

}  // extern "C"
