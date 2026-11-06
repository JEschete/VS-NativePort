#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Menu/Files.hpp"

struct Items_ContainerData;
struct Items_ContainerIndices;

// Binds the file screens (Files.hpp) to the memory-card driver, the native
// file menu, the menu framework, the save summaries, the pad, the disc queue
// and the GPU, in the shapes the framework's submenu dispatch, the container
// save, the native file presentation and the title screen call.
namespace VS::Menu::Files {
// MENU7.PRG vs_menu7_dataMenu (260.c:4187) over the live State.
int UpdateLiveDataMenu(std::uint8_t& step);
// MENU7.PRG vs_menu7_gameOver (260.c:3603) over the live State.
int UpdateLiveGameOver(std::uint8_t& step);
// MENU7.PRG _promptYesNo (260.c:3727) over the live State.
int UpdateLiveContainerConfirmation(int init);
// MENU7.PRG _initFileMenu (260.c:1447) over the live State.
void ResetLiveElements();
// MENU7.PRG _drawFileMenu (260.c:1951) over the live State.
void DrawLiveFrame(int frame_buffer);
// MENU7.PRG _copyContainer (260.c:3821) over the live container records; a
// null record is refused.
void CopyLiveSelectedContainerItems(Items_ContainerData* target,
                                    const Items_ContainerData* source,
                                    const Items_ContainerIndices* to_copy);

// The native file presentation's drawing in the original call shapes.
// MENU7.PRG _printCharacter (260.c:1524): the next x.
int DrawLiveGlyph(std::uint32_t glyph, int x, int y, int clut);
// MENU7.PRG _drawSaveInfoUI (260.c:1222).
void DrawLiveSummaryLabel(std::int32_t position, Label label);
// MENU7.PRG _drawHPMP (260.c:1270).
void DrawLiveHpMpSummary(std::int32_t position, Stat stat,
                         std::uint32_t current, std::uint32_t max);
// MENU7.PRG _drawFileMenuElement (260.c:1765) of an element outside the
// screens' table.
void DrawLiveElement(Element& element);
// MENU7.PRG _renderInteger (260.c:1239) and its TITLE.PRG copy.
void DrawLiveInteger(std::int32_t position, std::uint32_t value,
                     std::uint32_t divisor);
// MENU7.PRG _printString (260.c:1675) and its TITLE.PRG copy: the text runs
// to the end of the MCMAN.BIN table or the native footer it lies in; other
// text is refused.
void DrawLiveText(const char* text, int x, int y, int clut);
// MENU7.PRG _intepolateMenuItemBgColour (260.c:1738) and its TITLE.PRG
// copy; an inner factor past the colour tables is refused as black.
[[nodiscard]] std::uint32_t LiveElementBackgroundColor(std::uint32_t outer,
                                                       std::uint32_t inner);
// MENU7.PRG _drawFileMenuBg (260.c:2078) and its TITLE.PRG copy.
void DrawLiveBackground();

// TITLE.PRG (22C.c) over the title screen's variables.
// _initSaveScreen (22C.c:3074).
void InitializeLiveDisplay();
// _saveScreenSwapBuf (22C.c:3093).
void SwapLiveBuffers();
// _saveScreenConfirmation (22C.c:3226).
int UpdateLiveClearSaveReminder(int init);
// _saveScreenExitScreen (22C.c:3299).
void DrawLiveExitFade();
// _initFileMenu (22C.c:1321) over the title's elements.
void ResetLiveTitleElements();
// _drawFileMenu (22C.c:1784) over the title's elements.
void DrawLiveTitleFrame(int frame_buffer);
// _drawFileMenuElement (22C.c:1601) of an element outside the title's table.
void DrawLiveTitleElement(Element& element);
// _initFileMenuElement (22C.c:1332) with MCMAN.BIN text at `text_offset`
// halfwords; false when the table is missing, the offset lies past it or
// the id names no element.
bool InitializeLiveTitleElement(int id, std::uint32_t position,
                                std::uint32_t size, std::uint16_t text_offset);
// _fileMenuElementsActive (22C.c:1375) over the title's elements.
[[nodiscard]] bool AreLiveTitleAnimationsComplete();

// The live State, for the screens' tests.
[[nodiscard]] State& LiveFilesState();
// The live host (stateless between calls), for the screens' tests.
[[nodiscard]] Host& LiveFilesHost();
// Restores the MENU7.PRG file statics a fresh load of the overlay brings
// back.
void ResetLiveFilesStatics();
}  // namespace VS::Menu::Files
