// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.hpp"
#include "Render/Loading.hpp"

// decomp: _nowLoading_data (_nowLoading_data@SLUS_010.40)
std::uint32_t VS::Render::loadingScreenPixels[VS::Render::kLoadingScreenPixelWords]; /* contents: module image (ModuleData.cpp) */

// decomp: _nowLoading_header (_nowLoading_header@SLUS_010.40)
VS::Render::LegacyImageSize VS::Render::legacyLoadingImageHeader; /* contents: module image (ModuleData.cpp) */

