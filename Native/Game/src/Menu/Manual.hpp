#pragma once
// MENUE.PRG's manual statics and tables (src/MENU/MENUE.PRG/494.c). The
// manual screen, topic, page and illustration bindings (Manual*Live.cpp)
// share them as the original's functions did; ModuleData.cpp fills the menu
// text table from the module image.
#include <cstddef>
#include <cstdint>

#include "Content/DiscQueue.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/ManualScreen.hpp"
#include "Menu/ManualTopics.hpp"

namespace VS::Menu::manual {
// menuTextTable's halfwords: the offsets, then the topic strings.
inline constexpr std::size_t kMenuTextTableWords = 516;
// Each topic's text and asset files.
inline constexpr std::size_t kHelpFileCount{kTopicCount * kHelpFilesPerTopic};

// decomp: menuTextTable (src/MENU/MENUE.PRG/494.c:142)
extern std::uint16_t menuTextTable[kMenuTextTableWords];
// decomp: topMenu_state (src/MENU/MENUE.PRG/494.c:213)
extern std::int32_t topMenuState;
// decomp: menuSelectedRow (src/MENU/MENUE.PRG/494.c:214)
extern std::int32_t menuSelectedRow;
// decomp: helpTextCdQueue (src/MENU/MENUE.PRG/494.c:215)
extern VS::Content::DiscQueueSlot* helpTextCdQueue;
// decomp: helpAssetsCdQueue (src/MENU/MENUE.PRG/494.c:216)
extern VS::Content::DiscQueueSlot* helpAssetsCdQueue;
// decomp: showMenuState (src/MENU/MENUE.PRG/494.c:217)
extern std::int32_t showMenuState;
// decomp: scrollPosition (src/MENU/MENUE.PRG/494.c:218)
extern std::int32_t scrollPosition;
// decomp: totalLineCount (src/MENU/MENUE.PRG/494.c:219)
extern std::int32_t totalLineCount;
// decomp: pageArrowAnimState (src/MENU/MENUE.PRG/494.c:220)
extern std::int32_t pageArrowAnimState;
// decomp: helpPageLoadCounter (src/MENU/MENUE.PRG/494.c:221)
extern std::int32_t helpPageLoadCounter;
// decomp: helpText (src/MENU/MENUE.PRG/494.c:222)
extern std::int32_t* helpText;
// decomp: helpAssets (src/MENU/MENUE.PRG/494.c:223)
extern std::uint32_t* helpAssets;
// decomp: helpFileCdFiles (src/MENU/MENUE.PRG/494.c:241): each topic's
// asset file (HF1), then its text file (HF0).
extern const VS::Content::DiscFile kHelpFileCdFiles[kHelpFileCount];
}  // namespace VS::Menu::manual
