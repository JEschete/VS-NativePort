# Asset Pipeline: flow, output layout and Tier 1 tables

[Asset pipeline index](../ASSET_PIPELINE.md)

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
- Conversion is **offline**. `VS.Extract extract` converts the supported source files during extraction; `VS.Extract images <datadir>` reruns the image converters, `VS.Extract backgrounds <datadir>` the FAR layouts (§6.4.1), and `VS.Extract tables <datadir>` the table converters (the workshop recipes, §6.8). The field-background runtime reads the converted PNG/sidecar and typed layout JSON. It does not parse or reconstruct TIM/FAR payloads; the remaining room/image binding and renderer compatibility limits are separate gates.
- **Startup check:** `VS.Desktop`, Combat Lab and Launcher validate `manifest.json` and the required converter versions. They reject missing/stale converted sources (TIM, FBC, FBT and SYD), dependencies or outputs, and workshop tables that fail their checks, with an instruction to rerun extraction. They never fall back to reading the source file.

Original-source validation is accelerated by a disposable `.cache/source-verification.json`: matching physical size,
file-clock stamp and expected SHA-256 may reuse a previously validated digest. Native authoring files are not covered
by this timestamp optimization. Full verification (`--verify-all` in Desktop/Combat Lab, always during launcher
preparation) rehashes everything. A full read/hash checks the source stamp again before it can be cached; physical
lengths must equal `files.tsv`, not merely contain a matching prefix. All required outputs are still checked on every
run. Cache publication happens only after success and does not make a read-only data tree unusable. See
[BUILDING.md](../BUILDING.md#6-run) and #524/#525. Neither this cache nor the temporary source-tree dependency is
native adoption.

## 3. Output layout (`Native\data\`, git-ignored)

```
Native\data\
  manifest.json
  catalogs\          Tier 1: executable / PRG tables (§4); flat tables as <table>.csv + <table>.schema.json
    items\  materials.csv + .schema.json (G01905, §4.2)
            blades.csv grips.csv gems.csv armor.csv shields.csv accessories.csv consumables.csv misc.csv ...
    battle\ break-arts.csv abilities.csv spells.csv status-effects.csv
    enemies\ species.csv enemies.csv modelsets.csv loot.csv bestiary.csv      (ENEMY_DATA_SCHEME.md)
             zone-enemies.csv zone-enemy-limbs.csv zone-enemy-parts.csv   (ZND templates, §6.9)
    world\  zones.csv rooms.csv doors.csv keys.csv chests.csv save-points.csv puzzle-challenges.csv
             zone-rooms.csv zone-textures.csv                              (ZND, §6.9)
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
    animation-sets\<ESQ>.json      (shared animation sets: the bank each model number or kind plays, §6.2.1)
    weapons\<WEP>.gltf             (+ .bin, texture image files; material variants = blade materials, §6.2.4)
    armor\ shields\ ...            (as discovered per CONTENT_LEDGER)
    zud\<ZUD>.json                 (enemy archetype: names SHP/WEP/SEQ outputs; a part no OBJ file holds stays beside it, §6.2.5)
  rooms\<zone>\<MAPxxx>.gltf       (room geometry + textures)  + <MAPxxx>.room.json (collision, tiles, doors, scripts)
  rooms\automap\<ARM>.json         (minimap scenes per area: display geometry, doors, room names by string id)
  textures\<dir>\<name>.png        (TIM/GIM/FBT/standalone images, §6.4) + <name>.image.json
                                   (+ <name>.palettes.png when an image has several palettes)
    actors\<ETM>\<MODEL>.png       (actor-bound texture regions + .image.json, §6.2.1, §6.2.2)
    zone\<ZONE>\<nn>.png           (a zone's texture table TIMs + .image.json, §6.9)
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
  movies\<name>.mkv                (planned; today disc\MOV\TITLE.STR.mkv: AV1 video + FLAC audio, §6.7)
```

The exact file list is generated: `CONTENT_LEDGER.csv` (§9) gives every disc file its output path(s).
`ENEMY_DATA_SCHEME.md`, cited for the enemy catalogs, was removed from the repository; read the
[archived copy](https://github.com/JEschete/VS-NativePort/blob/0e3ae7d8df169bc684c8155fa44b90e731bb4e69/Docs/ENEMY_DATA_SCHEME.md).

---

## 4. Tier 1: executable and overlay tables → CSV tables and JSON catalogs

**Source:** initialized data in `SLUS_010.40` and the `*.PRG` overlays. `build\port\data.json` (`tools\port\data_inventory.py`) lists them, and `DATA_LEDGER.csv` gives each one a disposition. Only `catalog` rows go through this tier. `constexpr` rows are computed in code, `state` rows become zeroed module state and `drop` rows disappear.

Flat, row-shaped tables (items, actions, spells, status, recipes, enemies, loot, spawns, challenges) are written as CSV (§1.1). Data with nested structure (title layout, effect template parameters) is written as JSON.

**Target: schema-driven generated decoders, not hand-written authoring layouts.**
`tools\port\gen_table_decoders.py` is a proposed tool, **not an implemented command**. The current SYD/ZND
and model codecs are hand-written preservation/import implementations; their byte-exactness does not
satisfy this authoring contract. #499 and the per-format issues must establish shared, enforceable
schema/domain rules rather than copying the opaque dump layouts.

1. The planned generator reads the decomp struct definitions (libclang, PS1 layout: 32-bit pointers, 4-byte alignment, GCC bitfield order) and reviewed semantic schemas. For each catalog table it must emit:
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
5. Primary definition IDs carry meaning (`blade.battle-knife`, `blade.scramasax`), not just row positions.
   Shared definitions and distinct variants must be modeled deliberately. Initial migration must update every
   reference and retain an explicit old-ID mapping; changing a name later must not silently change a persisted identity.
6. Where meaning is known, schemas provide machine-readable domains and files store names: `bronze`, `piercing`,
   named status flags, and named instruction arguments. Numeric quantities and topology indices remain numbers with
   documented units/roles. Packed words are split only by proven semantics; an AI settings word is not assumed to be only flags.
7. Name column families by their mapped meaning, not ordinal position (`affinity_fire`, `class_undead`, `type_edged`).
   Reserved/unused elements require explicit classification; do not guess a name from an array index.
8. Combination data uses semantic ingredient/result references or an editable grid view of the same source. Keep
   directed pairs directed: reversing ingredients need not give the same result. Deduplicate or derive records only
   after proving the source relationship and preserving instance state.

**Native catalog conventions (§4.2–§4.7).** `VS.Extract tables`, `authoring` and full `extract` import every
family; most also have their own `VS.Extract <family> <datadir>` command. Rows lead with a semantic `id` and the informational
`display_name` (§1.2); `legacy_id`, where present, is the explicit compatibility map to the original index.
Each table's columns, enums and flag bits are described by its committed schema under `Native\Content\schemas\`.
`catalogs\<group>\import\<family>.json` holds the original bytes the catalog does not keep: it serves offline
reconstruction only, which never consults edited CSVs; runtime never reads it or repacks a record. Base, patch and
mod layers compile through the layered SQLite/cache (§6.8) into immutable typed definitions, separate from mutable
instance state. A missing or invalid catalog refuses as each section states; it never yields an all-zero rule or
falls back to a legacy read.

### 4.1 Lossless "extras"
If a value has no clean meaning yet, it is **kept**, not dropped. In a CSV table it is a `raw.<field>` column; in JSON it goes under `"_raw": { "<field>": value }`. Either way it is listed in `TYPE_LEDGER` as unnamed. Unnamed fields block the 1a gate, so no raw field may remain at the end of Phase 1.

These are temporary preservation representations, not final author-facing columns. Quarantine imported unknown
bytes/unused padding in one documented provenance representation rather than mixing `raw.byte7`, `raw_33` and
`raw_5a` into editable definitions. Unknown **consumed** semantics block authoring-format completion; moving them
to a sidecar or adding `unknown_` names does not satisfy the gate.

### 4.2 Native material definitions (#499)

`VS.Extract tables <datadir>`, `authoring <datadir>` and full `extract` import G01905 from **only** `SLUS_010.40`
offset `0x3F5DC`, eight 30-byte records (`resources\rood-reverse\src\SLUS_010.40\main.c:350` and `modifiers.h`
define the table and fields). Converter `material-csv` version 1 registers `catalog.items.materials`, writing
`catalogs\items\materials.csv`; its columns are described in
[materials.schema.json](../../Native/Content/schemas/catalogs/items/materials.schema.json). The executable remains
unconverted in the file-level content ledger; the note records this partial extent.

- **Identities:** `material.none`, `.wood`, `.leather`, `.bronze`, `.iron`, `.hagane`, `.silver` and `.damascus`.
  `legacy_id` maps them to the eight existing inventory, save and palette kinds. Missing, duplicate or mismatched
  identities are errors; additional kinds require #58's instance/appearance/persistence expansion.
- **Widths:** modifiers are signed 32-bit native quantities and `max_dp` is unsigned 32-bit, not the import's
  byte/halfword widths. The two reserved bytes per record live only in `catalogs\items\import\materials.json`, with
  source hash, byte offset, record size and material identity. Offline encoding checks its own legacy limits and
  preserves those bytes.
- **Loading:** the layered SQLite compiler/cache loads this table with any listed workshop tables, validates typed
  records before publication and recompiles a cache whose material records fail validation. Raw/synthetic stores and
  initial extractor bootstrap need no material outputs; production conversion checks require them when the
  executable is present. A missing live catalog refuses workshop combination; it never supplies zero bonuses.
  Unchanged conversion preserves authored values and labels, including quantities the legacy format cannot hold;
  `tables --force` explicitly reimports; layer loading never changes base/import files.
- **Consumers:** the workshop reads typed primary-stat bonuses from the catalog; G01905's `ModuleData` copy,
  `ModulePointers` entry and raw storage are removed. Palette data stays separate. Class, affinity and base-DP fields
  are preserved definitions, not new workshop calculations: combination keeps its ingredient affinity and
  averaged-DP rules.
- **Arithmetic:** original MENUC sums byte recipe stats with material fields and stores a byte (its compiler uses
  low-byte loads of the signed halfword fields). The native sum is signed 64-bit: modifier **150** plus recipe
  **-40** reaches the workshop as **110**. Results outside signed-byte inventory storage are refused, a #58 storage
  constraint, **not a gameplay clamp**. The original class/affinity fusion clamp at -100..100 is unchanged.

Open work: #499, #58.
