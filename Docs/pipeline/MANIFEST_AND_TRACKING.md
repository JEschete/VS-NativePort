# Asset Pipeline: manifest, tracking and dependencies

[Asset pipeline index](../ASSET_PIPELINE.md)

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
