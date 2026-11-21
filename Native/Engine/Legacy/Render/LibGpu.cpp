#include "Legacy/Render/LibGpu.hpp"

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Core/Log.hpp"
#include "Core/Unported.hpp"
#include "Legacy/Psx.hpp"
#include "Platform/CrashHandler.hpp"
#include "Render/DrawList.hpp"
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

// Ordering-table link handles. Outside the bound frame regions a packet keeps
// one handle for its address for the whole run: static packets and heap
// blocks are reused frame after frame, so the table stays the size of the
// memory that ever held them, far below the 2^23 handles below the frame-link
// flag. Handle 0 is never issued (a zeroed tag is not a link); 0xFFFFFF is
// the end marker.
struct LinkTable {
  std::unordered_map<const void*, std::uint32_t> handles;
  std::vector<const void*> pointers{nullptr};
};

LinkTable& Links() {
  static LinkTable t;
  return t;
}

// A tag word's link is its low 24 bits; the top byte is the packet length.
constexpr std::uint32_t kLinkMask = 0xFFFFFF;
static_assert(kGpuLinkEnd == kLinkMask);

// Frame-relative links (LibGpu.hpp): bit 23, a 6-bit generation, a 2-bit
// region and a 15-bit word offset.
constexpr std::uint32_t kFrameLinkFlag = 0x800000;
constexpr unsigned kFrameLinkGenerationShift = 17;
constexpr std::uint32_t kFrameLinkGenerationMask = 0x3F;
constexpr unsigned kFrameLinkRegionShift = 15;
constexpr std::uint32_t kFrameLinkRegionMask = 0x3;
constexpr std::uint32_t kFrameLinkWordMask = 0x7FFF;
// Generation 63 would turn the last word of region 3 into kGpuLinkEnd.
constexpr std::uint8_t kFrameLinkGenerations = 63;
constexpr std::size_t kLinkWordBytes = sizeof(std::uint32_t);
// Each frame buffer owns a primitive region and an ordering-table region.
constexpr std::size_t kFrameLinkBuffers = kGpuLinkRegionCount / 2;
static_assert(kGpuLinkRegionMaxBytes / kLinkWordBytes ==
              kFrameLinkWordMask + 1);

struct FrameLinkRegion {
  std::byte* base = nullptr;
  std::size_t bytes = 0;
  std::uint8_t generation = 0;
};

std::array<FrameLinkRegion, kGpuLinkRegionCount>& FrameRegions() {
  static std::array<FrameLinkRegion, kGpuLinkRegionCount> regions;
  return regions;
}

void AdvanceGeneration(FrameLinkRegion& region) {
  region.generation = static_cast<std::uint8_t>((region.generation + 1) %
                                                kFrameLinkGenerations);
}

bool Contains(const FrameLinkRegion& region, const std::byte* p) {
  return region.base != nullptr && !std::less<>{}(p, region.base) &&
         std::less<>{}(p, region.base + region.bytes);
}

// The frame link for `p`, when it lies in a bound region. A packet that is
// not word-aligned cannot be a link target on the GPU; it is reported and
// keeps a run-lifetime handle.
std::optional<std::uint32_t> FrameLink(const void* p) {
  const auto* byte = static_cast<const std::byte*>(p);
  const auto& regions = FrameRegions();
  for (std::size_t index = 0; index < regions.size(); ++index) {
    const FrameLinkRegion& region = regions[index];
    if (!Contains(region, byte)) {
      continue;
    }
    const auto offset = static_cast<std::size_t>(byte - region.base);
    if (offset % kLinkWordBytes != 0) {
      VS::Core::LogError("render",
                         "link to {} is not word-aligned in frame link region "
                         "{}; it keeps a run-lifetime handle",
                         p, index);
      return std::nullopt;
    }
    return kFrameLinkFlag |
           (std::uint32_t{region.generation} << kFrameLinkGenerationShift) |
           (static_cast<std::uint32_t>(index) << kFrameLinkRegionShift) |
           static_cast<std::uint32_t>(offset / kLinkWordBytes);
  }
  return std::nullopt;
}

// The memory a frame link names, or nullptr when it is stale.
void* FramePointer(std::uint32_t link) {
  const FrameLinkRegion& region =
      FrameRegions()[(link >> kFrameLinkRegionShift) & kFrameLinkRegionMask];
  const std::size_t offset = (link & kFrameLinkWordMask) * kLinkWordBytes;
  if (region.base == nullptr ||
      ((link >> kFrameLinkGenerationShift) & kFrameLinkGenerationMask) !=
          region.generation ||
      offset >= region.bytes) {
    return nullptr;
  }
  return std::span(region.base, region.bytes).subspan(offset).data();
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
  if (const auto frame_link = FrameLink(p)) {
    return *frame_link;
  }
  LinkTable& t = Links();
  auto [it, added] =
      t.handles.try_emplace(p, static_cast<std::uint32_t>(t.pointers.size()));
  if (added) {
    if (t.pointers.size() >= kFrameLinkFlag) {
      VS::Core::RecordUnported(
          "libgpu: more than 2^23 packet addresses linked");
      t.handles.erase(it);
      return kGpuLinkEnd;
    }
    t.pointers.push_back(p);
  }
  return it->second;
}

void* GpuPointer(std::uint32_t link) {
  link &= kLinkMask;
  if (link == kGpuLinkEnd) {
    return nullptr;
  }
  if ((link & kFrameLinkFlag) != 0) {
    return FramePointer(link);
  }
  const LinkTable& t = Links();
  if (link == 0 || link >= t.pointers.size()) {
    return nullptr;
  }
  return const_cast<void*>(t.pointers[link]);
}

GpuLinkState GpuLinkStatus(std::uint32_t link) {
  link &= kLinkMask;
  if (link == kGpuLinkEnd) {
    return GpuLinkState::kEnd;
  }
  if (GpuPointer(link) != nullptr) {
    return GpuLinkState::kLive;
  }
  return (link & kFrameLinkFlag) != 0 ? GpuLinkState::kStale
                                      : GpuLinkState::kUnknown;
}

bool BindGpuLinkRegion(GpuLinkRegion region, std::span<std::byte> memory) {
  auto& regions = FrameRegions();
  const auto index = static_cast<std::size_t>(region);
  if (index >= regions.size()) {
    VS::Core::LogError("render", "frame link region {} does not exist", index);
    return false;
  }
  FrameLinkRegion& bound = regions.at(index);
  AdvanceGeneration(bound);
  bound.base = nullptr;
  bound.bytes = 0;
  if (memory.empty()) {
    return true;
  }
  if (memory.size() > kGpuLinkRegionMaxBytes) {
    VS::Core::LogError("render",
                       "frame link region {} of {} bytes exceeds the {} a link "
                       "addresses; it stays released",
                       index, memory.size(), kGpuLinkRegionMaxBytes);
    return false;
  }
  for (std::size_t other = 0; other < regions.size(); ++other) {
    const FrameLinkRegion& held = regions.at(other);
    if (held.base != nullptr &&
        std::less<>{}(memory.data(), held.base + held.bytes) &&
        std::less<>{}(held.base, memory.data() + memory.size())) {
      VS::Core::LogError("render",
                         "frame link region {} at {} overlaps bound region "
                         "{}; it stays released",
                         index, static_cast<const void*>(memory.data()), other);
      return false;
    }
  }
  bound.base = memory.data();
  bound.bytes = memory.size();
  return true;
}

void BeginGpuLinkFrame(std::size_t buffer) {
  if (buffer >= kFrameLinkBuffers) {
    VS::Core::LogError("render", "frame buffer {} has no link regions", buffer);
    return;
  }
  auto& regions = FrameRegions();
  AdvanceGeneration(regions.at(
      static_cast<std::size_t>(GpuLinkRegion::kPrimitives0) + buffer));
  AdvanceGeneration(regions.at(
      static_cast<std::size_t>(GpuLinkRegion::kOrderingTable0) + buffer));
}

// Typed draw items (Render/DrawList.hpp) are drawn as the walk follows the
// link their slot entry held when they were submitted, where the equivalent
// packet would have been.
int GpuWalkList(const void* p) {
  int count = 0;
  const P_TAG* previous = nullptr;
  std::unordered_set<const void*> visited;
  VS::Render::FrameDrawList& items = VS::Render::GameFrameDrawList();
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
    const std::uint32_t link = tag->addr;
    items.DrawBefore(link, visited);
    if (link == kGpuLinkEnd) {
      break;
    }
    p = GpuPointer(link);
    if (p == nullptr && GpuLinkStatus(link) == GpuLinkState::kStale) {
      VS::Core::LogError("render",
                         "stale frame link 0x{:06X} in packet {} after {} "
                         "entries: its frame buffer began a later frame",
                         link, static_cast<const void*>(tag), count);
      VS::Platform::LogAddress(tag);
      return -1;
    }
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
  if (n > 0) {
    VS::Render::GameFrameDrawList().DiscardEntries(ot, static_cast<usize>(n));
  }
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
  if (n > 0) {
    VS::Render::GameFrameDrawList().DiscardEntries(ot, static_cast<usize>(n));
  }
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
