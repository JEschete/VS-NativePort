// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_CONTAINER_STORAGE_H_TYPES
#define VS_MENU_CONTAINER_STORAGE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Container_InventoryStorage Menu_Container_InventoryStorage;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_MENU_CONTAINER_STORAGE_H_SET_TYPES_ONLY
#endif
#include "Items/Inventory.h"
#ifdef VS_MENU_CONTAINER_STORAGE_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_MENU_CONTAINER_STORAGE_H_SET_TYPES_ONLY
#endif

// decomp: copyContainerFlags (src/MENU/MENUD.PRG/234.c:17)
enum Menu_Container_CopyContainerFlag { Menu_Container_CopyContainerFlag_CopyContainerFlagsCopy = 0x10 };
typedef enum Menu_Container_CopyContainerFlag Menu_Container_CopyContainerFlag;

// decomp: vs_menu_inventoryStorage_t (src/BATTLE/BATTLE.PRG/func_8006B57C_t.h:153)
typedef struct Menu_Container_InventoryStorage {
    char savedInventory[15][256];
    Items_InventoryIndices savedInventoryIndices;
    Items_ContainerData storedContainer;
    Items_ContainerData transferPreview;
    Items_ContainerData stagedTransfer;
    u_short itemsToTransfer[64];
    Items_Container container;
    Items_Container gadgetBag;
} Menu_Container_InventoryStorage;
_Static_assert(sizeof(Menu_Container_InventoryStorage) == 0x14730, "Menu_Container_InventoryStorage must keep its PS1 layout");

#endif // VS_MENU_CONTAINER_STORAGE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_CONTAINER_STORAGE_H_DECLS)
#define VS_MENU_CONTAINER_STORAGE_H_DECLS

extern u_short Menu_Container_transferredMiscCount;;
extern u_short Menu_Container_containerItemCapacities[];
int Menu_Container_FindPendingTransfer(int category_id, int item_index);
void Menu_Container_AppendPendingTransfer(int category_id, int item_index);
void Menu_Container_RebuildPendingTransfers(void);
void Menu_Container_Storage_ResetMenudStatics(void);

#endif // VS_MENU_CONTAINER_STORAGE_H_DECLS
