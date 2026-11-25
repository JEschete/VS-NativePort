# Asset Pipeline: FAR background layouts

[Asset pipeline index](../ASSET_PIPELINE.md)

Background images follow the image rules of §6.4 ([IMAGES.md](IMAGES.md)).

### 6.4.1 FAR background layouts

**Bounded slice (#210):** `VS.Extract backgrounds <datadir> [--force]` writes
`backgrounds\<artwork-slug>.background.json`, using `far-layout-json` version 2 and the committed
[background-layout.schema.json](../../Native/Content/schemas/background-layout.schema.json) version 2. Full
extraction runs it after images. The four layouts:

| Import provenance | Layout ID | Image ID | Grid | MPD references |
| --- | --- | --- | --- | --- |
| `001OP01A` | `background.castle-silhouette` | `image.bg.castle-silhouette` | 8×4 | MAP001, MAP215 |
| `002OP01A` | `background.dark-clouds` | `image.bg.dark-clouds` | 16×4 | MAP002, MAP216 |
| `007OP01A` | `background.sun-on-horizon` | `image.bg.sun-on-horizon` | 8×12 | MAP007 |
| `008OP01A` | `background.grey-clouds` | `image.bg.grey-clouds` | 8×12 | MAP008 |

The IDs describe the verified artwork, **not exact room names or a claimed sunrise/sunset**. The image PNG/sidecar
stems match the artwork slugs. Original `background.import.bg.<stem>` and `image.bg.<stem>` identities are explicit
manifest compatibility aliases, not the primary authoring IDs.

**Migration.** `backgrounds`, `images` and `authoring` run an edit-preserving upgrade first. It renames stock native
files, retags their owned metadata, upgrades old layout documents with `cell_size_units: 32`, and migrates stock
image references. Existing custom IDs/aliases, custom filenames, pixel/palette data, cell edits, custom labels and
import provenance are preserved. Already-current documents, including deliberately authored old-ID references, are
not reformatted. Source catalog schema 2 ([assets.schema.json](../../Native/Content/schemas/assets.schema.json))
gives the eight import records `asset.background-source.<slug>.layout` / `.image` IDs; old `asset.bg.*` IDs remain
aliases of the same handles. These records still identify import files, not native PNG/JSON resources. Version 1
catalogs remain readable.

**Refusals.** Conflicting semantic destinations, duplicate primary identities, alias ownership, malformed legacy
metadata and invalid migrated image bindings refuse before publication. A custom raw catalog `file` reference to a
renamed native file requires explicit migration and is refused, not silently broken; arbitrary cross-format URI
rewriting is not provided. The shared staged writer publishes prepared files, then the manifest, then removes
obsolete paths. `--force` is an explicit reimport, not permission to bypass a legacy migration conflict. A normal
repeat conversion retains authored native bytes.

The format follows the actual renderer, upstream `resources\rood-reverse\src\BATTLE\BATTLE.PRG\2842C.c:464`
(`func_8009147C`), rather than treating every rectangle as a positive extent:

- `columns`, `rows` and row-major `cells` describe the grid. Native schema 2 admits 1–64 cells per side, at most
  1,024 stored cells, and `cell_size_units` of 16, 32 or 64 in 320×240 screen-coordinate space. These are explicit
  renderer budgets; JSON is bounded to 1 MiB. Original FAR/schema-1 imports retain the 1–31-side, 32-unit geometry.
  Horizontal wrap remains 1,024 screen units. FAR has no cell-size field: the offline FAR encoder refuses
  native-only geometry, and the production reader never calls it.
- `source_texels.{left,top,right,bottom}` are inclusive sampled endpoints. Reversed endpoints mirror the texture;
  both endpoint pairs equal disable the cell. The original signed deltas are endpoint minus start; negative deltas
  occur in `007OP01A` and `008OP01A`.
- Named `top_left`, `top_right`, `bottom_left`, `bottom_right` RGB colours modulate the image in 1/128 units: 128 is
  neutral and zero is black. `image_depth: indexed8` represents the consumed second corner's code byte, shifted into
  texture-page depth bits. It is **not** semi-transparency.
- `presentation: camera-wrapped-grid` and `draw_order: row-major-farthest` retain the existing submission rules.
  Camera roll, offsets and Q12 parallax remain room/event state; image bit depth is not scene depth.
- `import_provenance` retains source and paired-image paths/hashes, the unconsumed header word at byte 8 and the
  other three unconsumed corner code bytes. For imported originals, cell index `i` maps to source offset
  `12 + 24*i`. No consumed layout field is hidden in an opaque payload.

The converter validates every layout and paired PNG/sidecar before writing, uses the shared staged writer/manifest,
and records the TIM as a dependency. Unchanged inputs retain validated authored JSON; changed/deleted dependencies
invalidate it, and `--force` replaces authored cells from the import source. The native reader enforces
schema/version, grid counts, byte-range endpoints inside the image, RGB ranges, image identity/material and source
hashes in all builds. Unsupported depth modes and missing/stale/invalid resources produce errors rather than a
raw-FAR fallback.

**Production.** `BackgroundLayoutDocument` loads into an owned `OwnedBackgroundLayout` whose cells are drawn
directly. The authored `image_id` resolves through the shared native-image/manifest reader, including aliases and
explicitly selected unreferenced artwork. The original MPD/TIM binding is import provenance/default only. The
selected image's pixels and palette are owned with the cells and carried through upload; upload does not reopen the
original TIM or re-resolve an ID after loading. Source/dependency hashes, canonical image metadata, sampled
rectangles and material rules remain checked. The current sampler accepts even-width indexed-8 images up to 256×256
with one 256-colour palette, transparent zero and semi-transparency off.

**Page allocation.** Image dimensions determine the required page count. The event's original page number is a
placement hint: the allocator keeps it when free, otherwise chooses another contiguous free range within lendable
pages 25–31. Occupied/resident pages are never overwritten; invalid owners, empty/oversized requests and exhaustion
fail without partial reservations. Drawing and retirement use the actual selected range. This is renderer
allocation, not FAR/TIM reconstruction. `EncodeFar` remains offline-only, and repeat entry, initialization and
session reset release native ownership and the actual page reservation.

Open work: #210 (matched-reference placement/gradients/seams/parallax, production transitions/re-entry, dormant
legacy module pointer declarations and image/page rendering compatibility). Artwork IDs do not claim exact room
names; semantic room integration belongs to the MPD/ARM handoff. This slice does not complete #210 or EC-04.
