#pragma once

// Transitional libgpu surface for the copied C (plan §2.2, strategy T).
// Primitive packets keep their PS1 layouts so the copied code that builds them
// is unchanged; the functions translate into Engine\Render
// (Legacy\Render\LibGpu.cpp). Ordering-table links store a 24-bit handle in
// each packet instead of an address: that replaces the "24-bit OT" hazard of
// plan Step 6 until the callers move to sort keys.
//
// The names keep their SDK spellings (SDK_API_LEDGER.md, the EC-01 register)
// inside VS::Legacy. <Windows.h> declares ::RECT and defines LoadImage as a
// macro, so a source that includes both names these qualified
// (VS::Legacy::RECT) rather than through using-declarations.

#include <cstdint>
#include <type_traits>

#include "Legacy/Core/PsxTypes.hpp"

namespace VS::Legacy {

// decomp: P_TAG (include/psx/libgpu.h:386)
struct P_TAG {
  std::uint32_t addr : 24;
  std::uint32_t len : 8;
  std::uint8_t r0, g0, b0, code;
};

// decomp: RECT (include/psx/libgpu.h:345)
struct RECT {
  std::int16_t x, y, w, h;
};

// decomp: DR_ENV (include/psx/libgpu.h:358)
struct DR_ENV {
  std::uint32_t tag;
  std::uint32_t code[15];
};

// decomp: DRAWENV (include/psx/libgpu.h:363)
struct DRAWENV {
  RECT clip;
  std::int16_t ofs[2];
  RECT tw;
  std::uint16_t tpage;
  std::uint8_t dtd, dfe, isbg, r0, g0, b0;
  DR_ENV dr_env;
};

// decomp: DISPENV (include/psx/libgpu.h:375)
struct DISPENV {
  RECT disp;
  RECT screen;
  std::uint8_t isinter, isrgb24, pad0, pad1;
};

// decomp: DR_AREA (include/psx/libgpu.h:615)
struct DR_AREA {
  std::uint32_t tag;
  std::uint32_t code[2];
};

// decomp: DR_MODE (include/psx/libgpu.h:605)
struct DR_MODE {
  std::uint32_t tag;
  std::uint32_t code[2];
};

// decomp: DR_MOVE (include/psx/libgpu.h:625)
struct DR_MOVE {
  std::uint32_t tag;
  std::uint32_t code[5];
};

// decomp: DR_STP (include/psx/libgpu.h:641)
struct DR_STP {
  std::uint32_t tag;
  std::uint32_t code[2];
};

// decomp: DR_TPAGE (include/psx/libgpu.h:636)
struct DR_TPAGE {
  std::uint32_t tag;
  std::uint32_t code[1];
};

// decomp: LINE_F2 (include/psx/libgpu.h:491)
struct LINE_F2 {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0, x1, y1;
};

// decomp: LINE_G2 (include/psx/libgpu.h:498)
struct LINE_G2 {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0;
  std::uint8_t r1, g1, b1, p1;
  std::int16_t x1, y1;
};

// decomp: LINE_G4 (include/psx/libgpu.h:536)
struct LINE_G4 {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0;
  std::uint8_t r1, g1, b1, p1;
  std::int16_t x1, y1;
  std::uint8_t r2, g2, b2, p2;
  std::int16_t x2, y2;
  std::uint8_t r3, g3, b3, p3;
  std::int16_t x3, y3;
  std::uint32_t pad;
};

// decomp: POLY_F3 (include/psx/libgpu.h:396)
struct POLY_F3 {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0, x1, y1, x2, y2;
};

// decomp: POLY_F4 (include/psx/libgpu.h:404)
struct POLY_F4 {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0, x1, y1, x2, y2, x3, y3;
};

// decomp: POLY_FT4 (include/psx/libgpu.h:424)
struct POLY_FT4 {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0;
  std::uint8_t u0, v0;
  std::uint16_t clut;
  std::int16_t x1, y1;
  std::uint8_t u1, v1;
  std::uint16_t tpage;
  std::int16_t x2, y2;
  std::uint8_t u2, v2;
  std::uint16_t pad1;
  std::int16_t x3, y3;
  std::uint8_t u3, v3;
  std::uint16_t pad2;
};

// decomp: POLY_G3 (include/psx/libgpu.h:437)
struct POLY_G3 {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0;
  std::uint8_t r1, g1, b1, pad1;
  std::int16_t x1, y1;
  std::uint8_t r2, g2, b2, pad2;
  std::int16_t x2, y2;
};

// decomp: POLY_G4 (include/psx/libgpu.h:447)
struct POLY_G4 {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0;
  std::uint8_t r1, g1, b1, pad1;
  std::int16_t x1, y1;
  std::uint8_t r2, g2, b2, pad2;
  std::int16_t x2, y2;
  std::uint8_t r3, g3, b3, pad3;
  std::int16_t x3, y3;
};

// decomp: POLY_GT3 (include/psx/libgpu.h:459)
struct POLY_GT3 {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0;
  std::uint8_t u0, v0;
  std::uint16_t clut;
  std::uint8_t r1, g1, b1, p1;
  std::int16_t x1, y1;
  std::uint8_t u1, v1;
  std::uint16_t tpage;
  std::uint8_t r2, g2, b2, p2;
  std::int16_t x2, y2;
  std::uint8_t u2, v2;
  std::uint16_t pad2;
};

// decomp: POLY_GT4 (include/psx/libgpu.h:472). Its first three vertices repeat
// POLY_GT3 by design (the PS1 packet layouts), which is not copy/paste to
// extract. jscpd:ignore-start
struct POLY_GT4 {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0;
  std::uint8_t u0, v0;
  std::uint16_t clut;
  std::uint8_t r1, g1, b1, p1;
  std::int16_t x1, y1;
  std::uint8_t u1, v1;
  std::uint16_t tpage;
  std::uint8_t r2, g2, b2, p2;
  std::int16_t x2, y2;
  std::uint8_t u2, v2;
  std::uint16_t pad2;
  std::uint8_t r3, g3, b3, p3;
  std::int16_t x3, y3;
  std::uint8_t u3, v3;
  std::uint16_t pad3;
};
// jscpd:ignore-end

// decomp: SPRT (include/psx/libgpu.h:552)
struct SPRT {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0;
  std::uint8_t u0, v0;
  std::uint16_t clut;
  std::int16_t w, h;
};

// decomp: SPRT_8 (include/psx/libgpu.h:567)
struct SPRT_8 {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0;
  std::uint8_t u0, v0;
  std::uint16_t clut;
};

// decomp: TILE (include/psx/libgpu.h:577)
struct TILE {
  std::uint32_t tag;
  std::uint8_t r0, g0, b0, code;
  std::int16_t x0, y0;
  std::int16_t w, h;
};

// decomp: TIM_IMAGE (include/psx/libgpu.h:684); native pointers, not a PS1
// layout
struct TIM_IMAGE {
  std::uint32_t mode;
  RECT* crect;
  std::uint32_t* caddr;
  RECT* prect;
  std::uint32_t* paddr;
};

// The PS1 packet sizes: the copied code steps through packet buffers by them.
static_assert(sizeof(P_TAG) == 8);
static_assert(sizeof(RECT) == 8);
static_assert(sizeof(DR_ENV) == 64);
static_assert(sizeof(DRAWENV) == 92);
static_assert(sizeof(DISPENV) == 20);
static_assert(sizeof(DR_AREA) == 12);
static_assert(sizeof(DR_MODE) == 12);
static_assert(sizeof(DR_MOVE) == 24);
static_assert(sizeof(DR_STP) == 12);
static_assert(sizeof(DR_TPAGE) == 8);
static_assert(sizeof(LINE_F2) == 16);
static_assert(sizeof(LINE_G2) == 20);
static_assert(sizeof(LINE_G4) == 40);
static_assert(sizeof(POLY_F3) == 20);
static_assert(sizeof(POLY_F4) == 24);
static_assert(sizeof(POLY_FT4) == 40);
static_assert(sizeof(POLY_G3) == 28);
static_assert(sizeof(POLY_G4) == 36);
static_assert(sizeof(POLY_GT3) == 40);
static_assert(sizeof(POLY_GT4) == 52);
static_assert(sizeof(SPRT) == 20);
static_assert(sizeof(SPRT_8) == 16);
static_assert(sizeof(TILE) == 16);

// ---- packet field setters (libgpu.h macros, as functions)
// Each store converts like the macro's assignment did, and each computed
// value keeps the integer type of the macro's expression.

namespace detail {
template <class Field, class Value>
constexpr void Store(Field& field, Value value) {
  field = static_cast<Field>(value);
}
}  // namespace detail

// decomp: setVector (include/psx/libgpu.h:82)
template <class V, class X, class Y, class Z>
constexpr void setVector(V* v, X x, Y y, Z z) {
  detail::Store(v->vx, x);
  detail::Store(v->vy, y);
  detail::Store(v->vz, z);
}

// decomp: setRECT (include/psx/libgpu.h:104)
template <class R, class X, class Y, class W, class H>
constexpr void setRECT(R* r, X x, Y y, W w, H h) {
  detail::Store(r->x, x);
  detail::Store(r->y, y);
  detail::Store(r->w, w);
  detail::Store(r->h, h);
}

// decomp: setRGB0 (include/psx/libgpu.h:119)
template <class P, class R, class G, class B>
constexpr void setRGB0(P* p, R r, G g, B b) {
  detail::Store(p->r0, r);
  detail::Store(p->g0, g);
  detail::Store(p->b0, b);
}

// decomp: setRGB1 (include/psx/libgpu.h:122)
template <class P, class R, class G, class B>
constexpr void setRGB1(P* p, R r, G g, B b) {
  detail::Store(p->r1, r);
  detail::Store(p->g1, g);
  detail::Store(p->b1, b);
}

// decomp: setRGB2 (include/psx/libgpu.h:125)
template <class P, class R, class G, class B>
constexpr void setRGB2(P* p, R r, G g, B b) {
  detail::Store(p->r2, r);
  detail::Store(p->g2, g);
  detail::Store(p->b2, b);
}

// decomp: setRGB3 (include/psx/libgpu.h:128)
template <class P, class R, class G, class B>
constexpr void setRGB3(P* p, R r, G g, B b) {
  detail::Store(p->r3, r);
  detail::Store(p->g3, g);
  detail::Store(p->b3, b);
}

// decomp: setXY0 (include/psx/libgpu.h:134)
template <class P, class X, class Y>
constexpr void setXY0(P* p, X x, Y y) {
  detail::Store(p->x0, x);
  detail::Store(p->y0, y);
}

// decomp: setXY2 (include/psx/libgpu.h:137)
template <class P, class X0, class Y0, class X1, class Y1>
constexpr void setXY2(P* p, X0 x0, Y0 y0, X1 x1, Y1 y1) {
  setXY0(p, x0, y0);
  detail::Store(p->x1, x1);
  detail::Store(p->y1, y1);
}

// decomp: setXY3 (include/psx/libgpu.h:141)
template <class P, class X0, class Y0, class X1, class Y1, class X2, class Y2>
constexpr void setXY3(P* p, X0 x0, Y0 y0, X1 x1, Y1 y1, X2 x2, Y2 y2) {
  setXY2(p, x0, y0, x1, y1);
  detail::Store(p->x2, x2);
  detail::Store(p->y2, y2);
}

// decomp: setXY4 (include/psx/libgpu.h:146)
template <class P, class X0, class Y0, class X1, class Y1, class X2, class Y2,
          class X3, class Y3>
constexpr void setXY4(P* p, X0 x0, Y0 y0, X1 x1, Y1 y1, X2 x2, Y2 y2, X3 x3,
                      Y3 y3) {
  setXY3(p, x0, y0, x1, y1, x2, y2);
  detail::Store(p->x3, x3);
  detail::Store(p->y3, y3);
}

// decomp: setWH (include/psx/libgpu.h:161)
template <class P, class W, class H>
constexpr void setWH(P* p, W w, H h) {
  detail::Store(p->w, w);
  detail::Store(p->h, h);
}

// decomp: setUV0 (include/psx/libgpu.h:166)
template <class P, class U, class V>
constexpr void setUV0(P* p, U u, V v) {
  detail::Store(p->u0, u);
  detail::Store(p->v0, v);
}

// decomp: setUV3 (include/psx/libgpu.h:169)
template <class P, class U0, class V0, class U1, class V1, class U2, class V2>
constexpr void setUV3(P* p, U0 u0, V0 v0, U1 u1, V1 v1, U2 u2, V2 v2) {
  setUV0(p, u0, v0);
  detail::Store(p->u1, u1);
  detail::Store(p->v1, v1);
  detail::Store(p->u2, u2);
  detail::Store(p->v2, v2);
}

// decomp: setUV4 (include/psx/libgpu.h:174)
template <class P, class U0, class V0, class U1, class V1, class U2, class V2,
          class U3, class V3>
constexpr void setUV4(P* p, U0 u0, V0 v0, U1 u1, V1 v1, U2 u2, V2 v2, U3 u3,
                      V3 v3) {
  setUV3(p, u0, v0, u1, v1, u2, v2);
  detail::Store(p->u3, u3);
  detail::Store(p->v3, v3);
}

// decomp: getTPage (include/psx/libgpu.h:259): texture mode, blend rate, then
// the page's x / 64 in bits 0-3 and y / 256 in bits 4 and 11.
template <class Tp, class Abr, class X, class Y>
constexpr auto getTPage(Tp tp, Abr abr, X x, Y y) {
  return ((tp & 0x3) << 7) | ((abr & 0x3) << 5) | ((y & 0x100) >> 4) |
         ((x & 0x3FF) >> 6) | ((y & 0x200) << 2);
}

// decomp: getClut (include/psx/libgpu.h:263): y in bits 6 and up, x / 16 in
// bits 0-5.
template <class X, class Y>
constexpr auto getClut(X x, Y y) {
  return (y << 6) | ((x >> 4) & 0x3F);
}

// decomp: setTPage (include/psx/libgpu.h:110)
template <class P, class Tp, class Abr, class X, class Y>
constexpr void setTPage(P* p, Tp tp, Abr abr, X x, Y y) {
  detail::Store(p->tpage, getTPage(tp, abr, x, y));
}

// decomp: setClut (include/psx/libgpu.h:113)
template <class P, class X, class Y>
constexpr void setClut(P* p, X x, Y y) {
  detail::Store(p->clut, getClut(x, y));
}

// decomp: _get_mode (include/psx/libgpu.h:275): the E1 draw-mode command word.
template <class Dfe, class Dtd, class TPage>
constexpr auto _get_mode(Dfe dfe, Dtd dtd, TPage tpage) {
  return 0xE1000000u | (dtd ? 0x200 : 0) | (dfe ? 0x400 : 0) | (tpage & 0x9FF);
}

// ---- packet header (length, command code, ordering-table link)
// A link is 24 bits, as on the PS1, but holds a handle from GpuHandle rather
// than an address, so packets anywhere in 64-bit memory can be linked.
// kGpuLinkEnd is the end of a list. Link words copied between packets
// (`*ot & 0xFFFFFF`, vs_getTag(type, *ot)) stay valid because they are
// already handles.
inline constexpr std::uint32_t kGpuLinkEnd = 0xFFFFFFu;

std::uint32_t GpuHandle(const void* p);
void* GpuPointer(std::uint32_t link);

// A link from a packet pointer, or from a link word copied from a packet.
template <class T>
inline std::uint32_t GpuLinkOf(T x) {
  if constexpr (std::is_pointer_v<T>) {
    return GpuHandle(x);
  } else {
    return static_cast<std::uint32_t>(x) & 0xFFFFFF;
  }
}

// decomp: setlen (include/psx/libgpu.h:236)
template <class P, class L>
void setlen(P* p, L len) {
  reinterpret_cast<P_TAG*>(p)->len = static_cast<std::uint8_t>(len);
}

// decomp: setaddr (include/psx/libgpu.h:237): takes a packet pointer or a
// copied link word.
template <class P, class A>
void setaddr(P* p, A addr) {
  reinterpret_cast<P_TAG*>(p)->addr = GpuLinkOf(addr);
}

// decomp: setcode (include/psx/libgpu.h:238)
template <class P, class C>
void setcode(P* p, C code) {
  reinterpret_cast<P_TAG*>(p)->code = static_cast<std::uint8_t>(code);
}

// decomp: getcode (include/psx/libgpu.h:241)
template <class P>
std::uint8_t getcode(const P* p) {
  return reinterpret_cast<const P_TAG*>(p)->code;
}

// decomp: getaddr (include/psx/libgpu.h:242)
template <class P>
std::uint32_t getaddr(const P* p) {
  return static_cast<std::uint32_t>(reinterpret_cast<const P_TAG*>(p)->addr);
}

// decomp: nextPrim (include/psx/libgpu.h:244)
template <class P>
void* nextPrim(const P* p) {
  return GpuPointer(getaddr(p));
}

// decomp: isendprim (include/psx/libgpu.h:245)
template <class P>
bool isendprim(const P* p) {
  return getaddr(p) == kGpuLinkEnd;
}

// decomp: addPrim (include/psx/libgpu.h:247): `p` takes the table entry's link,
// then the entry links to `p`.
template <class O, class P>
void addPrim(O* ot, P* p) {
  setaddr(p, getaddr(ot));
  setaddr(ot, p);
}

// decomp: catPrim (include/psx/libgpu.h:250)
template <class P0, class P1>
void catPrim(P0* p0, P1* p1) {
  setaddr(p0, p1);
}

// decomp: termPrim (include/psx/libgpu.h:251)
template <class P>
void termPrim(P* p) {
  reinterpret_cast<P_TAG*>(p)->addr = kGpuLinkEnd;
}

// decomp: setSemiTrans (include/psx/libgpu.h:253)
template <class P, class Abe>
void setSemiTrans(P* p, Abe abe) {
  if (abe) {
    setcode(p, getcode(p) | 0x02);
  } else {
    setcode(p, getcode(p) & ~0x02);
  }
}

// decomp: setShadeTex (include/psx/libgpu.h:256)
template <class P, class Tge>
void setShadeTex(P* p, Tge tge) {
  if (tge) {
    setcode(p, getcode(p) | 0x01);
  } else {
    setcode(p, getcode(p) & ~0x01);
  }
}

// decomp: setPolyF3 (include/psx/libgpu.h:316)
template <class P>
void setPolyF3(P* p) {
  setlen(p, 4);
  setcode(p, 0x20);
}

// decomp: setPolyF4 (include/psx/libgpu.h:320)
template <class P>
void setPolyF4(P* p) {
  setlen(p, 5);
  setcode(p, 0x28);
}

// decomp: setPolyFT4 (include/psx/libgpu.h:321)
template <class P>
void setPolyFT4(P* p) {
  setlen(p, 9);
  setcode(p, 0x2C);
}

// decomp: setPolyG3 (include/psx/libgpu.h:318)
template <class P>
void setPolyG3(P* p) {
  setlen(p, 6);
  setcode(p, 0x30);
}

// decomp: setPolyG4 (include/psx/libgpu.h:322)
template <class P>
void setPolyG4(P* p) {
  setlen(p, 8);
  setcode(p, 0x38);
}

// decomp: setPolyGT4 (include/psx/libgpu.h:323)
template <class P>
void setPolyGT4(P* p) {
  setlen(p, 12);
  setcode(p, 0x3C);
}

// decomp: setSprt (include/psx/libgpu.h:327)
template <class P>
void setSprt(P* p) {
  setlen(p, 4);
  setcode(p, 0x64);
}

// decomp: setSprt8 (include/psx/libgpu.h:325)
template <class P>
void setSprt8(P* p) {
  setlen(p, 3);
  setcode(p, 0x74);
}

// decomp: setTile (include/psx/libgpu.h:332)
template <class P>
void setTile(P* p) {
  setlen(p, 3);
  setcode(p, 0x60);
}

// decomp: setLineF2 (include/psx/libgpu.h:333)
template <class P>
void setLineF2(P* p) {
  setlen(p, 3);
  setcode(p, 0x40);
}

// decomp: setLineG2 (include/psx/libgpu.h:334)
template <class P>
void setLineG2(P* p) {
  setlen(p, 4);
  setcode(p, 0x50);
}

// decomp: setLineG4 (include/psx/libgpu.h:339): the polyline's terminator word
// and the unused vertex pads.
template <class P>
void setLineG4(P* p) {
  setlen(p, 9);
  setcode(p, 0x5C);
  p->pad = 0x55555555;
  p->p2 = 0;
  p->p3 = 0;
}

// Walks one ordering table / packet list from `p` to the end marker and hands
// each drawing packet to the renderer. Returns the number of packets visited.
int GpuWalkList(const void* p);

// ---- display, drawing environment and video memory
DISPENV* PutDispEnv(DISPENV* env);
DISPENV* SetDefDispEnv(DISPENV* env, int x, int y, int w, int h);
void SetDispMask(int mask);
DRAWENV* PutDrawEnv(DRAWENV* env);
DRAWENV* SetDefDrawEnv(DRAWENV* env, int x, int y, int w, int h);
int ClearImage(RECT* rect, std::uint8_t r, std::uint8_t g, std::uint8_t b);
int ClearImage2(RECT* rect, std::uint8_t r, std::uint8_t g, std::uint8_t b);
int LoadImage(RECT* rect, std::uint32_t* p);
int LoadImage2(RECT* rect, std::uint32_t* p);
int StoreImage(RECT* rect, std::uint32_t* p);
int MoveImage(RECT* rect, int x, int y);
std::uint16_t GetClut(int x, int y);
std::uint16_t GetTPage(int tp, int abr, int x, int y);

// ---- ordering tables and packets
std::uint32_t* ClearOTag(std::uint32_t* ot, int n);
std::uint32_t* ClearOTagR(std::uint32_t* ot, int n);
void DrawOTag(std::uint32_t* p);
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

}  // namespace VS::Legacy
