#pragma once

// libpress surface (plan §6.4, strategy T): the movie decoder calls, replaced by Engine\Movie.

#include "Legacy/Core/PsxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u_short DECDCTTAB[34816];

int DecDCTvlc2(u_long* bs, u_long* buf, DECDCTTAB table);
void DecDCTvlcBuild(u_short* table);
void DecDCTin(u_long* buf, int mode);
void DecDCTout(u_long* buf, int size);
int DecDCToutCallback(void (*func)(void));

#ifdef __cplusplus
}
#endif
