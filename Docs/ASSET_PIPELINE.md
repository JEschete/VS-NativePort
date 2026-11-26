# Asset Pipeline: disc → `Native\data\`

This index is the entry point to the contract between the extractor (`VS.Extract`) and the game's loaders
(`VS.Content` and each system). It holds the principles and format decisions; each topic file under
[`pipeline/`](pipeline/) holds the per-format contracts and keeps its original section numbers, so a citation such
as "ASSET_PIPELINE §6.4" resolves through the section map below. Whenever a format changes, bump that converter's
version (§8).

Evidence and history removed in the 2026-11-24 split remain in Git: git show 215cdd4d9:Docs/ASSET_PIPELINE.md.

Status: **design v1, accepted by the owner**. The owner's decisions are:
- Flat gameplay tables as CSV, compiled into SQLite; structured data as JSON (D25, refining the original "JSON for tables").
- Convert **everything up front**, not phase by phase.
- Converted output is git-ignored.
- Models match the `resources\Sydney Losstarot (Dying)\` glTF, packaged as `.gltf` + `.bin` + shared PNG files (D25).
- Video is **AV1**.
- Music is **sequenced**.
- Formats are chosen for ease of modding, without losing exactness (D25, §1.1).

**Implementation boundary:** this is a target contract, not a claim that every converter or loader exists. The character/event-animation packages in §6.2.3 are planned work, scheduled through ordered tracker [#4](https://github.com/JEschete/VS-NativePort/issues/4). They do not move asset migration ahead of maintained-code quality or the queued gameplay work. Legacy formats remain import sources, not the long-term authoring interface.

**Runtime adoption is not byte reconstruction (owner clarification,
2026-11-23).** Rebuilding ZND/ZUD/SHP/SEQ/ESQ/WEP/ETM bytes in memory, or keeping
SYD's byte/index matrices, is temporary compatibility work. The active adapters,
owning issues and retirement gates are in the
[EC-04 T-shim ledger](plan/EC_REGISTER_2.md#ec-04-asset-t-shim-ledger).
Production must read typed definitions and construct owned native instances.
Byte-exact reconstruction belongs only to offline preservation tests of imported
originals. A new native definition may be impossible to pack into a PS1 file and
must still be usable; gameplay policy and file-format limits are different.

## Section map

| § | File | Topic |
|---|---|---|
| 1 | [this file](#1-principles) | Principles |
| 1.1 | [this file](#11-format-decisions-owner-2026-11-21-plan-d25) | Format decisions (owner, 2026-11-21, plan D25) |
| 1.2 | [this file](#12-readable-names-owner-2026-11-23-plan-d26-520) | Readable names (owner, 2026-11-23, plan D26, #520) |
| 2 | [OUTPUT_AND_TABLES](pipeline/OUTPUT_AND_TABLES.md#2-flow) | Flow |
| 3 | [OUTPUT_AND_TABLES](pipeline/OUTPUT_AND_TABLES.md#3-output-layout-nativedata-git-ignored) | Output layout (`Native\data\`, git-ignored) |
| 4 | [OUTPUT_AND_TABLES](pipeline/OUTPUT_AND_TABLES.md#4-tier-1-executable-and-overlay-tables--csv-tables-and-json-catalogs) | Tier 1: executable and overlay tables → CSV tables and JSON catalogs |
| 4.1 | [OUTPUT_AND_TABLES](pipeline/OUTPUT_AND_TABLES.md#41-lossless-extras) | Lossless "extras" |
| 4.2 | [OUTPUT_AND_TABLES](pipeline/OUTPUT_AND_TABLES.md#42-native-material-definitions-499) | Native material definitions (#499) |
| 4.3 | [GAMEPLAY_CATALOGS](pipeline/GAMEPLAY_CATALOGS.md#43-native-encyclopedia-definitions-499500) | Native encyclopedia definitions (#499/#500) |
| 4.4 | [GAMEPLAY_CATALOGS](pipeline/GAMEPLAY_CATALOGS.md#44-native-learning-requirements-499) | Native learning requirements (#499) |
| 4.5 | [GAMEPLAY_CATALOGS](pipeline/GAMEPLAY_CATALOGS.md#45-native-status-rules-and-shared-timing-499) | Native status rules and shared timing (#499) |
| 4.6 | [GAMEPLAY_CATALOGS](pipeline/GAMEPLAY_CATALOGS.md#46-native-action-categories-499) | Native action categories (#499) |
| 4.7 | [GAMEPLAY_CATALOGS](pipeline/GAMEPLAY_CATALOGS.md#47-native-actions-499) | Native actions (#499) |
| 4.7.1 | [ACTION_HITS](pipeline/ACTION_HITS.md#471-action-hit-values-520) | Action hit values (#520) |
| 4.7.2 | [ACTION_HITS](pipeline/ACTION_HITS.md#472-action-behavior-flags-520) | Action behavior flags (#520) |
| 4.7.3 | [ACTION_HITS](pipeline/ACTION_HITS.md#473-action-volumes-520) | Action range and area volumes (#520) |
| 4.7.4 | [ACTION_HITS](pipeline/ACTION_HITS.md#474-action-presentation-and-limb-damage) | Action presentation and limb damage (#520) |
| 4.7.5 | [ACTION_HITS_COLD_READ](pipeline/ACTION_HITS_COLD_READ.md#475-seeded-action-hit-cold-read) | Seeded action-hit cold read (#520) |
| 4.7.6 | [ACTION_HITS_COLD_READ](pipeline/ACTION_HITS_COLD_READ.md#476-seeded-action-definition-cold-read) | Seeded action-definition cold read (#520) |
| 4.7.7 | [ACTION_HITS_COLD_READ](pipeline/ACTION_HITS_COLD_READ.md#477-seeded-action-category-cold-read) | Seeded action-category cold read (#520) |
| 5 | [TEXT](pipeline/TEXT.md#5-tier-2-text-and-fonts) | Tier 2: text and fonts |
| 5.1 | [TEXT](pipeline/TEXT.md#51-native-file-picker-prompt-adoption-54500) | Native file-picker prompt adoption (#54/#500) |
| 5.2 | [TEXT](pipeline/TEXT.md#52-shared-catalog-ownership-and-encyclopedia-text) | Shared catalog ownership and encyclopedia text |
| 6 | [FORMAT_MAP](pipeline/FORMAT_MAP.md#6-tier-3-disc-assets--modern-formats) | Tier 3: disc assets → modern formats |
| 6.1 | [FORMAT_MAP](pipeline/FORMAT_MAP.md#61-format-map) | Format map |
| 6.2 | [CHARACTER_MODELS](pipeline/CHARACTER_MODELS.md#62-character-models-shp--seqesq--etm--gltf-matching-the-sydney-reference) | Character models (SHP + SEQ/ESQ + ETM → glTF), matching the Sydney reference |
| 6.2.1 | [CHARACTER_MODELS](pipeline/CHARACTER_MODELS.md#621-container-schema-and-optional-extension) | Container, schema and optional extension |
| 6.2.2 | [ACTOR_ANIMATION](pipeline/ACTOR_ANIMATION.md#622-event-and-facial-texture-animation-contract) | Event and facial texture animation contract |
| 6.2.3 | [ACTOR_ANIMATION](pipeline/ACTOR_ANIMATION.md#623-bounded-implementation-packages) | Bounded implementation packages |
| 6.2.4 | [WEAPONS_AND_ARCHETYPES](pipeline/WEAPONS_AND_ARCHETYPES.md#624-weapon-models-wep--gltf-204) | Weapon models (WEP → glTF, #204) |
| 6.2.5 | [WEAPONS_AND_ARCHETYPES](pipeline/WEAPONS_AND_ARCHETYPES.md#625-enemy-archetypes-zud--json-200-516) | Enemy archetypes (ZUD → JSON, #200, #516) |
| 6.3 | [ROOMS_AND_ZONES](pipeline/ROOMS_AND_ZONES.md#63-rooms-mpd--gltf--room-json) | Rooms (MPD → glTF + room JSON) |
| 6.4 | [IMAGES](pipeline/IMAGES.md#64-textures-and-images-timgimfbtfbcbin-art--png--imagejson) | Textures and images (TIM/GIM/FBT/FBC/BIN art → PNG + `.image.json`) |
| 6.4.1 | [BACKGROUNDS](pipeline/BACKGROUNDS.md#641-far-background-layouts) | FAR background layouts |
| 6.5 | [AUDIO_VIDEO](pipeline/AUDIO_VIDEO.md#65-sequenced-audio-owner-choice-sequenced) | Sequenced audio (owner choice: **sequenced**) |
| 6.6 | [AUDIO_VIDEO](pipeline/AUDIO_VIDEO.md#66-xa-streams--flac) | XA streams → FLAC |
| 6.7 | [AUDIO_VIDEO](pipeline/AUDIO_VIDEO.md#67-movies-str--av1-matroska-owner-choice-av1) | Movies (STR → AV1 Matroska) (owner choice: **AV1**) |
| 6.8 | [WORKSHOP_RECIPES](pipeline/WORKSHOP_RECIPES.md#68-workshop-recipes-syd--csv-tables-499) | Workshop recipes (SYD → CSV tables, #499) |
| 6.9 | [ROOMS_AND_ZONES](pipeline/ROOMS_AND_ZONES.md#69-zones-znd--csv-tables-199-517) | Zones (ZND → CSV tables, #199, #517) |
| 7 | [VALIDATION](pipeline/VALIDATION.md#7-round-trip-and-validation-per-converter) | Round-trip and validation (per converter) |
| 7.1 | [VALIDATION](pipeline/VALIDATION.md#71-separate-preservation-preview-and-playback-acceptance) | Separate preservation, preview and playback acceptance |
| 7.2 | [VALIDATION](pipeline/VALIDATION.md#72-authoring-readability-gate-d26-owner-chose-4) | Authoring readability gate (D26, owner chose §4) |
| 8 | [MANIFEST_AND_TRACKING](pipeline/MANIFEST_AND_TRACKING.md#8-manifest-and-versioning) | Manifest and versioning |
| 9 | [MANIFEST_AND_TRACKING](pipeline/MANIFEST_AND_TRACKING.md#9-tracking-ledgers-and-gates) | Tracking (ledgers and gates) |
| 10 | [MANIFEST_AND_TRACKING](pipeline/MANIFEST_AND_TRACKING.md#10-dependencies) | Dependencies |

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
| 7 | Unused content | Valid content with no reference (e.g. `DBGFNT.TIM`) is **converted** and marked `unreferenced` in the manifest, so modders can use it. Empty files are `skipped` with a reason; exact duplicates are recorded as aliases of one output. All four FAR backgrounds have MPD references (§6.4.1). |
| 8 | Short palette files | Palette rows past the end of a file (FBC) are **not stored**. The runtime fills them with zero (transparent), as the zero sector padding after each file gave the original. Stale bytes past the sector read are not reproduced; no sprite samples them. |
| 9 | Layout | §3. Patch and mod layers mirror it and override by path (assets) or by ID (tables), plan §9.6. |
| 10 | Coverage gaps | Formats with no migration issue get one (images in menus, ending, `SYSTEM.DAT` and PRG data; the gameplay catalog; the string table; AKAO sequences; PLG/PRG data blobs). |

### 1.2 Readable names (owner, 2026-11-23, plan D26, #520)

**The owner chose §4's semantic authoring contract.** This annotation pass is preparatory compatibility work, not
the destination or a format-completion gate. Positional/filename IDs and encoded fields in the current outputs still
need the explicit schema/consumer migrations below. A readable label does not make a packed numeric field or unnamed
command stream an authoring format.

`display_name` is an optional **informational** plain-text label. It is not an ID, a reference, or a replacement for
an encoded gameplay name:

- CSV tables put it immediately after their first identity/key column. Schemas describe it as optional text.
  `ReadTable` excludes it from semantic cells, so existing numeric column positions and key/reference checks do not
  change. Unlabeled tables remain supported.
- Native JSON puts it early in the object. Strict readers accept this one reserved annotation as a string; other
  unknown fields are still errors. Native glTF puts it at the start of `asset.extras.vs`, not in an invalid custom
  root property. Joint names, geometry and animation values do not change.
- Image sidecars identify PNGs; bank/stream metadata identifies WAV/SFZ and streamed audio. Binary payloads (`.bin`,
  PNG, WAV, SFZ, FLAC, MKV) are not rewritten just to attach text. Conversion manifests and schemas remain
  operational descriptions, not additional copies of author-facing names.

The `labels` command does not rename existing keys or replace encoded values. The §4 migrations must introduce
semantic IDs and named values, update their references/consumers together, and preserve legacy ID mappings and
source bytes explicitly. IDs remain stable after that migration; editing a display name must not rename an identity.

`VS.Extract labels <datadir>` annotates an existing native tree **without reconverting it**; successful `extract`,
`catalogs`, `images`, `models`, `tables`, `banks` and `streams` commands also run it. It uses the current native
table values, the extracted item-name table when available, and the manifest's verified model/archetype references;
it does not need or modify the disc image. Later passes preserve existing labels and gameplay edits.
`labels <datadir> --force` recomputes **only labels**, never HP, equipment, placements, encoded names, geometry,
palettes or other gameplay data. Each file is replaced through the existing staged writer; unchanged files are not
rewritten. Errors identify the file and return failure; completed files remain valid and the command can resume.
Labeling is not a claim that a format has been converted. Explicit reconversion commands keep their overwrite rules;
the labeling pass does not protect an asset from a requested converter `--force` or a catalog rebuild. The generic
label pass leaves native text/font and import-only schemas to their semantic owners, including refresh runs (#526),
and does not label a shared native text catalog with the name of one contributing executable.

Current name sources:
- Enemy and weapon display names decode the ZND name fields, which zone-enemies.csv's `name` and `weapon_name`
  hold as exact game-glyph text since converter version 12 ([§6.9](pipeline/ROOMS_AND_ZONES.md)). Accents and
  punctuation are preserved. The label omits layout controls; unknown glyphs/controls remain visible markers.
  Localized string IDs remain #500.
- Equipment parts and workshop results use `MENU/ITEMNAME.BIN` where a name is available. Ingredient pairs and
  material rules describe their inputs/results.
- ZUD references carry enemy names to their shared/owned model files. Shared models keep the applicable names in
  sorted order rather than choosing an arbitrary enemy and collapsing variants.
- Assets without a known canonical name use an explicit source/role or ID label. Such labels are descriptions, not
  invented names or verified room, animation, sound or effect semantics.

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
