#pragma once

// The C library headers the copied decomp code relies on (memcpy, printf, abs,
// INT_MAX, size_t and the like), reached through Legacy/Psx.hpp and the SDK
// headers. The PS1 SDK scalar aliases are retired (#441): u_char, u_short and
// u_int became std::uint8_t, std::uint16_t and std::uint32_t, and u_long, 32
// bits under Windows x64 (LLP64), became std::uint32_t, so copied code that
// stores pointers in it is still a 64-bit hazard (PORTING_WATCHLIST.md ?2).
// ONE is VS::Core::Math::kQ12Unit and ABS is std::abs.

#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
