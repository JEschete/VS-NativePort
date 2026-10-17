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
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

extern u_short Menu_Container_transferredMiscCount;;
extern u_short Menu_Container_containerItemCapacities[];
void Menu_Container_BuildWeaponStats(Items_WeaponLoadout* target, Items_InventoryWeapon* source,
    Items_ContainerData* container);
void Menu_Container_BuildShieldStats(Items_ShieldLoadout* target, Items_InventoryShield* source,
    Items_ContainerData* container);
u_short* Menu_Container_CategoryIndices(int itemCategory, Items_Container* container);
int Menu_Container_FindItemRow(int itemCategory, int index, u_short* indices);
void Menu_Container_AppendItemIndex(int itemCategory, int index, u_short* indices);
void Menu_Container_ClearItem(
    int itemCategory, int index, Items_ContainerData* container);
int Menu_Container_ItemId(int type, int index, Items_ContainerData* container);
int Menu_Container_CountItems(int itemCategory, Items_ContainerData* container);
int Menu_Container_FindEmptySlot(int itemCategory, Items_ContainerData* container);
void Menu_Container_PreviewTransferredMisc(void);
void Menu_Container_RefreshMiscTransferCount(void);
int Menu_Container_CopyItem(int itemCategory, Items_ContainerData* targetContainer,
    int sourceSlot, Items_ContainerData* sourceContainer);
void Menu_Container_RebuildCategoryIndices(int itemCategory, Items_Container* container);
void Menu_Container_SnapshotGadgetBag(void);
void Menu_Container_CommitGadgetBag(void);
int Menu_Container_ParentWeaponIndex(
    int itemCategory, int index, Items_ContainerData* container);
int Menu_Container_ParentShieldIndex(
    int itemCategory, int index, Items_ContainerData* container);
void Menu_Container_SortWeapons(int stat);
void Menu_Container_SortShields(int arg0);
void Menu_Container_SortMisc(int arg0);
void Menu_Container_CopyEquipmentStats(Items_EquipmentPartStats* item, int type, int index);
void Menu_Container_SortEquipment(int arg0, int arg1);
int Menu_Container_UpdateSortMenu(int arg0);
int Menu_Container_UpdateDiscardMenu(int arg0);
int Menu_Container_CountWeaponGems(int arg0);
int Menu_Container_CountShieldGems(int arg0);
int Menu_Container_WouldExceedCapacity(int arg0, int arg1);
int Menu_Container_TransferCapacityError(int arg0, int arg1);
void Menu_Container_UndoPendingTransfer(int arg0);
void Menu_Container_StageItemTransfer(int arg0, int arg1);
int Menu_Container_FindPendingTransfer(int arg0, int arg1);
void Menu_Container_AppendPendingTransfer(int arg0, int arg1);
void Menu_Container_RebuildPendingTransfers(void);
void Menu_Container_Storage_ResetMenudStatics(void);

#endif // VS_MENU_CONTAINER_STORAGE_H_DECLS
