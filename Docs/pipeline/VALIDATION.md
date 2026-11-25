# Asset Pipeline: round-trip validation and acceptance gates

[Asset pipeline index](../ASSET_PIPELINE.md)

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
| SHP/SEQ/WEP | `decode(raw)` == `ModelLoader(glTF)` field by field (bones, vertices, UVs, colours, flags, every `extras` value, every keyframe); each extracted file decodes and re-encodes to its exact bytes, and each converted model loads back to its exact bytes (§6.2, §6.2.4) |
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
