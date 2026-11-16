#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.h"

struct Menu_Manual_AnimationState;

// Bindings from the manual illustration cores (ManualIllustration.hpp) to the
// help files, the scroll position, VRAM and the current frame's primitive
// buffer, keeping the original call shapes for MENUE.PRG's callers. Buffers
// outside a live game heap payload, records that run past theirs and
// packets that do not fit are refused with a throttled warning.
namespace VS::Menu::manual {
// decomp: MENUE.PRG _copySprites (494.c:481).
void LiveUploadSpriteTiles(u_long const* buffer, int count);
// decomp: MENUE.PRG _copyCluts (494.c:498).
void LiveUploadCluts(u_long* buffer, int count);
// decomp: MENUE.PRG _drawSprite (494.c:629). `data` points into the help
// text file.
void LiveDrawIllustration(std::int16_t* data);
// decomp: MENUE.PRG _drawContentLines (494.c:883).
void LiveDrawDiagramLines();
// decomp: MENUE.PRG _getAnimationState (494.c:940). Returns 0 after a
// logged refusal of a null state.
std::int16_t LiveAdvanceVisibilityAnimation(Menu_Manual_AnimationState* state);
// Refusals logged since start-up.
[[nodiscard]] std::uint64_t LiveIllustrationRefusalCount();
}  // namespace VS::Menu::manual
