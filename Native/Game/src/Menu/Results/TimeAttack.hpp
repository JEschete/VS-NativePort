#pragma once
// The MENUF.PRG results screens' DIS files and the time-attack and shared
// screen state (src/MENU/MENUF.PRG/3B8.c).
#include <cstddef>
#include <cstdint>

#include "DiscQueue.h"

namespace VS::Menu::Results {
// One DIS file per Menu_Results_DisFile, Rank through Esc.
inline constexpr std::size_t kDisFileCount = 5;

// decomp: _disFiles (_disFiles@src/MENU/MENUF.PRG/3B8.c): the disc location of
// each results screen's DIS file, indexed by Menu_Results_DisFile.
extern const Content_DiscFile kDisFiles[kDisFileCount];
// decomp: _newTimeSlot (_newTimeSlot@src/MENU/MENUF.PRG/3B8.c): the course
// record row the completed time took, or the row count when it set no record
// (TimeAttackState::new_slot).
extern std::int32_t newTimeSlot;
// decomp: _screenState (_screenState@src/MENU/MENUF.PRG/3B8.c): the phase of
// the active results screen.
extern std::int32_t screenState;
// decomp: _screenTimer (_screenTimer@src/MENU/MENUF.PRG/3B8.c): the frame
// timer of the active results screen.
extern std::int32_t screenTimer;
// decomp: _timeTrialTime (_timeTrialTime@src/MENU/MENUF.PRG/3B8.c): the
// completed time-attack time (TimeAttackState::time).
extern std::int32_t timeTrialTime;
}  // namespace VS::Menu::Results
