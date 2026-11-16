#pragma once
// Hand-maintained (tools/port/owned.csv).
// The original MENUF.PRG draw signatures bound to Menu_Results_disMap and the
// current frame's primitive buffer and ordering table. A texture index outside
// the atlas, a missing ordering entry or a full primitive buffer draws nothing
// for that quad and logs a throttled warning.

namespace VS::Menu::Results {
void LiveDrawFadingTexture(int x, int y, int texture_index, int brightness);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawFadingTextureClut(int x, int y, int texture_index, int brightness,
                               int clut);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawTime(int x, int y, int timer, int time, int is_new_record);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawPressButtonPrompt(int x, int y, int slide_timer, int bounce_timer);
}  // namespace VS::Menu::Results
