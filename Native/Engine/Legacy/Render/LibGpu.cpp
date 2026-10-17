#include "Legacy/Render/LibGpu.h"
#include "Core/Unported.h"

#include <cstdio>
#include <unordered_map>
#include <vector>

// Transitional shims (plan §2.2). Each records VS_UNPORTED until it translates into the modern system.

namespace {

// Ordering-table link handles. A packet keeps one handle for its address for the whole run: packet pools and heap
// blocks are reused frame after frame, so the table stays the size of the memory that ever held packets, far below
// the 2^24 a link can address. Handle 0 is never issued (a zeroed tag is not a link); 0xFFFFFF is the end marker.
struct LinkTable {
    std::unordered_map<const void*, u_long> handles;
    std::vector<const void*> pointers{nullptr};
};

LinkTable& Links()
{
    static LinkTable t;
    return t;
}

// Translation of drawing packets into Engine\Render happens here once the render queue exists; until then each
// primitive kind reached is recorded once, which lists the work the title slice needs.
void SubmitPacket(const P_TAG* p)
{
    if (p->len == 0) {
        return;  // an ordering-table entry, or an empty packet
    }
    char site[48];
    std::snprintf(site, sizeof site, "libgpu:packet code 0x%02X", p->code);
    VS_UNPORTED(site);
}

} // namespace

extern "C" {
u_long LegacyGpu_Handle(const void* p)
{
    LinkTable& t = Links();
    auto [it, added] = t.handles.try_emplace(p, static_cast<u_long>(t.pointers.size()));
    if (added) {
        if (t.pointers.size() >= LEGACY_GPU_LINK_END) {
            VS_UNPORTED("libgpu: more than 2^24 packet addresses linked");
            t.handles.erase(it);
            return LEGACY_GPU_LINK_END;
        }
        t.pointers.push_back(p);
    }
    return it->second;
}

void* LegacyGpu_Pointer(u_long link)
{
    link &= 0xFFFFFF;
    const LinkTable& t = Links();
    if (link == LEGACY_GPU_LINK_END || link == 0 || link >= t.pointers.size()) {
        return nullptr;
    }
    return const_cast<void*>(t.pointers[link]);
}

int LegacyGpu_WalkList(const void* p)
{
    int count = 0;
    // a list longer than every packet ever linked has a cycle
    for (size_t guard = Links().pointers.size() + 1; p != nullptr && guard != 0; --guard) {
        const P_TAG* tag = static_cast<const P_TAG*>(p);
        SubmitPacket(tag);
        ++count;
        p = tag->addr == LEGACY_GPU_LINK_END ? nullptr : LegacyGpu_Pointer(tag->addr);
    }
    return count;
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
    // forward list: each entry links to the next, the last ends the list
    for (int i = 0; i < n; ++i) {
        ot[i] = 0;
        if (i + 1 < n) {
            setaddr(&ot[i], &ot[i + 1]);
        } else {
            termPrim(&ot[i]);
        }
    }
    return ot;
}

u_long* ClearOTagR(u_long* ot, int n)
{
    // reverse list: each entry links to the previous, entry 0 ends the list; drawing starts at ot[n - 1]
    for (int i = 0; i < n; ++i) {
        ot[i] = 0;
        if (i > 0) {
            setaddr(&ot[i], &ot[i - 1]);
        } else {
            termPrim(&ot[i]);
        }
    }
    return ot;
}

void DrawOTag(u_long* p)
{
    LegacyGpu_WalkList(p);
}

void DrawPrim(void* p)
{
    // one packet, not a list: submit it alone
    const u_long saved = getaddr(p);
    termPrim(p);
    LegacyGpu_WalkList(p);
    ((P_TAG*)p)->addr = saved;
}

void AddPrim(void* ot, void* p)
{
    addPrim(ot, p);
}

void AddPrims(void* ot, void* p0, void* p1)
{
    // insert the already-linked run p0..p1 at the head of `ot`
    ((P_TAG*)p1)->addr = ((P_TAG*)ot)->addr;
    ((P_TAG*)ot)->addr = LegacyGpu_Handle(p0);
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
