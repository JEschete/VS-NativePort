// The MENUF.PRG results screens' DIS files and screen state.
#include "Menu/Results/TimeAttack.hpp"

#include <cstdint>

#include "Content/DiscLayout.h"
#include "DiscQueue.h"

namespace VS::Menu::Results {
// decomp: _disFiles (_disFiles@src/MENU/MENUF.PRG/3B8.c)
const Content_DiscFile kDisFiles[kDisFileCount] = {
    {VS_RANK_DIS_LBA, VS_RANK_DIS_SIZE},
    {VS_TIME_DIS_LBA, VS_TIME_DIS_SIZE},
    {VS_ATTACK_DIS_LBA, VS_ATTACK_DIS_SIZE},
    {VS_IQ_DIS_LBA, VS_IQ_DIS_SIZE},
    {VS_ESC_DIS_LBA, VS_ESC_DIS_SIZE},
};

// decomp: _newTimeSlot (_newTimeSlot@src/MENU/MENUF.PRG/3B8.c)
std::int32_t newTimeSlot;

// decomp: _screenState (_screenState@src/MENU/MENUF.PRG/3B8.c)
std::int32_t screenState;

// decomp: _screenTimer (_screenTimer@src/MENU/MENUF.PRG/3B8.c)
std::int32_t screenTimer;

// decomp: _timeTrialTime (_timeTrialTime@src/MENU/MENUF.PRG/3B8.c)
std::int32_t timeTrialTime;
}  // namespace VS::Menu::Results
