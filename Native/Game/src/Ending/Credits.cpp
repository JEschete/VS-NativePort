// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Content/DiscLayout.h"
#include "Ending/Credits.hpp"

// decomp: D_8006E3FC (D_8006E3FC@ENDING/ENDING.PRG)
std::uint8_t VS::Ending::Credits::scriptBytes[VS::Ending::Credits::kScriptBytes]; /* contents: module image (ModuleData.cpp) */

// decomp: _glyphWidths (_glyphWidths@ENDING/ENDING.PRG)
std::uint8_t VS::Ending::Credits::glyphWidths[VS::Ending::Credits::kGlyphWidthSpanBytes]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DB7B0 (D_800DB7B0@ENDING/ENDING.PRG)
VS::Ending::Credits::TimHeader* VS::Ending::Credits::illustrationTimWorkspace;

// decomp: D_800DC1A8 (D_800DC1A8@ENDING/ENDING.PRG)
void* VS::Ending::Credits::illustrationResources[VS::Ending::Credits::kIllustrationCount];

// decomp: _illustLbas (_illustLbas@ENDING/ENDING.PRG)
std::int32_t VS::Ending::Credits::illustrationDiscLocations[VS::Ending::Credits::kIllustrationCount] = { VS_ILLUST06_BIN_LBA, VS_ILLUST07_BIN_LBA, VS_ILLUST08_BIN_LBA,
    VS_ILLUST09_BIN_LBA, VS_ILLUST10_BIN_LBA, VS_ILLUST11_BIN_LBA, VS_ILLUST12_BIN_LBA,
    VS_ILLUST13_BIN_LBA, VS_ILLUST14_BIN_LBA, VS_ILLUST15_BIN_LBA, VS_ILLUST16_BIN_LBA, 0,
    0, 0, 0, 0 };

// decomp: _illustSizes (_illustSizes@ENDING/ENDING.PRG)
std::int32_t VS::Ending::Credits::illustrationDiscSizes[VS::Ending::Credits::kIllustrationCount] = { VS_ILLUST06_BIN_SIZE, VS_ILLUST07_BIN_SIZE, VS_ILLUST08_BIN_SIZE,
    VS_ILLUST09_BIN_SIZE, VS_ILLUST10_BIN_SIZE, VS_ILLUST11_BIN_SIZE,
    VS_ILLUST12_BIN_SIZE, VS_ILLUST13_BIN_SIZE, VS_ILLUST14_BIN_SIZE,
    VS_ILLUST15_BIN_SIZE, VS_ILLUST16_BIN_SIZE, 0, 0, 0, 0, 0 };

