// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Render/TextureRegionDraw.h"

#include "Legacy/Psx.h"
#include "Render/LegacyPackets.h"
#include "Render/TextureAnimation.h"

enum {
  kFadeSteps = 8,          // the tint fade's last partial step
  kFlashLightStep = 8,     // the flash starts at light grey
  kFlashBrightStep = 9,    // then brighter grey
  kFlashBlendStep = 10,    // then blends down to the region's colors
  kFlashBlendEnd = 14,     // where the colors are fully restored
  kWipeStripWidth = 12,    // texels per wipe strip
  kWipeFeather = 64,       // pixels over which the wipe's edge fades
  kTpageAdditive = 0x20,   // tpage ABR bits while a region fades in
  kOrderingOffset = 0x1C,  // the ordering-table depth of these primitives
};

static void SetRegionQuad(POLY_GT4* poly, const Render_TextureRegion* region,
                          int x, int y) {
  setPolyGT4(poly);
  setXY4(poly, x, y, region->w + x, y, x, region->h + y, region->w + x,
         region->h + y);
  setUV4(poly, region->x, region->y, region->x + region->w, region->y,
         region->x, region->y + region->h, region->x + region->w,
         region->y + region->h);
}

static void SetGradient(POLY_GT4* poly, const CVECTOR colors[]) {
  setRGB0(poly, colors[0].r, colors[0].g, colors[0].b);
  setRGB1(poly, colors[1].r, colors[1].g, colors[1].b);
  setRGB2(poly, colors[0].r, colors[0].g, colors[0].b);
  setRGB3(poly, colors[1].r, colors[1].g, colors[1].b);
}

// decomp: renderTextures.h _renderTextureFadeInTint (src/renderTextures.h:3)
void Render_DrawTextureTintFade(const Render_TextureRegion regions[], int x,
                                int y, int tex_id, CVECTOR colors[]) {
  POLY_GT4* poly;
  void** p;

  if (colors[0].cd == 0) {
    return;
  }

  poly = reinterpret_cast<POLY_GT4*>(
      *reinterpret_cast<void**>((uintptr_t)Core_ScratchBase() + 0x000));

  SetRegionQuad(poly, &regions[tex_id], x, y);

  if (colors[0].cd < kFadeSteps) {
    setRGB0(poly, (colors[0].r * colors[0].cd) / 8,
            (colors[0].g * colors[0].cd) / 8, (colors[0].b * colors[0].cd) / 8);
    setRGB1(poly, (colors[1].r * colors[0].cd) / 8,
            (colors[1].g * colors[0].cd) / 8, (colors[1].b * colors[0].cd) / 8);
    setRGB2(poly, (colors[0].r * colors[0].cd) / 8,
            (colors[0].g * colors[0].cd) / 8, (colors[0].b * colors[0].cd) / 8);
    setRGB3(poly, (colors[1].r * colors[0].cd) / 8,
            (colors[1].g * colors[0].cd) / 8, (colors[1].b * colors[0].cd) / 8);
  } else {
    SetGradient(poly, colors);
  }

  setSemiTrans(poly, 1);

  if (colors[0].cd < kFadeSteps) {
    poly->clut = regions[tex_id].clut + 1;
    poly->tpage = regions[tex_id].tpage | kTpageAdditive;
  } else {
    poly->clut = regions[tex_id].clut;
    poly->tpage = regions[tex_id].tpage;
  }

  p = reinterpret_cast<void**>((uintptr_t)Core_ScratchBase() + 0x000);

  AddPrim((reinterpret_cast<u_char*>(p[1])) - kOrderingOffset, poly++);

  p[0] = poly;
}

// decomp: renderTextures.h _renderTexturePopIn (src/renderTextures.h:60)
void Render_DrawTextureFlashFade(const Render_TextureRegion regions[], int x,
                                 int y, int tex_id, CVECTOR colors[]) {
  POLY_GT4* poly;
  void** p;

  if (colors[0].cd < kFadeSteps) {
    Render_DrawTextureTintFade(regions, x, y, tex_id, colors);
    return;
  }

  poly = reinterpret_cast<POLY_GT4*>(
      *reinterpret_cast<void**>((uintptr_t)Core_ScratchBase() + 0x000));
  SetRegionQuad(poly, &regions[tex_id], x, y);

  if (colors[0].cd == kFlashLightStep) {
    setRGB0(poly, 192, 192, 192);
    setRGB1(poly, 192, 192, 192);
    setRGB2(poly, 192, 192, 192);
    setRGB3(poly, 192, 192, 192);
  } else if (colors[0].cd == kFlashBrightStep) {
    setRGB0(poly, 224, 224, 224);
    setRGB1(poly, 224, 224, 224);
    setRGB2(poly, 224, 224, 224);
    setRGB3(poly, 224, 224, 224);
  } else if (colors[0].cd < kFlashBlendEnd) {
    int flash_blend_weight = colors[0].cd - kFlashBlendStep;
    setRGB0(poly, Render_BlendFlashComponent(colors[0].r, flash_blend_weight),
            Render_BlendFlashComponent(colors[0].g, flash_blend_weight),
            Render_BlendFlashComponent(colors[0].b, flash_blend_weight));
    setRGB1(poly, Render_BlendFlashComponent(colors[1].r, flash_blend_weight),
            Render_BlendFlashComponent(colors[1].g, flash_blend_weight),
            Render_BlendFlashComponent(colors[1].b, flash_blend_weight));
    setRGB2(poly, Render_BlendFlashComponent(colors[0].r, flash_blend_weight),
            Render_BlendFlashComponent(colors[0].g, flash_blend_weight),
            Render_BlendFlashComponent(colors[0].b, flash_blend_weight));
    setRGB3(poly, Render_BlendFlashComponent(colors[1].r, flash_blend_weight),
            Render_BlendFlashComponent(colors[1].g, flash_blend_weight),
            Render_BlendFlashComponent(colors[1].b, flash_blend_weight));
  } else {
    SetGradient(poly, colors);
  }

  setSemiTrans(poly, 1);

  if (colors[0].cd < kFlashBlendStep) {
    poly->clut = regions[tex_id].clut + getClut(16, 0);
    poly->tpage = regions[tex_id].tpage | getTPage(0, 1, 0, 0);
  } else {
    poly->clut = regions[tex_id].clut;
    poly->tpage = regions[tex_id].tpage;
  }

  p = reinterpret_cast<void**>((uintptr_t)Core_ScratchBase() + 0x000);

  AddPrim((reinterpret_cast<u_char*>(p[1])) - kOrderingOffset, poly++);

  p[0] = poly;
}

// decomp: renderTextures.h _renderTextureWipe (src/renderTextures.h:130)
void Render_DrawTextureWipe(const Render_TextureRegion regions[], int x, int y,
                            int tex_id, CVECTOR colors[], int wipe_edge_x) {
  int strip_right_x;
  int strip_width_or_fade_weight;
  int i;
  char texture_u;
  POLY_GT4* poly;
  void** scratch;

  if ((x + regions[tex_id].w) < (wipe_edge_x - kWipeFeather)) {
    scratch = reinterpret_cast<void**>((uintptr_t)Core_ScratchBase() + 0x000);
    poly = reinterpret_cast<POLY_GT4*>(scratch[0]);

    SetRegionQuad(poly, &regions[tex_id], x, y);
    SetGradient(poly, colors);
    setSemiTrans(poly, 1);

    poly->clut = regions[tex_id].clut;
    poly->tpage = regions[tex_id].tpage;

    scratch = reinterpret_cast<void**>((uintptr_t)Core_ScratchBase() + 0x000);

    AddPrim((reinterpret_cast<u_char*>(scratch[1])) - kOrderingOffset, poly++);

    scratch[0] = poly;
    return;
  }

  if (x < wipe_edge_x) {
    scratch = reinterpret_cast<void**>((uintptr_t)Core_ScratchBase() + 0x000);
    poly = reinterpret_cast<POLY_GT4*>(scratch[0]);
    texture_u = regions[tex_id].x;

    for (i = 0; i < regions[tex_id].w; i += kWipeStripWidth,
        x += kWipeStripWidth, texture_u += kWipeStripWidth) {
      strip_width_or_fade_weight = kWipeStripWidth;

      if ((i + kWipeStripWidth) >= regions[tex_id].w) {
        strip_width_or_fade_weight = regions[tex_id].w - i;
      }

      setPolyGT4(poly);
      strip_right_x = x + strip_width_or_fade_weight;
      setXY4(poly, x, y, strip_right_x, y, x, regions[tex_id].h + y,
             strip_right_x, regions[tex_id].h + y);
      setUV4(poly, texture_u, regions[tex_id].y,
             texture_u + strip_width_or_fade_weight, regions[tex_id].y,
             texture_u, regions[tex_id].y + regions[tex_id].h,
             texture_u + strip_width_or_fade_weight,
             regions[tex_id].y + regions[tex_id].h);

      strip_width_or_fade_weight = wipe_edge_x - x;

      if (strip_width_or_fade_weight > kWipeFeather) {
        strip_width_or_fade_weight = kWipeFeather;
      }

      if (strip_width_or_fade_weight < 0) {
        strip_width_or_fade_weight = 0;
      }

      setRGB0(poly, (colors[0].r * strip_width_or_fade_weight) / 64,
              (colors[0].g * strip_width_or_fade_weight) / 64,
              (colors[0].b * strip_width_or_fade_weight) / 64);
      setRGB2(poly, (colors[0].r * strip_width_or_fade_weight) / 64,
              (colors[0].g * strip_width_or_fade_weight) / 64,
              (colors[0].b * strip_width_or_fade_weight) / 64);

      strip_width_or_fade_weight = wipe_edge_x - strip_right_x;

      if (strip_width_or_fade_weight > kWipeFeather) {
        strip_width_or_fade_weight = kWipeFeather;
      }

      if (strip_width_or_fade_weight < 0) {
        strip_width_or_fade_weight = 0;
      }

      setRGB1(poly, (colors[1].r * strip_width_or_fade_weight) / 64,
              (colors[1].g * strip_width_or_fade_weight) / 64,
              (colors[1].b * strip_width_or_fade_weight) / 64);
      setRGB3(poly, (colors[1].r * strip_width_or_fade_weight) / 64,
              (colors[1].g * strip_width_or_fade_weight) / 64,
              (colors[1].b * strip_width_or_fade_weight) / 64);

      setSemiTrans(poly, 1);

      poly->clut = (regions[tex_id].clut + 1);
      poly->tpage = (regions[tex_id].tpage | kTpageAdditive);

      scratch = reinterpret_cast<void**>((uintptr_t)Core_ScratchBase() + 0x000);

      AddPrim((reinterpret_cast<u_char*>(scratch[1])) - kOrderingOffset,
              poly++);
    }

    scratch = reinterpret_cast<void**>((uintptr_t)Core_ScratchBase() + 0x000);
    scratch[0] = poly;
  }
}
}  // extern "C"
