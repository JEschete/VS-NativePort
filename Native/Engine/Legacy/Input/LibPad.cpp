#include "Legacy/Input/LibPad.h"
#include "Core/Unported.h"

// Transitional shims (plan §2.2). Each records VS_UNPORTED until it translates into the modern system.

extern "C" {
void PadInitDirect(unsigned char* port1, unsigned char* port2)
{
    (void)port1;
    (void)port2;
    VS_UNPORTED("libpad:PadInitDirect");
}

void PadStartCom(void)
{
    VS_UNPORTED("libpad:PadStartCom");
}

int PadGetState(int port)
{
    (void)port;
    VS_UNPORTED("libpad:PadGetState");
    return {};
}

int PadInfoMode(int port, int term, int offs)
{
    (void)port;
    (void)term;
    (void)offs;
    VS_UNPORTED("libpad:PadInfoMode");
    return {};
}

int PadSetActAlign(int port, unsigned char* table)
{
    (void)port;
    (void)table;
    VS_UNPORTED("libpad:PadSetActAlign");
    return {};
}

int PadSetMainMode(int port, int offs, int lock)
{
    (void)port;
    (void)offs;
    (void)lock;
    VS_UNPORTED("libpad:PadSetMainMode");
    return {};
}

void PadSetAct(int port, unsigned char* table, int len)
{
    (void)port;
    (void)table;
    (void)len;
    VS_UNPORTED("libpad:PadSetAct");
}

} // extern "C"
