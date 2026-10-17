#include "Legacy/Movie/LibPress.h"
#include "Core/Unported.h"

// Transitional shims (plan §2.2). Each records VS_UNPORTED until it translates into the modern system.

extern "C" {
int DecDCTvlc2(u_long* bs, u_long* buf, DECDCTTAB table)
{
    (void)bs;
    (void)buf;
    (void)table;
    VS_UNPORTED("libpress:DecDCTvlc2");
    return {};
}

void DecDCTvlcBuild(u_short* table)
{
    (void)table;
    VS_UNPORTED("libpress:DecDCTvlcBuild");
}

void DecDCTin(u_long* buf, int mode)
{
    (void)buf;
    (void)mode;
    VS_UNPORTED("libpress:DecDCTin");
}

void DecDCTout(u_long* buf, int size)
{
    (void)buf;
    (void)size;
    VS_UNPORTED("libpress:DecDCTout");
}

int DecDCToutCallback(void (*func)(void))
{
    (void)func;
    VS_UNPORTED("libpress:DecDCToutCallback");
    return {};
}

} // extern "C"
