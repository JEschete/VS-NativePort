#include "Render/PageSpace.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <stdexcept>

#include "Core/Log.hpp"

namespace VS::Render {

namespace {

constexpr i32 kDither[4][4] = {
    {-4, 0, -3, 1},
    {2, -2, 3, -1},
    {-3, 1, -4, 0},
    {3, -1, 2, -2}};  // NOLINT(whitespace/indent_namespace)

i32 Sext11(u32 v) { return static_cast<i32>(v << 21) >> 21; }

i32 Clamp(i32 v, i32 lo, i32 hi) { return v < lo ? lo : (v > hi ? hi : v); }

u32 Expand(u16 c) {
  const u32 r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
  return 0xFF000000u | ((b << 3 | b >> 2) << 16) | ((g << 3 | g >> 2) << 8) |
         (r << 3 | r >> 2);
}

}  // namespace

PageSpace::PageSpace() : m_px(static_cast<usize>(Width) * Height, 0) {}

std::expected<EffectPathTraceRange, std::string> EffectPathTraceRange::Parse(
    std::string_view text) {
  const auto colon = text.find(':');
  EffectPathTraceRange range{};
  const auto number = [](std::string_view value, u64& output) {
    const auto parsed =
        std::from_chars(value.data(), value.data() + value.size(), output);
    return parsed.ec == std::errc{} &&
           parsed.ptr == value.data() + value.size();
  };
  if (colon == std::string_view::npos ||
      !number(text.substr(0, colon), range.first) ||
      !number(text.substr(colon + 1), range.last) || range.first == 0 ||
      range.last < range.first) {
    return std::unexpected(
        "--trace-effect-paths requires first:last positive logical ticks");
  }
  return range;
}

void PageSpace::ConfigureEffectPathTrace(
    std::optional<EffectPathTraceRange> range) {
  if (range && (range->first == 0 || range->last < range->first)) {
    throw std::invalid_argument("Invalid effect-path trace range");
  }
  m_effectTraceRange = range;
  m_effectTrace = false;
}

void PageSpace::SetEffectPathTraceTick(u64 tick) {
  m_effectTrace = m_effectTraceRange && tick >= m_effectTraceRange->first &&
                  tick <= m_effectTraceRange->last;
  if (m_effectTrace) {
    VS_LOG_TRACE("effect-path", "{{\"kind\":\"frame\",\"logical_tick\":{}}}",
                 tick);
  }
}

void PageSpace::TracePrimitive(const u32* command,
                               std::span<const Vertex> vertices,
                               const Prim& primitive) const {
  if (!m_effectTrace || !primitive.semi ||
      !Core::IsLogEnabled(Core::LogLevel::Trace, "effect-path")) {
    return;
  }
  std::string points;
  for (const auto& vertex : vertices) {
    if (!points.empty()) points += ',';
    points += std::format(
        "{{\"screen\":[{},{}],\"raster\":[{},{}],"
        "\"uv\":[{},{}],\"rgb\":[{},{},{}]}}",
        vertex.x - m_offX, vertex.y - m_offY, vertex.x, vertex.y, vertex.u,
        vertex.v, vertex.r, vertex.g, vertex.b);
  }
  VS_LOG_TRACE(
      "effect-path",
      "{{\"kind\":\"primitive\",\"command_address\":{},\"opcode\":{},"
      "\"textured\":{},\"raw\":{},\"clut\":{},\"texture_page\":[{},{}],"
      "\"texture_depth\":{},\"blend\":{},\"draw_offset\":[{},{}],"
      "\"draw_area\":[{},{},{},{}],\"display\":[{},{},{},{}],"
      "\"display_lines\":[{},{}],"
      "\"vertices\":[{}]}}",
      reinterpret_cast<std::uintptr_t>(command), *command >> 24,
      primitive.textured, primitive.raw, primitive.clut, m_pageX, m_pageY,
      m_depth, m_semiMode, m_offX, m_offY, m_areaX1, m_areaY1, m_areaX2,
      m_areaY2, m_display.area.x, m_display.area.y, m_display.area.w,
      m_display.area.h, m_display.screenY, m_display.screenH, points);
}

PageSpace& GamePages() {
  static PageSpace pages;
  return pages;
}

// ------------------------------------------------------------------------------------------------
// transfers

void PageSpace::Fill(Rect r, u8 red, u8 green, u8 blue) {
  const u16 c =
      static_cast<u16>((red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10));
  for (i32 y = 0; y < r.h; ++y) {
    for (i32 x = 0; x < r.w; ++x) {
      m_px[((r.y + y) & (Height - 1)) * Width + ((r.x + x) & (Width - 1))] = c;
    }
  }
}

void PageSpace::Upload(Rect r, const u16* pixels) {
  for (i32 y = 0; y < r.h; ++y) {
    for (i32 x = 0; x < r.w; ++x) {
      u16& d =
          m_px[((r.y + y) & (Height - 1)) * Width + ((r.x + x) & (Width - 1))];
      if (m_maskCheck && (d & 0x8000)) {
        continue;
      }
      d = static_cast<u16>(pixels[y * r.w + x] | (m_maskSet ? 0x8000 : 0));
    }
  }
}

void PageSpace::Download(Rect r, u16* pixels) const {
  for (i32 y = 0; y < r.h; ++y) {
    for (i32 x = 0; x < r.w; ++x) {
      pixels[y * r.w + x] = At(r.x + x, r.y + y);
    }
  }
}

void PageSpace::Copy(Rect src, i32 dstX, i32 dstY) {
  std::vector<u16> tmp(static_cast<usize>(src.w) * src.h);
  Download(src, tmp.data());
  for (i32 y = 0; y < src.h; ++y) {
    for (i32 x = 0; x < src.w; ++x) {
      u16& d = m_px[((dstY + y) & (Height - 1)) * Width +
                    ((dstX + x) & (Width - 1))];
      if (m_maskCheck && (d & 0x8000)) {
        continue;
      }
      d = static_cast<u16>(tmp[y * src.w + x] | (m_maskSet ? 0x8000 : 0));
    }
  }
}

// ------------------------------------------------------------------------------------------------
// state

void PageSpace::SetDrawArea(i32 x1, i32 y1, i32 x2, i32 y2) {
  m_areaX1 = Clamp(x1, 0, Width - 1);
  m_areaY1 = Clamp(y1, 0, Height - 1);
  m_areaX2 = Clamp(x2, 0, Width - 1);
  m_areaY2 = Clamp(y2, 0, Height - 1);
}

void PageSpace::SetDrawOffset(i32 x, i32 y) {
  m_offX = x;
  m_offY = y;
}

void PageSpace::SetTexturePage(u16 tpage) {
  m_pageX = (tpage & 0xF) * 64;
  m_pageY = (tpage & 0x10) ? 256 : 0;
  m_semiMode = (tpage >> 5) & 3;
  m_depth = (tpage >> 7) & 3;
}

void PageSpace::SetTextureWindow(u32 w) {
  m_winMaskX = (w & 0x1F) * 8;
  m_winMaskY = ((w >> 5) & 0x1F) * 8;
  m_winOffX = ((w >> 10) & 0x1F) * 8;
  m_winOffY = ((w >> 15) & 0x1F) * 8;
}

// ------------------------------------------------------------------------------------------------
// commands

usize PageSpace::Execute(std::span<const u32> w) {
  if (w.empty()) {
    return 0;
  }
  const u32 cmd = w[0] >> 24;
  auto color = [](u32 word) { return word & 0xFFFFFF; };
  auto vertexAt = [&](u32 xy, u32 col) {
    return Vertex{Sext11(xy) + m_offX,
                  Sext11(xy >> 16) + m_offY,
                  static_cast<i32>(col & 0xFF),
                  static_cast<i32>((col >> 8) & 0xFF),
                  static_cast<i32>((col >> 16) & 0xFF),
                  0,
                  0};
  };

  if (cmd == 0x02 && w.size() >= 3) {  // fill
    const u32 c = color(w[0]);
    Fill(
        {static_cast<i32>(w[1] & 0x3F0), static_cast<i32>((w[1] >> 16) & 0x1FF),
         static_cast<i32>(((w[2] & 0x3FF) + 15) & ~15),
         static_cast<i32>((w[2] >> 16) & 0x1FF)},
        static_cast<u8>(c), static_cast<u8>(c >> 8), static_cast<u8>(c >> 16));
    return 3;
  }
  if (cmd >= 0x20 && cmd < 0x40) {  // polygon
    Prim p;
    p.gouraud = cmd & 0x10;
    const bool quad = cmd & 0x08;
    p.textured = cmd & 0x04;
    p.semi = cmd & 0x02;
    p.raw = cmd & 0x01;
    const int n = quad ? 4 : 3;
    const usize required =
        1 + n + (p.gouraud ? n - 1 : 0) + (p.textured ? n : 0);
    if (w.size() < required) {
      return 0;
    }
    Vertex v[4]{};
    usize i = 1;
    u32 col = color(w[0]);
    for (int k = 0; k < n; ++k) {
      if (p.gouraud && k > 0) {
        col = color(w[i++]);
      }
      if (i >= w.size()) {
        return w.size();
      }
      v[k] = vertexAt(w[i++], col);
      if (p.textured) {
        const u32 uv = w[i++];
        v[k].u = static_cast<i32>(uv & 0xFF);
        v[k].v = static_cast<i32>((uv >> 8) & 0xFF);
        if (k == 0) {
          p.clut = static_cast<u16>(uv >> 16);
        } else if (k == 1) {
          SetTexturePage(static_cast<u16>(uv >> 16));
        }
      }
    }
    TracePrimitive(w.data(), std::span(v, static_cast<usize>(n)), p);
    Triangle(v[0], v[1], v[2], p);
    if (quad) {
      Triangle(v[1], v[2], v[3], p);
    }
    return i;
  }
  if (cmd >= 0x40 && cmd < 0x60) {  // line / polyline
    Prim p;
    p.gouraud = cmd & 0x10;
    p.semi = cmd & 0x02;
    const bool poly = cmd & 0x08;
    if (w.size() < (p.gouraud ? 4u : 3u)) {
      return 0;
    }
    usize i = 1;
    u32 col = color(w[0]);
    Vertex prev = vertexAt(w[i++], col);
    for (;;) {
      if (i >= w.size()) {
        break;
      }
      if (poly && (w[i] & 0xF000F000) == 0x50005000) {
        ++i;
        break;
      }
      if (p.gouraud) {
        col = color(w[i++]);
      }
      if (i >= w.size()) {
        break;
      }
      Vertex next = vertexAt(w[i++], col);
      if (m_effectTrace && p.semi) {
        const std::array segment{prev, next};
        TracePrimitive(w.data(), segment, p);
      }
      Line(prev, next, p);
      prev = next;
      if (!poly) {
        break;
      }
    }
    return i;
  }
  if (cmd >= 0x60 && cmd < 0x80) {  // rectangle / sprite
    Prim p;
    p.textured = cmd & 0x04;
    p.semi = cmd & 0x02;
    p.raw = cmd & 0x01;
    const usize required =
        2 + (p.textured ? 1 : 0) + (((cmd >> 3) & 3) == 0 ? 1 : 0);
    if (w.size() < required) {
      return 0;
    }
    usize i = 1;
    const u32 col = color(w[0]);
    const u32 xy = w[i++];
    i32 u = 0, v = 0;
    if (p.textured) {
      const u32 uv = w[i++];
      u = static_cast<i32>(uv & 0xFF);
      v = static_cast<i32>((uv >> 8) & 0xFF);
      p.clut = static_cast<u16>(uv >> 16);
    }
    i32 rw = 1, rh = 1;
    switch ((cmd >> 3) & 3) {
      case 0:
        rw = static_cast<i32>(w[i] & 0x3FF),
        rh = static_cast<i32>((w[i] >> 16) & 0x1FF), ++i;
        break;
      case 1:
        rw = rh = 1;
        break;
      case 2:
        rw = rh = 8;
        break;
      default:
        rw = rh = 16;
        break;
    }
    const auto origin = vertexAt(xy, col);
    if (m_effectTrace && p.semi) {
      std::array<Vertex, 4> corners{origin, origin, origin, origin};
      corners[1].x += rw;
      corners[2].y += rh;
      corners[3].x += rw;
      corners[3].y += rh;
      for (usize corner = 0; corner < corners.size(); ++corner) {
        corners[corner].u = u + ((corner & 1) ? rw : 0);
        corners[corner].v = v + ((corner & 2) ? rh : 0);
      }
      TracePrimitive(w.data(), corners, p);
    }
    Rectangle(origin.x, origin.y, rw, rh, u, v, col, p);
    return i;
  }
  if (cmd == 0x80 && w.size() >= 4) {  // copy within the page space
    Copy(
        {static_cast<i32>(w[1] & 0x3FF), static_cast<i32>((w[1] >> 16) & 0x1FF),
         static_cast<i32>(((w[3] - 1) & 0x3FF) + 1),
         static_cast<i32>((((w[3] >> 16) - 1) & 0x1FF) + 1)},
        static_cast<i32>(w[2] & 0x3FF), static_cast<i32>((w[2] >> 16) & 0x1FF));
    return 4;
  }
  switch (cmd) {
    case 0xE1:
      SetTexturePage(static_cast<u16>(w[0] & 0x1FF));
      m_dither = (w[0] >> 9) & 1;
      return 1;
    case 0xE2:
      SetTextureWindow(w[0]);
      return 1;
    case 0xE3:
      m_areaX1 = static_cast<i32>(w[0] & 0x3FF);
      m_areaY1 = static_cast<i32>((w[0] >> 10) & 0x1FF);
      return 1;
    case 0xE4:
      m_areaX2 = static_cast<i32>(w[0] & 0x3FF);
      m_areaY2 = static_cast<i32>((w[0] >> 10) & 0x1FF);
      return 1;
    case 0xE5:
      m_offX = Sext11(w[0]), m_offY = Sext11(w[0] >> 11);
      return 1;
    case 0xE6:
      m_maskSet = w[0] & 1, m_maskCheck = (w[0] >> 1) & 1;
      return 1;
    case 0x00:
    case 0x01:
    default:
      return 1;  // no-op, cache clear, or unsupported command
  }
}

// ------------------------------------------------------------------------------------------------
// pixels

u16 PageSpace::Texel(i32 u, i32 v, u16 clut) const {
  u = (u & ~static_cast<i32>(m_winMaskX)) |
      static_cast<i32>(m_winOffX & m_winMaskX);
  v = (v & ~static_cast<i32>(m_winMaskY)) |
      static_cast<i32>(m_winOffY & m_winMaskY);
  u &= 0xFF;
  v &= 0xFF;
  const i32 cx = (clut & 0x3F) * 16, cy = (clut >> 6) & 0x1FF;
  switch (m_depth) {
    case 0: {
      const u16 word = At(m_pageX + u / 4, m_pageY + v);
      return At(cx + ((word >> ((u & 3) * 4)) & 0xF), cy);
    }
    case 1: {
      const u16 word = At(m_pageX + u / 2, m_pageY + v);
      return At(cx + ((word >> ((u & 1) * 8)) & 0xFF), cy);
    }
    default:
      return At(m_pageX + u, m_pageY + v);
  }
}

void PageSpace::Plot(i32 x, i32 y, i32 r, i32 g, i32 b, i32 u, i32 v,
                     const Prim& p, bool dither) {
  if (x < m_areaX1 || x > m_areaX2 || y < m_areaY1 || y > m_areaY2) {
    return;
  }
  u16& d = m_px[(y & (Height - 1)) * Width + (x & (Width - 1))];
  if (m_maskCheck && (d & 0x8000)) {
    return;
  }
  i32 rr, gg, bb;
  bool blend = p.semi;
  u16 top = 0;
  if (p.textured) {
    const u16 t = Texel(u, v, p.clut);
    if (t == 0) {
      return;  // fully transparent texel
    }
    blend = p.semi && (t & 0x8000);
    top = t & 0x8000;
    const i32 tr = (t & 31) << 3, tg = ((t >> 5) & 31) << 3,
              tb = ((t >> 10) & 31) << 3;
    if (p.raw) {
      rr = tr, gg = tg, bb = tb;
    } else {
      rr = (tr * r) >> 7, gg = (tg * g) >> 7, bb = (tb * b) >> 7;
    }
  } else {
    rr = r, gg = g, bb = b;
  }
  if (dither) {
    const i32 dv = kDither[y & 3][x & 3];
    rr += dv, gg += dv, bb += dv;
  }
  rr = Clamp(rr, 0, 255) >> 3, gg = Clamp(gg, 0, 255) >> 3,
  bb = Clamp(bb, 0, 255) >> 3;
  if (blend) {
    const i32 dr = d & 31, dg = (d >> 5) & 31, db = (d >> 10) & 31;
    switch (m_semiMode) {
      case 0:
        rr = (dr + rr) >> 1, gg = (dg + gg) >> 1, bb = (db + bb) >> 1;
        break;
      case 1:
        rr = std::min(31, dr + rr), gg = std::min(31, dg + gg),
        bb = std::min(31, db + bb);
        break;
      case 2:
        rr = std::max(0, dr - rr), gg = std::max(0, dg - gg),
        bb = std::max(0, db - bb);
        break;
      default:
        rr = std::min(31, dr + rr / 4), gg = std::min(31, dg + gg / 4),
        bb = std::min(31, db + bb / 4);
        break;
    }
  }
  d = static_cast<u16>(rr | (gg << 5) | (bb << 10) | top |
                       (m_maskSet ? 0x8000 : 0));
}

void PageSpace::Triangle(const Vertex& a, const Vertex& b, const Vertex& c,
                         const Prim& p) {
  // the GPU rejects polygons wider than 1023 or taller than 511
  const i32 minX = std::min({a.x, b.x, c.x}), maxX = std::max({a.x, b.x, c.x});
  const i32 minY = std::min({a.y, b.y, c.y}), maxY = std::max({a.y, b.y, c.y});
  if (maxX - minX >= 1024 || maxY - minY >= 512) {
    return;
  }
  const i64 area = static_cast<i64>(b.x - a.x) * (c.y - a.y) -
                   static_cast<i64>(b.y - a.y) * (c.x - a.x);
  if (area == 0) {
    return;
  }
  const Vertex* v0 = &a;
  const Vertex* v1 = &b;
  const Vertex* v2 = &c;
  i64 sarea = area;
  if (area < 0) {
    std::swap(v1, v2);
    sarea = -area;
  }
  auto edge = [](const Vertex* p0, const Vertex* p1, i32 x, i32 y) {
    return static_cast<i64>(p1->x - p0->x) * (y - p0->y) -
           static_cast<i64>(p1->y - p0->y) * (x - p0->x);
  };
  // top-left rule: pixels on a right or bottom edge are not drawn
  auto bias = [](const Vertex* p0, const Vertex* p1) {
    const bool topLeft = (p0->y == p1->y && p1->x > p0->x) || (p1->y < p0->y);
    return topLeft ? 0 : -1;
  };
  const i64 b0 = bias(v1, v2), b1 = bias(v2, v0), b2 = bias(v0, v1);
  const bool shade = p.gouraud;
  const bool dither = m_dither && (shade || (p.textured && !p.raw));
  const i32 x0 = std::max(minX, m_areaX1), x1 = std::min(maxX, m_areaX2);
  const i32 y0 = std::max(minY, m_areaY1), y1 = std::min(maxY, m_areaY2);
  for (i32 y = y0; y <= y1; ++y) {
    for (i32 x = x0; x <= x1; ++x) {
      const i64 w0 = edge(v1, v2, x, y), w1 = edge(v2, v0, x, y),
                w2 = edge(v0, v1, x, y);
      if (w0 + b0 < 0 || w1 + b1 < 0 || w2 + b2 < 0) {
        continue;
      }
      auto lerp = [&](i32 a0, i32 a1, i32 a2) {
        return static_cast<i32>((a0 * w0 + a1 * w1 + a2 * w2 + sarea / 2) /
                                sarea);
      };
      const i32 r = shade ? lerp(v0->r, v1->r, v2->r) : v0->r;
      const i32 g = shade ? lerp(v0->g, v1->g, v2->g) : v0->g;
      const i32 bl = shade ? lerp(v0->b, v1->b, v2->b) : v0->b;
      const i32 u = p.textured ? lerp(v0->u, v1->u, v2->u) : 0;
      const i32 v = p.textured ? lerp(v0->v, v1->v, v2->v) : 0;
      Plot(x, y, r, g, bl, u, v, p, dither);
    }
  }
}

void PageSpace::Rectangle(i32 x, i32 y, i32 w, i32 h, i32 u, i32 v, u32 color,
                          const Prim& p) {
  const i32 r = static_cast<i32>(color & 0xFF);
  const i32 g = static_cast<i32>((color >> 8) & 0xFF);
  const i32 b = static_cast<i32>((color >> 16) & 0xFF);
  const i32 xs = std::max(x, m_areaX1), xe = std::min(x + w - 1, m_areaX2);
  const i32 ys = std::max(y, m_areaY1), ye = std::min(y + h - 1, m_areaY2);
  for (i32 py = ys; py <= ye; ++py) {
    for (i32 px = xs; px <= xe; ++px) {
      Plot(px, py, r, g, b, u + (px - x), v + (py - y), p, false);
    }
  }
}

void PageSpace::Line(Vertex a, Vertex b, const Prim& p) {
  const i32 dx = b.x - a.x, dy = b.y - a.y;
  if (std::abs(dx) >= 1024 || std::abs(dy) >= 512) {
    return;
  }
  const i32 steps = std::max(std::abs(dx), std::abs(dy));
  const bool dither = m_dither && p.gouraud;
  for (i32 s = 0; s <= steps; ++s) {
    const i32 x =
        steps ? a.x + (dx * s + (dx >= 0 ? steps / 2 : -steps / 2)) / steps
              : a.x;
    const i32 y =
        steps ? a.y + (dy * s + (dy >= 0 ? steps / 2 : -steps / 2)) / steps
              : a.y;
    auto mix = [&](i32 c0, i32 c1) {
      return steps ? c0 + (c1 - c0) * s / steps : c0;
    };
    if (p.gouraud) {
      Plot(x, y, mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), 0, 0, p, dither);
    } else {
      Plot(x, y, a.r, a.g, a.b, 0, 0, p, false);
    }
  }
}

// ------------------------------------------------------------------------------------------------
// display

bool PageSpace::Present(std::vector<u32>& rgba, i32& width, i32& height) const {
  const Display& d = m_display;
  width = std::max(1, d.area.w);
  height = d.interlaced ? 480 : 240;
  rgba.assign(static_cast<usize>(width) * height, 0xFF000000u);
  if (!d.enabled) {
    return false;
  }
  const i32 scale = d.interlaced ? 2 : 1;
  const i32 lines = std::min(d.screenH * scale, std::max(0, d.area.h));
  for (i32 row = 0; row < lines; ++row) {
    const i32 outY = d.screenY * scale + row;
    if (outY < 0 || outY >= height) {
      continue;
    }
    const i32 srcY = d.area.y + row;
    u32* out = &rgba[static_cast<usize>(outY) * width];
    if (d.rgb24) {
      for (i32 x = 0; x < width; ++x) {
        const i32 byte = x * 3;
        auto byteAt = [&](i32 k) {
          const u16 word = At(d.area.x + k / 2, srcY);
          return static_cast<u32>((k & 1) ? word >> 8 : word & 0xFF);
        };
        out[x] = 0xFF000000u | (byteAt(byte + 2) << 16) |
                 (byteAt(byte + 1) << 8) | byteAt(byte);
      }
    } else {
      for (i32 x = 0; x < width; ++x) {
        out[x] = Expand(At(d.area.x + x, srcY));
      }
    }
  }
  return true;
}

}  // namespace VS::Render
