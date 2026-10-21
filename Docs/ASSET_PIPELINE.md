# Asset Pipeline: disc → `Native\data\`

Status: **design v1, accepted by the owner**. The owner's decisions are:
- JSON for tables.
- Convert **everything up front**, not phase by phase.
- Converted output is git-ignored.
- Models match the `resources\Sydney Losstarot (Dying)\` glTF.
- Video is **AV1**.
- Music is **sequenced**.

This document is the contract between the extractor (`VS.Extract`) and the game's loaders (`VS.Content` and each system). Whenever a format below changes, bump that converter's version (§8).

---

## 1. Principles

| # | Rule |
|---|---|
| P1 | **The game never reads the disc.** At runtime, `VS.Desktop` loads only `Native\data\` (plus the patch and mod layers). Only `VS.Extract` reads `.bin/.cue`. |
| P2 | **Convert everything up front.** One command (`extract-native-assets.cmd`) converts every file on the disc. Nothing is converted lazily or per phase. If a format's converter is not written yet, the run reports it as `unconverted` in the manifest, and the gate in §9 fails. |
| P3 | **Modern, open formats only:** JSON, glTF 2.0, PNG, WAV, FLAC and Matroska (AV1 + FLAC). There are no PS1 formats (TIM, VAB, AKAO, STR, XA, MPD, …) under `Native\data\`. |
| P4 | **Lossless with respect to the game.** Every value the original code reads must survive conversion. Fields with no standard home go in documented `extras` or JSON members (§4.1). A round-trip test proves this for every converter (§7). |
| P5 | **Not committed.** `Native\data\` is git-ignored. Only the extractor, its schemas and the hand-written patches are committed. Users supply their own disc. |
| P6 | **Readable by humans and tools.** The output opens in Blender, any image viewer, VLC and a text editor. This is what makes Phase 2/3 modding possible. |
| P7 | **Deterministic.** The same disc plus the same extractor version gives byte-identical output (sorted keys, fixed float formatting, fixed encoder settings), so diffs between extractor versions are meaningful. |

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
- **Startup check:** `VS.Desktop` reads `manifest.json`. If the file is missing, a converter version is older than the one the build expects, or any entry is `unconverted`, it stops with a clear message telling the user to run `extract-native-assets.cmd`. It never falls back to reading the disc.

---

## 3. Output layout (`Native\data\`, git-ignored)

```
Native\data\
  manifest.json
  catalogs\          Tier 1: executable / PRG tables as typed JSON (§4)
    items\  blades.json grips.json gems.json armor.json shields.json accessories.json consumables.json misc.json
    battle\ break-arts.json abilities.json spells.json status-effects.json
    enemies\ species.json enemies.json modelsets.json loot.json bestiary.json      (ENEMY_DATA_SCHEME.md)
    world\  zones.json rooms\<zone>\<room>.json doors.json keys.json chests.json save-points.json
    menu\   workshops.json combine-blades.json combine-armor.json ...
    title\  title.json             (layout, timings, menu items)
    effects\ effects.json          (template instantiations → parameters)
    music.json sfx.json            (index: id → sequence/bank files)
    engine\ <table>.json           (other exe tables; one file per DATA_LEDGER `catalog` row group)
  text\              Tier 2
    strings.en.json                (every game string: id → UTF-8 text with markup)
    fonts\ main.png main.font.json (glyph atlas + metrics)
  models\            Tier 3: glTF 2.0 (one file each, embedded buffers/PNG like the Sydney reference)
    characters\<SHP>.gltf          (skinned mesh + all SEQ clips that apply to it)
    weapons\<WEP>.gltf             (material variants = blade materials)
    armor\ shields\ ...            (as discovered per CONTENT_LEDGER)
    zud\<ZUD>.json                 (enemy model set: references SHP/WEP/SEQ glTFs; no duplicate meshes)
  rooms\<zone>\<MAPxxx>.gltf       (room geometry + textures)  + <MAPxxx>.room.json (collision, tiles, doors, scripts)
  textures\<dir>\<name>.png        (TIM/GIM/FBT/standalone images; indexed PNG + .palettes.json where palettes matter)
  ui\                              (menu art, title art, illustrations as PNG + layout JSON)
  events\<id>.json                 (EVT event scripts as decoded command lists; text by string id)
  effects\<id>.json + textures     (EFFECT .P/.FBT/.FBC data)
  audio\
    samples\<bank>\<n>.wav         (16-bit PCM, `smpl` loop chunk)
    banks\<bank>.sfz               (sfizz instrument regions over the decoded WAV samples)
    music\<id>.seq.json            (decoded AKAO event list per track)
    sfx\<id>.seq.json              (sound-effect sequences)
    streams\<name>\<channel>.flac  (XA streams: ENDING.XA, voice)
  movies\<name>.mkv                (AV1 video + FLAC audio)
```

The exact file list is generated: `CONTENT_LEDGER.csv` (§9) gives every disc file its output path(s).

---

## 4. Tier 1: executable and overlay tables → JSON catalogs

**Source:** initialized data in `SLUS_010.40` and the `*.PRG` overlays. `build\port\data.json` (`tools\port\data_inventory.py`) lists them, and `DATA_LEDGER.csv` gives each one a disposition. Only `catalog` rows go through this tier. `constexpr` rows are computed in code, `state` rows become zeroed module state and `drop` rows disappear.

**Decoders are generated, not hand-written:**

1. `tools\port\gen_table_decoders.py` reads the decomp struct definitions (libclang, PS1 layout: 32-bit pointers, 4-byte alignment, GCC bitfield order). For each catalog table it emits:
   - a C++ reader (`VS.Extract`, `src\tables\Generated*.cpp`) that pulls fields from the file image at the symbol's offset;
   - a JSON writer;
   - a JSON Schema (`Native\Content\schemas\<catalog>.schema.json`, committed);
   - the runtime C++ record type plus its loader (`VS.Content`, committed). The runtime type uses **modern widths** (D3: no layout matching).
2. Field names come from `TYPE_LEDGER.csv` (the final names), never `unk_0x10`. A field that is still unnamed fails generation.
3. **Pointers become references:**

   | Original field | Becomes |
   |---|---|
   | function pointer | the function's **final name** (`"Battle::Abilities::ApplyHeal"`), resolved through `BehaviorRegistry` at load |
   | pointer to a string | a string ID in `strings.en.json` |
   | pointer to another table row | that row's catalog ID (`"blade.scramasax"`) |
   | pointer to an asset | an asset path relative to `Native\data\` |

4. Every record keeps its `legacyId` (the original index) so copied logic and `.mcd` save import keep working (plan §9.2).

### 4.1 Lossless "extras"
If a value has no clean meaning yet, it is **kept**, not dropped. It goes under `"_raw": { "<field>": value }` and is listed in `TYPE_LEDGER` as unnamed. Unnamed fields block the 1a gate, so `_raw` must be empty at the end of Phase 1.

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
| `SMALL\*.ARM` (32), `SMALL\*.DIS` (22) | `catalogs\` JSON + `textures\` | `.ARM` = minimap/area data, `.DIS` = to confirm in CONTENT_LEDGER | GodHands, DataCrystal |
| `MAP\*.MPD` (512) | `rooms\<zone>\<MAP>.gltf` + `.room.json` | §6.3 | vstools `MPD.js`, VSViewer, decomp field loaders |
| `MAP\*.ZND` (256) | `catalogs\enemies\*`, `catalogs\world\zones.json`, zone textures → PNG | enemy records → the ENEMY_DATA_SCHEME catalogs; zone TIMs → PNG | ENEMY_DATA_SCHEME.md §3 |
| `EVENT\*.EVT` (512) | `events\<id>.json` | command list `{op, args}` with op names from the decomp's event interpreter; text → string IDs | decomp `Event\` module |
| `BG\*.TIM` (4), `BG\*.FAR` (4), root `*.TIM` | `textures\bg\*.png` + `.json` | TIM → PNG (§6.4); `.FAR` = to confirm | |
| `GIM\*.GIM` (49) | `textures\gim\*.png` | VS screen-effect images | |
| `MENU\*.BIN` (11), `ENDING\ILLUST*.BIN` (11) | `ui\*.png` + layout JSON | menu art, game-over, ending illustrations | decomp `Menu\`, `Ending\` |
| `MENU\*.SYD` (3) | `catalogs\items\*.json` | item definition tables | GodHands |
| `SMALL\MON.BIN` | `catalogs\enemies\bestiary.json` | | ENEMY_DATA_SCHEME |
| `SMALL\HELP*.HF0/HF1` (28) | `text\strings.en.json` | help pages | |
| `EFFECT\*.P` (290), `*.FBT` (755), `*.FBC` (277), `EFFECT\*.BIN` (311) | `effects\<id>.json` + `textures\effect\*.png` | `.FBT` texture pages + `.FBC` CLUTs → PNG; `.P`/`.BIN` parameters → JSON; plugin code is already in `Native\` (the decomp). Effect behaviors stay in code (plan §9.7) | decomp `Effects\` |
| `OBJ\*.ESQ` (97), `OBJ\*.ETM` (80) | `effects\` JSON + PNG | effect sequences / effect textures (to confirm) | |
| `SOUND\WAVE*.DAT` (101) | `audio\samples\` + `audio\banks\` | §6.5 | decomp `Audio\` |
| `MUSIC\MUSIC*.DAT` (192) | `audio\music\*.seq.json` | §6.5 | decomp `Audio\Seq` |
| `SE\EFFECT*.DAT` (102) | `audio\sfx\*.seq.json` (+ bank if embedded) | §6.5 | |
| `ENDING\ENDING.XA` (1, 77 MB) | `audio\streams\ending\<ch>.flac` | §6.6 | jpsxdec |
| `MOV\TITLE.STR` (1, 26 MB) | `movies\*.mkv` | §6.7 | ffmpeg `psxstr`, jpsxdec |
| `BATTLE\SYSTEM.DAT` | split by content (textures → PNG, tables → JSON) | to confirm | decomp `Battle\` |
| `SLUS_010.40`, `*.PRG`, `GIM\SCREFF2.PRG` | Tier 1 tables only (code is already in `Native\`) | §4 | |
| `license_data.dat`, `SYSTEM.CNF`, `DBGFNT.TIM`, `ENDING\NULL.DAT`, 192 zero-byte placeholders | nothing (`drop`) | recorded in the manifest as `skipped: reason` | |

"To confirm" rows are settled while building `CONTENT_LEDGER.csv`. Settling a row means: read the decomp loader that consumes the file, then write the converter. **No converter is written from community docs alone.** The decomp's loader is the ground truth for what each byte means. vstools, VagrantStoryImporter, GodHands and DataCrystal speed up understanding, but they are reference only (licenses: `TOOLS_INVENTORY.md`).

### 6.2 Character models (SHP + SEQ → glTF), matching the Sydney reference

`resources\Sydney Losstarot (Dying)\6F.SHP.gltf` is the target. Every character file must have the same structure:

| Element | Rule (from the reference) |
|---|---|
| asset | glTF 2.0, one scene, embedded buffer and PNG as base64 data URIs (one self-contained file) |
| mesh | one skinned mesh; attributes `POSITION`, `NORMAL`, `TEXCOORD_0`, `COLOR_0`, `JOINTS_0`, `WEIGHTS_0` + indices |
| skeleton | one node per SHP bone named `bone<N>` (N = SHP bone index), with rotation + translation; one `skin` with `inverseBindMatrices` |
| animations | **every** SEQ that applies to this SHP, one clip per animation, named `<SHP>_<SEQ>_<n>` (e.g. `6F_BT2_0`) |
| material | one material, `KHR_materials_unlit`, `alphaMode: BLEND`, `metallicFactor: 0`, `roughnessFactor: 0.9` |
| texture | PNG with the palette applied; sampler `magFilter/minFilter = 9728` (NEAREST), wrap `10497` (REPEAT) |

Additions the reference does not have, required by P4 (lossless). They go in `extras` so that standard tools ignore them safely:
- `extras.vs.shp`: SHP header values the game uses (collision/size, shadow, overlay/attachment points for weapons and effects, magic points, bone groups, body-part mapping used by targeting).
- `extras.vs.seq` on each animation: the SEQ key data the game reads at runtime (frame count, per-key flags, sound/effect triggers, root-motion flags).
- Vertex colors and per-polygon flags (semi-transparency, double-sided, lighting mode) go in `COLOR_0` plus a per-primitive `extras.vs.polyFlags`.
- **Which SEQs belong to which SHP** comes from the decomp's loaders and the ZUD bundles, not file-name guessing. The SEQ file prefix (e.g. `00BT1B00` → SHP `00`) is a hint that is checked against the loader.
- **Fixed-point scale:** positions are SHP units divided by a constant `VS_UNIT_SCALE`, recorded in `asset.extras.vs.unitScale` so the loader can return exact integer values. The Sydney reference's scale is measured and matched.

**Runtime loader:** `VS.Content::ModelLoader` reads glTF (nlohmann-json; base64 buffers) into the engine's `ModelDef` / `AnimationDef`. In Phase 1 the copied actor code expects SHP-shaped data. The loader provides it from the glTF (bone table, vertex groups, the `extras`). This is why the round-trip in §7 is mandatory: `decode(SHP) == ModelLoader(glTF)` field for field.

**Also emitted** (optional, `--posed`): `<SHP>_posed.obj/.mtl/_0.png`, like the reference folder. This is for quick previews only; the game never reads it.

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

### 6.4 Textures and images (TIM/GIM/FBT/FBC/BIN art → PNG)
- **Alpha:** PS1 colour `0x0000` becomes alpha **0** (transparent). A colour with the STP bit set becomes alpha **128**; the blend mode comes from the material or primitive, not the texel. Every other colour is alpha **255**. This 3-state mapping is lossless and reversible.
- **One palette in use:** the output is RGBA PNG.
- **Several palettes (CLUTs) in use:** the output is an **indexed PNG** (4/8-bit) plus `<name>.palettes.json` (every CLUT as RGBA). Examples are WEP materials, palette animation and effect colour swaps. The renderer applies a palette as a lookup in the shader; Phase 2 can bake it if needed.
- **Colour:** 5-bit channels are expanded by bit replication (`c<<3 | c>>2`). The raw 15-bit value is recoverable, so this is lossless.
- **Placement:** images that the original placed in a shared VRAM page are written as **separate** files. Their placement in VRAM is runtime layout, not content (plan §0.1). Any UV remapping is done by the converter and checked in the round-trip.

### 6.5 Sequenced audio (owner choice: **sequenced**)
The game plays music and sound effects by sequencing sampled instruments at runtime. Tempo, volume, fades and battle transitions change live, so the content is converted into equivalent modern pieces and our ported sequencer (`Audio::Seq`, copied from the decomp `Audio\Seq`) plays it:

| Piece | Format | Detail |
|---|---|---|
| Samples | `audio\samples\<bank>\<n>.wav` | PS1 4-bit ADPCM decoded once to **16-bit PCM** (the decode is deterministic, so this is lossless with respect to what the console plays). The loop start/end go in the WAV `smpl` chunk; the original sample rate is kept. |
| Instrument banks | `audio\banks\<bank>.sfz` | SFZ regions map samples, key ranges, root key, fine tune, loop points, envelope, volume and pan for sfizz (owner decision D23); any original values not represented by SFZ must remain available to the event/asset conversion path. |
| Music | `audio\music\<id>.seq.json` | the AKAO byte stream decoded into an event list per track: `{tick, op, args}` with op names taken from the decomp's sequencer opcode table (note on/off, tempo, volume, pan, program change, loop markers, jumps, vibrato/tremolo, reverb, …). Jumps target event indices, not byte offsets. |
| Sound effects | `audio\sfx\<id>.seq.json` | same event format; banks are shared or embedded as the original does |
| Index | `catalogs\music.json`, `catalogs\sfx.json` | `music.<name>` / `sfx.<name>` IDs → files, with `legacyId` (the original track/SE number) |

- **The event format is documented** in `Native\Content\schemas\akao-seq.schema.json`. An encoder exists for the round-trip test (`decode → JSON → encode == original bytes`), and that encoder is also the modding path for new songs.
- **Mixer/voices:** behavioral, at the envelope level, at 48 kHz (plan §6.3), never SPU registers.
- **Rendered previews:** none (owner choice). Phase 2 can add a `--render-previews` flag if wanted.

### 6.6 XA streams → FLAC
- **Source:** `ENDING\ENDING.XA` (77 MB, interleaved channels: ending music/voice). XA is read from the raw mode-2 form-2 sectors. It cannot come from the ISO-extracted `SLUS-01040\` folder, because that drops the subheaders.
- **Decode:** each channel (file/channel subheader) is decoded from XA ADPCM to PCM at its native rate (37.8 or 18.9 kHz, mono or stereo).
- **Output:** one **FLAC** file per channel (lossless) in `audio\streams\ending\<ch>.flac`, plus `streams.json` (channel → use, length, loop).
- **Runtime:** `Audio::StreamPlayer` decodes the FLAC (dr_flac or libFLAC) and resamples it to the 48 kHz mixer.

### 6.7 Movies (STR → AV1 Matroska) (owner choice: **AV1**)
- **Source:** `MOV\TITLE.STR` (26 MB, raw mode-2 sectors: MDEC video + interleaved XA audio). It is read from the `.bin`, like XA. Whether it holds more than one movie (intro, attract) is settled from the decomp's movie code (`_initIntroMovie`, `_playIntroMovie`) and recorded in CONTENT_LEDGER.
- **Pipeline** (extract time only):
  1. `VS.Extract` writes the raw 2352-byte sectors of the movie to a temporary file.
  2. ffmpeg decodes it with its `psxstr` demuxer and `mdec` decoder, and encodes:
     - video with **SVT-AV1** (`libsvtav1`), near-lossless at the source resolution and frame rate (no upscaling in Phase 1);
     - audio as **FLAC**.

     Fixed settings make the output reproducible: `-c:v libsvtav1 -preset 4 -crf 12 -pix_fmt yuv420p -g <fps*2> -c:a flac`.
  3. The result is muxed into Matroska: `movies\<name>.mkv`. WebM cannot carry FLAC, so the container is MKV.
- **Encoder dependency:** ffmpeg is an **extract-time tool only**. It is never linked or shipped. `VS.Extract` finds it on `PATH` or through `--ffmpeg <path>`, and fails clearly if it is missing or lacks `libsvtav1`. Verified on the dev machine: ffmpeg 8.1.1 (winget Gyan.FFmpeg) has `libsvtav1` and `flac`.
- **Runtime (`Engine\Movie`):**
  - Matroska demux: libwebm `mkvparser`, BSD.
  - AV1 decode: **dav1d**, BSD.
  - FLAC decode: dr_flac or libFLAC.
  - The YUV planes are uploaded to an SDL3 GPU texture and converted in a shader. Presentation is clocked to the audio, and skipping is the game's original behavior.
- **Validation:** frame N of the decoded MKV against a jpsxdec decode of the same frame, with a PSNR threshold of ≥ 45 dB (near-lossless), for the first 60 frames and every 300th frame after that.

---

## 7. Round-trip and validation (per converter)

Every converter ships with a test in `VS.Tests` (tagged `[extract]`; it runs only when a disc is configured):

| Converter | Test |
|---|---|
| Tier 1 tables | disc bytes → decoder → JSON → runtime loader → re-encode with the PS1 layout == the original bytes |
| text | string bytes → markup → encode == the original bytes, for every string |
| SHP/SEQ/WEP | `decode(raw)` == `ModelLoader(glTF)` field by field (bones, vertices, UVs, colours, flags, every `extras` value, every keyframe) |
| MPD | the same, via `RoomLoader` |
| textures | PNG + palettes → 15-bit texels/CLUTs == the original (exact) |
| samples | WAV PCM == the ADPCM decode (exact); loop points equal |
| sequences | JSON → encode == the original AKAO bytes |
| XA | FLAC PCM == the ADPCM decode (exact) |
| movies | PSNR test (§6.7); frame count and duration equal |
| ENEMY catalogs | ENEMY_DATA_SCHEME §3 (expanded `extends` == the original record) |

Schema validation runs at load in every build. Round-trip tests run in CI where a disc is available, and locally through `test-native.cmd --extract`.

---

## 8. Manifest and versioning

`Native\data\manifest.json`:

```json
{
  "schema_version": 1,
  "extractor_version": "0.1.0",
  "disc": { "id": "SLUS-01040", "bin_sha256": "…", "cue_sha256": "…" },
  "converters": { "shp-gltf": 1, "mpd-gltf": 1, "tim-png": 1, "akao-seq": 1, "xa-flac": 1, "str-av1": 1, "tables": 1, "text": 1 },
  "outputs": [
    { "source": "OBJ/6F.SHP", "source_sha256": "…", "converter": "shp-gltf", "files": ["models/characters/6F.gltf"] },
    { "source": "license_data.dat", "skipped": "not game content" },
    { "source": "MAP/MAP123.MPD", "unconverted": "converter not yet implemented" }
  ]
}
```

- **Versions:** each converter has an integer version in code (`VS.Extract`). The runtime embeds the minimum version it requires for each converter, so an old `Native\data\` is rejected with instructions (§2).
- **Wrong disc:** the disc hash is checked against the known SLUS-01040 hashes; another release or a modified disc gives a warning, not a failure.

---

## 9. Tracking (ledgers and gates)

- **`CONTENT_LEDGER.csv`** (generated from the ISO9660 listing, plan §4.5) has one row per disc file. Its columns: `path, size, kind, decoder, converter, output, round_trip, status, notes`.
- **`DATA_LEDGER.csv`:** each `catalog` row also names its output catalog file and decoder.
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
| FLAC encode/decode | libFLAC (or dr_flac for decode) | BSD-3 / public domain | VS.Extract, Engine\Audio |
| AV1 encode, STR decode | ffmpeg (external executable, extract time only) | LGPL/GPL build. Not linked, not shipped | user machine |

All of these come from vcpkg (`vcpkg.json`). The VS 2026 vcpkg registry has `dav1d` 1.5.4, `libwebm` 1.0.0.32 and `libflac` 1.5.0. Libraries are added to `gen_solution.py`'s `VCPKG_LIBS` when the first converter or loader that needs them lands.
