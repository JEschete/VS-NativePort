# Asset Pipeline: text and fonts

[Asset pipeline index](../ASSET_PIPELINE.md)

## 5. Tier 2: text and fonts

- **Strings:** every string the game shows (exe, MENU `ITEMNAME/ITEMHELP`, `SMALL\HELP*.HF0/HF1`, `MCDATA`, event text in `EVT`, menu labels) is decoded from the VS character table to UTF-8 and stored once in `text\strings.en.json` as `{ "id": "text", ... }`.
- **IDs** follow the owner prefix: `text.item.blade.scramasax.name`, `text.event.0012.004`, `text.menu.title.new_game`. Code and catalogs refer to IDs only.
- **Inline control codes** (colour, wait, newline, name insertion, button glyphs) become a small markup language: `{color:red}`, `{wait:30}`, `{name:ashley}`, `{btn:circle}`, `\n`. The decoder and encoder are exact inverses, so the round-trip is byte-identical (§7).
- **Fonts (target):** glyph bitmaps become a PNG atlas with JSON metrics (codepoint → rect, advance). #500 must supply the VS character code → Unicode contract and bidirectional codec. The proposed `Native\Content\schemas\charmap.json` is **not present yet**; the bounded label decoder in §1.2 is not a substitute for that text conversion. Reference: `tools\PSX\VagrantStory\VagrantStory-Font`.
- **Localization (target):** Phase 2 adds `strings.<lang>.json` overlays. The complete string catalog/schema/codec must exist before claiming this structure is ready.

**Offline text codec (#500):** `Content/GameTextCodec.hpp` provides strict reversible English glyphs, escaped
literal braces (`{{`, `}}`), named symbol/button glyphs, newline, `{page}`, `{confirm}` and `{spacing:N}`.
Spacing uses the renderer's nominal units: -16..-1 encode F0..FF, while 0..239 keep their positive values.
E6 waits for controller confirmation; it is not a fixed-duration `{wait:30}`. The parameterized import vocabulary is:

| Markup | Original meaning |
| --- | --- |
| `{glyph-cost:N}` | F8: cost subtracted from the progressive reveal budget for each glyph (0 disables chunking), not a literal frame duration. |
| `{sound-selector:N}` | F9: selector in the text sound bank, played when the rendered chunk count changes; 0 is silent. |
| `{speaker-pause:N}` | FC: negate the speaker pause counter only when it was positive; the drawer subsequently advances it. This is not a whole-dialogue wait. |
| `{font:base}`, `{font:alternate}` | FB 6/5: select glyph bank 0/1. These describe bank identity, not an invented typeface/style. Font-asset identities remain a #500/#54 dependency. |
| `{justify:toggle}` | FB 4: toggle line centering. |
| `{down:N}` | FB high five bits: move down by N nominal font units. Its ignored low three bits live in `DecodedGameText::ignored_font_bits`, not authoring text. |
| `{palette:slot=N,bank=current}` | FB 0..3: select a slot while retaining the current palette bank. |
| `{palette:slot=3,bank=upper}` | FB 7: select upper-bank slot 3. Other forced-bank combinations have no such legacy encoding. |
| `{decimal:arg=N,digits=auto}`, `{hex:arg=N,digits=D}` | FE/FD: explicit integer argument slot and significant/fixed digit count. Both formats accept either digit mode. Original large-count arithmetic remains the import consumer's behavior, not a promise of arbitrary-width numeric formatting. |
| `{string:arg=N}` | FF: insert one of the ten bound string arguments. Argument roles must be named by each future catalog's call-site bindings, not inferred here. |

The mapping follows upstream `src/BATTLE/BATTLE.PRG/5BF94.c`: `vs_battle_printf` (991), `printVariableWidthFont`
(1253) and `printFixedWidthFont` (1644). The byte-sized limits apply only to offline legacy reconstruction; this
codec must not become a runtime string-table repacker or constrain native string/layout definitions.
Unimplemented controls and unmapped glyphs fail with the string identity and byte position.

The decoder reports the exact consumed prefix through E7. Callers keep the remaining bytes as import provenance,
not text; an all-zero fixed slot is an empty import record, not a fabricated terminated string. Informational
labels reuse the same glyph map while keeping visible unknown-code markers. `EncodeGameText(decoded, id)` preserves
the ignored font bits; the markup-only overload emits canonical zero bits. Invalid, duplicate, stale or unused
provenance offsets fail explicitly instead of changing unrelated text.

This codec is **not** the complete string catalog, font/charmap asset or ID-based runtime adoption. Extended
Japanese glyph pairs (`F6 EF`, the Japanese table's block glyph in the upstream decoder) occur in MCMAN
location-label records 77..104 except 89, and 117..136. They are not a new English control: the English codec
rejects them until their encoding/usage and placeholder status are reconciled.

### 5.1 Native file-picker prompt adoption (#54/#500)

`VS.Extract text <datadir>` (also full extraction) publishes the save import, font and image outputs
(`text.ui.save`, `font.ui.save`, `image.ui.save-font`) and contributes to the shared `text.catalog.en` output.
Unchanged sources keep authored files; `--force` replaces only this contributor's strings.

- `text\strings.en.json` includes native-owned status/notice/operation messages and two imported MCMAN choices,
  `text.save.choice.yes/no`. Only MCMAN entries **36 and 37** (halfword offsets **779 and 782**) are imported; their
  exact decoded markup/consumed extent stays in `text\save-text.import.json`, and leading spacing is a view-layout
  concern. The other MCMAN entries, including the unsupported Japanese location labels, and other string tables are
  not migrated.
- `text\save.font.json` names `font.ui.save` and the shared `image.ui.save-font` image. Its glyph keys are Unicode
  scalars or named symbols, with atlas-pixel rectangles, advances and line height. The offline importer reuses
  `GameTextCodec`'s glyph facts, not a second runtime byte map.
- `textures\ui\save-font.png` plus `.image.json` preserve the 4-bit first English bank of TITLE's G02022 font:
  **13,824 bytes at 0x46B68**, plus **32 palette bytes at 0x54A68**. The rest of G02022, other TITLE/module images,
  SYSTEM.DAT and the wider #498 image corpus remain unconverted.

Schemas are [strings.schema.json](../../Native/Content/schemas/strings.schema.json) and
[font.schema.json](../../Native/Content/schemas/font.schema.json). Runtime `NativeText` parses Unicode and typed
newline, named-button/symbol and `{value:role}` tokens; instant labels do **not** implement progressive dialogue
controls. Unknown controls, duplicate JSON members/IDs, invalid Unicode, absent references/glyphs and overflow fail
explicitly. Literal braces are `{{` and `}}`. Typed argument values are strings or signed 64-bit integers, not
byte-sized import slots. String arguments are never parsed as markup; external CR/LF/tab become spaces.

The `catalog-layers.json` ordered roots can supply partial `text\strings.en.json` overlays (same font, explicit
scale and string map). The data store validates into owned typed content before publication. Each native
file-picker entry owns its content/layout snapshot; cancel, completion, reset, reconfiguration and shutdown release
it. The file-picker policy emits semantic status IDs, never an encoded footer. Default No, backup/invalid-data
cautions and the existing frame-delayed operations remain. The Yes/No choices and status share the native
rendering path. Legacy room summaries, breadcrumb labels, button legend and artwork are outside this slice.

Layout is independent of Game font globals: authored scale 0.5–4, measured Latin/symbol word wrapping and explicit
newlines, scalar emergency wrapping, and explicit pixel-box overflow refusal (no truncation or silent shrink). The
file view admits only content that fits its 304×30 status box and 100×16 choice boxes; these are view constraints,
not 24/48-byte string limits. The unchanged button legend moves to the top margin, leaving two full status lines
inside the file screen's 224-pixel drawing area. The existing Caution bitmap is redrawn after the third card, which
formerly covered its top three rows; this preserves the heading and is not an image-conversion claim. This is not
full Unicode shaping/UAX #14 conformance or a WCAG claim. Design references:
[Unicode line breaking](https://www.unicode.org/reports/tr14/) and
[text resizing without loss](https://www.w3.org/TR/WCAG22/#resize-text).

The checked backend renders native glyph rectangles as scaled textured quads. It temporarily binds the owned PNG
to the existing page/palette and restores both immediately after drawing. GPU packet/page-image rendering is
explicit EC-01/EC-04 debt, **not** full font or compatibility retirement. Migrated prompts use no runtime
UTF-8-to-legacy string/container reconstruction.

Open work: #54, #500.

### 5.2 Shared catalog ownership and encyclopedia text

`TextContribution` is the single offline producer of `text\strings.en.json`. Manifest output `text.catalog.en`
(`text-catalog-json`) is its sole file owner; source-specific outputs do not also claim that file. The import-only
`text\strings.import.json` records contributor source hashes and owned IDs; it is not a second primary string
store. The save-only bootstrap upgrades the previous catalog without replacing authored values. Bestiary and save
imports preserve one another, including when one family is force-reimported. Unchanged semantic content preserves
the authored file's bytes and timestamp, including formatting; ID collisions and unsupported controls fail before
publication.

The encyclopedia contributes the 78 names, 78 descriptions and two native-owned locked-entry labels. Its names'
proven leading `{spacing:14}` is view indentation (the original detail view skips those two bytes). Only observed
inter-word `{spacing:6}` becomes a space, whose default native advance is six units; other spacing fails instead of
being guessed. Exact original markup and suffix bytes remain in the bestiary import provenance. The descriptions use
plain English glyphs and newline; no progressive control or runtime re-encoding is needed.

The native font and ordered text layers are shared. Gazette glyph layouts are copied into a frame-owned overlay and
submitted **after** the menu ordering table so queued backgrounds cannot cover them; reset drops that overlay. This
reuses the explicit page/packet backend debt; it is not variable-font palette/pixel parity or full font retirement.
