#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>

#include "Legacy/Psx.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/TextBoxState.hpp"

namespace VS::Menu {
// Fresh value views of textBoxes and textBoxSelector. Indexes past
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
int SelectLiveTextBoxChoice(std::uint8_t box_mask);
void InitLiveInformationTextBox(InformationHeader header);
int DismissAllLiveTextBoxes();
TextBoxState* LiveTextBox(int id);
TextAnchor LiveTextBoxAnchor(int anchor_preset_index, int orientation_flags,
                             int text_column_count, int text_line_count);
void InitLiveTextBox(int id, int flags, int x, int y, int chars_per_line,
                     int line_count, int center_x, int center_y);
int DismissLiveTextBox(int id);
}  // namespace VS::Menu
