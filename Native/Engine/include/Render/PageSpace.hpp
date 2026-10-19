#pragma once

#include <array>
#include <span>
#include <vector>

#include "Core/Types.hpp"

namespace VS::Render {

// The PS1 game addresses every image, palette and frame by coordinates in one
// 1024x512 space of 16-bit pixels (owner decision 2026-10-17: one page image,
// plan §6.2). Uploads, readbacks, copies and drawing all act on it with the
// original semantics, so the copied game code runs unchanged; the host shows
// the display rectangle. Drawing commands are the GPU packet words the game
// builds (GP0 command set).
class PageSpace {
 public:
  static constexpr i32 Width = 1024;
  static constexpr i32 Height = 512;

  struct Rect {
    i32 x = 0, y = 0, w = 0, h = 0;
  };

  struct Display {
    Rect area{0, 0, 320,
              240};     // source rectangle in the page space (w in pixels)
    i32 screenY = 0;    // first TV line shown
    i32 screenH = 240;  // TV lines shown
    bool rgb24 = false;
    bool interlaced = false;
    bool enabled = false;
  };

  PageSpace();

  // ---- transfers
  void Fill(Rect r, u8 red, u8 green,
            u8 blue);  // ignores draw area and mask, like the GPU fill command
  void Upload(Rect r, const u16* pixels);
  void Download(Rect r, u16* pixels) const;
  void Copy(Rect src, i32 dstX, i32 dstY);

  // ---- drawing state (DRAWENV and the E1-E6 commands)
  void SetDrawArea(i32 x1, i32 y1, i32 x2, i32 y2);  // inclusive
  void SetDrawOffset(i32 x, i32 y);
  void SetTexturePage(u16 tpage);  // E1 low bits / polygon tpage field
  void SetDither(bool on) { m_dither = on; }
  void SetTextureWindow(u32 word);  // E2 payload
  void SetMask(bool set, bool check) {
    m_maskSet = set;
    m_maskCheck = check;
  }

  // Executes one packet's command words (the words after the ordering-table
  // tag). Returns words consumed.
  usize Execute(std::span<const u32> words);

  // ---- display
  Display& DisplayState() { return m_display; }
  const Display& DisplayState() const { return m_display; }
  // Converts what the TV shows to RGBA8 (0xAABBGGRR). Returns false when the
  // display is blanked.
  bool Present(std::vector<u32>& rgba, i32& width, i32& height) const;

  u16 At(i32 x, i32 y) const {
    return m_px[(y & (Height - 1)) * Width + (x & (Width - 1))];
  }
  std::span<const u16> Pixels() const { return m_px; }

 private:
  struct Vertex {
    i32 x, y;
    i32 r, g, b;
    i32 u, v;
  };
  struct Prim {
    bool textured = false, gouraud = false, semi = false, raw = false;
    u16 clut = 0;
  };

  void Triangle(const Vertex& a, const Vertex& b, const Vertex& c,
                const Prim& p);
  void Rectangle(i32 x, i32 y, i32 w, i32 h, i32 u, i32 v, u32 color,
                 const Prim& p);
  void Line(Vertex a, Vertex b, const Prim& p);
  void Plot(i32 x, i32 y, i32 r, i32 g, i32 b, i32 u, i32 v, const Prim& p,
            bool dither);
  u16 Texel(i32 u, i32 v, u16 clut) const;

  std::vector<u16> m_px;
  i32 m_areaX1 = 0, m_areaY1 = 0, m_areaX2 = 1023, m_areaY2 = 511;
  i32 m_offX = 0, m_offY = 0;
  i32 m_pageX = 0, m_pageY = 0, m_semiMode = 0, m_depth = 0;
  bool m_dither = false;
  bool m_maskSet = false, m_maskCheck = false;
  u32 m_winMaskX = 0, m_winMaskY = 0, m_winOffX = 0, m_winOffY = 0;
  Display m_display;
};

// The page space the copied game draws into (libgpu shims) and the host
// presents.
PageSpace& GamePages();

}  // namespace VS::Render
