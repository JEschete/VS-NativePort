// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Menu/Container/Storage.h"

// decomp: D_80109A82 (D_80109A82@src/MENU/MENUD.PRG/234.c)
u_short Menu_Container_transferredMiscCount;

// decomp: _containerItemCapacities (_containerItemCapacities@src/MENU/MENUD.PRG/234.c)
u_short Menu_Container_containerItemCapacities[] = { 32, 64, 64, 32, 64, 192, 256, 2 };

// MENUD.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Container_Storage_ResetMenudStatics(void)
{
    {
        static const u_short initial[] = { 32, 64, 64, 32, 64, 192, 256, 2 };
        memcpy((void*)&Menu_Container_containerItemCapacities, &initial, sizeof Menu_Container_containerItemCapacities);
    }
}

}  // extern "C"
