// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Render/Loading.hpp"

#include "Legacy/Psx.hpp"

// decomp: _nowLoading_data (_nowLoading_data@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::uint32_t
    VS::Render::loadingScreenPixels[VS::Render::kLoadingScreenPixelWords];

// decomp: _nowLoading_header (_nowLoading_header@SLUS_010.40)
// contents: module image (ModuleData.cpp)
VS::Render::LegacyImageSize VS::Render::legacyLoadingImageHeader;
