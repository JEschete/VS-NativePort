// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Menu/BattleAbilities.h"

// decomp: _abilityCount (_abilityCount@src/MENU/MENU2.PRG/64.c)
char Menu_BattleAbilities_abilityCount = 0;

// decomp: _abilityMenuState (_abilityMenuState@src/MENU/MENU2.PRG/64.c)
u_int Menu_BattleAbilities_abilityMenuState = 0;

// decomp: _abilityText (_abilityText@src/MENU/MENU2.PRG/64.c)
u_short* Menu_BattleAbilities_abilityText = NULL;

// decomp: _battleAbilityMenuStrings (_battleAbilityMenuStrings@src/MENU/MENU2.PRG/143C.c)
u_short Menu_BattleAbilities_battleAbilityMenuStrings[292]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _battleAbilityStrings (_battleAbilityStrings@MENU/MENU2.PRG)
u_short Menu_BattleAbilities_battleAbilityStrings[1060]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _firstAbility (_firstAbility@src/MENU/MENU2.PRG/64.c)
char Menu_BattleAbilities_firstAbility = 0;

// decomp: _highlightedAbility (_highlightedAbility@src/MENU/MENU2.PRG/64.c)
int Menu_BattleAbilities_highlightedAbility = 0;

// decomp: _selectedAbilityType (_selectedAbilityType@src/MENU/MENU2.PRG/64.c)
char Menu_BattleAbilities_selectedAbilityType = 0;

// decomp: _unlockableChainAbilities (_unlockableChainAbilities@src/MENU/MENU2.PRG/143C.c)
char Menu_BattleAbilities_unlockableChainAbilities[] = { 0x18, 0x19, 0x22, 0x25, 0x23, 0x1E, 0x1A,
    0x1B, 0x20, 0x1D, 0x1F };

// decomp: _unlockableDefenseAbilities (_unlockableDefenseAbilities@src/MENU/MENU2.PRG/143C.c)
char Menu_BattleAbilities_unlockableDefenseAbilities[] = { 0x2F, 0x30, 0x31, 0x32, 0x29, 0x2A, 0x33,
    0x34, 0x2C, 0x2D, 0x35 };

// decomp: _unlockedChainAbilities (_unlockedChainAbilities@src/MENU/MENU2.PRG/64.c)
char Menu_BattleAbilities_unlockedChainAbilities[16];

// decomp: _unlockedDefenseAbilities (_unlockedDefenseAbilities@src/MENU/MENU2.PRG/64.c)
char Menu_BattleAbilities_unlockedDefenseAbilities[16];

// MENU2.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_BattleAbilities_ResetMenu2Statics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_BattleAbilities_abilityCount, &initial, sizeof Menu_BattleAbilities_abilityCount);
    }
    {
        static const u_int initial = 0;
        memcpy((void*)&Menu_BattleAbilities_abilityMenuState, &initial, sizeof Menu_BattleAbilities_abilityMenuState);
    }
    {
        static const u_short* initial = NULL;
        memcpy((void*)&Menu_BattleAbilities_abilityText, &initial, sizeof Menu_BattleAbilities_abilityText);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_BattleAbilities_firstAbility, &initial, sizeof Menu_BattleAbilities_firstAbility);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_BattleAbilities_highlightedAbility, &initial, sizeof Menu_BattleAbilities_highlightedAbility);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_BattleAbilities_selectedAbilityType, &initial, sizeof Menu_BattleAbilities_selectedAbilityType);
    }
    {
        static const char initial[] = { 0x18, 0x19, 0x22, 0x25, 0x23, 0x1E, 0x1A,
    0x1B, 0x20, 0x1D, 0x1F };
        memcpy((void*)&Menu_BattleAbilities_unlockableChainAbilities, &initial, sizeof Menu_BattleAbilities_unlockableChainAbilities);
    }
    {
        static const char initial[] = { 0x2F, 0x30, 0x31, 0x32, 0x29, 0x2A, 0x33,
    0x34, 0x2C, 0x2D, 0x35 };
        memcpy((void*)&Menu_BattleAbilities_unlockableDefenseAbilities, &initial, sizeof Menu_BattleAbilities_unlockableDefenseAbilities);
    }
}

}  // extern "C"
