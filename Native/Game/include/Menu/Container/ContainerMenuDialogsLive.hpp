#pragma once

namespace VS::Menu::container {
int UpdateLiveSortMenu(int category_selection);
int UpdateLiveDiscardMenu(int packed_selection);
void RestoreLiveItemRows(int category);
int UpdateLiveTransferConfirmation(int initialize);

// Only initialized module data resets on ordinary MENUD loads.
void ResetLiveContainerDialogStatics();
// Cold session/test initialization; never an ordinary overlay-reset hook.
void ResetLiveContainerDialogSession();
}  // namespace VS::Menu::container
