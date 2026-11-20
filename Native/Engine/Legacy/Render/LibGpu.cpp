#include "Legacy/Render/LibGpu.hpp"

#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Core/Log.hpp"
#include "Core/Unported.hpp"
#include "Legacy/Psx.hpp"
#include "Platform/CrashHandler.hpp"
#include "Render/Frame.hpp"
#include "Render/PageSpace.hpp"

// Transitional libgpu shims (plan §2.2): each call acts on the page space
// (Render/PageSpace.hpp, owner decision "page-image") with the original
// semantics, so the copied code draws unchanged.

using VS::u16;
using VS::u32;
using VS::usize;
using VS::Render::GamePages;
using VS::Render::PageSpace;

namespace VS::Legacy {

namespace {

// Ordering-table link handles. A packet keeps one handle for its address for
// the whole run: packet pools and heap blocks are reused frame after frame, so
// the table stays the size of the memory that ever held packets, far below the
// 2^24 a link can address. Handle 0 is never issued (a zeroed tag is not a
// link); 0xFFFFFF is the end marker.
struct LinkTable {
  std::unordered_map<const void*, std::uint32_t> handles;
  std::vector<const void*> pointers{nullptr};
};

LinkTable& Links() {
  static LinkTable t;
  return t;
}

// A packet is its tag word followed by `len` command words, possibly several
// GPU commands back to back.
void SubmitPacket(const P_TAG* p) {
  const usize len = p->len;
  if (len == 0) {
    return;  // an ordering-table entry, or an empty packet
  }
  const std::span<const u32> words(reinterpret_cast<const u32*>(p) + 1, len);
  usize at = 0;
  while (at < words.size()) {
    const usize used = GamePages().Execute(words.subspan(at));
    if (used == 0) {
      VS::Core::LogError(
          "render",
          "truncated command 0x{:02X} in packet {} ({} words), offset "
          "{}; first=0x{:08X}",
          words[at] >> 24, static_cast<const void*>(p), len, at, words[0]);
      VS::Platform::LogAddress(p);
      throw std::runtime_error("Truncated render command");
    }
    at += used;
  }
}

PageSpace::Rect ToRect(const RECT* r) { return {r->x, r->y, r->w, r->h}; }

// E2 texture window payload from a RECT (mask and offset in 8-pixel units).
u32 TextureWindow(const RECT* tw) {
  if (tw == nullptr || tw->w == 0 || tw->h == 0) {
    return 0;
  }
  const u32 maskX = (static_cast<u32>(~(tw->w - 1)) & 0xFF) >> 3;
  const u32 maskY = (static_cast<u32>(~(tw->h - 1)) & 0xFF) >> 3;
  const u32 offX = (static_cast<u32>(tw->x) & 0xFF) >> 3;
  const u32 offY = (static_cast<u32>(tw->y) & 0xFF) >> 3;
  return maskX | (maskY << 5) | (offX << 10) | (offY << 15);
}

}  // namespace

std::uint32_t GpuHandle(const void* p) {
  LinkTable& t = Links();
  auto [it, added] =
      t.handles.try_emplace(p, static_cast<std::uint32_t>(t.pointers.size()));
  if (added) {
    if (t.pointers.size() >= kGpuLinkEnd) {
      VS::Core::RecordUnported(
          "libgpu: more than 2^24 packet addresses linked");
      t.handles.erase(it);
      return kGpuLinkEnd;
    }
    t.pointers.push_back(p);
  }
  return it->second;
}

void* GpuPointer(std::uint32_t link) {
  link &= 0xFFFFFF;
  const LinkTable& t = Links();
  if (link == kGpuLinkEnd || link == 0 || link >= t.pointers.size()) {
    return nullptr;
  }
  return const_cast<void*>(t.pointers[link]);
}

int GpuWalkList(const void* p) {
  int count = 0;
  const P_TAG* previous = nullptr;
  std::unordered_set<const void*> visited;
  for (; p != nullptr;) {
    const P_TAG* tag = static_cast<const P_TAG*>(p);
    if (!visited.insert(p).second) {
      VS::Core::LogError("render", "cyclic packet chain at {} after {} entries",
                         p, count);
      VS::Core::LogError(
          "render",
          "cycle len={} link=0x{:X} previous={} previous_len={} "
          "previous_link=0x{:X}; buffers={},{} tables={},{}",
          static_cast<unsigned>(tag->len), static_cast<unsigned>(tag->addr),
          static_cast<const void*>(previous),
          previous != nullptr ? static_cast<unsigned>(previous->len) : 0,
          previous != nullptr ? static_cast<unsigned>(previous->addr) : 0,
          VS::Render::legacyPrimitiveBuffers[0],
          VS::Render::legacyPrimitiveBuffers[1],
          VS::Render::legacyOrderingTables[0],
          VS::Render::legacyOrderingTables[1]);
      VS::Platform::LogAddress(p);
      if (previous != nullptr) VS::Platform::LogAddress(previous);
      return -1;
    }
    SubmitPacket(tag);
    ++count;
    previous = tag;
    p = tag->addr == kGpuLinkEnd ? nullptr : GpuPointer(tag->addr);
  }
  return count;
}

// ----------------------------------------------------------------------------------------------------
// display

DISPENV* PutDispEnv(DISPENV* env) {
  PageSpace::Display& d = GamePages().DisplayState();
  d.area = ToRect(&env->disp);
  d.screenY = env->screen.y;
  d.screenH = env->screen.h ? env->screen.h : 240;
  d.rgb24 = env->isrgb24 != 0;
  d.interlaced = env->isinter != 0;
  return env;
}

DISPENV* SetDefDispEnv(DISPENV* env, int x, int y, int w, int h) {
  *env = DISPENV{};
  setRECT(&env->disp, x, y, w, h);
  return env;
}

void SetDispMask(int mask) { GamePages().DisplayState().enabled = mask != 0; }

DRAWENV* PutDrawEnv(DRAWENV* env) {
  PageSpace& pages = GamePages();
  pages.SetDrawArea(env->clip.x, env->clip.y, env->clip.x + env->clip.w - 1,
                    env->clip.y + env->clip.h - 1);
  pages.SetDrawOffset(env->ofs[0], env->ofs[1]);
  pages.SetTextureWindow(TextureWindow(&env->tw));
  pages.SetTexturePage(env->tpage);
  pages.SetDither(env->dtd != 0);
  if (env->isbg) {
    pages.Fill(ToRect(&env->clip), env->r0, env->g0, env->b0);
  }
  return env;
}

DRAWENV* SetDefDrawEnv(DRAWENV* env, int x, int y, int w, int h) {
  *env = DRAWENV{};
  setRECT(&env->clip, x, y, w, h);
  env->ofs[0] = static_cast<short>(x);
  env->ofs[1] = static_cast<short>(y);
  env->tpage = static_cast<std::uint16_t>(getTPage(0, 0, 640, 0));
  env->dtd = 1;
  env->dfe = h < 289 ? 1 : 0;
  return env;
}

// ----------------------------------------------------------------------------------------------------
// transfers

int ClearImage(RECT* rect, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  GamePages().Fill(ToRect(rect), r, g, b);
  return 0;
}

int ClearImage2(RECT* rect, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  return ClearImage(rect, r, g, b);
}

int LoadImage(RECT* rect, std::uint32_t* p) {
  GamePages().Upload(ToRect(rect), reinterpret_cast<const u16*>(p));
  return 0;
}

int LoadImage2(RECT* rect, std::uint32_t* p) { return LoadImage(rect, p); }

int StoreImage(RECT* rect, std::uint32_t* p) {
  GamePages().Download(ToRect(rect), reinterpret_cast<u16*>(p));
  return 0;
}

int MoveImage(RECT* rect, int x, int y) {
  GamePages().Copy(ToRect(rect), x, y);
  return 0;
}

std::uint16_t GetClut(int x, int y) {
  return static_cast<std::uint16_t>(getClut(x, y));
}

std::uint16_t GetTPage(int tp, int abr, int x, int y) {
  return static_cast<std::uint16_t>(getTPage(tp, abr, x, y));
}

// ----------------------------------------------------------------------------------------------------
// lists

std::uint32_t* ClearOTag(std::uint32_t* ot, int n) {
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

std::uint32_t* ClearOTagR(std::uint32_t* ot, int n) {
  // reverse list: each entry links to the previous, entry 0 ends the list;
  // drawing starts at ot[n - 1]
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

void DrawOTag(std::uint32_t* p) { GpuWalkList(p); }

void DrawPrim(void* p) {
  // one packet, not a list
  SubmitPacket(static_cast<const P_TAG*>(p));
}

void AddPrim(void* ot, void* p) { addPrim(ot, p); }

void AddPrims(void* ot, void* p0, void* p1) {
  // insert the already-linked run p0..p1 at the head of `ot`
  ((P_TAG*)p1)->addr = ((P_TAG*)ot)->addr;
  ((P_TAG*)ot)->addr = GpuHandle(p0);
}

// ----------------------------------------------------------------------------------------------------
// packets

void SetDrawArea(DR_AREA* p, RECT* r) {
  setlen(p, 2);
  p->code[0] = 0xE3000000u | ((static_cast<u32>(r->y) & 0x3FF) << 10) |
               (static_cast<u32>(r->x) & 0x3FF);
  p->code[1] = 0xE4000000u |
               ((static_cast<u32>(r->y + r->h - 1) & 0x3FF) << 10) |
               (static_cast<u32>(r->x + r->w - 1) & 0x3FF);
}

void SetDrawMode(DR_MODE* p, int dfe, int dtd, int tpage, RECT* tw) {
  setlen(p, 2);
  p->code[0] = _get_mode(dfe, dtd, tpage);
  p->code[1] = 0xE2000000u | TextureWindow(tw);
}

void SetDrawStp(DR_STP* packet, int enabled) {
  setlen(packet, 2);
  packet->code[0] = 0xE6000000u | (enabled != 0 ? 1u : 0u);
  packet->code[1] = 0;
}

void SetDrawTPage(DR_TPAGE* p, int dfe, int dtd, int tpage) {
  setlen(p, 1);
  p->code[0] = _get_mode(dfe, dtd, tpage);
}

void SetDrawMove(DR_MOVE* p, RECT* rect, int x, int y) {
  // a GPU cache flush followed by a VRAM-to-VRAM copy
  setlen(p, 5);
  p->code[0] = 0x01000000u;
  p->code[1] = 0x80000000u;
  p->code[2] =
      (static_cast<u32>(rect->y) << 16) | (static_cast<u32>(rect->x) & 0xFFFF);
  p->code[3] = (static_cast<u32>(y) << 16) | (static_cast<u32>(x) & 0xFFFF);
  p->code[4] =
      (static_cast<u32>(rect->h) << 16) | (static_cast<u32>(rect->w) & 0xFFFF);
}

void SetLineF2(LINE_F2* p) { setLineF2(p); }
void SetPolyF4(POLY_F4* p) { setPolyF4(p); }
void SetPolyFT4(POLY_FT4* p) { setPolyFT4(p); }
void SetPolyG4(POLY_G4* p) { setPolyG4(p); }
void SetSemiTrans(void* p, int abe) { setSemiTrans(p, abe); }
void SetSprt(SPRT* p) { setSprt(p); }
void SetTile(TILE* p) { setTile(p); }

}  // namespace VS::Legacy
