// Hand-maintained (tools/port/owned.csv).
#include "Render/TextureRegionDraw.h"

#include "Legacy/Psx.h"
#include "Render/LegacyPackets.h"
#include "Render/TextureAnimation.h"

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
                                int y, int texId, CVECTOR colors[]) {
  POLY_GT4* poly;
  void** p;

  if (colors[0].cd == 0) {
    return;
  }

  poly = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);

  SetRegionQuad(poly, &regions[texId], x, y);

  if (colors[0].cd < 8) {
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

  if (colors[0].cd < 8) {
    poly->clut = regions[texId].clut + 1;
    poly->tpage = regions[texId].tpage | 0x20;
  } else {
    poly->clut = regions[texId].clut;
    poly->tpage = regions[texId].tpage;
  }

  p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

  AddPrim(p[1] - 0x1C, poly++);

  p[0] = poly;
}

// decomp: renderTextures.h _renderTexturePopIn (src/renderTextures.h:60)
void Render_DrawTextureFlashFade(const Render_TextureRegion regions[], int x,
                                 int y, int texId, CVECTOR colors[]) {
  POLY_GT4* poly;
  void** p;

  if (colors[0].cd < 8) {
    Render_DrawTextureTintFade(regions, x, y, texId, colors);
    return;
  }

  poly = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);
  SetRegionQuad(poly, &regions[texId], x, y);

  if (colors[0].cd == 8) {
    setRGB0(poly, 192, 192, 192);
    setRGB1(poly, 192, 192, 192);
    setRGB2(poly, 192, 192, 192);
    setRGB3(poly, 192, 192, 192);
  } else if (colors[0].cd == 9) {
    setRGB0(poly, 224, 224, 224);
    setRGB1(poly, 224, 224, 224);
    setRGB2(poly, 224, 224, 224);
    setRGB3(poly, 224, 224, 224);
  } else if (colors[0].cd < 14) {
    int flash_blend_weight = colors[0].cd - 10;
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

  if (colors[0].cd < 10) {
    poly->clut = regions[texId].clut + getClut(16, 0);
    poly->tpage = regions[texId].tpage | getTPage(0, 1, 0, 0);
  } else {
    poly->clut = regions[texId].clut;
    poly->tpage = regions[texId].tpage;
  }

  p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

  AddPrim(p[1] - 0x1C, poly++);

  p[0] = poly;
}

// decomp: renderTextures.h _renderTextureWipe (src/renderTextures.h:130)
void Render_DrawTextureWipe(const Render_TextureRegion regions[], int x, int y,
                            int texId, CVECTOR colors[], int wipe_edge_x) {
  int strip_right_x;
  int strip_width_or_fade_weight;
  int i;
  char texture_u;
  POLY_GT4* poly;
  void** scratch;

  if ((x + regions[texId].w) < (wipe_edge_x - 64)) {
    scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    poly = scratch[0];

    SetRegionQuad(poly, &regions[texId], x, y);
    SetGradient(poly, colors);
    setSemiTrans(poly, 1);

    poly->clut = regions[texId].clut;
    poly->tpage = regions[texId].tpage;

    scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

    AddPrim(scratch[1] - 0x1C, poly++);

    scratch[0] = poly;
    return;
  }

  if (x < wipe_edge_x) {
    scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    poly = scratch[0];
    texture_u = regions[texId].x;

    for (i = 0; i < regions[texId].w; i += 12, x += 12, texture_u += 12) {
      strip_width_or_fade_weight = 12;

      if ((i + 12) >= regions[texId].w) {
        strip_width_or_fade_weight = regions[texId].w - i;
      }

      setPolyGT4(poly);
      strip_right_x = x + strip_width_or_fade_weight;
      setXY4(poly, x, y, strip_right_x, y, x, regions[texId].h + y,
             strip_right_x, regions[texId].h + y);
      setUV4(poly, texture_u, regions[texId].y,
             texture_u + strip_width_or_fade_weight, regions[texId].y,
             texture_u, regions[texId].y + regions[texId].h,
             texture_u + strip_width_or_fade_weight,
             regions[texId].y + regions[texId].h);

      strip_width_or_fade_weight = wipe_edge_x - x;

      if (strip_width_or_fade_weight > 64) {
        strip_width_or_fade_weight = 64;
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

      if (strip_width_or_fade_weight > 64) {
        strip_width_or_fade_weight = 64;
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

      poly->clut = (regions[texId].clut + 1);
      poly->tpage = (regions[texId].tpage | 0x20);

      scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

      AddPrim(scratch[1] - 0x1C, poly++);
    }

    scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    scratch[0] = poly;
  }
}
