#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Menu/ManualPage.hpp"

// Bindings from the manual page cores (ManualPage.hpp) to the help text,
// the back buffer's draw environment, the page arrow counter, last frame's
// buttons and the current frame's primitive buffer, keeping the original
// call shapes for MENUE.PRG's callers. A help text outside a live game heap
// payload, an unknown gradient or arrow, an oversized control box, a
// missing ordering-table entry or a primitive buffer without room for the
// whole call is refused with a throttled warning, drawing nothing.
namespace VS::Menu::manual {
// decomp: MENUE.PRG _drawContent (494.c:577).
void LiveDrawPageContent();
// decomp: MENUE.PRG _setPageBg (494.c:716).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawPageBackground(int x, int y, int w, int h, int color);
// decomp: MENUE.PRG _fadeMenuUpper (494.c:740).
void LiveDrawUpperScrollFade();
// decomp: MENUE.PRG _fadeMenuLower (494.c:791).
void LiveDrawLowerScrollFade();
// decomp: MENUE.PRG _drawPaginationArrow (494.c:844).
void LiveDrawPageArrow(PageArrow arrow);
// decomp: MENUE.PRG _drawControlsBg (494.c:962).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawControlsBackground(int x, int y, int w, int h);
// Refusals logged since start-up.
[[nodiscard]] std::uint64_t LivePageRefusalCount();
}  // namespace VS::Menu::manual
