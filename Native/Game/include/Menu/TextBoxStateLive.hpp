#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>

#include "Menu/TextBoxState.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Menu/Framework/TextBoxes.h"
}

namespace VS::Menu {
// Fresh value views of Menu_textBoxes and Menu_textBoxSelector. Indexes past
// the eight boxes are refused by the callers below, never read.
[[nodiscard]] TextBox ReadLiveTextBox(std::size_t index);
void WriteLiveTextBox(std::size_t index, const TextBox& box);
[[nodiscard]] TextBoxSelector ReadLiveTextBoxSelector();
void WriteLiveTextBoxSelector(const TextBoxSelector& selector);

// The original module signatures, bound to the live boxes, the frame's
// pressed buttons and default choice, the menu sounds and the speakers'
// texture animations. Box ids and anchor presets outside the tables log and
// refuse (returning 0, a null box or a zero anchor) instead of reaching past
// them; a corrupt selector logs, restarts and returns 0.
int SetLiveTextBoxText(int box_id, char* text);
int SelectLiveTextBoxChoice(u_char box_mask);
void InitLiveInformationTextBox(enum Menu_InformationBoxHeader header);
int DismissAllLiveTextBoxes();
Menu_TextBoxState* LiveTextBox(int id);
Menu_TextAnchorWord LiveTextBoxAnchor(int anchor_preset_index,
                                      int orientation_flags,
                                      int text_column_count,
                                      int text_line_count);
void InitLiveTextBox(int id, int flags, int x, int y, int chars_per_line,
                     int line_count, int center_x, int center_y);
int DismissLiveTextBox(int id);
}  // namespace VS::Menu
