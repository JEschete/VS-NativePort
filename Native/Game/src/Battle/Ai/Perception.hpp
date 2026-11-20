#pragma once
// The packed relation word an actor keeps per other actor, and the
// perception pass's frame tick budget, second target and engagement flag.
// Perception.cpp defines the globals.

#include <cstdint>

namespace VS::Battle::Ai {

// The native layout names the reserved bits upstream leaves unnamed.
// decomp: func_800DA1D4_t (src/BATTLE/BATTLE.PRG/6E4CCShared.h:119)
struct RelationWord {
  /* 0x00 */ std::uint32_t active : 1;
  // Was active, clear at point height, in view, and a bit kept on Ashley's
  // restore.
  /* 0x00 */ std::uint32_t perception_flags : 4;
  /* 0x00 */ std::uint32_t visible : 1;
  /* 0x00 */ std::uint32_t classification : 2;
  /* 0x01 */ std::uint32_t counter : 7;
  /* 0x01 */ std::uint32_t timer : 5;
  /* 0x02 */ std::uint32_t classification_timer : 7;
  /* 0x03 */ std::uint32_t blocked : 1;
  /* 0x03 */ std::uint32_t previous_classification : 2;
  /* 0x03 */ std::uint32_t reserved3_bit6 : 2;
};
static_assert(sizeof(RelationWord) == sizeof(std::uint32_t),
              "RelationWord must keep its PS1 layout");

// decomp: D_800F16F4 (src/BATTLE/BATTLE.PRG/6FC00.c:14)
extern std::int32_t frameTickBudget;
// decomp: D_800F58D4 (src/BATTLE/BATTLE.PRG/6FC00.c:16)
extern std::int32_t secondPerceptionTarget;
// decomp: D_800F58E0 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:527)
extern std::int32_t candidateEngagedThisUpdate;

}  // namespace VS::Battle::Ai
