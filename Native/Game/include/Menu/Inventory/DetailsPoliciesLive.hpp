#pragma once

// Entry signatures of the MENU3.PRG and MENUB.PRG header instantiations.
namespace VS::Menu::Inventory {
void SetLivePanelVisibility(char visibility);
[[nodiscard]] int PollLiveDetailsPaging(int category, int page);
void BeginLiveDetailsTransition(int selected_row);
void InitializeLiveDetailsLayout(int selected_row, int stat_pages);
[[nodiscard]] int SelectLiveDetailsItem(int category, int page);
void ExitLiveDetailsView(int clear_dp_pp);
}  // namespace VS::Menu::Inventory
