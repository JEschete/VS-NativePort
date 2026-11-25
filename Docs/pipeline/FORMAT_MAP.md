# Asset Pipeline: disc format map

[Asset pipeline index](../ASSET_PIPELINE.md)

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
| `MAP\*.ZND` (256) | `catalogs\world\zones.csv`, `zone-rooms.csv`, `zone-textures.csv`; `catalogs\enemies\zone-enemies.csv`, `zone-enemy-limbs.csv`, `zone-enemy-parts.csv`; `textures\zone\<ZONE>\*.png` | converter `znd-tables` (§6.9, #517). The relational ENEMY_DATA_SCHEME catalogs remain #199 | ENEMY_DATA_SCHEME.md §3 |
| `EVENT\*.EVT` (512) | `events\<id>.json` | command list `{op, args}` with op names from the decomp's event interpreter; text → string IDs | decomp `Event\` module |
| `BG\*.TIM` (4), `BG\*.FAR` (4), root `*.TIM` | `textures\bg\<artwork-slug>.png` + `.image.json`; `backgrounds\<artwork-slug>.background.json` | TIM → PNG (§6.4). The BG TIM headers are not authoritative: they declare mode `0xA` (16bpp + CLUT) with a 256-colour CLUT and three share id `0x110`; the layout's draw depth selects 8-bit indices. FAR has a 12-byte header and 24-byte cells; the reader consumes typed JSON cells directly and `image_id` selects the native image (§6.4.1). MPD section 15 remains the layout compatibility lookup and original-image provenance | native `Content\BackgroundLayout.hpp`, `Field\Background.cpp`, `BackgroundLive.cpp` |
| `GIM\*.GIM` (49) | `textures\gim\*.png` + composition JSON | VS screen-effect images: per-layer cell maps over shared strips, 4 × 16-colour CLUTs and a shared or owned 256-colour palette | native `Render\GimImages.cpp` |
| `MENU\*.BIN` (11), `ENDING\ILLUST*.BIN` (11) | `ui\*.png` + `.image.json` + layout JSON | menu art, game-over, ending illustrations. `MENUBG` is word-RLE and `ILLUST*` are LZ/RLE-compressed TIMs, decoded at conversion; `ITEMNAME`, `ITEMHELP`, `MENU12` and `MCMAN` are text and tables, not images | native `Menu\`, `StaffRollCore.cpp` |
| `MENU\*.SYD` (3) | `catalogs\menu\combination-<kind>-{results,pairs,materials}.csv` + `.schema.json` | workshop **combination recipes** (result records, ingredient pairs and material rules), not item definitions; there is no global item-stat file. Converter `syd-recipe-csv` (§6.8) | native `Content\WorkshopRecipes.hpp`, `Menu\Setup\Combination.hpp` |
| `SMALL\MON.BIN` | `catalogs\enemies\bestiary.csv` | 150 records of `0x2C` bytes (§4.3) | ENEMY_DATA_SCHEME |
| `SMALL\HELP*.HF0/HF1` (28) | `text\strings.en.json` + `ui\help\<page>.json` + `ui\help\*.png` | `.HF0` = page text, sprite placements and line table; `.HF1` = 8×16 8bpp illustration blocks and 16-colour palettes | native `ManualTopics.hpp`, `ManualIllustration.hpp`, `Manual.cpp` |
| `EFFECT\*.P` (290), `*.FBT` (755), `*.FBC` (277), `EFFECT\*.BIN` (311) | `effects\<id>.json` + `textures\effect\*.png` | `.FBT` texture pages + `.FBC` palettes → PNG (§6.4). `.P` files are block streams (u32 header: size in the low 16 bits, type in the high 16 bits; type 0 ends) → effect JSON. `.BIN` files are **PLG code overlays** and EFFPURGE dispatch tables, already ported to C++ (`Effects\Plugins\PlgNNN.cpp`, `PurgeTables.cpp`); only their data blobs remain, as typed tables. Effect behaviors stay in code (plan §9.7) | decomp `Effects\` |
| `OBJ\*.ESQ` (97) | `models\animation-sets\<ESQ>.json` | shared animation sets: up to 15 entries, each a selector (a model number or kind), an optional texture row and a SEQ bank; `BATTLE.PRG`'s tables bind them to models. Converter `esq-json` (§6.2.1) | decomp `BATTLE.PRG/30DB0.c`; native `Actors/ActorAssets.cpp` |
| `OBJ\*.ETM` (80) | `textures\actors\<ETM>\<MODEL>.png` + `.image.json` | texture sheets of event actors' lines (facial frames, for example); each set entry with a texture row names the lines its model uses. Converter `etm-png` (§6.2.1); glTF references and texture tracks follow §6.2.2 | decomp `BATTLE.PRG/30DB0.c`, `40564.c`; native `Actors/TextureAnimation.cpp` |
| `SOUND\WAVE*.DAT` (101) | `audio\banks\<bank>\` (`bank.json`, `<n>.wav` + `<n>.sfz`) | 83 banks; 18 one-byte zero placeholders. Converter `wave-bank` (§6.5) | decomp `Audio\` |
| `MUSIC\MUSIC*.DAT` (192) | `audio\music\*.seq.json` | §6.5 | decomp `Audio\Seq` |
| `SE\EFFECT*.DAT` (102) | `audio\sfx\*.seq.json` (+ bank if embedded) | §6.5 | |
| `ENDING\ENDING.XA` (1, 77 MB) | `audio\streams\xa\ENDING\ENDING.XA.flac` | converter `xa-flac` (§6.6) | jpsxdec |
| `MOV\TITLE.STR` (1, 26 MB) | `disc\MOV\TITLE.STR.mkv` | converter `str-mkv` (§6.7) | ffmpeg `psxstr`, jpsxdec |
| `BATTLE\SYSTEM.DAT` | `textures\system\*.png` + `.image.json`, `text\fonts\` metrics | 8 sections: images (0, 1, 3, 4, 5), a palette (2), a fixed image (6) and interface images (7). It supplies the font and UI glyphs | native `SystemResources.hpp`, `Canvas.cpp` |
| `SLUS_010.40`, `*.PRG`, `GIM\SCREFF2.PRG` | Tier 1 tables only (code is already in `Native\`) | §4 | |
| `license_data.dat`, `SYSTEM.CNF`, `ENDING\NULL.DAT`, 192 zero-byte and 65 one-byte zero placeholders | nothing (`drop`) | recorded in the manifest as `skipped: reason`. `DBGFNT.TIM` (a debug font no code reads) is converted and marked `unreferenced` (§1.1) | |

Every row is identified (D25 inventory, 2026-11-21); open questions are listed per IMP issue. Settling a row's converter means reading the loader that consumes the file (the native port's loader, with the decomp as a cross-reference for original behaviour), then writing the converter. **No converter is written from community docs alone.** vstools, VagrantStoryImporter, GodHands and DataCrystal speed up understanding, but they are reference only (licenses: [TOOLS_INVENTORY.md](../TOOLS_INVENTORY.md)).
