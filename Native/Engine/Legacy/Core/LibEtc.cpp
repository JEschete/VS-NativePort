#include "Legacy/Core/LibEtc.h"
#include "Core/Unported.h"

// Transitional shims (plan §2.2). Each records VS_UNPORTED until it translates into the modern system.

extern "C" {
int VSync(int mode)
{
    (void)mode;
    VS_UNPORTED("libetc:VSync");
    return {};
}

int VSyncCallback(void (*f)(void))
{
    (void)f;
    VS_UNPORTED("libetc:VSyncCallback");
    return {};
}

long SetVideoMode(long mode)
{
    (void)mode;
    VS_UNPORTED("libetc:SetVideoMode");
    return {};
}

long SetRCnt(unsigned long spec, unsigned short target, long mode)
{
    (void)spec;
    (void)target;
    (void)mode;
    VS_UNPORTED("libetc:SetRCnt");
    return {};
}

long GetRCnt(unsigned long spec)
{
    (void)spec;
    VS_UNPORTED("libetc:GetRCnt");
    return {};
}

long StartRCnt(unsigned long spec)
{
    (void)spec;
    VS_UNPORTED("libetc:StartRCnt");
    return {};
}

long StopRCnt(unsigned long spec)
{
    (void)spec;
    VS_UNPORTED("libetc:StopRCnt");
    return {};
}

} // extern "C"
