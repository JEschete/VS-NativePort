#pragma once
// The field's menu request flag. Input.cpp defines it.

#include <cstdint>

namespace VS::Field {
// Nonzero while a menu request waits for the gameplay loop.
// decomp: D_800F1858 (D_800F1858@BATTLE/BATTLE.PRG,
// src/BATTLE/BATTLE.PRG/146C.c:314)
extern std::int32_t isMenuRequestQueued;
}  // namespace VS::Field
