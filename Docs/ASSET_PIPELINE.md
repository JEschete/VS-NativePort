# Asset Pipeline: disc → `Native\data\`

Status: **design v1, accepted by the owner**. The owner's decisions are:
- Flat gameplay tables as CSV, compiled into SQLite; structured data as JSON (D25, refining the original "JSON for tables").
- Convert **everything up front**, not phase by phase.
- Converted output is git-ignored.
- Models match the `resources\Sydney Losstarot (Dying)\` glTF, packaged as `.gltf` + `.bin` + shared PNG files (D25).
- Video is **AV1**.
- Music is **sequenced**.
- Formats are chosen for ease of modding, without losing exactness (D25, §1.1).

This document is the contract between the extractor (`VS.Extract`) and the game's loaders (`VS.Content` and each system). Whenever a format below changes, bump that converter's version (§8).

**Implementation boundary:** this is a target contract, not a claim that every converter or loader exists. The character/event-animation packages in §6.2.3 are planned work, scheduled through ordered tracker [#4](https://github.com/JEschete/VS-NativePort/issues/4). They do not move asset migration ahead of maintained-code quality or the queued gameplay work. Legacy formats remain import sources, not the long-term authoring interface.

---

## 1. Principles

| # | Rule |
|---|---|
| P1 | **The game never reads the disc.** At runtime, `VS.Desktop` loads only `Native\data\` (plus the patch and mod layers). Only `VS.Extract` reads `.bin/.cue`. |
| P2 | **Convert everything up front.** One command (`extract-native-assets.cmd`) converts every file on the disc. Nothing is converted lazily or per phase. If a format's converter is not written yet, the run reports it as `unconverted` in the manifest, and the gate in §9 fails. |
| P3 | **Modern, open formats only:** CSV, JSON, glTF 2.0, PNG, WAV, FLAC and Matroska (AV1 + FLAC); SQLite only as the compiled query form of the CSV tables. There are no PS1 formats (TIM, VAB, AKAO, STR, XA, MPD, …) under `Native\data\`. |
| P4 | **Lossless with respect to the game.** Every value the original code reads must survive conversion. Fields with no standard home go in documented `extras`, an optional project extension (§6.2.1), or JSON members (§4.1). A round-trip test proves data preservation for every converter; preview and playback fidelity have separate checks (§7). |
| P5 | **Not committed.** `Native\data\` is git-ignored. Only the extractor, its schemas and the hand-written patches are committed. Users supply their own disc. |
| P6 | **Readable by humans and tools.** The output opens in Blender, any image viewer, VLC and a text editor. This is what makes Phase 2/3 modding possible. |
| P7 | **Deterministic.** The same disc plus the same extractor version gives byte-identical output (sorted keys, fixed float formatting, fixed encoder settings), so diffs between extractor versions are meaningful. |

### 1.1 Format decisions (owner, 2026-11-21, plan D25)

The rule for choosing: **what is easiest for a modder to edit**, provided conversion stays exact.

| # | Topic | Decision |
|---|---|---|
| 1 | Gameplay tables | **CSV**, one table per file with a stable `id` column and references by ID; lists become join tables. Each table has a `<table>.schema.json` (columns, types, ranges, references). The loader merges the base, patch and mod layers and compiles them into **SQLite** for queries. The CSV files stay the source; the database is derived. |
| 2 | Images | **PNG** at the true depth (indexed 4-bit or 8-bit, RGBA for 16bpp) plus a `<name>.image.json` sidecar (§6.4). |
| 3 | Models | glTF 2.0 as **`.gltf` + `.bin` + shared PNG** files, so textures can be edited in any image editor and models open directly in Blender. A self-contained `.gltf`/`.glb` is an export option only. |
| 4 | Events, effect programs, layouts | **JSON** with a JSON Schema, so editors can complete and check them. A script language can be added later on top of the same data. |
| 5 | Unknown data | **Kept** as named opaque fields (§4.1), so re-encoding is exact. |
| 6 | Colour and STP | The **PNG is authoritative** for colour and transparency: what an editor changes is what the game uses. The sidecar holds only what PNG cannot: the STP bit, provenance and the material default (§6.4). The round trip proves the original 16-bit words are reproduced exactly (§7). |
| 7 | Unused content | Valid content with no reference (e.g. `DBGFNT.TIM`, `BG\007OP01A`) is **converted** and marked `unreferenced` in the manifest, so modders can use it. Empty files are `skipped` with a reason; exact duplicates are recorded as aliases of one output. |
| 8 | Short palette files | Palette rows past the end of a file (FBC) are **not stored**. The runtime fills them with zero (transparent), as the zero sector padding after each file gave the original. Stale bytes are not reproduced: the only stale row was slot 0's fifth, past the one sector read, and no sprite samples it. A disc test shows the only sprites that sample past their palette are E210's descriptors 520-544 (row 242, zero in both). |
| 9 | Layout | §3. Patch and mod layers mirror it and override by path (assets) or by ID (tables), plan §9.6. |
| 10 | Coverage gaps | Formats with no migration issue get one (images in menus, ending, `SYSTEM.DAT` and PRG data; the gameplay catalog; the string table; AKAO sequences; PLG/PRG data blobs). |

---

## 2. Flow

```
disks\SLUS-01040.cue/.bin
        │  VS.Content::Disc (raw 2352-byte sectors + ISO9660; mode-2 form-2 for XA/STR)
        ▼
VS.Extract  ──► decoders (VS.Content, typed C++ objects; same code the round-trip tests use)
        │       └─► converters (one per format, versioned) ──► Native\data\...
        │       └─► ffmpeg (extract-time tool only; AV1 encode) for movies
        ▼
Native\data\manifest.json   (disc hash, extractor version, every output + converter version)
        │
        ▼  runtime
VS.Content loaders: Native\data\  →  Native\Content\patches\  →  mods\<name>\
        │   validate (schemas) → freeze → typed registries / GPU resources / audio banks
        ▼
Game systems
```

- Run it with `extract-native-assets.cmd [--disc disks\SLUS-01040.cue] [--out Native\data] [--only <kind>] [--force]`.
- The runs are **incremental**. An output is rebuilt only when its source file hash or its converter version changes. `--force` rebuilds everything.
- Conversion is **offline**. `VS.Extract extract` converts the supported source files during extraction; `VS.Extract images <datadir>` reruns just the image converters and `VS.Extract tables <datadir>` just the table converters (the workshop recipes, §6.8). The field-background runtime now reads the converted PNG/sidecar and uploads the native pixels and palette; it does not parse or convert the source TIM. The FAR layout remains a raw file read through the disc-file catalog and is a separate migration boundary.
- **Startup check:** `VS.Desktop`, Combat Lab and Launcher validate `manifest.json` and the required converter versions. They reject missing/stale converted sources (TIM, FBC, FBT and SYD), dependencies or outputs, and workshop tables that fail their checks, with an instruction to rerun extraction. They never fall back to reading the source file.

---

## 3. Output layout (`Native\data\`, git-ignored)

```
Native\data\
  manifest.json
  catalogs\          Tier 1: executable / PRG tables (§4); flat tables as <table>.csv + <table>.schema.json
    items\  blades.csv grips.csv gems.csv armor.csv shields.csv accessories.csv consumables.csv misc.csv ...
    battle\ break-arts.csv abilities.csv spells.csv status-effects.csv
    enemies\ species.csv enemies.csv modelsets.csv loot.csv bestiary.csv      (ENEMY_DATA_SCHEME.md)
    world\  zones.csv rooms.csv doors.csv keys.csv chests.csv save-points.csv puzzle-challenges.csv
    menu\   combination-<kind>-results.csv -pairs.csv -materials.csv   (SYD workshop recipes, §6.8)
    title\  title.json             (layout, timings, menu items: structured, so JSON)
    effects\ effects.json          (template instantiations → parameters)
    music.json sfx.json            (index: id → sequence/bank files)
    engine\ <table>.csv            (other exe tables; one file per DATA_LEDGER `catalog` row group)
  text\              Tier 2
    strings.en.json                (every game string: id → UTF-8 text with markup)
    fonts\ main.png main.font.json (glyph atlas + metrics)
  models\            Tier 3: glTF 2.0 as .gltf + .bin, textures as shared image files (§6.4)
    characters\<SHP>.gltf          (skinned mesh + applicable SEQ/ESQ clips, texture frames and VS metadata)
    weapons\<WEP>.gltf             (material variants = blade materials)
    armor\ shields\ ...            (as discovered per CONTENT_LEDGER)
    zud\<ZUD>.json                 (enemy model set: references SHP/WEP/SEQ glTFs; no duplicate meshes)
  rooms\<zone>\<MAPxxx>.gltf       (room geometry + textures)  + <MAPxxx>.room.json (collision, tiles, doors, scripts)
  rooms\automap\<ARM>.json         (minimap scenes per area: display geometry, doors, room names by string id)
  textures\<dir>\<name>.png        (TIM/GIM/FBT/standalone images, §6.4) + <name>.image.json
                                   (+ <name>.palettes.png when an image has several palettes)
    actors\<ETM>\<binding>.png     (actor-bound atlas images + .image.json, §6.2.2)
  ui\                              (menu art, title art, illustrations as PNG + layout JSON)
  events\<id>.json                 (EVT event scripts as decoded command lists; text by string id)
  effects\<id>.json + textures     (EFFECT .P/.FBT/.FBC data)
  audio\
    banks\<bank>\bank.json         (versioned instrument metadata and original provenance)
    banks\<bank>\<n>.wav + <n>.sfz  (16-bit PCM and an explicit sfizz loop region)
    music\<id>.seq.json            (decoded AKAO event list per track)
    sfx\<id>.seq.json              (sound-effect sequences)
    streams\index.json             (effect/XA source locations and native assets)
    streams\akao\<id>\stream.json  (bounded effect-stream metadata and source header)
    streams\akao\<id>\<n>.wav + <n>.sfz
    streams\xa\<disc path>.flac    (lossless decoded XA, e.g. ENDING\ENDING.XA.flac)
  movies\<name>.mkv                (AV1 video + FLAC audio)
```

The exact file list is generated: `CONTENT_LEDGER.csv` (§9) gives every disc file its output path(s).

`ENEMY_DATA_SCHEME.md`, cited in this document for the enemy catalogs, was removed from the repository; read the
[archived copy](https://github.com/JEschete/VS-NativePort/blob/0e3ae7d8df169bc684c8155fa44b90e731bb4e69/Docs/ENEMY_DATA_SCHEME.md).

---

## 4. Tier 1: executable and overlay tables → CSV tables and JSON catalogs

**Source:** initialized data in `SLUS_010.40` and the `*.PRG` overlays. `build\port\data.json` (`tools\port\data_inventory.py`) lists them, and `DATA_LEDGER.csv` gives each one a disposition. Only `catalog` rows go through this tier. `constexpr` rows are computed in code, `state` rows become zeroed module state and `drop` rows disappear.

Flat, row-shaped tables (items, actions, spells, status, recipes, enemies, loot, spawns, challenges) are written as CSV (§1.1). Data with nested structure (title layout, effect template parameters) is written as JSON.

**Decoders are generated, not hand-written:**

1. `tools\port\gen_table_decoders.py` reads the decomp struct definitions (libclang, PS1 layout: 32-bit pointers, 4-byte alignment, GCC bitfield order). For each catalog table it emits:
   - a C++ reader (`VS.Extract`, `src\tables\Generated*.cpp`) that pulls fields from the file image at the symbol's offset;
   - a CSV writer (flat tables) or JSON writer (structured catalogs);
   - a schema (committed under `Native\Content\schemas\` at the table's path under `Native\data\`, e.g. `catalogs\menu\combination-blade-results.schema.json`); for a CSV table it lists the columns, types, ranges and references;
   - the runtime C++ record type plus its loader (`VS.Content`, committed). The runtime type uses **modern widths** (D3: no layout matching).
2. Field names come from `TYPE_LEDGER.csv` (the final names), never `unk_0x10`. A field that is still unnamed fails generation.
3. **Pointers become references:**

   | Original field | Becomes |
   |---|---|
   | function pointer | the function's **final name** (`"Battle::Abilities::ApplyHeal"`), resolved through `BehaviorRegistry` at load |
   | pointer to a string | a string ID in `strings.en.json` |
   | pointer to another table row | that row's catalog ID (`"blade.scramasax"`) |
   | pointer to an asset | an asset ID (`assets.json`, #47), never a disc location |

4. Every record keeps its `legacyId` (CSV column `legacy_id`; the original index) so copied logic and `.mcd` save import keep working (plan §9.2).

### 4.1 Lossless "extras"
If a value has no clean meaning yet, it is **kept**, not dropped. In a CSV table it is a `raw.<field>` column; in JSON it goes under `"_raw": { "<field>": value }`. Either way it is listed in `TYPE_LEDGER` as unnamed. Unnamed fields block the 1a gate, so no raw field may remain at the end of Phase 1.

---

## 5. Tier 2: text and fonts

- **Strings:** every string the game shows (exe, MENU `ITEMNAME/ITEMHELP`, `SMALL\HELP*.HF0/HF1`, `MCDATA`, event text in `EVT`, menu labels) is decoded from the VS character table to UTF-8 and stored once in `text\strings.en.json` as `{ "id": "text", ... }`.
- **IDs** follow the owner prefix: `text.item.blade.scramasax.name`, `text.event.0012.004`, `text.menu.title.new_game`. Code and catalogs refer to IDs only.
- **Inline control codes** (colour, wait, newline, name insertion, button glyphs) become a small markup language: `{color:red}`, `{wait:30}`, `{name:ashley}`, `{btn:circle}`, `\n`. The decoder and encoder are exact inverses, so the round-trip is byte-identical (§7).
- **Fonts:** glyph bitmaps become a PNG atlas with JSON metrics (codepoint → rect, advance). The VS character code → Unicode map is in `Native\Content\schemas\charmap.json` (committed; it is a table of our own making, not game data). Reference: `tools\PSX\VagrantStory\VagrantStory-Font`.
- **Localization:** Phase 2 adds `strings.<lang>.json` overlays. The structure is ready now.

---

## 6. Tier 3: disc assets → modern formats

The disc has 27 kinds of files. The "Source" column below names the files on the disc. Every converter keeps the decoder (in `VS.Content`) separate from the writer (in `VS.Extract`).

### 6.1 Format map

| Source (count) | Output | Converter notes | References |
|---|---|---|---|
| `OBJ\*.SHP` (202) + `OBJ\*.SEQ` (700) | `models\characters\<SHP>.gltf` | §6.2 | vstools `SHP.js`/`SEQ.js`, VagrantStoryImporter, decomp actor loaders |
| `OBJ\*.WEP` (127) | `models\weapons\<WEP>.gltf` | static mesh, one texture per blade material via `KHR_materials_variants` | vstools `WEP.js`, VagrantStoryImporter |
| `MAP\*.ZUD` (455) | `models\zud\<ZUD>.json` + any meshes not already emitted | a ZUD bundles SHP + WEP(s) + SEQ for an enemy. It is emitted as **references** to the shared glTFs (dedup by content hash), not copied | vstools `ZUD.js` |
| `SMALL\*.ARM` (32) | `rooms\automap\<ARM>.json` | minimap scenes: a room count, 12-byte room records (visited, payload length, zone, map), display geometry (vertices, triangles, quads, lines, doors) and 36-byte names with previous/next links; (zone, map) becomes the stable room ID and names become string IDs. 47 map IDs exceed their zone's room table: explain them before converting | native `Field\MinimapScene.hpp` |
| `SMALL\*.DIS` (22) | `textures\dis\*.png` + `.image.json`, `catalogs\world\puzzle-challenges.csv` | concatenated 4bpp TIMs; `IQ.DIS` also holds the 64-record cube-puzzle challenge table (keyed by room ID); `ENDSCR00–15` are 16bpp result screens | native `CubePuzzleLifecycle.hpp`, `ClearResultsCore.cpp` |
| `MAP\*.MPD` (512) | `rooms\<zone>\<MAP>.gltf` + `.room.json` | §6.3 | vstools `MPD.js`, VSViewer, decomp field loaders |
| `MAP\*.ZND` (256) | `catalogs\enemies\*`, `catalogs\world\zones.csv`, zone textures → PNG | enemy records → the ENEMY_DATA_SCHEME catalogs; zone TIMs → PNG | ENEMY_DATA_SCHEME.md §3 |
| `EVENT\*.EVT` (512) | `events\<id>.json` | command list `{op, args}` with op names from the decomp's event interpreter; text → string IDs | decomp `Event\` module |
| `BG\*.TIM` (4), `BG\*.FAR` (4), root `*.TIM` | `textures\bg\*.png` + `.image.json`; `.FAR` → background layout JSON | TIM → PNG (§6.4). The BG TIM headers are not authoritative: they declare mode `0xA` (16bpp + CLUT) with a 256-colour CLUT and three share id `0x110`; the layout's draw depth selects 8-bit indices. The runtime uploads the converted PNG's packed indices and exact palette words. `.FAR` = 12-byte header `{w, h, reserved}` then w×h 24-byte tiles (source rectangle + 4 corner colours); it is still loaded raw through MPD room section 15 | native `Field\Background.cpp`, `BackgroundLive.cpp` |
| `GIM\*.GIM` (49) | `textures\gim\*.png` + composition JSON | VS screen-effect images: per-layer cell maps over shared strips, 4 × 16-colour CLUTs and a shared or owned 256-colour palette | native `Render\GimImages.cpp` |
| `MENU\*.BIN` (11), `ENDING\ILLUST*.BIN` (11) | `ui\*.png` + `.image.json` + layout JSON | menu art, game-over, ending illustrations. `MENUBG` is word-RLE and `ILLUST*` are LZ/RLE-compressed TIMs, decoded at conversion; `ITEMNAME`, `ITEMHELP`, `MENU12` and `MCMAN` are text and tables, not images | native `Menu\`, `StaffRollCore.cpp` |
| `MENU\*.SYD` (3) | `catalogs\menu\combination-<kind>-{results,pairs,materials}.csv` + `.schema.json` | workshop **combination recipes** (result records, ingredient pairs and material rules), not item definitions; there is no global item-stat file. Converted (§6.8) | native `Content\WorkshopRecipes.hpp`, `Menu\Setup\Combination.hpp` |
| `SMALL\MON.BIN` | `catalogs\enemies\bestiary.csv` | 150 records of `0x2C` bytes | ENEMY_DATA_SCHEME |
| `SMALL\HELP*.HF0/HF1` (28) | `text\strings.en.json` + `ui\help\<page>.json` + `ui\help\*.png` | `.HF0` = page text, sprite placements and line table; `.HF1` = 8×16 8bpp illustration blocks and 16-colour palettes | native `ManualTopics.hpp`, `ManualIllustration.hpp`, `Manual.cpp` |
| `EFFECT\*.P` (290), `*.FBT` (755), `*.FBC` (277), `EFFECT\*.BIN` (311) | `effects\<id>.json` + `textures\effect\*.png` | `.FBT` texture pages + `.FBC` palettes → PNG (§6.4). `.P` files are block streams (u32 header: size in the low 16 bits, type in the high 16 bits; type 0 ends) → effect JSON. `.BIN` files are **PLG code overlays** and EFFPURGE dispatch tables, already ported to C++ (`Effects\Plugins\PlgNNN.cpp`, `PurgeTables.cpp`); only their data blobs remain, as typed tables. Effect behaviors stay in code (plan §9.7) | decomp `Effects\` |
| `OBJ\*.ESQ` (97), `OBJ\*.ETM` (80) | applicable character glTFs + the actor-bound images they reference | event animation banks and texture overlays, including facial frames; resolve actor/bank/palette bindings from loaders, not matching filenames (§6.2.2) | decomp `BATTLE.PRG/30DB0.c`, `40564.c`; native `Actors/TextureAnimation.c` |
| `SOUND\WAVE*.DAT` (101) | `audio\samples\` + `audio\banks\` | §6.5 | decomp `Audio\` |
| `MUSIC\MUSIC*.DAT` (192) | `audio\music\*.seq.json` | §6.5 | decomp `Audio\Seq` |
| `SE\EFFECT*.DAT` (102) | `audio\sfx\*.seq.json` (+ bank if embedded) | §6.5 | |
| `ENDING\ENDING.XA` (1, 77 MB) | `audio\streams\ending\<ch>.flac` | §6.6 | jpsxdec |
| `MOV\TITLE.STR` (1, 26 MB) | `movies\*.mkv` | §6.7 | ffmpeg `psxstr`, jpsxdec |
| `BATTLE\SYSTEM.DAT` | `textures\system\*.png` + `.image.json`, `text\fonts\` metrics | 8 sections: images (0, 1, 3, 4, 5), a palette (2), a fixed image (6) and interface images (7). It supplies the font and UI glyphs | native `SystemResources.hpp`, `Canvas.cpp` |
| `SLUS_010.40`, `*.PRG`, `GIM\SCREFF2.PRG` | Tier 1 tables only (code is already in `Native\`) | §4 | |
| `license_data.dat`, `SYSTEM.CNF`, `ENDING\NULL.DAT`, 192 zero-byte placeholders | nothing (`drop`) | recorded in the manifest as `skipped: reason`. `DBGFNT.TIM` (a debug font no code reads) is converted and marked `unreferenced` (§1.1) | |

Every row is now identified (D25 inventory, 2026-11-21); the open questions are listed per IMP issue. Settling a row's converter means reading the loader that consumes the file (the native port's loader, with the decomp as a cross-reference for original behaviour), then writing the converter. **No converter is written from community docs alone.** vstools, VagrantStoryImporter, GodHands and DataCrystal speed up understanding, but they are reference only (licenses: `TOOLS_INVENTORY.md`).

### 6.2 Character models (SHP + SEQ/ESQ + ETM → glTF), matching the Sydney reference

`resources\Sydney Losstarot (Dying)\6F.SHP.gltf` is the standard-tool preview baseline. Preserve that structure where compatible with the source; it does not override the lossless metadata or renderer requirements below:

| Element | Rule (from the reference) |
|---|---|
| asset | glTF 2.0, one scene, written as `.gltf` + `.bin` with textures as shared image files (§6.4) referenced by relative URI (D25). Blender and standard viewers open it directly, and textures can be edited without touching the model. A self-contained base64 `.gltf` or `.glb`, like the reference, is an export option only |
| mesh | one skinned mesh; attributes `POSITION`, `NORMAL`, `TEXCOORD_0`, `COLOR_0`, `JOINTS_0`, `WEIGHTS_0` + indices |
| skeleton | one node per SHP bone named `bone<N>` (N = SHP bone index), with rotation + translation; one `skin` with `inverseBindMatrices` |
| animations | **every** applicable SEQ/ESQ skeletal clip, one clip per animation, named `<SHP>_<bank>_<n>` (e.g. `6F_BT2_0`); texture/event tracks follow §6.2.2 |
| material | baseline: `KHR_materials_unlit`, `alphaMode: BLEND`, `metallicFactor: 0`, `roughnessFactor: 0.9`; split primitives/materials where source blend or lighting state differs |
| texture | the shared image files of §6.4 (PNG + `.image.json`), referenced by the material; sampler `magFilter/minFilter = 9728` (NEAREST), wrap `10497` (REPEAT) |

Additions the reference does not have, required by P4 (lossless). The initial contract uses `extras`; standard tools can display the baseline without interpreting VS behavior:
- `asset.extras.vs.schemaVersion`: version of the VS payload across the file's `extras.vs` objects (§6.2.1).
- `extras.vs.shp`: SHP header values the game uses (collision/size, shadow, overlay/attachment points for weapons and effects, magic points, bone groups, body-part mapping used by targeting).
- `extras.vs.seq` on each animation: the SEQ key data the game reads at runtime (frame count, per-key flags, sound/effect triggers, root-motion flags).
- `asset.extras.vs.textureAnimation`: actor-local atlas bindings, overlay regions, frame definitions and palette data; animation-local texture/event tracks live with `extras.vs.seq` (§6.2.2).
- Vertex colors and per-polygon flags (semi-transparency, double-sided, lighting mode) go in `COLOR_0` plus a per-primitive `extras.vs.polyFlags`. Group polygons by compatible flags rather than collapsing different states into one primitive.
- **Which SEQ/ESQ clips and ETM regions belong to which SHP** comes from the decomp's loaders, event commands and ZUD bundles, not file-name guessing. A matching prefix/stem is a candidate association, not proof.
- **Fixed-point scale:** positions are SHP units divided by a constant `VS_UNIT_SCALE`, recorded in `asset.extras.vs.unitScale` so the loader can return exact integer values. The Sydney reference's scale is measured and matched.

**Runtime loader:** `VS.Content::ModelLoader` reads glTF (nlohmann-json; the `.bin` buffer and image files beside it) into the engine's `ModelDef` / `AnimationDef`. In Phase 1 the copied actor code expects SHP-shaped data. The loader provides it from the glTF (bone table, vertex groups, the `extras`). This is why the round-trip in §7 is mandatory: `decode(SHP) == ModelLoader(glTF)` field for field.

**Also emitted** (optional, `--posed`): `<SHP>_posed.obj/.mtl/_0.png`, like the reference folder. This is for quick previews only; the game never reads it.

#### 6.2.1 Container, schema and optional extension

- **First delivery:** keep the existing `extras.vs` design and define its versioned contract in `Native\Content\schemas\actor-animation.schema.json` (planned). VS.Content and Workshop must agree on IDs, units, track semantics and fixtures; do not build incompatible per-tool interpretations.
- **Extension adoption gate:** once the payload is exercised by both consumers, assess packaging it as one documented optional vendor extension. Choose a project/vendor namespace, publish its schema and fallback behavior, and test both readers before switching the writer. Do not invent a `KHR_` or `EXT_` standard. The extension must be in `extensionsUsed`, but not `extensionsRequired`, only while the asset has a valid standard preview without it. See the [glTF extension rules](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#specifying-extensions) and [extension registry guidance](https://github.com/KhronosGroup/glTF/tree/main/extensions).
- There is **one authoritative VS payload** per asset. A packaging change requires a schema/converter-version change and an explicit migration; do not emit independently editable copies in both `extras` and an extension. A native loader rejects missing required VS data or unsupported versions with an actionable error, even when a generic viewer can still show the baseline.
- Ordinary viewers need not execute facial tracks, event commands or VS rendering rules. Ignoring custom data is not a promise to preserve it on re-export. Before accepting an edited model, validate the VS payload and references; missing or altered metadata must be reported, not silently defaulted.
- Keep scene-wide scripts, room logic and gameplay catalogs in their existing JSON contracts. Model-local metadata belongs with the model, but glTF extensibility is not a reason to move all game data into it.

#### 6.2.2 Event and facial texture animation contract

Treat skeletal pose, texture overlays and event synchronization as related but separate data. A correct ESQ bone pose alone does not prove facial playback, and texture-based expressions do not prove that every facial movement excludes bone animation.

The [original ETM loader](../resources/rood-reverse/src/BATTLE/BATTLE.PRG/30DB0.c) skips a 16-bit word, reads a 16-bit height and uploads 128 bytes per row. [Native texture animation](../Native/Game/src/Actors/TextureAnimation.c) selects rectangular frames and handles packed texture addressing. These are reference mechanisms, not a complete converted-format specification: AP1 must establish actual actor, palette and event bindings before implementing a converter.

| Contract component | Required content |
|---|---|
| Provenance and association | Source paths/hashes, legacy model/bank/clip IDs and evidence for the ESQ/ETM/SHP association. Preserve source meaning without using executable addresses as native IDs. |
| Atlas binding | Stable binding ID, glTF image reference, actor/model association, index depth and palette selection. Resolve 4-bit/8-bit interpretation per binding; do not assume an entire ETM uses one depth or carries its own palette. |
| Texture patches | Logical destination texture/region, source and destination rectangles in integer pixel coordinates, atlas columns/frame ordering and overlay-slot identity. Validate bounds and packing; keep original layout/offset provenance separate from native texture ownership. |
| Tracks and clock | Integer frame durations, declared tick rate, clip/event associations, start/stop commands, looping and synchronization with skeletal clips. Derive boundary/order semantics from the original scheduler rather than inferring them from filenames or a viewer's frame rate. |
| Lifecycle | Base-texture restoration, bank/clip switching, actor-local mutable texture state and cleanup on cancellation, scene exit and repeat entry. Shared immutable images must not make one actor's expressions change another actor. |

The character glTF references the bound images and carries the actor-local contract. The images are written once as `textures\actors\<ETM>\<binding>.png` plus `.image.json` (§6.4) and shared by the runtime, Workshop and editors; there is no second copy. Preserve palette indices, all consumed palette entries and transparency/STP information, not only a baked RGBA appearance. A chosen-palette RGBA PNG or individual expression-frame PNG is a convenient additional export, not a lossless replacement for the indexed data.

Resolve colors from the verified character palette binding; never guess a palette or interpret index bytes as RGB. Missing/ambiguous bindings remain explicit errors or `unconverted` entries with diagnostics, not plausible-looking successful exports. Inventory unresolved regions and aliases until their consumption is understood. Multiple characters may reference the same source bank without duplicating meshes; §8 tracks every dependency that contributes to each output.

#### 6.2.3 Bounded implementation packages

All packages below are **planned**, not completed by this document. Their prerequisites describe technical dependencies, not permission to reorder tracker #4. They are incremental implementation slices within the up-front conversion architecture; runtime lazy conversion remains prohibited.

| Package | Prerequisite | Deliverable and acceptance gate |
|---|---|---|
| AP1: establish source bindings | Asset work reaches its place in #4 | Trace original loaders, event producers and texture-frame consumers. Record exact SHP/ESQ/ETM/CLUT/region/clip mappings and scheduler/reset behavior for at least two actors covering 4-bit and 8-bit bindings. Keep observed header relations and same-stem filenames separate from verified pixel/timing evidence. |
| AP2: typed decode and schema | AP1 | Extend the existing content decoders and define the §6.2.1 schema. Fixtures cover indices/palettes, rectangle addressing, duration boundaries and association rules. Reject truncated data, invalid references, unsupported depth, out-of-bounds patches and ambiguous palettes without partially publishing an asset. Keep reference code read-only; native implementations belong in existing content/actor systems. |
| AP3: inspection and PNG export | AP2 | Extend Workshop's existing viewer/export path: show source IDs, actor/bank/clip/region/palette selection, decoded atlas and individual frames; provide readable playback/pause/step controls for skeletal and texture tracks together. Export palette-correct atlas/frame PNGs, label unsupported event/render behavior, and distinguish manual frame preview from verified event playback. |
| AP4: conversion and native loading | AP2 | Implement versioned glTF/PNG writers and VS.Content readers from the same typed contract, reusing Workshop decoder findings rather than a second guessed parser. Pass exact decoded-data comparisons and schema/glTF validation. Two unchanged extraction runs produce byte-identical outputs with no unnecessary rebuild; changing any consumed SHP/SEQ/ESQ/ETM/palette dependency rebuilds affected outputs. |
| AP5: production playback and fidelity | AP4 | Connect typed tracks to the existing native actor/event lifecycle, not a new hardware-shaped bridge. Exercise normal scene/event flow with synchronized body and expression playback, loops, cancellation, bank switching, two actors sharing source data and repeated scene entry. Prove patch pixels and unchanged pixels outside the target, then compare named scene/event states against the reference. Record inspection, preservation and rendering results separately (§7.1). |

AP3 and AP4 can proceed independently after AP2. The two-actor slice establishes the contract, not full coverage: before the content gate passes, inventory every ESQ/ETM and settle each consumed binding or record a justified skip. Do not claim universal viewer support or facial fidelity from a single successful clip.

**Rendering boundary:** the unlit glTF baseline and baked preview colors are not the final lighting implementation. Preserve vertex colors, per-primitive lighting/blend flags, palette changes and scene lighting inputs for the native renderer. Standard `alphaMode: BLEND` does not express every PS1 blend operation. For a reported cutscene rim/backlight effect, trace the specific shot's texture, lighting, vertex-color and event inputs before choosing its native representation; do not assume it is an animated texture.

### 6.3 Rooms (MPD → glTF + room JSON)
- **Geometry:** one glTF per room. Each texture page/CLUT group is one primitive with an unlit, nearest-sampled PNG. Vertex colors are kept, and the same `VS_UNIT_SCALE` applies.
- **Everything that is not drawing** goes in `<MAP>.room.json` (schema `room.schema.json`):
  - collision / floor-height tile grid;
  - doors and exits (target room IDs, not LBAs);
  - camera areas;
  - trap, lock and treasure markers;
  - room script triggers;
  - lighting/fog;
  - enemy spawn references → `catalogs\enemies` (ENEMY_DATA_SCHEME).
- Room IDs are `room.<zone>.<n>` with the original `legacyId` (zone/room index).

### 6.4 Textures and images (TIM/GIM/FBT/FBC/BIN art → PNG + `.image.json`)

Modding first (D25): the PNG is what an image editor changes, so the PNG is authoritative for colour and transparency. The `<name>.image.json` sidecar holds only what PNG cannot carry. Conversion stays exact: PNG + sidecar reproduce the original 16-bit words, and the round trip (§7) proves it.

The strict TIM and native-image readers/types are in `VS.Content`; the TIM-to-image conversion, PNG/sidecar writers and manifest writer are in `VS.Extract`. These writers are not part of the runtime Content library. The extractor writes the native files once. The field-background runtime resolves the converted PNG/sidecar from the manifest and uploads its pixels and palette; it does not parse the source TIM. Its FAR tile layout remains raw and is a separate migration boundary.

Background material (#208): the original draws each FAR tile as an opaque Gouraud-shaded textured quad (`setPolyGT4`, `2842C.c:497`) with CLUT (768, 238), in the farthest ordering slot over a black full-screen fill. Its corner colours on the disc are neutral 128 (the texel unchanged) or black (an edge fade). There are no normals or lights, and the only change to the texture while it is shown is the event script's palette transform.

TIMs inside other files: the shared strict decoder finds 822 at 4-byte-aligned offsets in 99 files.
- 782 are in the first texture table of 76 `MAP\*.ZND` zones; each zone's table count matches. They are texture pages and the CLUT rows the zone copies into the room palette, all 16-bit words.
- 28 are in `SMALL\*.DIS`: 12 at 4 bits with 16 palettes, and 16 direct-colour 320 × 224 end screens.
- 12 are in `ENDING\ENDING.PRG`: 7 at 4 bits (one of them with two palettes) and 5 at 8 bits.

A disc-backed `[extract]` test converts each one with `ConvertTim` and rebuilds its exact bytes. The containers' converters adopt them: zones in #203, DIS in #211 and the ending program in #498. `MENU\*.BIN`, `SYSTEM.DAT` and `TITLE.PRG` hold no raw TIMs, and `ENDING\ILLUST06-16.BIN` are compressed (#498).

Effect FBC palettes (first slice, #215): `VS.Extract images` reads each raw FBC as rows of 256 little-endian 16-bit colour words and writes an editable direct-colour PNG (one row per CLUT row, in source order) plus the standard `.image.json` (and `.stp.png` mask when STP is mixed) under `textures/effect/palettes/`. The manifest records the source path and hash. A file must hold whole rows and at most the 5 rows of the effect CLUT area (VRAM rows 240-244); anything else is a conversion error. Rows that an effect's CLUT count names past the end of a short file are not written. Conversion is offline. At runtime the effect loader resolves a CLUT's disc sector to its converted palette through the manifest (`LoadLiveEffectPalette`), uploads the PNG's rows and zero-fills the rest of the slot (decision 8). It never reads the FBC and has no disc fallback. The CLUT is refused and reported, while the effect's texture pages still load, when its palette is missing or does not match its manifest entry or is not a 256-entry direct-colour image (`ResourceRefusal::kPaletteMissing`), when it is empty or ends in a partial row (`kPaletteRows`), or when it has more rows than its slot reaches by row 244 (`kPaletteSlot`). `DataStore::RequireConversions` refuses to start when any FBC lacks a palette or its source hash is stale. An edit to the PNG is what the next load uploads.

Effect palette variants and palette animation are not palette data. Each texture descriptor in an effect's `.P` texture tables (block type 1, 0x18-byte descriptors) carries a CLUT word that selects one effect CLUT row (all 13,348 descriptors on the disc use column 768). Bit 15 of the CLUT word makes the draw semi-transparent, and the page word supplies the blend rule. A texture-animation frame selects a list of descriptors, so frames that swap CLUT rows over the same texture region animate the palette on the frame program's timing. The texture tables of 95 of the 290 `.P` files use more than one row. In 19 of them, frames swap rows over the same texture region (E046, E077-E080, E098, E134, E157, E206...); no frame mixes rows over one region. No code rewrites the effect CLUT area during playback. The decomp's only writer is the effect loader (`6E4CC.c`), and VRAM dumps of the manor opening show rows 241-242 unchanged from E206's upload (tick 1460) until E207 reloads them (tick 2729). So the converted palette rows stay one PNG per FBC; variants and animation remain descriptor and frame data for the `.P` migration. Tests check that converted rows composite 4-bit and 8-bit indexed textures word for word: transparent zero, other blacks, STP with each blend rule, and zero-filled rows past the file. They also check that an animation's frames switch rows on time.

Binding a package's texture tables checks each indexed descriptor against the palette rows its effect loaded (`CheckEffectPaletteReferences`). A 4-bit (16-entry) or 8-bit (256-entry) descriptor must sample columns 768-1023 of one of rows 240-244. The row must be filled by its effect's CLUT, which covers its slot through row 244, or be row 240. Row 240 holds the battle's system palette, SYSTEM.DAT's fixed image, which INITBTL.PRG uploads to (768, 240) at `18.c:311`. Direct-colour descriptors sample no palette and are skipped. A descriptor that fails stops the bind before anything is published. `BindLiveEffectTextureTables` logs the descriptor index, its CLUT word, the reason, the loaded rows and the action effect, then aborts, as it already does for a malformed texture block. On the disc, every package the runtime loads (290, 278 with texture tables) passes using only its own CLUT's rows. The only descriptors on row 240 are E000.P's 95, filled by its own slot-0 CLUT. E000.P is also BATTLE.PRG's resident empty package, which binds against the system palette alone and passes too. E252-E399 name E000.P's package offset and load no CLUT. The runtime plays them without loading a package (`FinishPurgeOverlay`), so their descriptors are never bound over another effect's palette.

Effect FBT texture pages (#214): an FBT is one whole VRAM texture page, 64 × 256 words (32,768 bytes). `VS.Extract images` (`Extract/EffectTextureConversion.hpp`) binds each page to the one effect that loads it through EFFPURGE.BIN's tables (each effect's FBT count, slots and file extents; extents count sectors from `E000.P`). The page goes to texture page 24 + its slot (slots 0-7). Its depth comes from the descriptors in that effect's `.P` texture block that sample its page. A 4-bit page becomes a 256 × 256 indexed PNG, an 8-bit page a 128 × 256 one, and the indices are the page's bytes unchanged. A page that no descriptor samples (only `E040_3.FBT`) converts at 8 bits with a `note`. A page bound to no effect or to two, sampled at two depths or as direct colour, sampled past its page's edge, or sampled through a CLUT row its effect's FBC does not hold is a conversion error. The PNG's palette is a preview only: the effect FBC's most sampled row and first entry, the lowest on a tie. The sidecar names it in `provenance.preview_palette` (palette image id, row and entry), and the runtime draws with the converted FBC, so colours are edited in the palette image. `provenance.texture_page` records the effect, slot, page and VRAM rectangle. The dependencies are the package (role `sampler`), the FBC (`preview palette`) and EFFPURGE.BIN (`binding`); a change to any of them reconverts the page. Pages with the same bytes at the same depth are converted once. The first by path holds the files and the others are manifest aliases (§8), so an edit to the shared PNG changes every effect that loads it. On the disc, the 755 FBTs give 516 converted pages (15 at 4 bits, 501 at 8 bits) and 239 aliases, each of a page in another effect.

At runtime the effect loader finds a page's converted image by its sector through the manifest (`LoadLiveEffectTexture`), following an alias to its target, and uploads its packed words. It never reads the FBT and has no disc fallback. A page whose image is missing, stale or invalid (`ResourceRefusal::kTextureMissing`), or is not an indexed 64 × 256-word page (`kTextureSize`), is refused and reported, and its texture page is not reserved. `DataStore::RequireConversions` refuses to start when an FBT lacks its conversion or the page or a dependency has changed. The loader still allocates the 32 KB buffer the original read each page into, so the high heap sees the same allocations. A page now loads in one poll, as a palette does; the disc queue used to take several polls per page, and neither matches the original's CD read time.

- **Pixels:** 4bpp and 8bpp images are **indexed PNGs at their true bit depth**. Indices are never re-quantized or remapped. 16bpp images are RGBA PNGs (24bpp, if any, RGB).
- **Colour:** 5-bit channels are expanded by bit replication (`c<<3 | c>>2`). Import takes the nearest 5-bit value (`(c*31 + 127) / 255`), which inverts replication exactly and rounds edited colours sensibly.
- **Transparency:** alpha **0** means the word `0x0000`, the colour the hardware never draws. Every other word is alpha **255**. Import maps alpha 0 to `0x0000` whatever its colour, and rejects partial alpha with the file and pixel or palette index. Opaque pure black is always `0x8000`, because `0x0000` would be transparent. (An image shown directly, such as a full-screen picture, displays `0x0000` as black; the material says which applies.)
- **STP (semi-transparency bit):** stored in the sidecar per palette entry, or per texel for 16bpp: `"none"`, `"all"`, a list of palette indices, or a 1-bit mask image `<name>.stp.png`. STP colours display opaque in editors; whether STP blends, and how, is the material's rule, not the texel's.
- **Palettes:** the indexed PNG's own palette is the image's default palette. An image with several palettes (CLUT variants, palette animation, effect colour swaps, blade materials) also has `<name>.palettes.png`: one palette per row, row 0 = the default, with the same colour and transparency rules and STP per row in the sidecar. The converter checks that the PNG's palette equals row 0 and reports a mismatch instead of picking one. An image whose palette lives elsewhere (ETM, zone textures) gets a preview palette from its default binding; the sidecar names the palette's source, and import ignores the preview.
- **Sidecar** (schema `vs.image`, version 1; `Content/ImageFiles.hpp`): `schema`, `schema_version`, the stable image `id`, `depth` (4, 8 or 16), `width` and `height`; for indexed images `palette` = `{entries, count, stp}` with one STP value per palette row; for 16bpp images `stp` = `"none"`, `"all"` or `"mask"` (then `<name>.stp.png`); `material` = `{zero_word, semi_transparency}` only when the consumer is known; `unreferenced` only when true; and `provenance` = source path, SHA-256 and byte offset, the TIM's id, flags and VRAM rectangles, an effect page's `texture_page` (`effect`, `slot`, `page`, `vram`) and `preview_palette` (`image`, `row`, `entry`), any `dependencies` (source, hash and role, such as the layout that fixes a background's depth) and a `note`. Unknown members are errors, so a misspelt field is reported rather than ignored. Unused palette entries keep their words.
- **Placement:** images that the original placed in a shared VRAM page are written as **separate** files. Their placement in VRAM is runtime layout, not content (plan §0.1). Any UV remapping is done by the converter and checked in the round-trip.

### 6.5 Sequenced audio (owner choice: **sequenced**)
The game plays music and sound effects by sequencing sampled instruments at runtime. Tempo, volume, fades and battle transitions change live, so the content is converted into equivalent modern pieces and our ported sequencer (`Audio::Seq`, copied from the decomp `Audio\Seq`) plays it:

| Piece | Format | Detail |
|---|---|---|
| Samples (implemented first slice) | `audio\banks\<bank>\<n>.wav` | 4-bit ADPCM decoded to mono PCM16 at 44.1 kHz. Predictor, nibble order, saturation, termination and loop bounds have synthetic tests; console waveform parity is not yet established. |
| Instruments | `audio\banks\<bank>\<n>.sfz`, `bank.json` | SFZ supplies a sample/loop region to sfizz. Version-1 JSON retains original sample/loop offsets, tuning, ADSR, availability and optional `source_record` bytes (16 or 64). The existing sequencer computes pitch/pan; native envelopes run separately, so SFZ key/velocity tracking and envelope shaping are neutral. Pitch bend has no smoothing and spans 12 octaves in either direction. |
| Music (planned) | `audio\music\<id>.seq.json` | Decoded AKAO `{tick, op, args}` events with semantic jump targets; currently the original bytecode still drives the ported sequencer. |
| Sound effects (planned) | `audio\sfx\<id>.seq.json` | Same future event format; current effects retain original AKAO programs and native sample-bank bindings. |
| Index (planned) | `catalogs\music.json`, `catalogs\sfx.json` | Semantic IDs with original track/SE provenance. |

- The semantic event schema/round-trip encoder remains planned, not implemented by the sample importer.
- **Current import:** `VS.Extract audio Native\data\disc Native\data` converts all **83 nonempty banks**
  on the supported disc; 18 one-byte files are placeholders. Optional numeric wave-file IDs select a subset.
  Exact-end records and equal start/loop offsets in erased, final DMA padding are explicitly unavailable,
  not fabricated silence. Active ADPCM remains strictly validated. WAVE0032's older 64-byte articulation
  is recognized by its payload layout, pitch table and envelope fields. Q20 base pitch is converted to
  native root/fine tuning; the complete twelve-pitch table and unpacked envelope survive in `source_record`.
  Its all-zero pitch tables are unavailable records. Original data/provenance remain import sources,
  not a new authoring-format requirement. Two complete imports produced identical hashes for 5,993 files.
  Reimport after upgrading: existing SFZ files need the unsmoothed, extended pitch-bend range for FM.
- **Mixer/voices:** behavioral, at the envelope level, at 48 kHz (plan §6.3), never SPU registers.
  Final dry+wet output (after any Mono fold-down) saturates to the original signed-PCM range,
  [-1, 32767/32768], before delivery to SDL. Quiet mixes remain additive; source voice gains are
  unchanged, including the deliberately doubled mono-stream pair. This is not normalization or
  compression, and intentional source overload may still sound distorted. Independent XA/movie
  sources are not part of this primary-mix boundary.
- **Current clock/ownership:** the existing 240-Hz sequencer publishes complete, ordered 200-frame control
  ticks through a bounded single-producer/single-consumer queue. SDL demand renders the native voices at
  48 kHz, so active samples/envelopes continue while gameplay stalls; the callback never runs the sequencer.
  Instrument files are loaded into separate sfizz programs on the control thread, then ownership transfers
  to the renderer. Consumed queue cells return retired programs to the control thread for destruction.
  A failed load stops the requested voice explicitly; queue exhaustion is a logged fatal error.
  Pending-note envelope feedback prevents voice allocation from mistaking queued notes for finished voices.
  Reset invalidates queued controls before clearing/repriming the native stream, without touching movies.
  The device must stop its callback before the voice engine is destroyed.
  VSync advances counter 2 using the host clock's exact 60000/1001 field-rate ratio,
  retaining fractional clocks in an integer remainder. Assuming 60 fields/s instead produced only
  about 47,952 frames/s and gradually exhausted the 48-kHz output buffer. Stop/resume preserves the
  remainder; counter setup resets it. A production VSync regression covers 60,000 fields (1,001 seconds).
  SFZ loading remains synchronous for gameplay, not for the renderer. Long stalls can delay new musical
  events; active samples continue rather than exhausting pre-rendered PCM. Tick boundaries are retained
  when queued controls catch up, not collapsed or rendered simultaneously.
  Open/reset retains 2,400 frames (50 ms) of native-stream priming. No file I/O, instrument loading,
  logging or control-thread waits occur in the render callback. Debug logs report shortages, rendered
  frames, callback duration and control-thread gaps. Native postmix tests verify a full second without
  control-thread progress, plus independent program replacement, reset and reopen.
  Accelerated desktop replays (`--speed > 1`) retain synchronous rendering and the 250-ms stale-output
  cap; real-time listening acceptance applies at speed 1.
  `VS_AUDIO_CAPTURE` records rendered native PCM through a two-second bounded staging buffer, drained
  on the control thread. Overflow explicitly fails capture, not playback. Captures exclude queue padding,
  physical output gaps and independent movie/XA sources; their length follows device demand, not logical tick count.
- **Music state:** G00248-G00250 are views of G00246's `VS::Field::ZoneContext` fields, not separate storage.
  This lets normal zone initialization/load and opcode `0x92` reach the populated music slot.
  Save/layer/crossfade/restore use complete native config/channel objects, not PS1 byte extents.
  Saved-layer conflict resolution detaches displaced assignments while protecting active SFX and stream voices.
- **In-game menus:** the five owned wrappers dispatch original open/invalid/leave/select/change effects
  (10/7/6/5/4), preserving channel tags, centered pan and volume. These differ from the existing title-menu calls.
- **Effect streams:** `VS.Extract streams disks\SLUS-01040.cue Native\data` imports block 13 from
  all 113 shipped stream-bearing effect packages (112 mono, one stereo). The 64-byte source header,
  native frame loop and source pitch remain in `stream.json`; the ID is a stable FNV content identity,
  not an authenticity check. Stereo uses separate predictor histories and 0x800-byte source-channel chunks.
  Buffer-end ADPCM flags are not treated as logical sample endpoints. Runtime binding validates the bounded
  source and native assets, uses canonical room slot 3, and preserves native command-pointer width.
  The existing sfizz engine supplies the original reserved voice pair, controls, pause/resume, fades,
  completion and cleanup. A paused voice remains active even before its first rendered sample.
- **Fidelity modes:** a shared 44.1-kHz noise sequence uses the original clock/parity rules, independently
  of voice pitch. FM changes sfizz sample pitch using the preceding voice's envelope-scaled, pre-pan signal;
  voice zero is not modulated, pitch is bounded to the original range, and zero pitch holds sample position.
  Noise-only notes require no sample file. Mode changes, envelopes and effect controls share complete-tick
  publication and renderer reset semantics.
  Reverb uses all ten original presets, named native delay/filter state and signed stereo wet gains.
  Its 22.05-kHz reflection/comb/all-pass network uses the documented 39-tap input/output filters;
  linear interpolation bridges the 44.1-kHz filter clock and 48-kHz mixer. Disabling writes preserves
  existing reads/tails; preset changes clear the delay state and reset wet depth as the original SDK did.
  Tails continue after voice stops and are cleared by playback reset. The network uses floating-point
  arithmetic, not bit-exact SPU rounding, register storage or sample RAM.
  The existing Mono choice folds down the complete dry+wet output at the same control-tick boundary,
  not just per-voice pan. Stereo delay state is retained when switching back. Actual Sound-menu PCM
  verifies exact channel equality in both mono windows and restored separation in stereo.
- **Limits:** complete bank/effect-stream import is not all-track or all-scene listening acceptance.
  Every music/scene transition, perceptual reference parity and 5.1 remain separate acceptance work.
  Unused DMA/refill APIs, the unused actor interpolation wrapper and unproduced stream slots 1/2 are
  not promoted to supported runtime paths by the new stream implementation.
  sfizz interpolation and the float effect network are not claims of bit-identical console output.
  See [HANDOFF.md](HANDOFF.md) for measured evidence.
- **Format/DSP sources:** [PSX-SPX SPU documentation](https://psx-spx.consoledev.net/soundprocessingunitspu/)
  and [AKAO articulation format research](https://github.com/vgmtrans/vgmtrans/blob/master/src/main/formats/Akao/AkaoInstr.cpp);
  preset ordering and control/reset semantics were checked against the read-only original libspu source.
- **Rendered previews:** none (owner choice). Phase 2 can add a `--render-previews` flag if wanted.

### 6.6 XA streams → FLAC
- **Source:** the shipped standalone XA track is `ENDING\ENDING.XA`. Raw mode-2 sectors are required;
  a cooked ISO-extracted folder loses the necessary subheaders. Original playback selects file 1/channel 0
  at 75 sectors/s. The second `BeginXaStream` argument is initial CD volume, not a byte count.
- **Decode:** the `streams` command (also called by `extract`) validates sector/subheader bounds,
  coding and playback cadence before using the existing FFmpeg XA decoder and FLAC encoder.
  This track repeats each stereo payload four times. Concatenating all copies would incorrectly produce
  1774.08 seconds; selecting one per playback period produces **443.52 seconds, 37800 Hz, stereo, 16-bit**.
  Consecutive identical playback periods are retained; this is cadence selection, not content deduplication.
  Missing channels/timing gaps, changing coding, mismatched redundant payloads and partial periods fail explicitly.
- **Output:** `audio\streams\xa\ENDING\ENDING.XA.flac`; `audio\streams\index.json` records its source
  LBA/extent, selected file/channel and rate. The legacy `files.tsv` row remains `stream`: it describes
  sector-addressed data access, not availability of the separate native audio asset. Decoder PCM hashes
  match the uncompressed import exactly. All 250 effect/XA outputs are stable on repeated imports.
- **Runtime:** the owned `XaPlayback` service uses the existing dr_flac dependency to decode on the
  control thread, then supplies an independent SDL source on demand. This shipped track occupies about
  67 MB of PCM while playing; there is no callback file I/O, decoder work or gameplay-thread refill.
  Signed CD gain, fades and original stereo/attenuated-mono mixing feed that source. EOF, requested stop,
  driver/disc reset, device close and re-entry release its buffers and update the disc-request state.
  Missing/malformed/truncated assets become logged disc errors, not a successful silent completion.
- **Acceptance boundary:** unit tests inspect actual SDL postmix, including a full second without
  control-thread progress. When the installed ending FLAC is available, a disc-conditional regression
  also verifies audible initial playback, a 30-second seek, stop/re-entry and natural final-sector
  EOF/drain through the production disc queue and output. Full ending-scene/reference listening
  remains distinct from this API coverage.

#### Audio producer and cleanup audit (#7)

| Surface | Production route | Evidence and boundary |
| --- | --- | --- |
| Title and in-game menus | Title calls and `Menu/Sounds.cpp` -> centered SFX commands -> AKAO/sfizz | Original IDs/tags tested; real open/change/select/leave/refusal and re-entry recorded. |
| Music and event transitions | `Event/ScriptAudio.c`, `Audio/Music.c`, zone state and `MusicState.cpp` | MUSIC016/110/054, native-sized save/restore/swap and 1536 saved-voice conflict combinations; not every scene crossfade. |
| Actor clips, weapon sounds and footsteps | Actor animation/movement commands -> actor/model/surface SFX helpers -> `Audio/Sfx.c` | Prologue and combat recordings use these production routes; no new speech synthesis or claim of voiced text dialogue. |
| Room, weather, doors and interactions | `Field/Environment.c`, `RoomEffects.c`, `Doors.c`, `Interactions.c` -> ordinary/bank SFX, pan and fades | Existing producer paths retain the common renderer; every weather/room variant has not been replayed. |
| Event sound commands | `Event/ScriptAudio.c` -> bank loading, play/stop, pitch/pan/volume slides and saved music | Native-width stream parameters and typed callbacks preserve the shared dispatcher; no second sequencer. |
| Effect-package streams | Block 13 -> room slot 3 -> effect timeline -> `StreamControl.cpp` / `StreamControlLive.cpp` | All 113 payloads import; real E044 Solid Shock binds/starts/stops; pause, EOF, reset, unbind and re-entry have regressions. |
| Movies | Matroska/FLAC -> existing `MoviePlayer` -> independent SDL source | Native-mix clear/reset leaves independent sources intact; primary-mix capture is not movie-output evidence. |
| Ending credits | Credits start/stop -> disc queue -> `XaPlayback.cpp` -> independent SDL source | Source duration/lossless import, installed-asset postmix start/seek/EOF and synthetic lifecycle tests; full credits navigation/listening is not established. |

Cleanup is exercised at voice stop/EOF, package unbind, driver/disc reset and device close.
The existing unused `Audio_UpdateActorSoundTransition` has no discovered producer/caller and is
marked unused upstream. Stream slots 1/2 have no discovered producers; shipped effects use slot 3.
The unused generic `Audio_FreeCutsceneStreamSlot` still assumes owned storage: it must not be
called on borrowed slot-3 package data. Normal production effects use the tested unbind route.
These unused APIs and DMA/IRQ helpers remain explicit EC-02 migration debt, not demonstrated
gameplay failures or claims of completed compatibility retirement. General discontinuous/multichannel
XA, all-content perceptual parity and 5.1 are outside this bounded acceptance.

#### Functional acceptance evidence (#7, 2026-10-21)

- Debug and Release full suites: **379 cases / 991,535 assertions** each.
  Release `[audio],[clock]`: **72 cases / 163,008 assertions**.
  Complete changed-header quality: **313 existing findings, zero new**;
  no baselines or suppressions changed. Commands are in BUILDING.md and QUALITY.md.
- Final speed-1 controller replays cover all five in-game menu sound actions,
  simultaneous music/SFX, the unskipped opening scene (only the intro movie is
  skipped), and E044 Solid Shock. The unavailable L2+Circle shortcut produces
  the invalid-action sound at tick 5404 without opening a spell menu.
- Isolated Combat Lab run `1792631613651010-0` binds E044 at tick 515, starts
  voices 22/23 at 607, stops at 691, then reuses them for menu audio at 1143.
  This run contains one cast, not two. The earlier float mix exceeded full
  scale; the final source-backed signed-PCM boundary yields [-1, 32767/32768]
  without altering original stream gains. Saturation is not a claim that
  intentional source overload sounds undistorted.
- Final menu/opening captures peak at -5.826359/-2.462624 dBFS. They and the
  isolated combat capture contain no NaNs/Infs or post-startup stream shortages.
  Initial pre-attachment shortages remain explicitly outside steady playback.
- The installed ending FLAC passes actual SDL postmix at sectors 270000 and
  272250, stop/re-entry and natural EOF/drain from 303214. The missing-asset
  control explicitly skips that case while ordinary clock tests pass.
- Two complete imports retain 5,993 bank files; two effect/XA imports retain
  250 files. Two source/project generations after ledger reconciliation retain
  966 files. All 17 port-generator tests pass.
- Local evidence is under `build\quality\audio-output-*` and `audio-ending-*`.
  Paired menu/opening reference auditions and isolated combat are
  `audio-listen-*.wav`. Reference windows match scene/action state rather than
  equal wall-clock timestamps; the portable reference recording is AAC-derived,
  not lossless or sample-synchronized. No subjective parity approval is claimed.
  Disc-derived audio/captures are not committed or attached to issues.

Issue #7 owns functional output acceptance, not EC-02 retirement. Full credits
navigation/listening, every effect/track in context, bit-exact DSP parity and
5.1 are not established by these bounded results.

### 6.7 Movies (STR → AV1 Matroska) (owner choice: **AV1**)
- **Source:** `MOV\TITLE.STR` (26 MB, raw mode-2 sectors: MDEC video + interleaved XA audio). It is read from the `.bin`, like XA. Whether it holds more than one movie (intro, attract) is settled from the decomp's movie code (`_initIntroMovie`, `_playIntroMovie`) and recorded in CONTENT_LEDGER.
- **Pipeline** (extract time only):
  1. `VS.Extract` writes the raw 2352-byte sectors of the movie to a temporary file.
  2. ffmpeg decodes it with its `psxstr` demuxer and `mdec` decoder, and encodes:
     - video with **SVT-AV1** (`libsvtav1`), or **libaom** (`libaom-av1`) in FFmpeg builds without it, at the source resolution and frame rate (no upscaling in Phase 1);
     - audio as **FLAC**.

     The settings are `-c:v libsvtav1 -preset 6 -svtav1-params lp=2 -crf 18` or `-c:v libaom-av1 -cpu-used 6 -row-mt 1 -threads 2 -crf 18 -b:v 0`, each with `-pix_fmt yuv420p -color_range pc -colorspace bt470bg -c:a flac` (`Content/MovieEncoder`). The encoder writes a random container ID, so two conversions differ as files while decoding to identical frames.
  3. The result is muxed into Matroska: `movies\<name>.mkv`. WebM cannot carry FLAC, so the container is MKV.
- **Encoder dependency:** ffmpeg is an **extract-time tool only**. It is never linked or shipped. `VS.Extract` finds it on `PATH` or through `--ffmpeg <path>`. The launcher and `VS.Extract` both stop with an error naming the FFmpeg if it has neither encoder, and `VS.Extract` stops if a conversion fails. Verified on the dev machine: ffmpeg 8.1.1 (winget Gyan.FFmpeg) has `libsvtav1`, `libaom-av1` and `flac`. Gyan's "essentials" 9.0.2 build has `libaom-av1` but no `libsvtav1` (#254).
- **Encoder comparison** (ffmpeg 8.1.1, whole intro, against a lossless decode of the same source): `libsvtav1` 44.5 dB, 10.2 MB, about 1 s; `libaom-av1` 44.0 dB, 9.3 MB, about 9 s. Both have 593 frames and identical audio, and both play through the game's dav1d path.
- **Runtime (`Engine\Movie`):**
  - Matroska demux: libwebm `mkvparser`, BSD.
  - AV1 decode: **dav1d**, BSD.
  - FLAC decode: dr_flac or libFLAC.
  - The YUV planes are uploaded to an SDL3 GPU texture and converted in a shader. Presentation is clocked to the audio, and skipping is the game's original behavior.
- **Validation:** frame N of the decoded MKV against a jpsxdec decode of the same frame, with a PSNR threshold of ≥ 45 dB (near-lossless), for the first 60 frames and every 300th frame after that.

### 6.8 Workshop recipes (SYD → CSV tables, #499)

`MENU\BLADE.SYD`, `SHIELD.SYD` and `ARMOR.SYD` are the workshop's combination recipes. Each one starts with a 12-byte header of three offsets: the pair matrix (the result record by both ingredients' record indices), the material matrix (the material two ingredients' materials give; blades and armor also index it by category) and the result records (16 bytes for a blade, 8 for a shield or armor piece). `VS.Extract tables <datadir>` (and `extract`) converts each file to three CSV tables under `catalogs\menu\` (`Extract/WorkshopRecipeConversion.hpp`, `Content/WorkshopRecipes.hpp`):

- `combination-<kind>-results.csv`: one row per result record, with a stable `id` (`blade-001`), its `legacy_id` (the record index the item records and the original's matrices use) and the named fields (model, category, gem slots or the blade's damage, cost and range fields, and the strength, intelligence and agility modifiers). The unnamed bytes are kept as `raw.byte7` and `raw.byte11` (§4.1).
- `combination-<kind>-pairs.csv`: `first,second,result`, all result ids, one row per ingredient pair that makes something.
- `combination-<kind>-materials.csv`: `first_material,[first_category,]second_material,[second_category,]material`, one row per combination that gives a material (1 wood to 7 damascus).

Empty matrix entries have no row. Each table's schema is committed in `Native\Content\schemas\catalogs\menu\` and written beside it, and a test checks that the two match. The manifest lists each kind as one output (`catalog.menu.combination-<kind>`, converter `syd-recipe-csv`) holding its six files and the source hash.

Conversion keeps edits. A rerun leaves the tables of an unchanged SYD alone, reconverts those whose SYD changed and with `--force` rewrites them all. The converter only writes tables that parse back to the decoded recipes and rebuild the original file byte for byte, so a disc that does not convert exactly is an error rather than a lossy output. At startup the data store reads and checks every table (types and ranges, every reference names a result, ingredients fit the pair matrix, `legacy_id`s run from 0 without gaps) and refuses to open on the first error, naming the file, line and column and telling the user to rerun `VS.Extract tables`. `RequireConversions` refuses to start when a SYD has no tables or its source hash is stale. The workshop builds its recipe book from the loaded tables (`Menu::Setup::BuildRecipeBook`, which rebuilds the original's dense matrices) and never reads the SYD. On the disc, the three files give 96 blade, 17 shield and 81 armor result records (record 0 of each is the empty one), with 8,100, 256 and 4,096 pairs and 2,025, 49 and 784 material rules.

Not yet done: compiling the tables into SQLite, the patch and mod layers, replacing the result fields with references to the item catalog when there is one, and an in-game combination replay (#58, #499).

---

## 7. Round-trip and validation (per converter)

Every converter ships with a test in `VS.Tests` (tagged `[extract]`; it runs only when a disc is configured):

| Converter | Test |
|---|---|
| Tier 1 tables | disc bytes → decoder → CSV/JSON → runtime loader → re-encode with the PS1 layout == the original bytes |
| workshop recipes | each SYD → decode → CSV tables → parse == the decoded recipes → re-encode == the original bytes (§6.8) |
| text | string bytes → markup → encode == the original bytes, for every string |
| SHP/SEQ/WEP | `decode(raw)` == `ModelLoader(glTF)` field by field (bones, vertices, UVs, colours, flags, every `extras` value, every keyframe) |
| ESQ/ETM actor bindings | decoded source == converted/reloaded skeletal and texture tracks, indexed pixels, palettes, patch rectangles, associations, timing and lifecycle commands; malformed or unresolved inputs fail explicitly |
| MPD | the same, via `RoomLoader` |
| textures | PNG + `.image.json` (+ `.palettes.png`, `.stp.png`) → indices and 16-bit texel/palette words == the original (exact), including `0x0000` vs `0x8000` and every STP bit |
| samples | WAV PCM == the ADPCM decode (exact); loop points equal |
| sequences | JSON → encode == the original AKAO bytes |
| XA | FLAC PCM == the ADPCM decode (exact) |
| movies | PSNR test (§6.7); frame count and duration equal |
| ENEMY catalogs | ENEMY_DATA_SCHEME §3 (expanded `extends` == the original record) |

Schema validation runs at load in every build. Round-trip tests run in CI where a disc is available, and locally through `test-native.cmd --extract`.

### 7.1 Separate preservation, preview and playback acceptance

| Evidence class | Required check | Does not establish |
|---|---|---|
| Data preservation | Exact comparisons of every consumed field/index/palette value, references and track timing; schema validation; dependency invalidation; deterministic second extraction. | Correct scheduling or rendering in a running scene. |
| Standard-tool preview | glTF validation and opening the exported model in Blender plus an independent standard viewer. Base appearance and skeletal clips work without VS-specific processing. Test an editor re-export through the native metadata validator; lost VS data is rejected explicitly. | Facial/event playback or lossless re-export by arbitrary editors. |
| Native behavior and visual fidelity | Actual production scene/event playback at matched event states, including patch contents, unaffected pixels, restoration, actor isolation and repeat-entry behavior. Compare named shots for lighting/blending; record exact frame/tick/state and differences. | Untested characters, scenes, render modes or full-game fidelity. |

Record source revision/dirty state, disc identity, tool/converter/schema versions, configuration, exact commands or UI actions, selected actor/bank/clip/palette, expected results, observed results and remaining limits. Keep disc-derived fixtures/captures local and out of commits or issue attachments. Converter round trips, viewer inspection and native/reference comparisons are independent acceptance records; none substitutes for another.

---

## 8. Manifest and versioning

`Native\data\manifest.json`:

```json
{
  "schema_version": 2,
  "extractor_version": "0.1.0",
  "converters": { "fbc-palette-png": 1, "fbt-texture-png": 1, "syd-recipe-csv": 1, "tim-png": 1 },
  "outputs": [
    {
      "id": "image.bg.001op01a",
      "source": "BG/001OP01A.TIM",
      "source_sha256": "…",
      "converter": "tim-png",
      "files": [
        "textures/bg/001OP01A.png",
        "textures/bg/001OP01A.image.json"
      ],
      "dependencies": [
        { "source": "BG/001OP01A.FAR", "source_sha256": "…" }
      ]
    },
    {
      "id": "image.effect.e017_1",
      "source": "EFFECT/E017_1.FBT",
      "source_sha256": "…",
      "converter": "fbt-texture-png",
      "files": [],
      "dependencies": [ … ],
      "alias_of": "image.effect.e013_1"
    }
  ]
}
```

- This is the current schema: every output has a stable `id`, primary source and hash, converter name, and output paths; dependencies include every other consumed input and its hash. Unknown members and paths outside the data root are errors. `files.tsv` remains the raw disc-file catalog.
- **Aliases** (schema version 2): an exact duplicate lists no files and names in `alias_of` an output of the same converter that has files and the same source hash. Readers load the target's files (`ConversionManifest::FilesOf`), and an edit to them applies to every alias. Effect FBT pages are the first aliased outputs (§6.4). Version 1 manifests, which have no aliases, still read.
- **Unused content:** valid content known to be unused is still converted and marked `"unreferenced": true` (for example `DBGFNT.TIM`). Skipped and unconverted output entries are not part of this schema.
- **Versions:** each converter has an integer version in code (`VS.Extract`). The runtime embeds the minimum version it requires for each converter, so an old `Native\data\` is rejected with instructions (§2).
- **Composite dependencies:** an output entry retains its primary `source` and adds a sorted `dependencies` array of `{ "source", "source_sha256" }` records for every other consumed input. Character outputs include the resolved SEQ/ESQ/ETM files and palette sources, not just SHP. A changed/deleted dependency or contributing converter/schema version invalidates the output; a missing dependency is an error, never a reason to reuse a stale model.
- **Character contract changes:** version ESQ/ETM conversion alongside the model converter and `asset.extras.vs.schemaVersion` (or its explicitly migrated extension equivalent). Runtime requirements must include the VS payload version; successful generic glTF loading is not an acceptable fallback for incompatible gameplay data.
- **Wrong disc:** the disc hash is checked against the known SLUS-01040 hashes; another release or a modified disc gives a warning, not a failure.

---

## 9. Tracking (ledgers and gates)

- **`CONTENT_LEDGER.csv`** (plan §4.5, #503) has one row per disc file. Its columns: `path, size, kind, decoder, converter, output, round_trip, status, notes`. `python tools\ledger\content.py --sync` rebuilds it from an extracted disc (`Native\data\files.tsv` and `manifest.json`); `--check` runs in CI without a disc and compares the ledger with the extracted data when there is one.
  - `converted`: the file is the source of a manifest output. Its converter must be one the runtime reads (`kRequiredConverters`). `round_trip=pass` needs every disc-corpus test `content.py` names for the converter to be a Catch2 test cited by a passing `VERIFY_LEDGER` record; otherwise `pending`.
  - `skipped`: a zero-byte placeholder, or a file §6.1 drops (`SYSTEM.CNF`, `ENDING\NULL.DAT`), with the reason first in `notes`; the other columns are `n/a`.
  - `unconverted`: everything else, noting the issue that converts it. A file that only feeds another conversion (a `BG\*.FAR` fixing its TIM's depth) stays `unconverted` until its own converter, and outputs written outside the manifest (sample banks, `ENDING.XA`, `TITLE.STR`; #504) do not count as converted.

<!-- content:begin (generated by tools/ledger/content.py --sync) -->

**1,040 of 5,162 disc files converted**, 1,040 of them with a passing round trip; 194 skipped with a reason; 3,928 unconverted.

| Kind | Files | Converted | Round trip pass | Skipped | Unconverted | Converts under |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| actor archetype | 455 | 0 | 0 | 0 | 455 | #200 |
| actor shape | 202 | 0 | 0 | 0 | 202 | #203 |
| actor texture overlay | 80 | 0 | 0 | 0 | 80 | #207 |
| area map | 32 | 0 | 0 | 0 | 32 | #201 |
| background layout | 4 | 0 | 0 | 0 | 4 | #210 |
| bestiary | 1 | 0 | 0 | 0 | 1 | #499 |
| boot configuration | 1 | 0 | 0 | 1 | 0 | - |
| combination recipes | 3 | 3 | 3 | 0 | 0 | #499 |
| effect package | 290 | 0 | 0 | 0 | 290 | #216 |
| effect palette | 277 | 277 | 277 | 0 | 0 | #215 |
| effect plugin overlay data | 120 | 0 | 0 | 0 | 120 | #502 |
| effect texture page | 755 | 755 | 755 | 0 | 0 | #214 |
| ending illustration | 11 | 0 | 0 | 0 | 11 | #498 |
| equipment model | 127 | 0 | 0 | 0 | 127 | #204 |
| event script | 512 | 0 | 0 | 0 | 512 | #202 |
| filler | 1 | 0 | 0 | 1 | 0 | - |
| help graphics | 14 | 0 | 0 | 0 | 14 | #213 |
| help page | 14 | 0 | 0 | 0 | 14 | #212 |
| layered artwork | 49 | 0 | 0 | 0 | 49 | #209 |
| main executable data | 1 | 0 | 0 | 0 | 1 | #499, #500, #502 |
| menu image, text or table | 11 | 0 | 0 | 0 | 11 | #498, #499, #500 |
| movie | 1 | 0 | 0 | 0 | 1 | #504 |
| music sequence | 192 | 0 | 0 | 0 | 192 | #501 |
| placeholder | 192 | 0 | 0 | 192 | 0 | - |
| presentation images | 22 | 0 | 0 | 0 | 22 | #211 |
| program overlay data | 20 | 0 | 0 | 0 | 20 | #500, #502 |
| room | 512 | 0 | 0 | 0 | 512 | #198 |
| sample bank | 101 | 0 | 0 | 0 | 101 | #504 |
| shared animation set | 97 | 0 | 0 | 0 | 97 | #206 |
| skeletal animation | 700 | 0 | 0 | 0 | 700 | #205 |
| sound-effect sequences | 102 | 0 | 0 | 0 | 102 | #501 |
| system images and font | 1 | 0 | 0 | 0 | 1 | #498 |
| TIM image | 5 | 5 | 5 | 0 | 0 | #208 |
| XA audio stream | 1 | 0 | 0 | 0 | 1 | #504 |
| zone | 256 | 0 | 0 | 0 | 256 | #199 |

<!-- content:end -->
- **`DATA_LEDGER.csv`:** each `catalog` row also names its output catalog file and decoder.
- **Character evidence:** use the existing content-ledger notes/evidence links to distinguish preservation, standard-tool preview and native playback results (§7.1). `round_trip=pass` proves preservation only. AP package status and remaining limits belong in the ordered issue/handoff; this plan does not promote any ESQ/ETM row to converted or behavior-verified.
- **Asset gate:**
  - 1a requires the extractor skeleton, the manifest, Tier 1 for the tables that 1b reads, and text.
  - **1b requires the title-slice assets:** title art, fonts, menu text, `TITLE.STR`, and the title music and its banks.
  - **The Phase 1 exit gate requires CONTENT_LEDGER at 100 %.** Every row must be `converted` + `round_trip=pass`, or `skipped` with a reason, and the manifest must have 0 `unconverted` entries.

---

## 10. Dependencies

| Use | Library | License | Where |
|---|---|---|---|
| JSON | nlohmann-json | MIT | VS.Content, VS.Extract |
| PNG read/write | lodepng (or libspng) | zlib / BSD | VS.Content, VS.Extract |
| AV1 decode | dav1d | BSD-2 | Engine\Movie |
| Matroska demux | libwebm | BSD-3 | Engine\Movie |
| CHD disc images | libchdr (overlay port) | BSD-3, with bundled LZMA SDK, miniz and zstd | Content\Disc |
| FLAC encode/decode | libFLAC (or dr_flac for decode) | BSD-3 / public domain | VS.Extract, Engine\Audio |
| AV1 encode, STR decode | ffmpeg (external executable, extract time only) | LGPL/GPL build. Not linked, not shipped | user machine |

All of these come from vcpkg (`vcpkg.json`). The VS 2026 vcpkg registry has `dav1d` 1.5.4, `libwebm` 1.0.0.32 and `libflac` 1.5.0. Libraries are added to `gen_solution.py`'s `VCPKG_LIBS` when the first converter or loader that needs them lands.
