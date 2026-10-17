#pragma once

// C entry points to Core::Random for the copied code (strategy R for libc rand/srand).
// Core_Random_Rand is the PS1 BIOS rand(): same LCG, same 15-bit output, seeded with 1 at boot.

#ifdef __cplusplus
extern "C" {
#endif

int Core_Random_Rand(void);
void Core_Random_Seed(unsigned int seed);

#ifdef __cplusplus
}
#endif
