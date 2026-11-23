// MENUE.PRG's manual statics and tables.
#include "Menu/Manual.hpp"

#include <cstdint>

#include "Content/DiscLayout.hpp"
#include "Content/DiscQueue.hpp"
#include "Legacy/Psx.hpp"

using VS::Content::disc::LocateAs;

namespace VS::Menu::manual {
// decomp: menuTextTable (src/MENU/MENUE.PRG/494.c:142); contents: the module
// image (ModuleData.cpp).
std::uint16_t menuTextTable[kMenuTextTableWords];
// decomp: topMenu_state (src/MENU/MENUE.PRG/494.c:213)
std::int32_t topMenuState;
// decomp: menuSelectedRow (src/MENU/MENUE.PRG/494.c:214)
std::int32_t menuSelectedRow;
// decomp: helpTextCdQueue (src/MENU/MENUE.PRG/494.c:215)
VS::Content::DiscQueueSlot* helpTextCdQueue;
// decomp: helpAssetsCdQueue (src/MENU/MENUE.PRG/494.c:216)
VS::Content::DiscQueueSlot* helpAssetsCdQueue;
// decomp: showMenuState (src/MENU/MENUE.PRG/494.c:217)
std::int32_t showMenuState;
// decomp: scrollPosition (src/MENU/MENUE.PRG/494.c:218)
std::int32_t scrollPosition;
// decomp: totalLineCount (src/MENU/MENUE.PRG/494.c:219)
std::int32_t totalLineCount;
// decomp: pageArrowAnimState (src/MENU/MENUE.PRG/494.c:220)
std::int32_t pageArrowAnimState;
// decomp: helpPageLoadCounter (src/MENU/MENUE.PRG/494.c:221)
std::int32_t helpPageLoadCounter;
// decomp: helpText (src/MENU/MENUE.PRG/494.c:222)
std::int32_t* helpText;
// decomp: helpAssets (src/MENU/MENUE.PRG/494.c:223)
std::uint32_t* helpAssets;

// decomp: helpFileCdFiles (src/MENU/MENUE.PRG/494.c:241): mkHfoPair(HELPnn)
// for topics 1 to 14.
const VS::Content::DiscFile kHelpFileCdFiles[kHelpFileCount] = {
    LocateAs<VS::Content::DiscFile>("SMALL/HELP01.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP01.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP02.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP02.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP03.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP03.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP04.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP04.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP05.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP05.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP06.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP06.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP07.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP07.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP08.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP08.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP09.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP09.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP10.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP10.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP11.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP11.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP12.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP12.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP13.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP13.HF0"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP14.HF1"),
    LocateAs<VS::Content::DiscFile>("SMALL/HELP14.HF0"),
};
}  // namespace VS::Menu::manual
