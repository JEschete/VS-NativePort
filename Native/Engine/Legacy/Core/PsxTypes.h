#pragma once

// Basic PS1 SDK scalar types used by the copied C (Phase 1a). `u_long` is 32
// bits here because the build is Windows x64 (LLP64), matching the original;
// copied code that stores pointers in it is a 64-bit hazard
// (PORTING_WATCHLIST.md §2).

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using u_char = unsigned char;
using u_short = unsigned short;
using u_int = unsigned int;
using u_long = unsigned long;
using ushort = unsigned short;
using uint = unsigned int;

struct quad {
  long val[2];
};

#define ONE 4096
#define ABS(x) (((x) >= 0) ? (x) : (-(x)))
