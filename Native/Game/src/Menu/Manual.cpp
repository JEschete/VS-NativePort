// MENUE.PRG's manual statics and tables.
#include "Menu/Manual.hpp"

#include <cstdint>

#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Legacy/Psx.hpp"

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
    {VS_HELP01_HF1_LBA, VS_HELP01_HF1_SIZE},
    {VS_HELP01_HF0_LBA, VS_HELP01_HF0_SIZE},
    {VS_HELP02_HF1_LBA, VS_HELP02_HF1_SIZE},
    {VS_HELP02_HF0_LBA, VS_HELP02_HF0_SIZE},
    {VS_HELP03_HF1_LBA, VS_HELP03_HF1_SIZE},
    {VS_HELP03_HF0_LBA, VS_HELP03_HF0_SIZE},
    {VS_HELP04_HF1_LBA, VS_HELP04_HF1_SIZE},
    {VS_HELP04_HF0_LBA, VS_HELP04_HF0_SIZE},
    {VS_HELP05_HF1_LBA, VS_HELP05_HF1_SIZE},
    {VS_HELP05_HF0_LBA, VS_HELP05_HF0_SIZE},
    {VS_HELP06_HF1_LBA, VS_HELP06_HF1_SIZE},
    {VS_HELP06_HF0_LBA, VS_HELP06_HF0_SIZE},
    {VS_HELP07_HF1_LBA, VS_HELP07_HF1_SIZE},
    {VS_HELP07_HF0_LBA, VS_HELP07_HF0_SIZE},
    {VS_HELP08_HF1_LBA, VS_HELP08_HF1_SIZE},
    {VS_HELP08_HF0_LBA, VS_HELP08_HF0_SIZE},
    {VS_HELP09_HF1_LBA, VS_HELP09_HF1_SIZE},
    {VS_HELP09_HF0_LBA, VS_HELP09_HF0_SIZE},
    {VS_HELP10_HF1_LBA, VS_HELP10_HF1_SIZE},
    {VS_HELP10_HF0_LBA, VS_HELP10_HF0_SIZE},
    {VS_HELP11_HF1_LBA, VS_HELP11_HF1_SIZE},
    {VS_HELP11_HF0_LBA, VS_HELP11_HF0_SIZE},
    {VS_HELP12_HF1_LBA, VS_HELP12_HF1_SIZE},
    {VS_HELP12_HF0_LBA, VS_HELP12_HF0_SIZE},
    {VS_HELP13_HF1_LBA, VS_HELP13_HF1_SIZE},
    {VS_HELP13_HF0_LBA, VS_HELP13_HF0_SIZE},
    {VS_HELP14_HF1_LBA, VS_HELP14_HF1_SIZE},
    {VS_HELP14_HF0_LBA, VS_HELP14_HF0_SIZE},
};
}  // namespace VS::Menu::manual
