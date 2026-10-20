#pragma once

// Transitional libgpu surface for the copied C (plan §2.2, strategy T).
// Primitive packets keep their PS1 layouts so the copied code that builds them
// is unchanged; the functions translate into Engine\Render
// (Legacy\Render\LibGpu.cpp). Ordering-table links store a 24-bit address in
// each packet: on x64 that truncates pointers, which is the "24-bit OT" hazard
// of plan Step 6, replaced by sort keys when the callers are modernized.

#include "Legacy/Core/PsxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct P_TAG {
  unsigned addr : 24;
  unsigned len : 8;
  u_char r0, g0, b0, code;
} P_TAG;

typedef struct RECT {
  short x, y, w, h;
} RECT;

typedef struct DR_ENV {
  u_long tag;
  u_long code[15];
} DR_ENV;

typedef struct DRAWENV {
  RECT clip;
  short ofs[2];
  RECT tw;
  u_short tpage;
  u_char dtd, dfe, isbg, r0, g0, b0;
  DR_ENV dr_env;
} DRAWENV;

typedef struct DISPENV {
  RECT disp;
  RECT screen;
  u_char isinter, isrgb24, pad0, pad1;
} DISPENV;

typedef struct DR_AREA {
  u_long tag;
  u_long code[2];
} DR_AREA;
typedef struct DR_MODE {
  u_long tag;
  u_long code[2];
} DR_MODE;
typedef struct DR_MOVE {
  u_long tag;
  u_long code[5];
} DR_MOVE;
typedef struct DR_STP {
  u_long tag;
  u_long code[2];
} DR_STP;
typedef struct DR_TPAGE {
  u_long tag;
  u_long code[1];
} DR_TPAGE;

typedef struct LINE_F2 {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0, x1, y1;
} LINE_F2;

typedef struct LINE_G2 {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0;
  u_char r1, g1, b1, p1;
  short x1, y1;
} LINE_G2;

typedef struct LINE_G4 {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0;
  u_char r1, g1, b1, p1;
  short x1, y1;
  u_char r2, g2, b2, p2;
  short x2, y2;
  u_char r3, g3, b3, p3;
  short x3, y3;
  u_long pad;
} LINE_G4;

typedef struct POLY_F3 {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0, x1, y1, x2, y2;
} POLY_F3;

typedef struct POLY_F4 {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0, x1, y1, x2, y2, x3, y3;
} POLY_F4;

typedef struct POLY_FT4 {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0;
  u_char u0, v0;
  u_short clut;
  short x1, y1;
  u_char u1, v1;
  u_short tpage;
  short x2, y2;
  u_char u2, v2;
  u_short pad1;
  short x3, y3;
  u_char u3, v3;
  u_short pad2;
} POLY_FT4;

typedef struct POLY_G3 {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0;
  u_char r1, g1, b1, pad1;
  short x1, y1;
  u_char r2, g2, b2, pad2;
  short x2, y2;
} POLY_G3;

typedef struct POLY_G4 {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0;
  u_char r1, g1, b1, pad1;
  short x1, y1;
  u_char r2, g2, b2, pad2;
  short x2, y2;
  u_char r3, g3, b3, pad3;
  short x3, y3;
} POLY_G4;

typedef struct POLY_GT3 {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0;
  u_char u0, v0;
  u_short clut;
  u_char r1, g1, b1, p1;
  short x1, y1;
  u_char u1, v1;
  u_short tpage;
  u_char r2, g2, b2, p2;
  short x2, y2;
  u_char u2, v2;
  u_short pad2;
} POLY_GT3;

typedef struct POLY_GT4 {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0;
  u_char u0, v0;
  u_short clut;
  u_char r1, g1, b1, p1;
  short x1, y1;
  u_char u1, v1;
  u_short tpage;
  u_char r2, g2, b2, p2;
  short x2, y2;
  u_char u2, v2;
  u_short pad2;
  u_char r3, g3, b3, p3;
  short x3, y3;
  u_char u3, v3;
  u_short pad3;
} POLY_GT4;

typedef struct SPRT {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0;
  u_char u0, v0;
  u_short clut;
  short w, h;
} SPRT;

typedef struct SPRT_8 {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0;
  u_char u0, v0;
  u_short clut;
} SPRT_8;

typedef struct TILE {
  u_long tag;
  u_char r0, g0, b0, code;
  short x0, y0;
  short w, h;
} TILE;

typedef struct TIM_IMAGE {
  u_long mode;
  RECT* crect;
  u_long* caddr;
  RECT* prect;
  u_long* paddr;
} TIM_IMAGE;

// ---- packet field setters
#define setVector(v, _x, _y, _z) \
  ((v)->vx = (_x), (v)->vy = (_y), (v)->vz = (_z))
#define applyVector(v, _x, _y, _z, op) \
  ((v)->vx op(_x), (v)->vy op(_y), (v)->vz op(_z))
#define copyVector(v0, v1) \
  ((v0)->vx = (v1)->vx, (v0)->vy = (v1)->vy, (v0)->vz = (v1)->vz)
#define addVector(v0, v1) \
  ((v0)->vx += (v1)->vx, (v0)->vy += (v1)->vy, (v0)->vz += (v1)->vz)
#define setRECT(r, _x, _y, _w, _h) \
  ((r)->x = (_x), (r)->y = (_y), (r)->w = (_w), (r)->h = (_h))
#define setRGB0(p, r, g, b) ((p)->r0 = (r), (p)->g0 = (g), (p)->b0 = (b))
#define setRGB1(p, r, g, b) ((p)->r1 = (r), (p)->g1 = (g), (p)->b1 = (b))
#define setRGB2(p, r, g, b) ((p)->r2 = (r), (p)->g2 = (g), (p)->b2 = (b))
#define setRGB3(p, r, g, b) ((p)->r3 = (r), (p)->g3 = (g), (p)->b3 = (b))
#define setXY0(p, x, y) ((p)->x0 = (x), (p)->y0 = (y))
#define setXY2(p, a, b, c, d) \
  ((p)->x0 = (a), (p)->y0 = (b), (p)->x1 = (c), (p)->y1 = (d))
#define setXY3(p, a, b, c, d, e, f) \
  (setXY2(p, a, b, c, d), (p)->x2 = (e), (p)->y2 = (f))
#define setXY4(p, a, b, c, d, e, f, g, h) \
  (setXY3(p, a, b, c, d, e, f), (p)->x3 = (g), (p)->y3 = (h))
#define setWH(p, _w, _h) ((p)->w = (_w), (p)->h = (_h))
#define setUV0(p, u, v) ((p)->u0 = (u), (p)->v0 = (v))
// Google formatting aligns these expression continuations at an odd column.
#define setUV3(p, a, b, c, d, e, f)                                           \
  ((p)->u0 = (a), (p)->v0 = (b), (p)->u1 = (c), (p)->v1 = (d), (p)->u2 = (e), \
   (p)->v2 = (f))  // NOLINT(whitespace/indent)
#define setUV4(p, a, b, c, d, e, f, g, h) \
  (setUV3(p, a, b, c, d, e, f), (p)->u3 = (g), (p)->v3 = (h))
#define getTPage(tp, abr, x, y)                                        \
  ((((tp) & 0x3) << 7) | (((abr) & 0x3) << 5) | (((y) & 0x100) >> 4) | \
   (((x) & 0x3FF) >> 6) | (((y) & 0x200) << 2))  // NOLINT(whitespace/indent)
#define getClut(x, y) (((y) << 6) | (((x) >> 4) & 0x3F))
#define setTPage(p, tp, abr, x, y) ((p)->tpage = getTPage(tp, abr, x, y))
#define setClut(p, x, y) ((p)->clut = getClut(x, y))
#define _get_mode(dfe, dtd, tpage) \
  (0xE1000000u | ((dtd) ? 0x200 : 0) | ((dfe) ? 0x400 : 0) | ((tpage) & 0x9FF))

// ---- packet header (length, command code, ordering-table link)
// A link is 24 bits, as on the PS1, but holds a handle from LegacyGpu_Handle
// rather than an address, so packets anywhere in 64-bit memory can be linked.
// 0xFFFFFF is the end of a list. Link words copied between packets
// (`*ot & 0xFFFFFF`, vs_getTag(type, *ot)) stay valid because they are already
// handles.
#define LEGACY_GPU_LINK_END 0xFFFFFFu
u_long LegacyGpu_Handle(const void* p);
void* LegacyGpu_Pointer(u_long link);
static inline u_long LegacyGpu_LinkOfWord(u_long word) {
  return word & 0xFFFFFF;
}
static inline u_long LegacyGpu_LinkOfPointer(const void* p) {
  return LegacyGpu_Handle(p);
}
// setaddr takes either a packet pointer or a link word copied from another
// packet
#ifdef __cplusplus
#define LegacyGpu_Link(x) LegacyGpu_LinkOf(x)
#else
// C generic selection must distinguish built-in types, even at equal widths.
#define LegacyGpu_Link(x)                                                 \
  _Generic((x),                                                           \
      int: LegacyGpu_LinkOfWord,                                          \
      unsigned int: LegacyGpu_LinkOfWord,                                 \
      long: LegacyGpu_LinkOfWord,               /* NOLINT(runtime/int) */ \
      unsigned long: LegacyGpu_LinkOfWord,      /* NOLINT(runtime/int) */ \
      long long: LegacyGpu_LinkOfWord,          /* NOLINT(runtime/int) */ \
      unsigned long long: LegacyGpu_LinkOfWord, /* NOLINT(runtime/int) */ \
      default: LegacyGpu_LinkOfPointer)(x)
#endif
#define setlen(p, _len) (((P_TAG*)(p))->len = (u_char)(_len))
#define setaddr(p, _addr) (((P_TAG*)(p))->addr = LegacyGpu_Link(_addr))
#define setcode(p, _code) (((P_TAG*)(p))->code = (u_char)(_code))
#define getcode(p) ((u_char)(((P_TAG*)(p))->code))
#define getaddr(p) ((u_long)(((P_TAG*)(p))->addr))
#define nextPrim(p) LegacyGpu_Pointer(getaddr(p))
#define isendprim(p) (getaddr(p) == LEGACY_GPU_LINK_END)
#define addPrim(ot, p) (setaddr(p, getaddr(ot)), setaddr(ot, p))
#define catPrim(p0, p1) setaddr(p0, p1)
#define termPrim(p) (((P_TAG*)(p))->addr = LEGACY_GPU_LINK_END)
#define setSemiTrans(p, abe) \
  ((abe) ? setcode(p, getcode(p) | 0x02) : setcode(p, getcode(p) & ~0x02))
#define setShadeTex(p, tge) \
  ((tge) ? setcode(p, getcode(p) | 0x01) : setcode(p, getcode(p) & ~0x01))
#define setPolyF3(p) (setlen(p, 4), setcode(p, 0x20))
#define setPolyF4(p) (setlen(p, 5), setcode(p, 0x28))
#define setPolyFT4(p) (setlen(p, 9), setcode(p, 0x2C))
#define setPolyG3(p) (setlen(p, 6), setcode(p, 0x30))
#define setPolyG4(p) (setlen(p, 8), setcode(p, 0x38))
#define setPolyGT4(p) (setlen(p, 12), setcode(p, 0x3C))
#define setSprt(p) (setlen(p, 4), setcode(p, 0x64))
#define setSprt8(p) (setlen(p, 3), setcode(p, 0x74))
#define setTile(p) (setlen(p, 3), setcode(p, 0x60))
#define setLineF2(p) (setlen(p, 3), setcode(p, 0x40))
#define setLineG2(p) (setlen(p, 4), setcode(p, 0x50))
#define setLineG4(p)                                                   \
  (setlen(p, 9), setcode(p, 0x5C), (p)->pad = 0x55555555, (p)->p2 = 0, \
   (p)->p3 = 0)  // NOLINT(whitespace/indent)

// Walks one ordering table / packet list from `p` to the end marker and hands
// each drawing packet to the renderer. Returns the number of packets visited.
int LegacyGpu_WalkList(const void* p);

// ---- display, drawing environment and video memory
DISPENV* PutDispEnv(DISPENV* env);
DISPENV* SetDefDispEnv(DISPENV* env, int x, int y, int w, int h);
void SetDispMask(int mask);
DRAWENV* PutDrawEnv(DRAWENV* env);
DRAWENV* SetDefDrawEnv(DRAWENV* env, int x, int y, int w, int h);
int ClearImage(RECT* rect, u_char r, u_char g, u_char b);
int ClearImage2(RECT* rect, u_char r, u_char g, u_char b);
int LoadImage(RECT* rect, u_long* p);
int LoadImage2(RECT* rect, u_long* p);
int StoreImage(RECT* rect, u_long* p);
int MoveImage(RECT* rect, int x, int y);
u_short GetClut(int x, int y);
u_short GetTPage(int tp, int abr, int x, int y);

// ---- ordering tables and packets
u_long* ClearOTag(u_long* ot, int n);
u_long* ClearOTagR(u_long* ot, int n);
void DrawOTag(u_long* p);
void DrawPrim(void* p);
void AddPrim(void* ot, void* p);
void AddPrims(void* ot, void* p0, void* p1);
void SetDrawArea(DR_AREA* p, RECT* r);
void SetDrawMode(DR_MODE* p, int dfe, int dtd, int tpage, RECT* tw);
void SetDrawStp(DR_STP* packet, int enabled);
void SetDrawTPage(DR_TPAGE* p, int dfe, int dtd, int tpage);
void SetDrawMove(DR_MOVE* p, RECT* rect, int x, int y);
void SetLineF2(LINE_F2* p);
void SetPolyF4(POLY_F4* p);
void SetPolyFT4(POLY_FT4* p);
void SetPolyG4(POLY_G4* p);
void SetSemiTrans(void* p, int abe);
void SetSprt(SPRT* p);
void SetTile(TILE* p);

#ifdef __cplusplus
}

#include <type_traits>

template <class T>
inline u_long LegacyGpu_LinkOf(T x) {
  if constexpr (std::is_pointer_v<T>) {
    return LegacyGpu_Handle(x);
  } else {
    return static_cast<u_long>(x) & 0xFFFFFF;
  }
}
#endif
