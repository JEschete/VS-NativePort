#pragma once
// The MENUF.PRG results screens' DIS files and the time-attack and shared
// screen state (src/MENU/MENUF.PRG/3B8.c).
#include <cstddef>
#include <cstdint>
#include <utility>

#include "DiscQueue.h"

namespace VS::Menu::Results {
// decomp: _disFileIndices (src/MENU/MENUF.PRG/3B8.c:44): the results screens'
// DIS image files.
enum class DisFile : std::uint8_t { kRank, kTime, kAttack, kIq, kEsc };

// One DIS file per DisFile, kRank through kEsc.
inline constexpr std::size_t kDisFileCount = 5;
static_assert(std::to_underlying(DisFile::kEsc) + 1 == kDisFileCount);

// decomp: _disFiles (_disFiles@src/MENU/MENUF.PRG/3B8.c): the disc location of
// each results screen's DIS file, indexed by DisFile.
extern const Content_DiscFile kDisFiles[kDisFileCount];

[[nodiscard]] inline const Content_DiscFile& DiscFileFor(
    DisFile file) noexcept {
  return kDisFiles[std::to_underlying(file)];
}
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
