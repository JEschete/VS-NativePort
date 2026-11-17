// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Menu/Results/TimeAttack.h"

// decomp: _disFiles (_disFiles@src/MENU/MENUF.PRG/3B8.c)
Content_DiscFile const Menu_Results_kDisFiles[] = { { VS_RANK_DIS_LBA, VS_RANK_DIS_SIZE },
    { VS_TIME_DIS_LBA, VS_TIME_DIS_SIZE }, { VS_ATTACK_DIS_LBA, VS_ATTACK_DIS_SIZE },
    { VS_IQ_DIS_LBA, VS_IQ_DIS_SIZE }, { VS_ESC_DIS_LBA, VS_ESC_DIS_SIZE } };

// decomp: _newTimeSlot (_newTimeSlot@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_newTimeSlot;

// decomp: _screenState (_screenState@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_screenState;

// decomp: _screenTimer (_screenTimer@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_screenTimer;

// decomp: _timeTrialTime (_timeTrialTime@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_timeTrialTime;

}  // extern "C"
