#include "Legacy/Render/LibGpu.h"
#include "Core/Unported.h"

// Transitional shims (plan §2.2). Each records VS_UNPORTED until it translates into the modern system.

extern "C" {
void* LegacyGpu_NextPrim(void* p)
{
    (void)p;
    VS_UNPORTED("libgpu:LegacyGpu_NextPrim");
    return {};
}

DISPENV* PutDispEnv(DISPENV* env)
{
    (void)env;
    VS_UNPORTED("libgpu:PutDispEnv");
    return {};
}

DISPENV* SetDefDispEnv(DISPENV* env, int x, int y, int w, int h)
{
    *env = DISPENV{};
    setRECT(&env->disp, x, y, w, h);
    return env;
}

DRAWENV* PutDrawEnv(DRAWENV* env)
{
    (void)env;
    VS_UNPORTED("libgpu:PutDrawEnv");
    return {};
}

DRAWENV* SetDefDrawEnv(DRAWENV* env, int x, int y, int w, int h)
{
    *env = DRAWENV{};
    setRECT(&env->clip, x, y, w, h);
    env->ofs[0] = static_cast<short>(x);
    env->ofs[1] = static_cast<short>(y);
    env->tpage = static_cast<u_short>(getTPage(0, 0, 640, 0));
    env->dfe = 1;
    return env;
}

int ClearImage(RECT* rect, u_char r, u_char g, u_char b)
{
    (void)rect;
    (void)r;
    (void)g;
    (void)b;
    VS_UNPORTED("libgpu:ClearImage");
    return {};
}

int ClearImage2(RECT* rect, u_char r, u_char g, u_char b)
{
    (void)rect;
    (void)r;
    (void)g;
    (void)b;
    VS_UNPORTED("libgpu:ClearImage2");
    return {};
}

int LoadImage(RECT* rect, u_long* p)
{
    (void)rect;
    (void)p;
    VS_UNPORTED("libgpu:LoadImage");
    return {};
}

int LoadImage2(RECT* rect, u_long* p)
{
    (void)rect;
    (void)p;
    VS_UNPORTED("libgpu:LoadImage2");
    return {};
}

int StoreImage(RECT* rect, u_long* p)
{
    (void)rect;
    (void)p;
    VS_UNPORTED("libgpu:StoreImage");
    return {};
}

int MoveImage(RECT* rect, int x, int y)
{
    (void)rect;
    (void)x;
    (void)y;
    VS_UNPORTED("libgpu:MoveImage");
    return {};
}

u_short GetClut(int x, int y)
{
    return static_cast<u_short>(getClut(x, y));
}

u_short GetTPage(int tp, int abr, int x, int y)
{
    return static_cast<u_short>(getTPage(tp, abr, x, y));
}

u_long* ClearOTag(u_long* ot, int n)
{
    for (int i = 0; i < n; ++i) {
        ot[i] = 0;
    }
    VS_UNPORTED("libgpu:ClearOTag links");
    return ot;
}

u_long* ClearOTagR(u_long* ot, int n)
{
    for (int i = 0; i < n; ++i) {
        ot[i] = 0;
    }
    VS_UNPORTED("libgpu:ClearOTagR links");
    return ot;
}

void DrawOTag(u_long* p)
{
    (void)p;
    VS_UNPORTED("libgpu:DrawOTag");
}

void DrawPrim(void* p)
{
    (void)p;
    VS_UNPORTED("libgpu:DrawPrim");
}

void AddPrim(void* ot, void* p)
{
    (void)ot;
    (void)p;
    VS_UNPORTED("libgpu:AddPrim");
}

void AddPrims(void* ot, void* p0, void* p1)
{
    (void)ot;
    (void)p0;
    (void)p1;
    VS_UNPORTED("libgpu:AddPrims");
}

void SetDrawArea(DR_AREA* p, RECT* r)
{
    (void)p;
    (void)r;
    VS_UNPORTED("libgpu:SetDrawArea");
}

void SetDrawMode(DR_MODE* p, int dfe, int dtd, int tpage, RECT* tw)
{
    (void)p;
    (void)dfe;
    (void)dtd;
    (void)tpage;
    (void)tw;
    VS_UNPORTED("libgpu:SetDrawMode");
}

void SetDrawTPage(DR_TPAGE* p, int dfe, int dtd, int tpage)
{
    setlen(p, 1);
    p->code[0] = _get_mode(dfe, dtd, tpage);
}

void SetDrawMove(DR_MOVE* p, RECT* rect, int x, int y)
{
    (void)p;
    (void)rect;
    (void)x;
    (void)y;
    VS_UNPORTED("libgpu:SetDrawMove");
}

void SetLineF2(LINE_F2* p)
{
    setLineF2(p);
}

void SetPolyF4(POLY_F4* p)
{
    setPolyF4(p);
}

void SetPolyFT4(POLY_FT4* p)
{
    setPolyFT4(p);
}

void SetPolyG4(POLY_G4* p)
{
    setPolyG4(p);
}

void SetSemiTrans(void* p, int abe)
{
    setSemiTrans(p, abe);
}

void SetSprt(SPRT* p)
{
    setSprt(p);
}

void SetTile(TILE* p)
{
    setTile(p);
}

} // extern "C"
