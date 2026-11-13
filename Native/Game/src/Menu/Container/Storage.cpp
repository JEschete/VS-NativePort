// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Items/ContainerIndexingLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/FrameworkLiveState.h"

// decomp: D_80109A82 (D_80109A82@src/MENU/MENUD.PRG/234.c)
u_short Menu_Container_transferredMiscCount;

// decomp: _containerItemCapacities (_containerItemCapacities@src/MENU/MENUD.PRG/234.c)
u_short Menu_Container_containerItemCapacities[] = { 32, 64, 64, 32, 64, 192, 256, 2 };

// decomp: MENUD.PRG func_801069B0 (src/MENU/MENUD.PRG/234.c:2061)
int Menu_Container_FindPendingTransfer(int category_id, int item_index)
{
    int i;

    for (i = 0; i < 64; ++i) {
        if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[i] == ((category_id << 8) | (item_index + 1))) {
            return i + 1;
        }
    }
    return 0;
}

// decomp: MENUD.PRG func_80106A04 (src/MENU/MENUD.PRG/234.c:2073)
void Menu_Container_AppendPendingTransfer(int category_id, int item_index)
{
    int i;
    u_short* pending_transfer_items;

    i = 0;
    pending_transfer_items = (u_short*)Menu_Framework_unusedInventoryStorage + 0x61D8;
    for (i = 0; i < 64; ++i) {
        if (pending_transfer_items[i] == 0) {
            pending_transfer_items[i] = ((category_id << 8) | (item_index + 1));
            return;
        }
    }
}

// decomp: MENUD.PRG func_80106A50 (src/MENU/MENUD.PRG/234.c:2088)
void Menu_Container_RebuildPendingTransfers(void)
{
    int j;
    int next_occupied_transfer_index;
    int i;
    int k;
    u_short* pending_transfer_items = Menu_Framework_unusedInventoryStorage->itemsToTransfer;

    for (i = 0; i < 64; ++i) {
        int source = pending_transfer_items[i];
        if ((source != 0)
            && (VS::Items::LiveContainerItemId((source >> 8), (source - 1) & 0xFF,
                    &Menu_Framework_unusedInventoryStorage->stagedTransfer)
                == 0)) {
            pending_transfer_items[i] = 0;
        }
    }

    i = 0;

    while (1) {
        if (pending_transfer_items[i] != 0) {
            ++i;
            if (i == 0x3F) {
                break;
            }
        } else {
            for (next_occupied_transfer_index = i + 1; next_occupied_transfer_index < 0x40; ++next_occupied_transfer_index) {
                if (pending_transfer_items[next_occupied_transfer_index] != 0) {
                    break;
                }
            }

            if (next_occupied_transfer_index == 0x40) {
                break;
            }

            for (j = next_occupied_transfer_index; j < 0x40; ++j) {
                pending_transfer_items[j + i - next_occupied_transfer_index] = pending_transfer_items[j];
            }

            for (j = j + (i - next_occupied_transfer_index); j < 64; ++j) {
                pending_transfer_items[j] = 0;
            }
        }
    }

    for (k = 0; k < 7; ++k) {
        for (i = 0; i < Menu_Container_containerItemCapacities[k]; ++i) {
            if ((VS::Items::LiveContainerItemId(k, i, &Menu_Framework_unusedInventoryStorage->stagedTransfer) != 0)
                && (Menu_Container_FindPendingTransfer(k, i) == 0)) {
                Menu_Container_AppendPendingTransfer(k, i);
            }
        }
    }
}

// MENUD.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Container_Storage_ResetMenudStatics(void)
{
    {
        static const u_short initial[] = { 32, 64, 64, 32, 64, 192, 256, 2 };
        memcpy((void*)&Menu_Container_containerItemCapacities, &initial, sizeof Menu_Container_containerItemCapacities);
    }
}

}  // extern "C"
