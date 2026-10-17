#pragma once

// Basic PS1 SDK scalar types used by the copied C (Phase 1a). `u_long` is 32 bits here because the build is
// Windows x64 (LLP64), matching the original; copied code that stores pointers in it is a 64-bit hazard
// (PORTING_WATCHLIST.md §2).

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned short ushort;
typedef unsigned int uint;

typedef struct quad {
    long val[2];
} quad;

#define ONE 4096
#define ABS(x) (((x) >= 0) ? (x) : (-(x)))
