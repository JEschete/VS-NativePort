// The MENUF.PRG results screens' DIS files and screen state.
#include "Menu/Results/TimeAttack.hpp"

#include <cstdint>

#include "Content/DiscLayout.hpp"
#include "Content/DiscQueue.hpp"

using VS::Content::disc::LocateAs;

namespace VS::Menu::Results {
// decomp: _disFiles (_disFiles@src/MENU/MENUF.PRG/3B8.c)
const VS::Content::DiscFile kDisFiles[kDisFileCount] = {
    LocateAs<VS::Content::DiscFile>("SMALL/RANK.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/TIME.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ATTACK.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/IQ.DIS"),
    LocateAs<VS::Content::DiscFile>("SMALL/ESC.DIS"),
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
