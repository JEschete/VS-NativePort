# Asset Pipeline: textures and images

[Asset pipeline index](../ASSET_PIPELINE.md)

### 6.4 Textures and images (TIM/GIM/FBT/FBC/BIN art → PNG + `.image.json`)

Modding first (D25): the PNG is what an image editor changes, so the PNG is authoritative for colour and transparency. The `<name>.image.json` sidecar holds only what PNG cannot carry. Conversion stays exact: PNG + sidecar reproduce the original 16-bit words, and the round trip (§7) proves it.

The strict TIM and native-image readers/types are in `VS.Content`; the TIM-to-image conversion, PNG/sidecar writers
and manifest writer are in `VS.Extract`. These writers are not part of the runtime Content library. The extractor
writes the native files once. The field-background runtime resolves the converted PNG/sidecar from the manifest and
uploads its pixels and palette; it does not parse the source TIM. Its FAR layout uses the typed JSON reader in
§6.4.1 ([BACKGROUNDS.md](BACKGROUNDS.md)).

Background material (#208): the original draws each FAR tile as an opaque Gouraud-shaded textured quad
(`setPolyGT4`, `2842C.c:497`) with CLUT (768, 238), in the farthest ordering slot over a black full-screen fill. Its
corner colours on the disc are neutral 128 (the texel unchanged) or black (an edge fade). There are no normals or
lights, and the only change to the texture while it is shown is the event script's palette transform.

TIMs inside other files: the shared strict decoder finds TIMs at 4-byte-aligned offsets in the first texture table
of `MAP\*.ZND` zones (texture pages and the CLUT rows the zone copies into the room palette, all 16-bit words), in
`SMALL\*.DIS` (4-bit with 16 palettes, and direct-colour 320 × 224 end screens) and in `ENDING\ENDING.PRG` (4- and
8-bit). Each container's converter adopts its TIMs: zones (§6.9, #517), DIS (#211), the ending program (#498).
`MENU\*.BIN`, `SYSTEM.DAT` and `TITLE.PRG` hold no raw TIMs, and `ENDING\ILLUST06-16.BIN` are compressed (#498).

**Effect FBC palettes (#215):** `VS.Extract images` reads each raw FBC as rows of 256 little-endian 16-bit colour
words and writes an editable direct-colour PNG (one row per CLUT row, in source order) plus the standard
`.image.json` (and `.stp.png` mask when STP is mixed) under `textures/effect/palettes/`. The manifest records the
source path and hash. A file must hold whole rows and at most the 5 rows of the effect CLUT area (VRAM rows
240-244); anything else is a conversion error. Rows that an effect's CLUT count names past the end of a short file
are not written. At runtime the effect loader resolves a CLUT's disc sector to its converted palette through the
manifest (`LoadLiveEffectPalette`), uploads the PNG's rows and zero-fills the rest of the slot (decision 8). It never
reads the FBC and has no disc fallback. The CLUT is refused and reported, while the effect's texture pages still
load, when its palette is missing, does not match its manifest entry or is not a 256-entry direct-colour image
(`ResourceRefusal::kPaletteMissing`), when it is empty or ends in a partial row (`kPaletteRows`), or when it has more
rows than its slot reaches by row 244 (`kPaletteSlot`). `DataStore::RequireConversions` refuses to start when any FBC
lacks a palette or its source hash is stale. An edit to the PNG is what the next load uploads.

**Effect palette variants and palette animation are not palette data.** Each texture descriptor in an effect's `.P`
texture tables (block type 1, 0x18-byte descriptors) carries a CLUT word that selects one effect CLUT row (column
768). Bit 15 of the CLUT word makes the draw semi-transparent, and the page word supplies the blend rule. A
texture-animation frame selects a list of descriptors, so frames that swap CLUT rows over the same texture region
animate the palette on the frame program's timing; no frame mixes rows over one region. No code rewrites the effect
CLUT area during playback (the decomp's only writer is the effect loader, `6E4CC.c`). So converted palette rows stay
one PNG per FBC; variants and animation remain descriptor and frame data for the `.P` migration.

Binding a package's texture tables checks each indexed descriptor against the palette rows its effect loaded
(`CheckEffectPaletteReferences`). A 4-bit (16-entry) or 8-bit (256-entry) descriptor must sample columns 768-1023 of
one of rows 240-244. The row must be filled by its effect's CLUT, which covers its slot through row 244, or be row
240, which holds the battle's system palette (SYSTEM.DAT's fixed image, uploaded by INITBTL.PRG to (768, 240) at
`18.c:311`). Direct-colour descriptors sample no palette and are skipped. A failing descriptor stops the bind before
anything is published: `BindLiveEffectTextureTables` logs the descriptor index, its CLUT word, the reason, the loaded
rows and the action effect, then aborts, as it does for a malformed texture block. E000.P is also BATTLE.PRG's
resident empty package and binds against the system palette alone. E252-E399 name E000.P's package offset and load
no CLUT; the runtime plays them without loading a package (`FinishPurgeOverlay`), so their descriptors are never
bound over another effect's palette.

**Effect FBT texture pages (#214):** an FBT is one whole VRAM texture page, 64 × 256 words (32,768 bytes).
`VS.Extract images` (`Extract/EffectTextureConversion.hpp`) binds each page to the one effect that loads it through
EFFPURGE.BIN's tables (each effect's FBT count, slots and file extents; extents count sectors from `E000.P`). The
page goes to texture page 24 + its slot (slots 0-7). Its depth comes from the descriptors in that effect's `.P`
texture block that sample its page. A 4-bit page becomes a 256 × 256 indexed PNG, an 8-bit page a 128 × 256 one, and
the indices are the page's bytes unchanged. A page no descriptor samples converts at 8 bits with a `note`. A page
bound to no effect or to two, sampled at two depths or as direct colour, sampled past its page's edge, or sampled
through a CLUT row its effect's FBC does not hold is a conversion error.

- The PNG's palette is a preview only: the effect FBC's most sampled row and first entry, the lowest on a tie. The
  sidecar names it in `provenance.preview_palette` (palette image id, row and entry); the runtime draws with the
  converted FBC, so colours are edited in the palette image. `provenance.texture_page` records the effect, slot,
  page and VRAM rectangle.
- Dependencies are the package (role `sampler`), the FBC (`preview palette`) and EFFPURGE.BIN (`binding`); a change
  to any of them reconverts the page. Pages with the same bytes at the same depth are converted once: the first by
  path holds the files and the others are manifest aliases (§8), so an edit to the shared PNG changes every effect
  that loads it.
- At runtime the effect loader finds a page's converted image by its sector through the manifest
  (`LoadLiveEffectTexture`), following an alias to its target, and uploads its packed words. It never reads the FBT
  and has no disc fallback. A page whose image is missing, stale or invalid (`ResourceRefusal::kTextureMissing`), or
  is not an indexed 64 × 256-word page (`kTextureSize`), is refused and reported, and its texture page is not
  reserved. `DataStore::RequireConversions` refuses to start when an FBT lacks its conversion or the page or a
  dependency has changed. The loader still allocates the 32 KB buffer the original read each page into, so the high
  heap sees the same allocations. A page loads in one poll; neither this nor the old disc queue matches the
  original's CD read time.

Image rules for every converter:

- **Pixels:** 4bpp and 8bpp images are **indexed PNGs at their true bit depth**. Indices are never re-quantized or remapped. 16bpp images are RGBA PNGs (24bpp, if any, RGB).
- **Colour:** 5-bit channels are expanded by bit replication (`c<<3 | c>>2`). Import takes the nearest 5-bit value (`(c*31 + 127) / 255`), which inverts replication exactly and rounds edited colours sensibly.
- **Transparency:** alpha **0** means the word `0x0000`, the colour the hardware never draws. Every other word is alpha **255**. Import maps alpha 0 to `0x0000` whatever its colour, and rejects partial alpha with the file and pixel or palette index. Opaque pure black is always `0x8000`, because `0x0000` would be transparent. (An image shown directly, such as a full-screen picture, displays `0x0000` as black; the material says which applies.)
- **STP (semi-transparency bit):** stored in the sidecar per palette entry, or per texel for 16bpp: `"none"`, `"all"`, a list of palette indices, or a 1-bit mask image `<name>.stp.png`. STP colours display opaque in editors; whether STP blends, and how, is the material's rule, not the texel's.
- **Palettes:** the indexed PNG's own palette is the image's default palette. An image with several palettes (CLUT variants, palette animation, effect colour swaps, blade materials) also has `<name>.palettes.png`: one palette per row, row 0 = the default, with the same colour and transparency rules and STP per row in the sidecar. The converter checks that the PNG's palette equals row 0 and reports a mismatch instead of picking one. An image whose palette lives elsewhere (ETM, zone textures) gets a preview palette from its default binding; the sidecar names the palette's source, and import ignores the preview.
- **Sidecar** (schema `vs.image`, version 1; `Content/ImageFiles.hpp`): `schema`, `schema_version`, the stable image `id`, `depth` (4, 8 or 16), `width` and `height`; for indexed images `palette` = `{entries, count, stp}` with one STP value per palette row; for 16bpp images `stp` = `"none"`, `"all"` or `"mask"` (then `<name>.stp.png`); `material` = `{zero_word, semi_transparency}` only when the consumer is known; `unreferenced` only when true; and `provenance` = source path, SHA-256 and byte offset, the TIM's id, flags and VRAM rectangles, an effect page's `texture_page` (`effect`, `slot`, `page`, `vram`) and `preview_palette` (`image`, `row`, `entry`), any `dependencies` (source, hash and role, such as the layout that fixes a background's depth) and a `note`. Unknown members are errors, so a misspelt field is reported rather than ignored. Unused palette entries keep their words.
- **Placement:** images that the original placed in a shared VRAM page are written as **separate** files. Their placement in VRAM is runtime layout, not content (plan §0.1). Any UV remapping is done by the converter and checked in the round-trip.
