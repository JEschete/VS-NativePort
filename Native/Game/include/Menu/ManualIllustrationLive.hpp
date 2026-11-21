#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.hpp"

// Bindings from the manual illustration cores (ManualIllustration.hpp) to the
// help files, the scroll position, VRAM and the current frame's primitive
// buffer, keeping the original call shapes for MENUE.PRG's callers. Buffers
// outside a live game heap payload, records that run past theirs and
// packets that do not fit are refused with a throttled warning.
namespace VS::Menu::manual {
struct VisibilityAnimation;

// decomp: MENUE.PRG copySprites (src/MENU/MENUE.PRG/494.c:485; was
// _copySprites).
void LiveUploadSpriteTiles(std::uint32_t const* buffer, int count);
// decomp: MENUE.PRG copyCluts (src/MENU/MENUE.PRG/494.c:502; was _copyCluts).
void LiveUploadCluts(std::uint32_t* buffer, int count);
// decomp: MENUE.PRG drawSprite (src/MENU/MENUE.PRG/494.c:633; was _drawSprite).
// `data` points into the help text file.
void LiveDrawIllustration(std::int16_t* data);
// decomp: MENUE.PRG drawContentLines (src/MENU/MENUE.PRG/494.c:887; was
// _drawContentLines).
void LiveDrawDiagramLines();
// decomp: MENUE.PRG getAnimationState (src/MENU/MENUE.PRG/494.c:944; was
// _getAnimationState). Returns 0 after a logged refusal of a null state.
std::int16_t LiveAdvanceVisibilityAnimation(VisibilityAnimation* state);
// Refusals logged since start-up.
[[nodiscard]] std::uint64_t LiveIllustrationRefusalCount();
}  // namespace VS::Menu::manual
