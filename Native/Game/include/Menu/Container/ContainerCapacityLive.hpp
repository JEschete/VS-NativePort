#pragma once

namespace VS::Menu::container {
void DrawLiveCapacityCount(int limit, int position, int occupied);
void DrawLiveStorageCapacities();
[[nodiscard]] int WouldExceedLiveCapacity(int category, int additional);
[[nodiscard]] int LiveTransferCapacityError(int category, int slot);
void UndoLivePendingTransfer(int row);
void StageLiveItemTransfer(int category, int slot);
}  // namespace VS::Menu::container
