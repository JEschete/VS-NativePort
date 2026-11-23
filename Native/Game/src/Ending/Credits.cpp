// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Ending/Credits.hpp"

#include "Content/DiscLayout.hpp"

using VS::Content::disc::Locate;

// decomp: D_8006E3FC (D_8006E3FC@ENDING/ENDING.PRG)
// contents: module image (ModuleData.cpp)
std::uint8_t
    VS::Ending::Credits::scriptBytes[VS::Ending::Credits::kScriptBytes];

// decomp: _glyphWidths (_glyphWidths@ENDING/ENDING.PRG)
// contents: module image (ModuleData.cpp)
std::uint8_t
    VS::Ending::Credits::glyphWidths[VS::Ending::Credits::kGlyphWidthSpanBytes];

// decomp: D_800DB7B0 (D_800DB7B0@ENDING/ENDING.PRG)
VS::Ending::Credits::TimHeader* VS::Ending::Credits::illustrationTimWorkspace;

// decomp: D_800DC1A8 (D_800DC1A8@ENDING/ENDING.PRG)
void* VS::Ending::Credits::illustrationResources
    [VS::Ending::Credits::kIllustrationCount];

// decomp: _illustLbas (_illustLbas@ENDING/ENDING.PRG)
std::int32_t VS::Ending::Credits::illustrationDiscLocations
    [VS::Ending::Credits::kIllustrationCount] = {
        Locate("ENDING/ILLUST06.BIN").lba,
        Locate("ENDING/ILLUST07.BIN").lba,
        Locate("ENDING/ILLUST08.BIN").lba,
        Locate("ENDING/ILLUST09.BIN").lba,
        Locate("ENDING/ILLUST10.BIN").lba,
        Locate("ENDING/ILLUST11.BIN").lba,
        Locate("ENDING/ILLUST12.BIN").lba,
        Locate("ENDING/ILLUST13.BIN").lba,
        Locate("ENDING/ILLUST14.BIN").lba,
        Locate("ENDING/ILLUST15.BIN").lba,
        Locate("ENDING/ILLUST16.BIN").lba,
        0,
        0,
        0,
        0,
        0};

// decomp: _illustSizes (_illustSizes@ENDING/ENDING.PRG)
std::int32_t VS::Ending::Credits::illustrationDiscSizes
    [VS::Ending::Credits::kIllustrationCount] = {
        Locate("ENDING/ILLUST06.BIN").size,
        Locate("ENDING/ILLUST07.BIN").size,
        Locate("ENDING/ILLUST08.BIN").size,
        Locate("ENDING/ILLUST09.BIN").size,
        Locate("ENDING/ILLUST10.BIN").size,
        Locate("ENDING/ILLUST11.BIN").size,
        Locate("ENDING/ILLUST12.BIN").size,
        Locate("ENDING/ILLUST13.BIN").size,
        Locate("ENDING/ILLUST14.BIN").size,
        Locate("ENDING/ILLUST15.BIN").size,
        Locate("ENDING/ILLUST16.BIN").size,
        0,
        0,
        0,
        0,
        0};
