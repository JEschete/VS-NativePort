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

**Runtime adoption is not byte reconstruction (owner clarification,
2026-11-23).** Rebuilding ZND/ZUD/SHP/SEQ/ESQ/WEP/ETM bytes in memory, or keeping
SYD's byte/index matrices, is temporary compatibility work. The active adapters,
owning issues and retirement gates are in the
[EC-04 T-shim ledger](NATIVE_SOURCE_PORT_PLAN.md#ec-04-asset-t-shim-ledger).
Production must read typed definitions and construct owned native instances.
Byte-exact reconstruction belongs only to offline preservation tests of imported
originals. A new native definition may be impossible to pack into a PS1 file and
must still be usable; gameplay policy and file-format limits are different.

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
| 7 | Unused content | Valid content with no reference (e.g. `DBGFNT.TIM`) is **converted** and marked `unreferenced` in the manifest, so modders can use it. Empty files are `skipped` with a reason; exact duplicates are recorded as aliases of one output. All four FAR backgrounds have verified MPD references (§6.4.1); the earlier `007OP01A` example was incorrect. |
| 8 | Short palette files | Palette rows past the end of a file (FBC) are **not stored**. The runtime fills them with zero (transparent), as the zero sector padding after each file gave the original. Stale bytes are not reproduced: the only stale row was slot 0's fifth, past the one sector read, and no sprite samples it. A disc test shows the only sprites that sample past their palette are E210's descriptors 520-544 (row 242, zero in both). |
| 9 | Layout | §3. Patch and mod layers mirror it and override by path (assets) or by ID (tables), plan §9.6. |
| 10 | Coverage gaps | Formats with no migration issue get one (images in menus, ending, `SYSTEM.DAT` and PRG data; the gameplay catalog; the string table; AKAO sequences; PLG/PRG data blobs). |

### 1.2 Readable names (owner, 2026-11-23, plan D26, #520)

**The owner chose §4's semantic authoring contract.** This annotation pass is
preparatory compatibility work, not the destination or a format-completion
gate. Positional/filename IDs and encoded fields in the current outputs still
need the explicit schema/consumer migrations below. A readable label does not
make a packed numeric field or unnamed command stream an authoring format.

`display_name` is an optional **informational** plain-text label. It is not an
ID, a reference, or a replacement for an encoded gameplay name:

- CSV tables put it immediately after their first identity/key column.
  Schemas describe it as optional text. `ReadTable` excludes it from semantic
  cells, so existing numeric column positions and key/reference checks do not
  change. Unlabeled tables remain supported.
- Native JSON puts it early in the object. Strict readers accept this one
  reserved annotation as a string; other unknown fields are still errors.
  Native glTF puts it at the start of `asset.extras.vs`, not in an invalid
  custom root property. Joint names, geometry and animation values do not change.
- Image sidecars identify PNGs; bank/stream metadata identifies WAV/SFZ and
  streamed audio. Binary payloads (`.bin`, PNG, WAV, SFZ, FLAC, MKV) are not
  rewritten just to attach text. Conversion manifests and schemas remain
  operational descriptions, not additional copies of author-facing names.

The `labels` command deliberately does not rename existing keys or replace
encoded values. The subsequent §4 migrations must introduce semantic IDs and
named values, update their references/consumers together, and preserve legacy
ID mappings and source bytes explicitly. IDs remain stable after that migration;
editing a display name must not implicitly rename an identity.

`VS.Extract labels <datadir>` annotates an existing native tree **without
reconverting it**. Successful `extract`, `catalogs`, `images`, `models`, `tables`,
`banks` and `streams` commands also run this pass. It uses the current native
table values, the extracted item-name table when available, and the manifest's
verified model/archetype references. It does not need or modify the disc image.

The first pass adds labels; later passes preserve existing labels and gameplay
edits. `labels <datadir> --force` recomputes **only labels**, never HP, equipment,
placements, encoded names, geometry, palettes or other gameplay data. Each file
is replaced through the existing staged writer; unchanged files are not
rewritten. Errors identify the file and return failure. Completed files remain
valid, and the same command can resume after a failure. Labeling is not a claim
that an unconverted format has been converted or that all content is ready.
Explicit reconversion commands retain their existing overwrite rules; the
labeling pass does not protect an asset from a requested converter `--force`
or a catalog rebuild.

Current name sources:
- Enemy and weapon display names decode the bounded English-edition ZND name
  fields. E7 ends the name; trailing padding is ignored only for the label and
  remains in the original hex field. Accents and punctuation are preserved.
  Layout controls are omitted; unknown glyphs/controls remain visible markers.
  This is not the lossless text/markup converter still required by #500.
- Equipment parts and workshop results use `MENU/ITEMNAME.BIN` where a name is
  available. Ingredient pairs and material rules describe their inputs/results.
- ZUD references carry enemy names to their shared/owned model files. Shared
  models keep the applicable names in sorted order rather than choosing an
  arbitrary enemy and collapsing variants.
- Assets without a known canonical name use an explicit source/role or ID
  label. Such labels are descriptions, not invented names or verified room,
  animation, sound or effect semantics.

**Required for later converters and adoption work:**

| Work | Name requirement still to apply/refine |
|---|---|
| #199 relational species, variants, loadouts and loot | Early names on definitions and useful join rows; keep distinct variants and encoded/string-ID authority. |
| #198 MPD and #201 ARM | Resolve actual room/area names and introduce semantic room/area IDs with an explicit mapping from old keys; update exits, events, saves and other references together. |
| #499 items, actions, status, bestiary and other tables | Early labels from source names/string IDs; recipes keep their ingredient/result descriptions. |
| #500 text and #202 EVT | Readable string/event labels alongside stable IDs and lossless markup/provenance, not a competing authoritative text field. |
| #209 GIM, #210 FAR, #211 DIS, #498 embedded/UI art | Label artwork, layers/layouts and challenge/result rows by source purpose and verified associations. |
| #212 HF0 and #213 HF1 | Label pages, topics and diagrams with decoded titles and page associations. |
| #216 effects and #502 module data | Label effect definitions/resources and tables by verified behavior/purpose; keep unknown meanings explicit. |
| #501 music/SFX sequences | Label sequences and tracks through verified music/SFX associations; do not invent track titles. |
| #203/#205/#206/#207 character inspection | Refine source-only model/clip/set/texture labels as verified character and animation associations become available. |

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
- Conversion is **offline**. `VS.Extract extract` converts the supported source files during extraction; `VS.Extract images <datadir>` reruns the image converters, `VS.Extract backgrounds <datadir>` the FAR layouts (§6.4.1), and `VS.Extract tables <datadir>` the table converters (the workshop recipes, §6.8). The field-background runtime reads the converted PNG/sidecar and typed layout JSON. It does not parse or reconstruct TIM/FAR payloads; the remaining room/image binding and renderer compatibility limits are separate gates.
- **Startup check:** `VS.Desktop`, Combat Lab and Launcher validate `manifest.json` and the required converter versions. They reject missing/stale converted sources (TIM, FBC, FBT and SYD), dependencies or outputs, and workshop tables that fail their checks, with an instruction to rerun extraction. They never fall back to reading the source file.

The current original-source validation is accelerated by a disposable
`.cache/source-verification.json`: matching physical size, file-clock stamp and
expected SHA-256 may reuse a previously validated digest. Native authoring
files are not covered by this timestamp optimization. Full verification
(`--verify-all` in Desktop/Combat Lab, always during launcher preparation)
rehashes everything. A full read/hash checks the source stamp again before it
can be cached; physical lengths must equal `files.tsv`, not merely contain a
matching prefix. All required outputs are still checked on every run. Cache
publication happens only after success and does not make a read-only data tree
unusable. See [BUILDING.md](BUILDING.md#6-run) and #524/#525.
Neither this cache nor the temporary source-tree dependency is native adoption.

---

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

`ENEMY_DATA_SCHEME.md`, cited in this document for the enemy catalogs, was removed from the repository; read the
[archived copy](https://github.com/JEschete/VS-NativePort/blob/0e3ae7d8df169bc684c8155fa44b90e731bb4e69/Docs/ENEMY_DATA_SCHEME.md).

---

## 4. Tier 1: executable and overlay tables → CSV tables and JSON catalogs

**Source:** initialized data in `SLUS_010.40` and the `*.PRG` overlays. `build\port\data.json` (`tools\port\data_inventory.py`) lists them, and `DATA_LEDGER.csv` gives each one a disposition. Only `catalog` rows go through this tier. `constexpr` rows are computed in code, `state` rows become zeroed module state and `drop` rows disappear.

Flat, row-shaped tables (items, actions, spells, status, recipes, enemies, loot, spawns, challenges) are written as CSV (§1.1). Data with nested structure (title layout, effect template parameters) is written as JSON.

**Target: schema-driven generated decoders, not hand-written authoring layouts.**
`tools\port\gen_table_decoders.py` is a proposed tool, **not an implemented
command**. The current SYD/ZND and model codecs are hand-written preservation/
import implementations; their byte-exactness does not satisfy this authoring
contract. #499 and the per-format issues must establish shared, enforceable
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

5. Primary definition IDs carry meaning (`blade.battle-knife`,
   `blade.scramasax`), not just row positions. Shared definitions and distinct
   variants must be modeled deliberately. Initial migration must update every
   reference and retain an explicit old-ID mapping; changing a name later must
   not silently change a persisted identity.
6. Where meaning is known, schemas provide machine-readable domains and files
   store names: `bronze`, `piercing`, named status flags, and named instruction
   arguments. Numeric quantities and topology indices remain numbers with
   documented units/roles. Packed words must be split according to proven
   semantics; an AI settings word is not assumed to be only flags.
7. Name column families by their actual mapped meaning, not ordinal position
   (`affinity_fire`, `class_undead`, `type_edged`). Reserved/unused elements
   require explicit classification; do not guess a name from an array index.
8. Combination data uses semantic ingredient/result references or an editable
   grid view of the same source. Keep directed pairs directed: do not assume
   reversing ingredients gives the same result. Deduplicate or derive records
   only after proving the source relationship and preserving instance state.

### 4.1 Lossless "extras"
If a value has no clean meaning yet, it is **kept**, not dropped. In a CSV table it is a `raw.<field>` column; in JSON it goes under `"_raw": { "<field>": value }`. Either way it is listed in `TYPE_LEDGER` as unnamed. Unnamed fields block the 1a gate, so no raw field may remain at the end of Phase 1.

Those are temporary preservation representations, not final author-facing
columns. Quarantine imported unknown bytes/unused padding in one documented
provenance representation rather than mixing `raw.byte7`, `raw_33` and
`raw_5a` into editable definitions. Unknown **consumed** semantics block
authoring-format completion; moving them to a sidecar or adding `unknown_`
names does not satisfy the gate.

### 4.2 Native material definitions (#499)

`VS.Extract tables <datadir>`, `authoring <datadir>` and full `extract` import
G01905 from **only** `SLUS_010.40` offset `0x3F5DC`, eight 30-byte records.
Converter `material-csv` version 1 registers `catalog.items.materials`, writing
`catalogs\items\materials.csv` and its schema. The containing executable remains
unconverted in the file-level content ledger; the note records this partial
extent. `resources\rood-reverse\src\SLUS_010.40\main.c:350` and `modifiers.h`
define the original table and fields.

The identities are `material.none`, `.wood`, `.leather`, `.bronze`, `.iron`,
`.hagane`, `.silver` and `.damascus`, with readable `display_name` metadata.
`legacy_id` is an explicit compatibility map for the eight existing inventory,
save and palette kinds. Missing, duplicate or mismatched identities are errors;
additional kinds require #58's instance/appearance/persistence expansion.

The editable fields are:
- `class_human/beast/undead/phantom/dragon/evil`: class modifier points;
- `attack_affinity_` and `defense_affinity_` families for `physical`, `air`,
  `fire`, `earth`, `water`, `light`, `dark`: affinity modifier points;
- `max_dp`: internal durability units, 100 per displayed DP;
- `strength`, `intelligence`, `agility`: additive primary-stat points.

Modifiers use signed 32-bit native quantities and `max_dp` uses unsigned 32-bit,
not the import's byte/halfword widths. The two reserved bytes per original
record live exclusively in `catalogs\items\import\materials.json`, with source
hash, byte offset, record size and material identity. Offline encoding checks
its own legacy limits and preserves those bytes; runtime does not read this
provenance or repack a material record.

The existing layered SQLite compiler/cache loads this table alongside any
listed workshop tables, validates typed records before cache publication, and
recompiles a cache whose typed material records fail validation. Raw/synthetic
stores and initial extractor bootstrap need no material outputs. Production
conversion checks require them when the executable source is present; a missing
live catalog refuses workshop combination, never supplies zero bonuses.
Unchanged conversion preserves authored values and labels, including quantities
that cannot be exported to the legacy format. `tables --force` explicitly
reimports; normal layer loading never changes base/import files.

The workshop now reads typed material primary-stat bonuses from the catalog.
G01905's `ModuleData` copy, `ModulePointers` address entry and raw storage are
removed. Palette data stays separate. Class, affinity and material-base DP
fields are preserved definitions, not newly added workshop calculations:
combination retains its existing ingredient affinity and averaged-DP rules.

Original MENUC code sums byte recipe stats with material fields and stores a
byte. Its MIPS compiler uses low-byte loads even though the material source
fields are signed halfwords. The native sum uses signed 64-bit arithmetic.
A modifier of **150** plus recipe **-40** reaches the live/core workshop path as
**110**. Final results outside signed-byte inventory storage are explicitly
refused, a remaining #58 storage constraint, **not a gameplay clamp**. The
separate original class/affinity fusion clamp at -100..100 is unchanged.
Controller-driven workshop/action parity and broader #499/#58 acceptance remain
open; typed-host tests are not a substitute.

---

### 4.3 Native encyclopedia definitions (#499/#500)

`VS.Extract bestiary <datadir>` (also `tables`, `authoring`, and `extract`)
imports `SMALL\MON.BIN` into `catalogs\enemies\bestiary.csv` and its schema.
`bestiary.*` IDs identify encyclopedia entries, **not spawned enemies**.
Names and descriptions reference the shared native text catalog by ID.
The layered SQLite compiler owns validation and immutable runtime records.

The original file contains 150 44-byte records and a 150-entry description
offset table. Only records **0..77** enter the original list, selection and
previous/next navigation; the detail viewer can only select those entries.
Consequently the other **72 records are out-of-list provenance**, not invented
enemy definitions. Their zero fields, Japanese-name bytes and empty
descriptions are preserved exactly without assigning them unproved meanings.
`catalogs\enemies\import\bestiary.json` retains all 150 original records and
description extents, including initial UI values, reserved/name-suffix bytes
and padding. Reconstruction is an **offline original-preservation check**;
runtime never reads this provenance or reconstructs MON.BIN.

Editable columns:

- `id`, `name`, `description`: stable definition and native text references;
- `legacy_id`: original visible record 0..77, or **-1 for native-only entries**;
- `display_order`: unique sort position; numbering is dense in the resulting
  list. The three-digit view deliberately admits at most 999 entries;
- `class`: `human`, `beast`, `undead`, `phantom`, `dragon`, or `evil`;
- `preview_table_index`: an explicit 0..255 **MENU9 preview-table adapter**,
  not an actor/species identity. It selects that existing table's model,
  equipment and palette policy. #200/#55 still own its replacement;
- `unlock`: `defeated-subtype-range` tests any saved defeat bit in the
  half-open range `[first_defeated_subtype, first + count)`. Those indices are
  **actor subtypes**, not MON record numbers or preview indices. The original
  defeat producer writes subtypes below 256. `always` explicitly exposes a
  native information entry with both range fields zero; neither policy
  writes progress or implies a new enemy spawn.

Proven semantics come from upstream MENU9 `238.c` (definitions 372, unlock
1921, list 1977, detail 2074), preview `592C.c` (table 28), and BATTLE
`146C.c:1437` (defeat-bit producer). Extending actor identities or their
persistent defeat model remains #199/#200/#203, not an inferred field meaning.

The Gazette owns its snapshot, selection/highlight/link state and native
layouts separately from immutable definitions. Cancel, re-entry, MENU9
replacement, cold-session and pre-heap boundaries release that ownership.
Native cursor/link indices are not bytes; the old BATTLE byte cursor remains
an explicitly limited compatibility bookmark. Native text has no 28-byte
name limit. List text uses a 220-pixel content box and a wider background;
the viewer gives the name its own lower-left position. These view bounds and
the eight-row/999-entry policy are deliberate, not container limits.

Normal imports preserve authored CSV/text and foreign text contributors.
`bestiary --force` reimports only this family, not save prompts or neighboring
tables. Missing definitions, bad references, invalid ranges or layouts refuse
entry rather than falling back to a legacy read. Model preview, actor assets,
progress storage and the GPU page/packet backend remain tracked dependencies.
This is not completion of the wider #499/#500 or neighboring migration issues.

### 4.4 Native learning requirements (#499)

`VS.Extract learning <datadir>` imports two immutable learning tables; `tables`,
`authoring` and full extraction include the same converter. `learning-csv` v1
owns `catalog.battle.learning`, the two CSV/schema pairs under `catalogs\battle\`,
and `catalogs\battle\import\learning.json`. Base, patch and mod layers compile
through the existing SQLite/cache and publish checked `Content::LearningCatalog`
definitions. The battle and both menu readers use those definitions directly:
there is no reconstructed halfword table, module copy or address binding.

| Table | Identity and meaningful order | Editable quantity |
|---|---|---|
| `break-art-learning.csv` | `learning.whistle-sting`, etc.; named weapon category, `first`..`fourth` learning tier, named action reference | `required_weapon_kills`: cumulative qualifying kills in that category, **not** an incremental price |
| `battle-ability-learning.csv` | `learning.first-battle-ability-choice`, etc.; `choice_order` 1..22 is the next opportunity to choose an unlearned chain/defense ability, **not** an action ID | `required_ability_points`: cumulative successful offensive/defensive timing credit, despite the retained save name `total_kills` |

Labels appear immediately after IDs. Categories are bare hands, dagger, sword,
great sword, axe/mace, great axe, staff, heavy mace, polearm and crossbow. The schema's
named enum codes explicitly adapt the retained equipment/progress indices.
Names reuse the existing `GameplayDomains.hpp` blade-category domain, with only
category zero renamed for its learning role.
Equipment category 10 maps modulo 10 to **bare hands**, not material `none`.
The supported zero equipment alias normalizes to category 10 for the action
and notification name, while still reading progress category zero.
Action codes 184..219 belong to weapon categories 1..9; 220..223 are the four
bare-hands arts. Named action references, category/tier and stable identities
must agree. Unsupported identities, missing definitions, duplicate keys/orders,
bad values and mismatched references refuse publication with file/row/column
diagnostics. General mod-defined action/category/progress identities await
#499/#58; this slice does not invent additional compatible action slots.

Thresholds are nonnegative signed 32-bit native quantities, including values
above 65535. No monotonic rule is imposed: production checks the next tier,
and an authored lower later threshold can become eligible immediately.
Both next-points displays use safe decimal arithmetic and retain `max(0,
requirement-progress)` presentation. More than five remaining-point digits
use a single right-aligned `NEXT <value>PT` run instead of overlapping the
original label/value slots; stock positioning is unchanged. Mutable kills, learned counts, chosen
actions, notification phases and saves remain separate. #183's owner policy
still banks successful ability timing, including lethal hits and pending
rewards, saturating at 65535; only a qualifying death starts learning.
Weapon-kill awards cannot wrap: if another point is required but the existing
uint16 counter cannot hold it, the award reports refusal and rolls back its
kill-counted mark. It does not truncate the threshold or invent a new clamp.
Changing persistent progress width is separate work.

**Precise original coverage:** pinned `src/SLUS_010.40/main.c:722,735,751,755`
and the configured executable agree on 80 requirement bytes at `0x405E4`,
40 unused `artsActionIds` bytes at `0x40634`, 44 requirement bytes at `0x4065C`,
then 256 `actorExtraShp` bytes at `0x40688`. The previous G01916/G01917 copies
incorrectly treated the 120/300-byte address gaps as table extents. Only **40
plus 22 requirements** become authoring definitions. Original CSV snapshots
and the two separately named neighboring byte arrays reload independently of
authored edits and reconstruct all 420 bytes **offline**. G01918's actor-shape
consumer remains unmigrated; this is not whole-executable conversion.
The placeholder actor's terminated name no longer reads a fake four-byte
threshold tail; native padding is zero and the original bytes remain offline.

Normal reruns preserve edited files and timestamps. Missing owned schemas are
repaired without replacing existing owned CSVs. Foreign output files are
refused even with `--force`; a changed source requires explicit force before
replacing owned content. Existing material, bestiary, text/font, FAR and save
assets keep their ownership. See HANDOFF for exact local checks and remaining
controller/CI limits; threshold edits are authored content, not earned progress.
The generic label pass also leaves native text/font and import-only schemas to
their semantic owners, including refresh runs (#526). It does not label a shared
native text catalog with the name of one contributing executable.

### 4.5 Native status rules and shared timing (#499)

The adopted contract is verified in HANDOFF ("Native status catalog adoption").
Both tables lead with a plain-text `display_name` after the semantic `id`, and
the committed schemas under `Native\Content\schemas\catalogs\battle\` must
equal the generated schema for each table spec (a Catch2 test compares them).

`VS.Extract status <datadir>` (also `tables`, `authoring` and full extraction)
imports **only G01906**, the 32 sixteen-byte records at SLUS offset `0x3F6CC`.
The `status-csv` bundle is `catalog.battle.status`:

- `catalogs\battle\status-rules.csv`: semantic rule IDs and named
  `effect_channels`, `replaces`, `blocked_by` sets.
- `catalogs\battle\status-timings.csv`: six named shared duration/pulse roles.
- Matching schemas define named flag members with explicit original bit
  mappings. Sets use `|`, or `none`; numeric masks, unnamed bits, duplicate
  members and unknown names are not native authoring syntax.
- `catalogs\battle\import\status.json` is offline provenance for **every word
  of every original record**, including unused/duplicate durations. Original
  reconstruction never consults edited CSVs and is not a runtime path.

Timing is measured in **eligible status updates**, not wall-clock seconds:
the live vitals loop calls the status updater at counter multiples of 60,
only for an eligible living actor. AI estimates the same poison/regeneration
intervals in its planning-time calculation.

| Authored role | Actual state affected | Original source |
|---|---|---|
| `timing.attribute-modifiers` | Strength, Intelligence and Agility up/down share timer 0 | Row 5 duration low byte |
| `timing.movement-modifiers` | Quicken and impaired legs share timer 1; recovered legs can clear earlier | Row 11 duration low byte |
| `timing.equipment-and-elements` | Equipment up/down and all eight elemental enchantments share timer 2 | Row 19 duration low byte |
| `timing.paralysis-and-numbness` | Separate timers 3/4, both initialized from this one definition | Row 13 duration low byte |
| `timing.poison-pulse` | HP pulse timer 5; poison wins if both effects are present | Row 14 interval |
| `timing.regeneration-pulse` | Same HP pulse timer 5, not a separate regeneration timer | Row 17 interval |

The remaining row durations are **not independently effective**. In particular,
equipment-up/element durations and numbness's own row duration are preserved
offline, not advertised as working per-rule controls. The retained actor/save
timers are bytes: definitions above 255 refuse; zero duration means no expiry
countdown, while HP pulse intervals must be positive. Original AI reads the
full interval halfword but lifecycle uses its low byte. Non-stock originals
where these disagree remain exactly reconstructible offline but refuse native
adoption; there is no silent narrowing or new actor sidecar.

The last three bits were resolved by **consumers and producers**, not position:

- 29 is **Analyze**. Original `func_80083D7C` queues it and independently marks
  the target analyzed on execution, even if the rule has no effect.
- 30 is the **instant-defeat channel** checked by the undead/living
  prerequisites and removed by the effect that requests all current HP.
  It is not a timed “Spirit Drain” ailment.
- 31 is **spell immunity**, not an unidentified reserved flag. Enemy
  initialization copies this resistance bit into active statuses;
  `doesAttackHit` zeroes a spell's threshold and `func_80085390` suppresses
  spell dispatch. No fixed original `applyStatus(31)` caller was found.
  The retained rule record does not author enemy spawn resistance.

The source is pinned decomp `c9a869b8919efc561894fb6a851b300791553bd3`:
`main.c:524`, `BATTLE/BATTLE.PRG/146C.c:6085`, `:10072`, `:10429`,
`:11129`, `:12287`, `:12432`, `:12858`, `:13697`, `:14243`, `:14565`,
and `74088.c:591`. The original binary confirms the initialization, sign-bit
spell tests, seven LBU-to-SB timer assignments and AI's LHU interval reads.
Legacy callback names containing “Banish” remain call-boundary compatibility
names; they do not define the catalog's meaning.

Definitions use the existing layered SQLite/cache, independently of mutable
actor statuses/timers and module resets. G01906's module copy/address/storage
is removed; other globals beside it are unchanged. This is **not** action,
enemy-resistance, actor/save-width or whole-executable adoption. Current local
validation and remaining controller/reference limits belong in HANDOFF.

### 4.6 Native action categories (#499)

The adopted contract is verified in HANDOFF ("Native action category catalog
adoption"). `VS.Extract action-categories <datadir>` (also `tables`,
`authoring` and full extraction) imports the sixteen category records that
BATTLE.PRG stored as five parallel byte tables (needs-selection `0x7F984`,
area `0x7F994`, includes-user `0x7F9B4`, body-part `0x7F9C4`, target-kind masks
`0x7F9D4`; the sixteen bytes at `0x7F9A4` are zero padding). The
`action-category-csv` bundle is `catalog.battle.action-categories`:

- `catalogs\battle\action-categories.csv`: `action-category.<slug>` rows with
  a plain-text `display_name` and the named columns `selection`
  (`automatic`/`chosen`), `extent` (`single`/`area`), `includes_user`
  (`no`/`yes`), `aim` (`actor`/`limb`) and `target_kinds`, a `|` set of
  `leader`, `neutral`, `hostile`, `peaceful`, `object`, `cloudstone` and
  `chest` (or `none`).
- The matching schema defines each enum and flag with its original bit; the
  committed copy must equal the generated schema (a Catch2 test compares them).
- `catalogs\battle\import\action-categories.json` holds the five original
  bytes of every record for offline reconstruction only; reconstruction never
  consults edited CSVs and is not a runtime path. The importer refuses a source
  whose padding bytes are not zero or whose mask uses the unnamed `0x40` bit
  instead of narrowing it, and authoring cannot spell either.

Live targeting, camera selection and action-result code read the checked
catalog through `RequireLiveActionCategory`. A missing catalog logs and aborts
like the status catalog; it never yields an all-zero rule. The module data
globals G00019..G00023 and their address bindings are removed.

Actions name their category by identity in the action catalog (section 4.7).
The retained action definition array keeps a four-bit category field, so at
most sixteen categories exist. AI category sets, action preview lists and the
damage distribution policy remain keyed by category number in code.

### 4.7 Native actions (#499)

`VS.Extract actions <datadir>` (also `tables`, `authoring` and full
extraction) imports the 256 fifty-two-byte action records of G01904 at SLUS
offset `0x3C1DC`. The `action-csv` bundle is `catalog.battle.actions` and needs
the action categories to be published too (opening a root with actions but no
action categories fails and says so):

- `catalogs\battle\actions.csv`: one row per action. `id` is
  `action.<slug>` from the in-game name (`-2`, `-3` after a repeated name,
  `untitled-NNN` for placeholder records), and `name` follows it as readable
  text (`Explosion {level}`; `{level}` is the only markup allowed). Then
  `legacy_id` (the compatibility index saves and scripts still use), `kind`
  (`none`, `spell`, `ability`, `break-art`, `weapon-pp`, `item`, `base-attack`,
  `trap`), `category` (an `action-category.<slug>` reference), `cost`,
  `recovery_delay`, `camera_zoom` (`follow`, `zoom-in`, `zoom-out`, `hold`),
  `effect_id`, the extents `range_x/y/z` and `area_range_x/y/z` with
  `range_shape`, `range_angle`, `area_shape` and `area_angle`, and `behavior`,
  a `|` set of the twelve named AI bits plus `menu-action`,
  `hits-first-obstruction` and `starts-unlocked` (or `none`).
- `catalogs\battle\action-hits.csv`: one row per hit, keyed by `action` and
  `slot` (`first`/`second`), with `effect`, `prerequisite`, `hit_function`,
  `stat_calculator`, `stat_factor`, `damage_type` and `affinity`; every column
  but the effect and the factor is a name. An action with no second hit has no
  second row, and an all-zero row is refused.
- The matching schemas define every enum and flag; the committed copies in
  `Native\Content\schemas\catalogs\battle` must equal the generated ones (a
  Catch2 test compares them). Numeric spellings of an enum are accepted as
  legacy aliases and read back as the name.
- `catalogs\battle\import\actions.json` holds the bits the catalog does not
  keep: the four unread AI bits, the reserved flag nibble, the reserved word
  and the bytes after each name's terminator. Reconstruction never consults
  edited CSVs and is not a runtime path. The importer refuses a source whose
  hit codes have no name (prerequisite codes 40-63, stat calculators 27-63).

`Content::LoadModuleData` calls `LoadCatalogBackedGlobals`, which fills the
`Battle::actionDefinitions` array from the catalog through
`Battle::BuildActionTable`. This is a **temporary bridge (EC-04)**: about 110
references in 62 files still read the 52-byte bitfield record, and the table
retires with them under #499/#58. A missing catalog logs and leaves the table
unchanged; it never yields an all-zero rule. The module data entry for G01904
is removed.

Tracked blockers: the action's own `effect_id` is an unnamed integer (a
different index space from the hit effects, whose mediators are code); hit
prerequisites that behave identically keep separate names; Learning's `action`
column still names break arts by slug rather than by action identity.

## 5. Tier 2: text and fonts

- **Strings:** every string the game shows (exe, MENU `ITEMNAME/ITEMHELP`, `SMALL\HELP*.HF0/HF1`, `MCDATA`, event text in `EVT`, menu labels) is decoded from the VS character table to UTF-8 and stored once in `text\strings.en.json` as `{ "id": "text", ... }`.
- **IDs** follow the owner prefix: `text.item.blade.scramasax.name`, `text.event.0012.004`, `text.menu.title.new_game`. Code and catalogs refer to IDs only.
- **Inline control codes** (colour, wait, newline, name insertion, button glyphs) become a small markup language: `{color:red}`, `{wait:30}`, `{name:ashley}`, `{btn:circle}`, `\n`. The decoder and encoder are exact inverses, so the round-trip is byte-identical (§7).
- **Fonts (target):** glyph bitmaps become a PNG atlas with JSON metrics (codepoint → rect, advance). #500 must supply the VS character code → Unicode contract and bidirectional codec. The proposed `Native\Content\schemas\charmap.json` is **not present yet**; the bounded label decoder in §1.2 is not a substitute for that text conversion. Reference: `tools\PSX\VagrantStory\VagrantStory-Font`.
- **Localization (target):** Phase 2 adds `strings.<lang>.json` overlays. The complete string catalog/schema/codec must exist before claiming this structure is ready.

**Implemented offline text-codec foundation (#500):** `Content/GameTextCodec.hpp`
provides strict reversible English glyphs, escaped literal braces (`{{`, `}}`),
named symbol/button glyphs, newline, `{page}`, `{confirm}` and `{spacing:N}`.
Spacing uses the renderer's nominal units: -16..-1 encode F0..FF, while 0..239
retain their positive values. E6 waits for controller confirmation; it is not
a fixed-duration `{wait:30}`. The parameterized import vocabulary is:

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

The mapping follows upstream `src/BATTLE/BATTLE.PRG/5BF94.c`:
`vs_battle_printf` (991), `printVariableWidthFont` (1253), and
`printFixedWidthFont` (1644). The byte-sized limits apply only to offline legacy
reconstruction. This codec must not become a runtime string-table repacker or
constrain native string/layout definitions. Unimplemented controls and unmapped
glyphs fail with the string identity and byte position.

The decoder reports the exact consumed prefix through E7. Callers retain the
remaining bytes as import provenance, not text; an all-zero fixed slot is an
empty import record rather than a fabricated terminated string. Informational
labels reuse the same glyph map while retaining visible unknown-code markers.
`EncodeGameText(decoded, id)` preserves the ignored font bits; the markup-only
overload emits their canonical zero bits. Invalid, duplicate, stale or unused
provenance offsets fail explicitly instead of changing unrelated text.

This codec is **not** the complete string catalog, font/charmap asset or ID-based
runtime adoption. Inspection also found extended Japanese glyph pairs in 47
MCMAN location-label records (indices 77..104 except 89, and 117..136):
`F6 EF` maps to the Japanese table's block glyph in the upstream decoder, not a
new English control to invent. Their encoding/usage and placeholder status need
explicit reconciliation; the English codec continues to reject them.

### 5.1 Native file-picker prompt adoption (#54/#500)

`VS.Extract text <datadir>` (also full extraction) publishes the save import,
font and image outputs (`text.ui.save`, `font.ui.save`, `image.ui.save-font`)
and contributes to the shared `text.catalog.en` output. Unchanged sources
keep authored files; `--force` replaces only this contributor's strings.

- `text\strings.en.json` includes 25 native-owned status/notice/operation
  messages and two imported MCMAN choices, `text.save.choice.yes/no`.
  Only MCMAN entries **36 and 37**, halfword offsets **779 and 782**, are
  imported. Their exact decoded markup/consumed extent remains in
  `text\save-text.import.json`; leading spacing is a view-layout concern.
  The other **135 entries**, including all **47 unsupported Japanese
  location labels**, are not migrated. Other string tables remain unchanged.
- `text\save.font.json` names `font.ui.save` and the shared
  `image.ui.save-font` image. Its 159 glyph keys are Unicode scalars or named symbols,
  with atlas-pixel rectangles, advances and line height. The offline importer
  reuses `GameTextCodec`'s glyph facts, not a second runtime byte map.
- `textures\ui\save-font.png` plus `.image.json` preserve the 4-bit first
  English bank of TITLE's G02022 font: **13,824 bytes at 0x46B68**, plus
  **32 palette bytes at 0x54A68**. The rest of G02022, other TITLE/module
  images, SYSTEM.DAT and the wider #498 image corpus remain unconverted.

Schemas are `Native\Content\schemas\strings.schema.json` and
`font.schema.json`. Runtime `NativeText` parses Unicode and typed newline,
named-button/symbol and `{value:role}` tokens; instant labels do **not**
implement progressive dialogue controls. Unknown controls, duplicate JSON
members/IDs, invalid Unicode, absent references/glyphs and overflow fail
explicitly. Literal braces are `{{` and `}}`. Typed argument values are
strings or signed 64-bit integers, not byte-sized import slots. String
arguments are never parsed as markup; external CR/LF/tab become spaces.

The existing `catalog-layers.json` ordered roots can supply partial
`text\strings.en.json` overlays (same font, explicit scale and string map).
The data store validates into owned typed content before publication. Each
native file-picker entry owns its content/layout snapshot; cancel, completion,
reset, reconfiguration and shutdown release it. The native file-picker
policy emits semantic status IDs, never an encoded footer. Default No,
backup/invalid-data cautions and the existing frame-delayed operations remain.
The Yes/No choices and status share the native rendering path. Legacy room
summaries, breadcrumb labels, button legend and artwork remain outside this
bounded prompt/consent slice.

Layout is independent of Game font globals: authored scale 0.5–4, measured
Latin/symbol word wrapping and explicit newlines, scalar emergency wrapping,
and explicit pixel-box overflow refusal (no truncation or silent shrink).
The file view admits only content that fits its 304×30 status box and
100×16 choice boxes. These are view constraints, not 24/48-byte string limits.
The unchanged button legend moves to the top margin, leaving two full status
lines inside the file screen's 224-pixel drawing area.
The existing Caution bitmap is redrawn after the third card: that card formerly
covered its top three rows. This preserves the required heading, not a new
image-conversion claim (defect recorded on #54 before repair).
This is not full Unicode shaping/UAX #14 conformance or a WCAG claim.
Design references: [Unicode line breaking](https://www.unicode.org/reports/tr14/)
and [text resizing without loss](https://www.w3.org/TR/WCAG22/#resize-text).

The checked backend renders native glyph rectangles as scaled textured quads.
It temporarily binds the owned PNG to the existing page/palette and restores
both immediately after drawing. GPU packet/page-image rendering is explicit
EC-01/EC-04 debt, **not** full font or compatibility retirement. No runtime
UTF-8-to-legacy string/container reconstruction is used by migrated prompts.
Verification evidence and limits are recorded in HANDOFF; #54/#500 remain open.

### 5.2 Shared catalog ownership and encyclopedia text

`TextContribution` is the single offline producer of `text\strings.en.json`.
Manifest output `text.catalog.en` (`text-catalog-json`) is its sole file owner;
source-specific outputs do not also claim that file. The import-only
`text\strings.import.json` records contributor source hashes and owned IDs.
It is not a second primary string store. The save-only bootstrap upgrades
the previous catalog without replacing authored values. Bestiary and save
imports preserve one another, including when one family is force-reimported.
Unchanged semantic content preserves the authored file's bytes and timestamp,
including formatting; ID collisions and unsupported controls fail before
publication.

The encyclopedia contributes 78 names, 78 descriptions and two native-owned
locked-entry labels. Its names' proven leading `{spacing:14}` is view
indentation (the original detail view skips those two bytes). Only observed
inter-word `{spacing:6}` becomes a space, whose default native advance is six
units. Other spacing fails instead of being guessed. Exact original markup
and suffix bytes remain in the bestiary import provenance. The actual
descriptions use plain English glyphs and newline; no progressive control
or runtime re-encoding is needed.

The existing native font and ordered text layers are shared. Gazette glyph
layouts are copied into a frame-owned overlay and submitted **after** the
menu ordering table so queued backgrounds cannot cover them; reset drops
that overlay. This reuses the explicit page/packet backend debt. It is not
variable-font palette/pixel parity or full font retirement.

---

## 6. Tier 3: disc assets → modern formats

The disc has 27 kinds of files. The "Source" column below names the files on the disc. Every converter keeps the decoder (in `VS.Content`) separate from the writer (in `VS.Extract`).

### 6.1 Format map

| Source (count) | Output | Converter notes | References |
|---|---|---|---|
| `OBJ\*.SHP` (202) + `OBJ\*.SEQ` (700) | `models\characters\<SHP>.gltf` | §6.2 | vstools `SHP.js`/`SEQ.js`, VagrantStoryImporter, decomp actor loaders |
| `OBJ\*.WEP` (127) | `models\weapons\<WEP>.gltf` | skinned mesh; one palette row per blade material, shown through `KHR_materials_variants` (§6.2.4) | vstools `WEP.js`, VagrantStoryImporter |
| `MAP\*.ZUD` (455) | `models\zud\<ZUD>.json` + any body not already emitted | a ZUD bundles SHP + WEP(s) + SEQ for an enemy. It is emitted as **references** to the shared outputs (matched by content hash), not copied (§6.2.5) | vstools `ZUD.js` |
| `SMALL\*.ARM` (32) | `rooms\automap\<ARM>.json` | minimap scenes: a room count, 12-byte room records (visited, payload length, zone, map), display geometry (vertices, triangles, quads, lines, doors) and 36-byte names with previous/next links; (zone, map) becomes the stable room ID and names become string IDs. 47 map IDs exceed their zone's room table: explain them before converting | native `Field\MinimapScene.hpp` |
| `SMALL\*.DIS` (22) | `textures\dis\*.png` + `.image.json`, `catalogs\world\puzzle-challenges.csv` | concatenated 4bpp TIMs; `IQ.DIS` also holds the 64-record cube-puzzle challenge table (keyed by room ID); `ENDSCR00–15` are 16bpp result screens | native `CubePuzzleLifecycle.hpp`, `ClearResultsCore.cpp` |
| `MAP\*.MPD` (512) | `rooms\<zone>\<MAP>.gltf` + `.room.json` | §6.3 | vstools `MPD.js`, VSViewer, decomp field loaders |
| `MAP\*.ZND` (256) | `catalogs\world\zones.csv`, `zone-rooms.csv`, `zone-textures.csv`; `catalogs\enemies\zone-enemies.csv`, `zone-enemy-limbs.csv`, `zone-enemy-parts.csv`; `textures\zone\<ZONE>\*.png` | done (`znd-tables`, §6.9, #517): rows and images rebuild each zone exactly. The relational ENEMY_DATA_SCHEME catalogs remain #199 | ENEMY_DATA_SCHEME.md §3 |
| `EVENT\*.EVT` (512) | `events\<id>.json` | command list `{op, args}` with op names from the decomp's event interpreter; text → string IDs | decomp `Event\` module |
| `BG\*.TIM` (4), `BG\*.FAR` (4), root `*.TIM` | `textures\bg\<artwork-slug>.png` + `.image.json`; `backgrounds\<artwork-slug>.background.json` | TIM → PNG (§6.4). The BG TIM headers are not authoritative: they declare mode `0xA` (16bpp + CLUT) with a 256-colour CLUT and three share id `0x110`; the layout's draw depth selects 8-bit indices. FAR has a 12-byte header and 24-byte cells; the reader consumes typed JSON cells directly and `image_id` selects the native image (§6.4.1). MPD section 15 remains the layout compatibility lookup and original-image provenance | native `Content\BackgroundLayout.hpp`, `Field\Background.cpp`, `BackgroundLive.cpp` |
| `GIM\*.GIM` (49) | `textures\gim\*.png` + composition JSON | VS screen-effect images: per-layer cell maps over shared strips, 4 × 16-colour CLUTs and a shared or owned 256-colour palette | native `Render\GimImages.cpp` |
| `MENU\*.BIN` (11), `ENDING\ILLUST*.BIN` (11) | `ui\*.png` + `.image.json` + layout JSON | menu art, game-over, ending illustrations. `MENUBG` is word-RLE and `ILLUST*` are LZ/RLE-compressed TIMs, decoded at conversion; `ITEMNAME`, `ITEMHELP`, `MENU12` and `MCMAN` are text and tables, not images | native `Menu\`, `StaffRollCore.cpp` |
| `MENU\*.SYD` (3) | `catalogs\menu\combination-<kind>-{results,pairs,materials}.csv` + `.schema.json` | workshop **combination recipes** (result records, ingredient pairs and material rules), not item definitions; there is no global item-stat file. Converted (§6.8) | native `Content\WorkshopRecipes.hpp`, `Menu\Setup\Combination.hpp` |
| `SMALL\MON.BIN` | `catalogs\enemies\bestiary.csv` | 150 records of `0x2C` bytes | ENEMY_DATA_SCHEME |
| `SMALL\HELP*.HF0/HF1` (28) | `text\strings.en.json` + `ui\help\<page>.json` + `ui\help\*.png` | `.HF0` = page text, sprite placements and line table; `.HF1` = 8×16 8bpp illustration blocks and 16-colour palettes | native `ManualTopics.hpp`, `ManualIllustration.hpp`, `Manual.cpp` |
| `EFFECT\*.P` (290), `*.FBT` (755), `*.FBC` (277), `EFFECT\*.BIN` (311) | `effects\<id>.json` + `textures\effect\*.png` | `.FBT` texture pages + `.FBC` palettes → PNG (§6.4). `.P` files are block streams (u32 header: size in the low 16 bits, type in the high 16 bits; type 0 ends) → effect JSON. `.BIN` files are **PLG code overlays** and EFFPURGE dispatch tables, already ported to C++ (`Effects\Plugins\PlgNNN.cpp`, `PurgeTables.cpp`); only their data blobs remain, as typed tables. Effect behaviors stay in code (plan §9.7) | decomp `Effects\` |
| `OBJ\*.ESQ` (97) | `models\animation-sets\<ESQ>.json` | shared animation sets: up to 15 entries, each a selector (a model number or kind), an optional texture row and a SEQ bank; `BATTLE.PRG`'s tables bind them to models. Converter `esq-json` (§6.2.1) | decomp `BATTLE.PRG/30DB0.c`; native `Actors/ActorAssets.cpp` |
| `OBJ\*.ETM` (80) | `textures\actors\<ETM>\<MODEL>.png` + `.image.json` | texture sheets of event actors' lines (facial frames, for example); each set entry with a texture row names the lines its model uses. Converter `etm-png` (§6.2.1); glTF references and texture tracks follow §6.2.2 | decomp `BATTLE.PRG/30DB0.c`, `40564.c`; native `Actors/TextureAnimation.c` |
| `SOUND\WAVE*.DAT` (101) | `audio\banks\<bank>\` (`bank.json`, `<n>.wav` + `<n>.sfz`) | 83 banks; 18 one-byte zero placeholders. Converter `wave-bank` (§6.5) | decomp `Audio\` |
| `MUSIC\MUSIC*.DAT` (192) | `audio\music\*.seq.json` | §6.5 | decomp `Audio\Seq` |
| `SE\EFFECT*.DAT` (102) | `audio\sfx\*.seq.json` (+ bank if embedded) | §6.5 | |
| `ENDING\ENDING.XA` (1, 77 MB) | `audio\streams\xa\ENDING\ENDING.XA.flac` | converter `xa-flac` (§6.6) | jpsxdec |
| `MOV\TITLE.STR` (1, 26 MB) | `disc\MOV\TITLE.STR.mkv` | converter `str-mkv` (§6.7) | ffmpeg `psxstr`, jpsxdec |
| `BATTLE\SYSTEM.DAT` | `textures\system\*.png` + `.image.json`, `text\fonts\` metrics | 8 sections: images (0, 1, 3, 4, 5), a palette (2), a fixed image (6) and interface images (7). It supplies the font and UI glyphs | native `SystemResources.hpp`, `Canvas.cpp` |
| `SLUS_010.40`, `*.PRG`, `GIM\SCREFF2.PRG` | Tier 1 tables only (code is already in `Native\`) | §4 | |
| `license_data.dat`, `SYSTEM.CNF`, `ENDING\NULL.DAT`, 192 zero-byte and 65 one-byte zero placeholders | nothing (`drop`) | recorded in the manifest as `skipped: reason`. `DBGFNT.TIM` (a debug font no code reads) is converted and marked `unreferenced` (§1.1) | |

Every row is now identified (D25 inventory, 2026-11-21); the open questions are listed per IMP issue. Settling a row's converter means reading the loader that consumes the file (the native port's loader, with the decomp as a cross-reference for original behaviour), then writing the converter. **No converter is written from community docs alone.** vstools, VagrantStoryImporter, GodHands and DataCrystal speed up understanding, but they are reference only (licenses: `TOOLS_INVENTORY.md`).

### 6.2 Character models (SHP + SEQ/ESQ + ETM → glTF), matching the Sydney reference

Runtime read errors (#518): missing, stale or invalid converted SHP, WEP,
SEQ/ESQ and ETM data fails the actor-asset request. The loader frees its
unpublished file allocation and does not parse cleared bytes or fall back to
the extracted legacy file. Only a full legacy disc-request queue permits the
existing synchronous fallback, and that fallback's failure is checked too.

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

**Runtime target:** typed `ModelDef` / `AnimationDef` consumers read glTF and
native metadata directly. The current SHP-shaped reconstruction is
EC-04-T-SHP/SEQ migration debt, not the target `ModelLoader` contract. D21's
temporary renderer does not require limiting glTF authoring to SHP layouts.
Section 7's offline comparison proves imported fields survive; it does not
require the production loader to emit SHP or SEQ bytes.

**Also emitted** (optional, `--posed`): `<SHP>_posed.obj/.mtl/_0.png`, like the reference folder. This is for quick previews only; the game never reads it.

**Current state (SHP and SEQ slices, #203, #205).** `VS.Extract models <datadir>` (and `extract`) converts all 202 `OBJ\*.SHP` files with converter `shp-gltf` version 2 (`Extract/ShpModelConversion.hpp`, `Content/ShpModelFiles.hpp`, `Content/ShpFile.hpp`), and carries all 700 `OBJ\*.SEQ` banks in their models' glTF files (§6.2.1):

- Each model writes `models\characters\<NN>.gltf` + `<NN>.bin`. A model with its own texture also writes `<NN>.png`, `.image.json` and `.palettes.png` (§6.4). Its id is `model.obj.<nn>` and its image id is `image.obj.<nn>`. The 10 models that borrow another model's texture write only the glTF and the buffer. Their glTF shows the lender's PNG, and their manifest entry lists the lender's SHP as a dependency.
- The glTF has one skinned mesh, a `root` node turned 180° about X and one `bone<N>` node per SHP bone. Positions are the SHP's integer units (scale 1) with each bone's rest origin added.
  - Primitives are grouped by polygon flags 0x71: double-sided, semi-transparent and blend mode.
  - Each corner carries the custom `_VS_VERTEX` attribute, which holds its SHP vertex number.
  - Each primitive's `extras.vs.polygons` lists its polygons in triangle order.
- `asset.extras.vs` holds the payload (schema `vs.shp-model` version 2): the source and its hash, the bones, the polygon counts, the limb ranges and the 0xE8-byte layout block as named fields. For example, `collision_radius` can be edited as a number. A buffer view holds the sound bytes. `sequence` is the SHP's embedded bank and `sequences` lists the `OBJ\*.SEQ` banks the model owns, both as `vs.actor-animation` JSON (§6.2.1).
- The payload and the geometry are authoritative. The materials, textures and images only preview the polygon flags. The reader rebuilds the SHP from those two sources and checks it against every value the payload records. The Khronos glTF validator reports 0 errors and 0 warnings for all 202 models.
- A rerun leaves the files of an unchanged SHP alone, so edits survive. `--force` rewrites every model.
- At runtime, `Content::LoadConvertedModel` (`Content/ConvertedModels.hpp`) handles the actor model read (`ActorAssetsLive` `BeginFileRead`, `kActorModel`) and never reads the SHP from the disc. It matches the read's LBA and sector-rounded size to the extracted SHP and rebuilds the bytes from that model's files. An edit is validated and loaded, or refused with the file and member named. `RequireConversions` refuses to start without the `shp-gltf` batch.
  - The load completes in the same update that requests it, where the original waited for the CD.
  - An edited model must still fit the original sector-rounded allocation; a larger one is refused.

Not yet done: glTF preview animations for the SEQ banks and ESQ clips (#205, #206), glTF references to the ETM texture regions and their texture tracks (#207), a model larger than its original allocation, and a re-export from Blender through the validator. Weapons are §6.2.4 and ZUD enemy archetypes §6.2.5.

#### 6.2.1 Container, schema and optional extension

- **First delivery:** keep the existing `extras.vs` design and define its versioned contract in `Native\Content\schemas\actor-animation.schema.json`. VS.Content and Workshop must agree on IDs, units, track semantics and fixtures; do not build incompatible per-tool interpretations.
  - **SEQ banks (#205, landed):** schema `vs.actor-animation` version 1 (`Content/ActorAnimation.hpp`) writes one bank as its source, the source's hash, the bone count and the bank.
    - The bank lists its clips, the animation-to-clip map and the motion pool.
    - Each pool element is a track (an optional initial key and its commands as one flat list of frames, x, y, z groups), an event script (`[frame, op, arguments...]` per event) or unread bytes (lowercase hex).
    - Clips name pool elements by number, so a track can be lengthened without recomputing offsets.
    - The JSON Schema checks types and ranges. The native reader also checks references, clip flags and event lengths (`EncodeSeqBank`).
    - All 700 `OBJ\*.SEQ` files and the 202 banks embedded in SHP files read back from this JSON equal to their decoded banks and rebuild their exact bytes (`SeqFileTests.cpp`).
    - `tools\test\test_content_schemas.py` checks the writer's fixture (`Native\Tests\data\animation\sample-bank.json`) against the schema in CI.
  - **SEQ banks in their models (#205, landed):** each bank is stored once, in `asset.extras.vs.sequences` of the one model that loads it.
    - Ownership comes from what the game reads, not from file names. A model owns the banks its layout block names (`battle_sequence_lbas`/`_sizes` and `special_sequence_lbas`/`_sizes`, LBA and sector count). `OBJ\00.SHP` and `OBJ\02.SHP` also own the material and material-model banks that `BATTLE.PRG`'s tables name (`ShpModelConversion.cpp` `BankOwners`).
    - The extractor refuses a bank that no model or two models load, and a layout entry that names no listed SEQ.
    - Every bank gets a `seq-gltf` output (id `animation.obj.<bank>`) that lists its owner's `.gltf` and `.bin`. Its dependencies are `BATTLE\BATTLE.PRG` (the event lengths) and the owner SHP at the hash the model output records. A bank with a changed source or owner reconverts its owner.
    - At runtime, `Content::LoadConvertedSequence` handles the animation bank read (`ActorAssetsLive` `BeginFileRead`, `kSequence`) of an `OBJ\*.SEQ` and never reads the bank from the disc. It finds the bank by LBA and sector-rounded size, reads its owner's glTF and rebuilds the bank's bytes. The last four models decoded are kept, keyed by their files' size and time, so one model's banks decode once. The ESQ sets the same stream reads load from their own JSON (below).
    - An edited bank is validated and loaded, or refused with the file and member named. It must fit the original sector-rounded read; a larger one is refused and leaves an empty bank.
    - `RequireConversions` refuses to start without the `seq-gltf` batch or with a bank that has no current output.
    - All 700 banks load back from their owners' glTF files as their exact bytes (`SeqFileTests.cpp`), and the glTF files still pass the Khronos validator with 0 errors and 0 warnings. `00.gltf`, which holds the most banks, is about 4.1 MB.
    - Not yet done: glTF animation clips that preview the banks, and playback and timing evidence for the converted banks.
  - **ESQ shared animation sets (#206, landed):** converter `esq-json` writes each `OBJ\E*.ESQ` as `models\animation-sets\<ESQ>.json` (id `animation-set.obj.<stem>`) in schema `vs.animation-set` version 1 (`Content/AnimationSet.hpp`, `Native\Content\schemas\animation-set.schema.json`).
    - A set holds up to 15 entries in the order the game searches them. Each entry has a selector (a model number or a model kind), a texture row or `null`, the bone count and its bank in the `vs.actor-animation` form above.
    - A set is its own file rather than part of a model's glTF because several models play it.
    - Which models play an entry comes from `BATTLE.PRG`'s tables, not file names (`EsqConversion.cpp`). A model plays the first entry whose selector is its number or its kind (the kind table), and the set tables name each set's sectors.
    - The extractor refuses an entry no listed model plays, a model whose bone count differs from its entry's, a texture row for a selector that is not a listed model number, a repeated selector, and a set the tables do not name or a table entry naming no listed set. The output depends on `BATTLE.PRG` and on every model the set binds, so a change to any of them reconverts the set.
    - Each entry's bank includes the padding to a word that follows it on disc, as the game has it in memory.
    - Two quirks of the original sets, absent from the standalone SEQ files, are kept rather than repaired:
      - In 36 sets a track's last command is cut off by the end of its bank (once, by the next element). Every clip that plays such a track ends before the cut, so the game never reads it. The cut bytes stay as unread bytes, and the decoder and encoder refuse a clip that would play into them.
      - Two entries map an animation to the clip one past the last (`E02F` entry 4, `E054` entry 2). Like the game, the reader accepts a clip number whose header still fits in the bank, and refuses one whose header would end past it.
    - All 97 sets (355 entries) read back from their JSON equal to their decoded sets and rebuild their exact bytes, and load through the runtime loader as their exact bytes (`SeqFileTests.cpp`). `tools\test\test_content_schemas.py` checks the writer's fixture (`Native\Tests\data\animation\sample-set.json`) against the schema in CI.
    - At runtime, `Content::LoadConvertedAnimationSet` handles an ESQ read of the animation bank stream (`ActorAssetsLive` `BeginFileRead`, `kSequence`) and never reads the set from the disc. An edited set is validated and loaded, or refused with the file and member named. It must fit the original sector-rounded read; a larger one is refused and leaves an empty set. `RequireConversions` refuses to start without the `esq-json` batch.
    - Not yet done: a load-time check of an edited entry's bone count against the models that play it, glTF clips that preview the sets, and playback and timing evidence for the converted sets.
  - **ETM actor texture sheets (#207 first slice, #515, landed):** converter `etm-png` writes each `OBJ\E*.ETM` as one indexed PNG plus `.image.json` (§6.4) per model it textures, `textures\actors\<ETM>\<MODEL>.png` (output id `actor-texture.obj.<stem>`, image id `image.obj.<stem>.<model>`; `Content/EtmFile.hpp`, `Extract/EtmConversion.hpp`).
    - A sheet is a 4-byte header (marker and height) and lines of 64 halfwords that the game uploads below the actor textures. Which lines belong to which model comes from the ESQ set `BATTLE.PRG`'s tables pair with the sheet: each entry with a texture row starts a model's region, which runs to the next entry's row or the end of the sheet. The game binds a model to the first entry whose selector is its model number, so a texture row's selector is that number.
    - A region's depth is its model's SHP texture depth, and its preview palette is the model's own palette; a model that borrows another's texture uses the lender's (2 regions). The sidecar names the palette's source, and the indices are authoritative. All 210 regions in the 80 sheets are 8-bit.
    - 33 regions use indices past their model's palette. The game draws those from the rest of the model's CLUT row as filled at run time (a weapon's palette shares the row from column 160), so the preview shows them transparent and the sidecar's note says so.
    - The extractor refuses a sheet entry that names no listed ETM or the wrong sector count, a sheet named by no entry or by two, an entry naming a sheet but no set, a set whose texture rows give no model line 0, repeat a model or a line or pass the sheet's end, and a region whose model is not a listed SHP. The output depends on `BATTLE.PRG`, the set and each region's model and lender, so a change to any of them reconverts the sheet.
    - All 80 sheets rebuild their exact bytes from their regions and load through the runtime loader as their exact bytes (`ShpFileTests.cpp`). Two independent conversions write identical files. A rerun leaves unchanged sheets and their edits alone; `--force` rewrites them.
    - At runtime, `Content::LoadConvertedActorTexture` handles the effect texture read (`ActorAssetsLive` `BeginFileRead`, `kEffectTexture`) and never reads the sheet from the disc. It rebuilds the sheet from the region images, which must be the sheet's indexed images, one line wide and in contiguous lines, each at the depth its model (named by the sidecar's `texture` dependency and loaded from its own glTF) draws its texture at; an edit that breaks this is refused with the image named, and a rebuilt sheet larger than the original read is refused and uploads nothing. `RequireConversions` refuses to start without the `etm-png` batch. The prologue's opening cutscene loads `E001` this way.
    - Not yet done: glTF references to the regions and the texture tracks that select frames from them (§6.2.2), and pixel evidence of an edited region in a scene.
- **Extension adoption gate:** once the payload is exercised by both consumers, assess packaging it as one documented optional vendor extension. Choose a project/vendor namespace, publish its schema and fallback behavior, and test both readers before switching the writer. Do not invent a `KHR_` or `EXT_` standard. The extension must be in `extensionsUsed`, but not `extensionsRequired`, only while the asset has a valid standard preview without it. See the [glTF extension rules](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#specifying-extensions) and [extension registry guidance](https://github.com/KhronosGroup/glTF/tree/main/extensions).
- There is **one authoritative VS payload** per asset. A packaging change requires a schema/converter-version change and an explicit migration; do not emit independently editable copies in both `extras` and an extension. A native loader rejects missing required VS data or unsupported versions with an actionable error, even when a generic viewer can still show the baseline.
- Ordinary viewers need not execute facial tracks, event commands or VS rendering rules. Ignoring custom data is not a promise to preserve it on re-export. Before accepting an edited model, validate the VS payload and references; missing or altered metadata must be reported, not silently defaulted.
- Keep scene-wide scripts, room logic and gameplay catalogs in their existing JSON contracts. Model-local metadata belongs with the model, but glTF extensibility is not a reason to move all game data into it.

#### 6.2.2 Event and facial texture animation contract

Treat skeletal pose, texture overlays and event synchronization as related but separate data. A correct ESQ bone pose alone does not prove facial playback, and texture-based expressions do not prove that every facial movement excludes bone animation.

The [original ETM loader](../resources/rood-reverse/src/BATTLE/BATTLE.PRG/30DB0.c) skips a 16-bit word, reads a 16-bit height and uploads 128 bytes per row. [Native texture animation](../Native/Game/src/Actors/TextureAnimation.c) selects rectangular frames and handles packed texture addressing. The sheet-to-model binding (which lines each model uses, at which depth and with which palette) is established and converted (§6.2.1, `etm-png`); the frame, palette-animation and event bindings below are not yet, and AP1 must establish them before the glTF carries texture tracks.

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

#### 6.2.4 Weapon models (WEP → glTF, #204)

A WEP is the SHP magic and counts, a model section laid out as an SHP's (flat polygons, no sound), a texture with one palette per blade material, and one rest rotation per bone (`Content/WepFile.hpp`). `VS.Extract models <datadir>` (and `extract`) converts all 127 `OBJ\*.WEP` files with converter `wep-gltf` version 1 (`Extract/WepModelConversion.hpp`, `Content/WepModelFiles.hpp`), sharing the character model writer and reader:

- Each weapon writes `models\weapons\<NN>.gltf` + `<NN>.bin`, its texture `<NN>.png` + `.image.json` + `.palettes.png` (§6.4), and six previews `<NN>.<material>.preview.png`. Its ids are `model.obj.<nn>.wep` and `image.obj.<nn>.wep`, so they do not collide with the character model of the same number.
- The texture is 8-bit and 32 rows tall. `.palettes.png` has seven rows in the order iron, bronze, hagane, silver, damascus, wood, leather; BATTLE.PRG's `weaponMaterialPaletteMap` gives each item material its row. Each row is the handle's colours then the material's. The PNG's own palette is row 0 (iron), and edits to any row are validated as §6.4 requires.
- The glTF has the character model layout, with each bone after the first turned by its rest rotation, so it shows the weapon as the game draws it. `asset.extras.vs` holds the payload (schema `vs.wep-model` version 1), including the stored rotations.
- **Material variants:** the glTF declares `KHR_materials_variants` with one variant per material row. Each primitive maps `iron` to its own material, and each other variant to a copy that shows that material's preview. A preview is the texture with that row's palette. Blender and other variant-aware viewers can switch blade materials.
- The previews are derived and only for viewers: the game and the reader never read them. After editing `.palettes.png`, convert again with `--force` to refresh them.
- At runtime, `Content::LoadConvertedWeapon` serves the weapon read (`ActorAssetsLive` `BeginFileRead`, `kWeapon`) as §6.2 describes for characters. The game picks the material row when it loads the weapon (`LoadWeaponMaterialPalette`), as the original did. `RequireConversions` refuses to start without the `wep-gltf` batch.
- The Khronos glTF validator reports 0 errors and 0 warnings for all 127 weapons, and two forced conversions write identical files.

Not yet done: a weapon larger than its original allocation, and a re-export from Blender through the validator. ZUD bundles name their weapons and shields (both WEP files) by these outputs (§6.2.5).

#### 6.2.5 Enemy archetypes (ZUD → JSON, #200, #516)

A ZUD is the enemy bundle a room spawn reads (`Content/ZudFile.hpp`). It has a 0x30-byte header and then five sections packed in order with no gaps: body (an SHP), weapon (a WEP), shield (a WEP), common animation and battle animation (SEQ banks). The header holds 8 identity bytes (character, weapon, battle-sequence model, weapon material, shield, shield material, default material, and a zero pad) and then one offset and size per section. `VS.Extract models <datadir>` (and `extract`) converts all 455 `MAP\*.ZUD` files with converter `zud-json` version 1 (`Extract/ZudConversion.hpp`):

- Each bundle writes `models\zud\<ZUD>.json` (id `archetype.map.<zud>`) in schema `vs.actor-archetype` version 1 (`Content/ActorArchetype.hpp`, `Native\Content\schemas\actor-archetype.schema.json`). It holds the source, its hash, the identity bytes as named fields, and one entry per section (`null` when the section is empty).
- **A section is named, not copied, when it already has an output.** The converter matches each section's SHA-256 against the `OBJ` files (the identity byte that numbers the part breaks a tie). A match names that file's output: `{"model": "model.obj.<nn>"}` for a body, `{"model": "model.obj.<nn>.wep"}` for a weapon or shield, `{"animation": "animation.obj.<bank>"}` for a bank. An edit to that model, weapon or bank reaches every archetype that names it.
- **A section no `OBJ` file holds stays with the archetype.** A body becomes `models\zud\<ZUD>.gltf` + `.bin` in the §6.2 layout (model id `model.map.<zud>`; a texture of its own writes `<ZUD>.png`, `.image.json` and `.palettes.png` with image id `image.map.<zud>`, and a borrowed texture's image is `../characters/<NN>.png`), named as `{"file": "<ZUD>.gltf"}`. A bank is written inline as `{"bones": N, "bank": {...}}` in the `vs.actor-animation` bank form (§6.2.1). A weapon or shield must match a WEP.
- Corpus: 451 bodies name an SHP and 4 are the archetype's own (`Z049U06`, `Z060U01`, `Z063U00`, `Z063U01`); 239 weapons and 52 shields name a WEP; 3 common animations and 85 battle animations name a SEQ bank, while 10 and 303 are inline.
- The output depends on each `OBJ` file it names, at the hash that file's output records, and on `BATTLE\BATTLE.PRG` when it writes its own body or bank (the event lengths). A change to any of them reconverts the archetype. A rerun leaves unchanged archetypes and their edits alone; `--force` rewrites them.
- The extractor refuses a bundle with no body, a non-zero pad byte, a section that does not start where the one before ends or runs past the file, bytes after the last section, a weapon or shield that matches no listed WEP, a named part that has no current output ("convert the models first"), and an own part when `files.tsv` does not list `BATTLE\BATTLE.PRG`.
- At runtime, `Content::LoadConvertedArchetype` handles the room spawn's ZUD read (`ActorSpawningLive` `BeginZudRead` through `ReadLiveConvertedArchetype`) and never reads the bundle from the disc. It finds the bundle by LBA, loads each named part as its own read would (`LoadConvertedModel`, `LoadConvertedWeapon`, `LoadConvertedSequence`), loads an own body from its glTF beside the JSON (which must name the same source), and rebuilds the bundle's bytes. An id that names no output of the right converter, an edit that fails validation, or a file that does not match its manifest is refused with the file and member named. A rebuilt bundle larger than the read's allocation is refused and the spawn fails as a failed read did. `RequireConversions` refuses to start without the `zud-json` batch.
- All 455 bundles decode and rebuild their exact bytes, and load from their archetype JSON through the runtime loader as their exact bytes (`ZudFileTests.cpp`). Synthetic bundles cover every kind of section, an edited part reaching the archetype, and each refusal (`ShpFileTests.cpp`). `tools\test\test_content_schemas.py` checks the writer's fixture (`Native\Tests\data\animation\sample-archetype.json`) against the schema in CI.

Not yet done: the encyclopedia's enemy previews, which read ZUD files through their own path; editing the identity bytes against the character, weapon and material tables; and a bundle larger than its original allocation.

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

The strict TIM and native-image readers/types are in `VS.Content`; the TIM-to-image conversion, PNG/sidecar writers and manifest writer are in `VS.Extract`. These writers are not part of the runtime Content library. The extractor writes the native files once. The field-background runtime resolves the converted PNG/sidecar from the manifest and uploads its pixels and palette; it does not parse the source TIM. Its FAR layout now uses the typed JSON reader in §6.4.1.

Background material (#208): the original draws each FAR tile as an opaque Gouraud-shaded textured quad (`setPolyGT4`, `2842C.c:497`) with CLUT (768, 238), in the farthest ordering slot over a black full-screen fill. Its corner colours on the disc are neutral 128 (the texel unchanged) or black (an edge fade). There are no normals or lights, and the only change to the texture while it is shown is the event script's palette transform.

TIMs inside other files: the shared strict decoder finds 822 at 4-byte-aligned offsets in 99 files.
- 782 are in the first texture table of 76 `MAP\*.ZND` zones; each zone's table count matches. They are texture pages and the CLUT rows the zone copies into the room palette, all 16-bit words.
- 28 are in `SMALL\*.DIS`: 12 at 4 bits with 16 palettes, and 16 direct-colour 320 × 224 end screens.
- 12 are in `ENDING\ENDING.PRG`: 7 at 4 bits (one of them with two palettes) and 5 at 8 bits.

A disc-backed `[extract]` test converts each one with `ConvertTim` and rebuilds its exact bytes. The containers' converters adopt them: the zone converter writes the zones' TIMs (§6.9, #517); DIS in #211 and the ending program in #498. `MENU\*.BIN`, `SYSTEM.DAT` and `TITLE.PRG` hold no raw TIMs, and `ENDING\ILLUST06-16.BIN` are compressed (#498).

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

### 6.4.1 FAR background layouts

**Implemented bounded slice (#210):** `VS.Extract backgrounds <datadir>
[--force]` writes `backgrounds\<artwork-slug>.background.json`, using
`far-layout-json` version 2 and the committed
`Native\Content\schemas\background-layout.schema.json` version 2.
Full extraction runs it after images. The configured SLUS-01040 corpus has
four nonempty inputs and 288 cells:

| Import provenance | Layout ID | Image ID | Grid | MPD references |
| --- | --- | --- | --- | --- |
| `001OP01A` | `background.castle-silhouette` | `image.bg.castle-silhouette` | 8×4 | MAP001, MAP215 |
| `002OP01A` | `background.dark-clouds` | `image.bg.dark-clouds` | 16×4 | MAP002, MAP216 |
| `007OP01A` | `background.sun-on-horizon` | `image.bg.sun-on-horizon` | 8×12 | MAP007 |
| `008OP01A` | `background.grey-clouds` | `image.bg.grey-clouds` | 8×12 | MAP008 |

All four bindings have consistent room section tables and sector-rounded
file sizes. MAP007 was also compared byte-for-byte with the configured disc;
the earlier claim that `007OP01A` was unreferenced was incorrect
([investigation evidence](https://github.com/JEschete/VS-NativePort/issues/210#issuecomment-7121319720)).
The IDs describe the verified artwork, **not exact room names or a claimed
sunrise/sunset**. The image PNG/sidecar stems match the artwork slugs.
Original `background.import.bg.<stem>` and `image.bg.<stem>` identities are
explicit manifest compatibility aliases, not the primary authoring IDs.

`backgrounds`, `images` and `authoring` run an edit-preserving upgrade first.
It renames stock native files, retags their owned metadata, upgrades old layout
documents with `cell_size_units: 32`, and migrates stock image references.
Existing custom IDs/aliases, custom filenames, pixel/palette data, cell edits,
custom labels and import provenance are preserved. Already-current documents,
including deliberately authored old-ID references, are not reformatted.
Source catalog schema 2 (`assets.schema.json`) gives the eight import records
`asset.background-source.<slug>.layout` / `.image` IDs; old `asset.bg.*` IDs
remain aliases of the same handles. These records still identify import files,
not native PNG/JSON resources. Version 1 catalogs remain readable.

Conflicting semantic destinations, duplicate primary identities, alias
ownership, malformed legacy metadata and invalid migrated image bindings refuse
before publication. A custom raw catalog `file` reference to a renamed native
file currently requires explicit migration and is refused, not silently broken;
arbitrary cross-format URI rewriting is not provided. The shared staged writer
publishes prepared files, then the manifest, then removes obsolete paths.
`--force` is an explicit reimport, not permission to bypass a legacy migration
conflict. A normal repeat conversion retains authored native bytes.

The format follows the actual renderer, upstream
`resources\rood-reverse\src\BATTLE\BATTLE.PRG\2842C.c:464`
(`func_8009147C`), rather than treating every rectangle as a positive extent:

- `columns`, `rows` and row-major `cells` describe the grid. Native schema 2
  admits 1–64 cells per side, at most 1,024 stored cells, and
  `cell_size_units` of 16, 32 or 64 in 320×240 screen-coordinate space.
  These are explicit renderer budgets; JSON is bounded to 1 MiB.
  Original FAR/schema-1 imports retain the 1–31-side, 32-unit geometry.
  Horizontal wrap remains 1,024 screen units. The offline FAR encoder refuses
  native-only geometry; the production reader never calls it.
- `source_texels.{left,top,right,bottom}` are inclusive sampled endpoints.
  Reversed endpoints mirror the texture; both endpoint pairs equal disable
  the cell. The original signed deltas are endpoint minus start; negative
  deltas occur in `007OP01A` and `008OP01A`.
- Named `top_left`, `top_right`, `bottom_left`, `bottom_right` RGB colours
  modulate the image in 1/128 units: 128 is neutral and zero is black.
  `image_depth: indexed8` represents the consumed second corner's code byte,
  shifted into texture-page depth bits. It is **not** semi-transparency.
- `presentation: camera-wrapped-grid` and
  `draw_order: row-major-farthest` retain the existing submission rules.
  Camera roll, offsets and Q12 parallax remain room/event state; image bit
  depth is not scene depth.
- `import_provenance` retains source and paired-image paths/hashes, the
  unconsumed header word at byte 8 and the other three unconsumed corner
  code bytes. For imported originals, cell index `i` maps to source offset
  `12 + 24*i`. No consumed layout field is hidden in an opaque payload.

The converter validates every layout and paired PNG/sidecar before writing,
uses the shared staged writer/manifest, and records the TIM as a dependency.
Unchanged inputs retain validated authored JSON; changed/deleted dependencies
invalidate it, and `--force` replaces authored cells from the import source.
The native reader enforces schema/version, grid counts, byte-range endpoints
inside the image, RGB ranges, image identity/material and source hashes in
all builds. Unsupported depth modes and missing/stale/invalid resources
produce errors rather than a raw-FAR fallback.

Production loads `BackgroundLayoutDocument` into an owned
`OwnedBackgroundLayout` and draws its cells directly. The authored `image_id`
now resolves through the shared native-image/manifest reader, including aliases
and explicitly selected unreferenced artwork. The original MPD/TIM binding is
import provenance/default only. The selected image's pixels and palette are
owned with the cells and carried through upload; upload does not reopen the
original TIM or re-resolve an ID after loading. Source/dependency hashes,
canonical image metadata, sampled rectangles and material rules remain checked.
The current sampler accepts even-width indexed-8 images up to 256×256 with one
256-colour palette, transparent zero and semi-transparency off.

Image dimensions determine the required page count. The event's original page
number is a placement hint: the allocator keeps it when free, otherwise chooses
another contiguous free range within lendable pages 25–31. Occupied/resident
pages are never overwritten; invalid owners, empty/oversized requests and
exhaustion fail without partial reservations. Drawing and retirement use the
actual selected range. This is renderer allocation, not FAR/TIM reconstruction.
`EncodeFar` remains offline-only, and repeat entry, initialization and session
reset release native ownership and the actual page reservation.

The bounded renderer probe changed only 001's `image_id` to `image.bg.002op01a`.
Its old hint 27 could not fit two pages; relocation selected pages 29–30.
At tick 3200, captured renderer storage contained the selected image's exact
pixels and palette. Six captured scene frames matched the original camera
frames: the background was occluded in that interval, so this demonstrates
actual renderer upload, **not** a scene-visible artwork change. The authored
layout was restored byte-for-byte; evidence is
`build\evidence\issue-210\issue-210-rebinding-relocated-comparison.json`.

The later non-skipped MAP001 route (`1000:Start,1300:Circle`, with no prologue
skip) exposes the artwork through the windows at ticks 4000/6000, event 0,
thread 0 PC `+0x626`. A native PNG edit changed 7,815/7,800 scene pixels at
matched camera/model/event states (`build\evidence\issue-210\issue-210-enabled-comparison.json`).
The previously inspected open-sky MAP062 view is **not FAR**: its MPD background
section is empty and the skipped MAP008 route had already transitioned there.

The semantic-layout follow-up changes **only** `cell_size_units: 32` to `16`.
The actual production loader reports those sizes, and the matched MAP001 frames
change **4,401/4,391 pixels** within x24..294/y48..169. Camera, models and
normalized event state are identical; both Debug runs exit normally with zero
errors. Source texels, corner colors and the PNG remain unchanged, and all
watched native files restore byte-exactly. This is both a scene-visible JSON
layout edit and a native-only capability: FAR has no cell-size field and the
offline encoder refuses 16-unit geometry. Evidence:
`build\evidence\issue-210\issue-210-layout-comparison.json` and
`build\evidence\issue-210\issue-210-layout-comparison-{4000,6000}.png`.

Cold-read evidence (`build\evidence\issue-210\issue-210-semantic-corpus.json`) records all four
artwork/grid/image identities and one seeded cell: seed 210 selects global
cell 196, `background.grey-clouds` cell 4 (row 0, column 4), sampling
(127,0)..(159,30), with all four RGB corners neutral at 128. The intended
corner-darkening edit is identifiable directly from the schema without decomp
lookups. The same report verifies all 288 imported cells, unchanged PNG/source
bytes, eight compatibility aliases and the source-catalog migration.

**Remaining gates:** matched-reference placement/gradients/seams/parallax and
production transitions/re-entry remain open. Exact room names are not claimed
by artwork IDs; semantic room integration belongs to the MPD/ARM handoff. Dormant
legacy module pointer declarations and image/page rendering compatibility are
not retired by this slice. The four-file offline round trip, direct reader
tests, native-only edit and bounded prologue evidence do **not** establish full
#210 or EC-04 completion. Publication and exact-revision CI remain required.

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
- **Current import:** `VS.Extract banks Native\data` (also called by `extract`) converts all **83 banks**
  on the supported disc through the `wave-bank` converter; the 18 one-byte zero files are placeholders
  (unused bank slots) that it and the runtime skip. Each bank is a manifest output
  (`audio.bank.<wave file>`) listing its `bank.json`, `.wav` and `.sfz` files with the source hash, so a
  rerun keeps unchanged banks, `--force` rewrites them, and two banks writing one bank id are refused.
  Exact-end records and equal start/loop offsets in erased, final DMA padding are explicitly unavailable,
  not fabricated silence. Active ADPCM remains strictly validated. WAVE0032's older 64-byte articulation
  is recognized by its payload layout, pitch table and envelope fields. Q20 base pitch is converted to
  native root/fine tuning; the complete twelve-pitch table and unpacked envelope survive in `source_record`.
  Its all-zero pitch tables are unavailable records. Original data/provenance remain import sources,
  not a new authoring-format requirement. Two complete imports produced identical hashes for 5,993 files.
  Reimport after upgrading: existing SFZ files need the unsmoothed, extended pitch-bend range for FM.
- **Effect-package sample streams are not sample banks:** the `audio\streams\akao\<id>\` outputs below come
  from block 13 of the `EFFECT\*.P` packages. They are ADPCM samples, not AKAO sequences, and they stay
  outside the manifest until the effect-package conversion (#216) registers each package as one output.
  A separate converter for block 13 alone would mark the packages converted while their other blocks are not.
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
  The manifest records the FLAC as output `audio.xa.<name>` of converter `xa-flac`, with the hash of the
  file's raw 2352-byte sectors (§8); an unchanged source keeps its FLAC on the next extraction.
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
  3. The result is muxed into Matroska: `disc\MOV\TITLE.STR.mkv` (`files.tsv` lists the movie with form `mkv`). WebM cannot carry FLAC, so the container is MKV. The manifest records it as output `movie.<name>` of converter `str-mkv`, with the hash of the movie's raw sectors (§8); an unchanged source keeps its MKV, so a second extraction does not re-encode it.
- **Encoder dependency:** ffmpeg is an **extract-time tool only**. It is never linked or shipped. `VS.Extract` finds it on `PATH` or through `--ffmpeg <path>`. The launcher and `VS.Extract` both stop with an error naming the FFmpeg if it has neither encoder, and `VS.Extract` stops if a conversion fails. Verified on the dev machine: ffmpeg 8.1.1 (winget Gyan.FFmpeg) has `libsvtav1`, `libaom-av1` and `flac`. Gyan's "essentials" 9.0.2 build has `libaom-av1` but no `libsvtav1` (#254).
- **Encoder comparison** (ffmpeg 8.1.1, whole intro, against a lossless decode of the same source): `libsvtav1` 44.5 dB, 10.2 MB, about 1 s; `libaom-av1` 44.0 dB, 9.3 MB, about 9 s. Both have 593 frames and identical audio, and both play through the game's dav1d path.
- **Runtime (`Engine\Movie`):**
  - Matroska demux: libwebm `mkvparser`, BSD.
  - AV1 decode: **dav1d**, BSD.
  - FLAC decode: dr_flac or libFLAC.
  - The YUV planes are uploaded to an SDL3 GPU texture and converted in a shader. Presentation is clocked to the audio, and skipping is the game's original behavior.
- **Validation:** frame N of the decoded MKV against a jpsxdec decode of the same frame, with a PSNR threshold of ≥ 45 dB (near-lossless), for the first 60 frames and every 300th frame after that. Today's disc-corpus test checks that the MKV keeps every frame FFmpeg's `psxstr` demuxer completes from the STR (593) at 15 frames a second; the PSNR comparison is not yet a test.

### 6.8 Workshop recipes (SYD → CSV tables, #499)

`MENU\BLADE.SYD`, `SHIELD.SYD` and `ARMOR.SYD` are the workshop's combination recipes. Each one starts with a 12-byte header of three offsets: the pair matrix (the result record by both ingredients' record indices), the material matrix (the material two ingredients' materials give; blades and armor also index it by category) and the result records (16 bytes for a blade, 8 for a shield or armor piece). `VS.Extract tables <datadir>` (and `extract`) converts each file to three CSV tables under `catalogs\menu\` (`Extract/WorkshopRecipeConversion.hpp`, `Content/WorkshopRecipes.hpp`):

- `combination-<kind>-results.csv`: one row per result definition, with a semantic `id` (`blade.battle-knife`), its compatibility `legacy_id` (the record index the original matrices use) and the current fields (model, category, gem slots or the blade's damage, cost and range fields, and the strength, intelligence and agility modifiers). Named enum domains and removal of remaining `raw.byte7`/`raw.byte11` authoring columns are still required by §4; semantic IDs alone do not complete that work.
- `combination-<kind>-pairs.csv`: `first,second,result`, all result ids, one row per ingredient pair that makes something.
- `combination-<kind>-materials.csv`: `first_material,[first_category,]second_material,[second_category,]material`, one row per combination that gives a material (1 wood to 7 damascus).

Empty matrix entries have no row. Each table's schema is committed in `Native\Content\schemas\catalogs\menu\` and written beside it, and a test checks that the two match. The manifest lists each kind as one output (`catalog.menu.combination-<kind>`, converter `syd-recipe-csv`) holding its six files and the source hash.

Conversion keeps edits. A rerun leaves the tables of an unchanged SYD alone, reconverts those whose SYD changed and with `--force` rewrites them all. The converter only writes tables that parse back to the decoded recipes and rebuild the original file byte for byte, so a disc that does not convert exactly is an error rather than a lossy output. At startup the data store reads and checks every table (types and ranges, every reference names a result, ingredients fit the pair matrix, `legacy_id`s run from 0 without gaps) and refuses to open on the first error, naming the file, line and column and telling the user to rerun `VS.Extract tables`. `RequireConversions` refuses to start when a SYD has no tables or its source hash is stale. The workshop builds its recipe book from the loaded tables (`Menu::Setup::BuildRecipeBook`, which rebuilds the original's dense matrices) and never reads the SYD. On the disc, the three files give 96 blade, 17 shield and 81 armor result records (record 0 of each is the empty one), with 8,100, 256 and 4,096 pairs and 2,025, 49 and 784 material rules.

SQLite compilation and patch/mod layers are implemented below. Still required:
shared item-definition references, retirement of the recipe matrix bridge, and
an in-game combination replay (#58, #499).

#### Semantic recipe identities and import-only records

`VS.Extract authoring <datadir>` upgrades generated `<kind>-NNN` result keys
and every directed pair reference in place, without reconverting source data.
`tables` and full `extract` also run this step. Existing custom identities,
labels and value edits are preserved. The importer uses the source item names;
ambiguous or unencodable names require an explicit identity, not a numeric
suffix or an invented meaning.

The two Footman's Maces use their verified categories:
`blade.footmans-mace.axe-mace` and `blade.footmans-mace.heavy-mace`.
The same rule disambiguates duplicate names only when a known category
distinguishes them. Remaining collisions fail explicitly.

The five stock `untitled` tail records have no recipe references. They move
to `catalogs/menu/import/combination-blade.json` as original record bytes keyed
by `legacy_id`, described by `workshop-import.schema.json`. This is **import
provenance**, not another item catalog. The game does not read these records;
the offline `EncodeWorkshopAuthoring` export restores them for exact legacy
reconstruction. An edited/referenced record or custom caption is not discarded:
it needs an explicit authoring identity. Explicitly authoring a former unused
slot takes precedence over its archived original.

Result schemas describe their old-ID alias pattern. Readers resolve aliases
through `legacy_id`, normalize pairs to current IDs and reject ambiguous aliases
or duplicate resolved directed pairs. Old numeric identifiers do not remain the
primary authoring keys, and renaming a display label does not rename an ID.

The same pass writes verified domains as names: materials (`bronze`, `iron`,
`hagane`, etc.), blade categories (`dagger`, `axe-mace`, `heavy-mace`, etc.),
damage (`blunt`, `edged`, `piercing`) and resource cost (`mp`, `risk`, `hp`,
`weapon-pp`, or `none` with zero cost). Each CSV schema carries its code-to-name
`enum` map. Numeric aliases remain explicitly accepted for legacy imports;
the authoring pass emits canonical names, and key comparisons normalize aliases
so numeric/named spellings cannot create duplicate rules.

`cost_type` is the weapon resource selector, not `ActionDefinition::type`:
`EquipmentStats` passes it through `WeaponLoadout.action_type` to
`ActionCostsLive` and the resource mapping in `ActionCosts`. Unsupported
selectors remain byte-preservable by the legacy codec but are refused as native
authoring values. Numeric quantities remain numeric. Other enum/flag families
still require their own verified mappings.

The shield/armor overlap is not byte-identical: 15 shield records have gem
slots where the corresponding armor import byte is zero. Original and native
`combineArmor` do not read the imported model or gem-slot bytes, whereas
`combineShields` does. Those context-specific values remain exact. Shared IDs
must agree on the category, stats and legacy identity both paths consume;
divergent authored variants require distinct IDs. A single shared item
definition with all import-only fields separated is still part of #499.

#### Layered compiled workshop catalogs (#499)

The data store loads the nine workshop CSV tables and the material definitions
(§4.2) when their outputs are listed, applies the ordered roots in
optional `catalog-layers.json`, validates their values/references and compiles
them into a private SQLite database. Production recipe books are built from
queries of that database, not a separate unpatched CSV path. The importer keeps
its base-only path so offline preservation/reconversion does not bake mods into
the source catalog.

Layers implement plan section 9.6: partial-field merge by schema key, complete
add/replace, and explicit remove. Directed pair keys preserve order. Enum codes
and legacy result aliases normalize before matching; ambiguous identities and
duplicate resolved keys fail. Later roots win with a deterministic conflict
report. Numeric values, readable enum names and row source locations survive
SQLite compilation. Foreign keys are enabled; compilation is transactional and
the published database is read-only.

Validated catalogs persist as `.cache/catalog.sqlite`. The input fingerprint
includes actual CSV and schema contents, the serialized compiler schema
contract, compiler version, layer configuration and every selected layer file
in deterministic root/file order. CSV timestamps are never an authority.
Unchanged input opens the cache read-only and snapshots it into the owned
immutable catalog; releasing its file handle permits atomic replacement on
Windows while a previous store remains alive. Conflict provenance is preserved.

Compilation reads an immutable snapshot of the same input bytes that were
fingerprinted, then checks the current fingerprint again. Only successfully typed
consumer records may publish the serialized SQLite file. Failed validation or
changed inputs leave the old cache intact; missing/corrupt/stale caches rebuild.
An unavailable cache does not make a read-only asset tree unusable. Base files
and import provenance are never changed by loading layers, and a SQLite cache
does not retire EC-04-T-SYD's remaining recipe/item layout constraints.
See [BUILDING.md section 5.1](BUILDING.md#51-recipe-patch-and-mod-layers) for the
configuration and patch syntax. Other catalog families and asset-path overrides
are not adopted; these bounded slices do not complete #499 or #520.

### 6.9 Zones (ZND → CSV tables, #199, #517)

A `MAP\ZONE*.ZND` is what the room loader reads when the zone changes (`Content/ZndFile.hpp`; the game's `_loadZnd`). Its 0x1C-byte header holds the room table, enemy section and texture section (each as offset and size) and the zone's music id. A reserved word follows, zero in every file. The sections are packed back to back to the end of the file:
- **Room table:** (sector, read size) per slot; an empty slot is (0, 0).
- **Enemy section:** a count, that many ZUD (sector, read size) entries, then that many 0x464-byte templates (`vs_CharInitData`, `Actors/CharacterInit.hpp`).
- **Texture section:** either 32 zero bytes, or four table sizes of which only the first is used, followed by that table: a count and that many TIMs, each after its byte size.

`VS.Extract tables <datadir>` (and `extract`) converts all 256 zones with converter `znd-tables` version 2 (`Extract/ZndConversion.hpp`, `Content/ZoneTables.hpp`). `tables` runs after `models`, because the zones name the enemy archetypes (§6.2.5). It writes six CSV tables, each with a schema committed in `Native\Content\schemas\catalogs\` and written beside it (a test checks that the two match):
- `catalogs\world\zones.csv`: `id` (`zone.map.zone009`), `music_id` and `room_slots` (the room table's length, used or not).
- `catalogs\world\zone-rooms.csv`: one row per used slot, naming the room by id (`room.map.map009`).
- `catalogs\world\zone-textures.csv`: the texture table's images by id, in upload order.
- `catalogs\enemies\zone-enemies.csv`: one row per template.
  - `id` (`enemy.map.zone009.00`), `zone`, `slot` and the `archetype` it loads (`archetype.map.z009u00`).
  - The template's scalars, with the weapon, shield and accessory fields that sit around their parts.
  - The two 24-byte names as hexadecimal in the game's encoding (column type `bytes`), until the string table (#500) gives them text ids.
- `catalogs\enemies\zone-enemy-limbs.csv`: one row per limb, except its armour part.
- `catalogs\enemies\zone-enemy-parts.csv`: one row per equipment part: `weapon.blade`, `weapon.grip`, `weapon.gem1-3`, `shield.base`, `shield.gem1-3`, `accessory` and `limb0-5.armor`.

All six tables receive early informational `display_name` columns through
§1.2. In particular, `zone-enemies.csv` shows the decoded enemy name while its
`name` and `weapon_name` hex fields continue to preserve every source byte.

**Named fields and domains (version 2).**
- `enemy_class` uses `human`, `beast`, `undead`, `phantom`, `dragon`, `evil`.
  Weapon, shield and limb-armor material fields use the shared named material
  domain (`none`, `wood` through `damascus`).
- Physical-type columns name `none`, `blunt`, `edged`, `piercing`; class ratings
  name the six classes; affinities name `physical`, `air`, `fire`, `earth`,
  `water`, `light`, `dark`. The remaining array tail bytes are explicit
  `storage_6`/`storage_7` fields, not invented classes/elements. They still need
  separation into import provenance when native template consumers are adopted.
- DP splash columns identify their zero-based destination limb and percent
  units. A limb index is topology, not a universal head/arm/leg name; actual
  body-part names depend on the model.
- The old enemy `material` column becomes `movement_class`. It feeds both
  gait/movement and body-load selection, **not** the equipment material enum.
  Its packed semantics remain an explicit blocker rather than guessed names.
- Mappings follow `StringTables.hpp`'s status indices, `Targeting.cpp`,
  `EquipmentStats.cpp`, `EquipmentAffinity.cpp` and `ActorSpawning.cpp`, with
  upstream structures/consumers in `BATTLE.PRG/charInit.h` and `146C.c`.

Schemas expose each renamed column's `legacy_name`. Readers accept either name,
but refuse both names for one column; enum numeric aliases remain supported.
`VS.Extract authoring <datadir>` upgrades existing headers and enum spellings
without replacing IDs, labels, record values or row order. It validates all six
tables and compares every decoded native record before writing, then publishes
the new converter version last. A normal `tables` run upgrades version-1 tables
before considering source reconversion, preserving existing native edits.
Unknown enum codes remain preservable by the raw ZND codec, but are refused as
native authoring values. This does not resolve the remaining part categories,
flags, strings, semantic identities or relational enemy definitions.

**Exactness.**
- Every bit of a template is in exactly one column, and a test checks the coverage. Unnamed bytes are `raw_<offset>` columns (§4.1).
- A limb or part whose bytes are all zero, and an empty room slot, has no row; a missing row reads as zeros.
- Rows name files by id, never by sector. The rebuild takes each named file's sector and sector-rounded size from `files.tsv` as it is now.

**Zone textures.** The texture table's TIMs become `textures\zone\<ZONE>\<nn>.png` with their sidecars (image id `image.map.zone009.00`), using the shared TIM rules (§6.4). Provenance names the zone, its hash and the TIM's byte offset. All 782 are 16-bit: room palette rows and texture pages, kept as the words the zone uploads. A page's packed 4- or 8-bit indices become an indexed image when the rooms (#198) bind each page's depth and palette.

**Manifest and reruns.** The manifest has one output per zone (`zone.map.zone009`), listing the twelve table files it shares with every zone and its own images. The tables hold every zone, so a run that converts any zone writes them all. A run whose zones are all unchanged writes nothing, so edits survive; `--force` rewrites everything.

**Refusals.** The converter writes nothing unless every zone's rows and images rebuild the source file byte for byte and the tables read back to the same rows. It refuses:
- a room or ZUD entry whose sector starts no extracted MPD or ZUD, or whose size is not that file's read;
- an empty texture table;
- an archetype that the `zud-json` converter has not recorded ("convert the models first").

**Runtime.** The room loader's zone read (`RoomLoadingLive`, `BeginConvertedZoneRead`) fills the block the disc read would have filled from `Content::LoadConvertedZone` and never reads the zone file. It completes in the requesting update.
This is **EC-04-T-ZND**, not completion of #199: the native input is still
repacked in memory for an original-layout consumer. Replace that path with
typed zone/enemy definitions; do not make its byte-sized block the native
authoring or allocation limit.
- The tables are read once and kept while their files are unchanged.
- Each texture image must name its zone and that zone's manifest hash, and is rebuilt into its TIM (`Content::TimFromImage` and `EncodeTim`, shared with the importer).
- A table error names the file, line and column. An unknown room, archetype or image, a stale image, or a zone rebuilt larger than the read's block is refused with the cause logged. The loader then refuses the zone as a failed read.
- `RequireConversions` refuses to start without a current `znd-tables` output for every zone.

**Corpus and tests.**
- 256 zones: 455 templates, 512 rooms and 421 empty slots, and 782 TIMs in 76 tables.
- The tables hold 455 template rows, 2,730 limb rows and 1,470 part rows.
- All 256 zones decode and rebuild exactly (`ZndFileTests.cpp`) and load from their tables as their exact bytes (`ZoneConversionTests.cpp`).
- A synthetic zone covers edits to a template field, a room slot and a texture, each refusal, the live read and two identical forced conversions (`ZoneConversionTests.cpp`, `ZoneTableTests.cpp`).

Not yet done (#199):
- the relational species, variant, loadout and loot tables of the enemy data scheme;
- references from the parts to the item catalog;
- the texture pages' depth and palette binding;
- SQLite and the patch and mod layers;
- production spawn, combat, drop and bestiary evidence for edited enemies;
- a zone rebuilt larger than its original read.

---

## 7. Round-trip and validation (per converter)

The comparisons below are **offline preservation tests of imported sources**.
They are not instructions to reconstruct legacy files during gameplay.
Production adoption is proved separately by typed-definition consumers,
native-only edits (including definitions not representable in the old format),
lifecycle tests and the format's gameplay/reference evidence. Every converter
ships with a test in `VS.Tests` (tagged `[extract]`; it runs only when a disc is configured):

| Converter | Test |
|---|---|
| Tier 1 tables | disc bytes → decoder → CSV/JSON → runtime loader → re-encode with the PS1 layout == the original bytes |
| workshop recipes | each SYD → decode → CSV tables → parse == the decoded recipes → re-encode == the original bytes (§6.8) |
| text | string bytes → markup → encode == the original bytes, for every string |
| SHP/SEQ/WEP | `decode(raw)` == `ModelLoader(glTF)` field by field (bones, vertices, UVs, colours, flags, every `extras` value, every keyframe). SHP and WEP today: each extracted file decodes and re-encodes to its exact bytes, and each converted model loads back to its exact bytes (§6.2, §6.2.4) |
| ESQ/ETM actor bindings | decoded source == converted/reloaded skeletal and texture tracks, indexed pixels, palettes, patch rectangles, associations, timing and lifecycle commands; malformed or unresolved inputs fail explicitly |
| MPD | the same, via `RoomLoader` |
| textures | PNG + `.image.json` (+ `.palettes.png`, `.stp.png`) → indices and 16-bit texel/palette words == the original (exact), including `0x0000` vs `0x8000` and every STP bit |
| samples | WAV PCM == the ADPCM decode (exact); loop points equal. Every extracted bank (`[content-audio][extract]`) |
| sequences | JSON → encode == the original AKAO bytes |
| XA | FLAC PCM == the ADPCM decode (exact), checked against an independent decoder of the selected sectors |
| movies | PSNR test (§6.7; not yet a test); frame count and duration equal (`[content-movie][extract]`) |
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

### 7.2 Authoring readability gate (D26, owner chose §4)

This is an additional acceptance gate for every open asset-migration issue,
including already converted preservation slices. It does **not** replace exact
rebuild, deterministic output, native adoption, lifecycle or gameplay evidence.
A label-only pass or one changed numeric value is not sufficient.

- [ ] Definitions use semantic IDs and readable names. Positional/filename
  identifiers remain only in explicit provenance/compatibility mappings.
  References, joins and persisted identities are migrated and validated together;
  distinct variants and mutable instances are not silently collapsed.
- [ ] Known enums have machine-readable schema domains and are authored by name.
  Known flags use named sets. Packed fields expose their proven component roles,
  and numeric quantities/indices have units and documented meaning.
- [ ] Column families, instruction arguments and animation/track records name
  their meaning; no known value is hidden in an ordinal column or untyped integer
  list. Source citations support the mappings; guesses do not pass.
- [ ] Recipe/grid editing is understandable from semantic ingredient/result
  references or a grid view over one authoritative source. Direction-dependent
  entries, shared definitions and derived values have preservation tests.
- [ ] Unknown consumed meanings are explicitly tracked and block completion.
  Proven unused/reserved bytes remain lossless import provenance, not misleading
  authoring controls. Placeholder/dummy records are identified explicitly.
- [ ] A cold-read check selects five records per affected family with a recorded
  seed and records their interpretation: what each record is, what its values
  mean, what it references and how to make the intended edit, without opening
  the decomp. Missing meanings fail the gate rather than becoming guessed labels.
- [ ] The same records pass schema validation and native-consumer/round-trip
  checks after an authoring edit; ordinary loading, reset and repeat entry still
  work. Include exact revision/configuration, commands, source evidence, CI and
  remaining limits before closure.

---

## 8. Manifest and versioning

`Native\data\manifest.json`:

```json
{
  "schema_version": 2,
  "extractor_version": "0.1.0",
  "converters": { "far-layout-json": 2, "fbc-palette-png": 1, "fbt-texture-png": 1, "seq-gltf": 1, "shp-gltf": 2, "str-mkv": 1, "syd-recipe-csv": 1, "tim-png": 2, "wave-bank": 1, "wep-gltf": 1, "xa-flac": 1 },
  "outputs": [
    {
      "id": "image.bg.castle-silhouette",
      "source": "BG/001OP01A.TIM",
      "source_sha256": "…",
      "converter": "tim-png",
      "files": [
        "textures/bg/castle-silhouette.png",
        "textures/bg/castle-silhouette.image.json"
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
- **Aliases** (schema version 2): an exact duplicate or compatibility ID lists no files and names in `alias_of` an output of the same converter that has files and the same source hash. Readers load the target's files (`ConversionManifest::FilesOf`), and an edit to them applies to every alias. Multiple IDs for one physical source are accepted only when they share that canonical owner; each alias retains its dependency checks. Effect FBT pages were the first aliases (§6.4); background source-era IDs now alias their semantic resources (§6.4.1). Version 1 manifests, which have no aliases, still read.
- **Unused content:** valid content known to be unused is still converted and marked `"unreferenced": true` (for example `DBGFNT.TIM`). Skipped and unconverted output entries are not part of this schema.
- **Versions:** each converter has an integer version in code (`VS.Extract`). The runtime embeds the minimum version it requires for each converter, so an old `Native\data\` is rejected with instructions (§2).
- **Interleaved sources:** an XA or STR file (`files.tsv` form `stream` or `mkv`) has no extracted copy under `disc\`; its `source_sha256` is the hash of its raw 2352-byte sectors, read from the disc image by `VS.Extract`. The runtime cannot rehash these, so it checks only that each one has its output and that the output's files exist. One-byte zero files, like empty ones, are placeholders and need no output.
- **Composite dependencies:** an output entry retains its primary `source` and adds a sorted `dependencies` array of `{ "source", "source_sha256" }` records for every other consumed input. Character outputs include the resolved SEQ/ESQ/ETM files and palette sources, not just SHP. A changed/deleted dependency or contributing converter/schema version invalidates the output; a missing dependency is an error, never a reason to reuse a stale model.
- **Character contract changes:** version ESQ/ETM conversion alongside the model converter and `asset.extras.vs.schemaVersion` (or its explicitly migrated extension equivalent). Runtime requirements must include the VS payload version; successful generic glTF loading is not an acceptable fallback for incompatible gameplay data.
- **Wrong disc:** the disc hash is checked against the known SLUS-01040 hashes; another release or a modified disc gives a warning, not a failure.

---

## 9. Tracking (ledgers and gates)

- **`CONTENT_LEDGER.csv`** (plan §4.5, #503) has one row per disc file. Its columns: `path, size, kind, decoder, converter, output, round_trip, status, notes`. `python tools\ledger\content.py --sync` rebuilds it from an extracted disc (`Native\data\files.tsv` and `manifest.json`); `--check` runs in CI without a disc and compares the ledger with the extracted data when there is one.
  - `converted`: the file is the source of a manifest output. Its converter must be one the runtime reads (startup `kRequiredConverters`, or the FAR reader's `kBackgroundLayoutConverters`). `round_trip=pass` needs every disc-corpus test `content.py` names for the converter to be a Catch2 test cited by a passing `VERIFY_LEDGER` record; otherwise `pending`.
  - `skipped`: a placeholder (no bytes, or one zero byte), or a file §6.1 drops (`SYSTEM.CNF`, `ENDING\NULL.DAT`), with the reason first in `notes`; the other columns are `n/a`.
  - `unconverted`: everything else, noting the issue that converts it. Feeding another conversion alone does not count: FAR was unconverted while it only supplied TIM depth, and now has its own layout output. Outputs written outside the manifest (effect-package sample streams; #216) do not count as converted.

<!-- content:begin (generated by tools/ledger/content.py --sync) -->

**3,047 of 5,162 disc files converted**, 3,047 of them with a passing round trip; 259 skipped with a reason; 1,856 unconverted.

| Kind | Files | Converted | Round trip pass | Skipped | Unconverted | Converts under |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| actor archetype | 455 | 455 | 455 | 0 | 0 | #200 |
| actor shape | 202 | 202 | 202 | 0 | 0 | #203 |
| actor texture overlay | 80 | 80 | 80 | 0 | 0 | #207 |
| area map | 32 | 0 | 0 | 0 | 32 | #201 |
| background layout | 4 | 4 | 4 | 0 | 0 | #210 |
| bestiary | 1 | 1 | 1 | 0 | 0 | #499 |
| boot configuration | 1 | 0 | 0 | 1 | 0 | - |
| combination recipes | 3 | 3 | 3 | 0 | 0 | #499 |
| effect package | 290 | 0 | 0 | 0 | 290 | #216 |
| effect palette | 277 | 277 | 277 | 0 | 0 | #215 |
| effect plugin overlay data | 120 | 0 | 0 | 0 | 120 | #502 |
| effect texture page | 755 | 755 | 755 | 0 | 0 | #214 |
| ending illustration | 11 | 0 | 0 | 0 | 11 | #498 |
| equipment model | 127 | 127 | 127 | 0 | 0 | #204 |
| event script | 512 | 0 | 0 | 0 | 512 | #202 |
| filler | 1 | 0 | 0 | 1 | 0 | - |
| help graphics | 14 | 0 | 0 | 0 | 14 | #213 |
| help page | 14 | 0 | 0 | 0 | 14 | #212 |
| layered artwork | 49 | 0 | 0 | 0 | 49 | #209 |
| main executable data | 1 | 0 | 0 | 0 | 1 | #499, #500, #502 |
| menu image, text or table | 11 | 0 | 0 | 0 | 11 | #498, #499, #500 |
| movie | 1 | 1 | 1 | 0 | 0 | #504 |
| music sequence | 169 | 0 | 0 | 0 | 169 | #501 |
| placeholder | 257 | 0 | 0 | 257 | 0 | - |
| presentation images | 22 | 0 | 0 | 0 | 22 | #211 |
| program overlay data | 20 | 0 | 0 | 0 | 20 | #500, #502 |
| room | 512 | 0 | 0 | 0 | 512 | #198 |
| sample bank | 83 | 83 | 83 | 0 | 0 | #504 |
| shared animation set | 97 | 97 | 97 | 0 | 0 | #206 |
| skeletal animation | 700 | 700 | 700 | 0 | 0 | #205 |
| sound-effect sequences | 78 | 0 | 0 | 0 | 78 | #501 |
| system images and font | 1 | 0 | 0 | 0 | 1 | #498 |
| TIM image | 5 | 5 | 5 | 0 | 0 | #208 |
| XA audio stream | 1 | 1 | 1 | 0 | 0 | #504 |
| zone | 256 | 256 | 256 | 0 | 0 | #199 |

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
