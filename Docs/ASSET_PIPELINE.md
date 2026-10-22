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
- **Music state:** G00248-G00250 are views of G00246's `Field_ZoneContext` fields, not separate storage.
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
| Effect-package streams | Block 13 -> room slot 3 -> effect timeline -> `StreamPlayback.cpp` | All 113 payloads import; real E044 Solid Shock binds/starts/stops; pause, EOF, reset, unbind and re-entry have regressions. |
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
