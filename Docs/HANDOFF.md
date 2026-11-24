# Current Handoff

## Verified offline text controls (#500), 2026-11-24

Base `ef82a0f066446257307913a8c8698155361aa489`, with the text codec/header,
`GameTextCodecTests.cpp`, `ZoneConversionTests.cpp` and text documentation dirty
during verification. Concurrent FAR identity files were untracked/unregistered,
not part of these builds. Scoped publication and exact-revision CI are pending.

- F8..FF now decode into named controls and operands: glyph reveal cost, text
  sound selector, spacing, glyph bank, palette slot/bank, centering, vertical
  offset, conditional speaker pause, numeric formatting and string arguments.
  The original parameter semantics come from upstream
  `src/BATTLE/BATTLE.PRG/5BF94.c`, not inferred timing or color names.
- FB move-down's ignored low bits stay in separate import provenance. Offline
  reconstruction preserves them; markup-only encoding canonicalizes them to
  zero. Invalid/stale/duplicate provenance is refused explicitly.
- Debug and Release test-project builds passed with `VS_JOBS=4`, `/m:2`:
  `MSBuild.exe Native\Tests\VS.Tests.vcxproj /m:2 /p:Configuration=Debug
  /p:Platform=x64` (repeat with Release). Each
  `build\<configuration>\VS.Tests.exe "[game-text-codec]"` passed **17 cases /
  11,591 assertions**. This includes all **766 ITEMHELP/MENU12 strings**,
  **1,419 nonempty item/enemy names**, post-terminator bytes, all **1,802 valid
  parameter combinations**, malformed inputs and the production formatter's
  numeric/string substitution behavior.
- Four-file source quality passed. Focused codec/new-test `quality.py --files
  Native\Content\src\GameTextCodec.cpp Native\Tests\src\GameTextCodecTests.cpp
  --tidy` passed with **0 findings / 0 new**; eight exact fixture literals are
  informational. Hazard synchronization reports **274 sites / 0 violations**
  with no ledger changes. No test identity, assertion threshold or replay
  baseline was weakened or refreshed.
- Evidence: `build\issue-500-controls-{Debug,Release}-{build,tests}.log`,
  `build\issue-500-controls-quality.json`, and
  `build\issue-500-controls-tidy.{log,json}`.

This is an **offline import codec**, not a new runtime string-table repacker or
completed #500/#54 adoption. MCMAN's extended Japanese glyph records remain
unsupported: 47 location-label entries (77..104 except 89, and 117..136), with
208 F6 prefixes; their usage/placeholder status is not established. The finding
is recorded on #500. Full catalogs, semantic call-site argument bindings,
font/charmap assets, remaining encodings and typed UI/layout consumers remain.

## Verified cache/CI checkpoint

`7a69555d2530270b630cccf78238a7f310b07dcf` passed exact-revision CI
([45342671515](https://github.com/JEschete/VS-NativePort/actions/runs/45342671515)).
Startup caching #524 and physical-source integrity #525 are closed with measured
evidence. The earlier boundary, source-test identity, verification-reference and
hazard-bookkeeping failures are resolved without lowering replay/assertion gates.
FAR #210 remains open for semantic identities, reference parity and the remaining
format gates. The open-sky route was MAP062, not a FAR scene. A subsequent
non-skipped MAP001 run established a visible native PNG edit at matched event
PC `+0x626`; #210 retains its exact commands and pixel comparisons.
Other asset migration issues remain open.

## Authored FAR image binding and placement (#210), 2026-11-24

Verified worktree based on `0f7e0f00e991a3ef8be4bfd12cb7272754c3c1d2`, with
FAR image-binding and parent source-cache changes dirty. The authored `image_id`
selects the actual native image, including aliases/explicit unreferenced art;
the loaded pixels/palette stay owned through upload. Original image bindings,
TIM upload coordinates and page counts no longer veto a valid authored target.
The renderer retains a free preferred page range or allocates another contiguous
lendable range; it never overwrites occupied/resident pages. No FAR/TIM bytes
are rebuilt at runtime.

- Full Debug/Release x64 builds passed (`VS_JOBS=4`, `/m:2`).
- Each configuration passed **136 cases / 18,215 assertions** with
  `[far-layout],[field-background],[content-images],[native-image],[source-verification],[asset-catalog]`.
  `[native-image]` was unmatched; `NativeImageTests.cpp` is covered by
  `[content-images]`. Parent cache/catalog selections passed in these runs.
- After the placement fix, each configuration passed **64 cases / 2,208
  assertions** with `[far-layout],[field-background],[render-textures]`.
  The refactored existing TIM fixture also passed alone in each configuration:
  `extracted TIM corpus converts without changing its source bytes`,
  **1 case / 1,683 assertions**. Unrelated suites were not repeated.
- Two-image tests verify original-binding independence, preserved authored
  references on repeat conversion, missing/invalid/non-image IDs, image material,
  alias-owner dependencies, held reload snapshots and selected-image upload.
  Allocation tests cover preferred/relocated ranges, occupied pages, exhaustion,
  invalid owners/counts and ownership-preserving retirement.
- Renderer probe: Release `--frames 3300 --scripted --press
  1000:Start,1300:Circle --at 2704,2710,2750,2800,3000,3200
  --no-gpu-validation --speed 8 --save-dir build\issue-210-rebind-save`,
  changing only 001's image ID from `image.bg.001op01a` to `image.bg.002op01a`.
  Original hint 27+2 was occupied (logged on #210); the fix used pages 29–30.
  With `--capture-pages`, tick 3200 contains the selected image's exact pixel
  words and palette. All six scene frames equal the original frames: the
  background is occluded here. Both runs exited normally; the original authored
  JSON was restored exactly. Do not claim a scene-visible artwork change.

Evidence: `build\issue-210-rebinding-{Debug,Release}-{build,tests}.log`,
`build\issue-210-rebinding-{Debug,Release}-placement-{build,tests}.log`,
`build\issue-210-rebinding-placement-tidy.log`,
`build\issue-210-rebind-before`, `build\issue-210-rebind-relocated`, and
`build\issue-210-rebinding-relocated-comparison.json`. The earlier failed
placement probe is retained separately; its camera divergence was not success.
Rebinding code/new tests pass scoped tidy; expanded scanning of
`ImageConversionTests.cpp` still reports the unchanged `RequiredConversion`
fixture's size warning. No quality baseline was changed.

This follow-up still requires scoped publication and exact-revision CI.
The separate verification-reference repair (`VR-FIELD-CAMERA-BACKGROUND`,
SUMMARY count 480→481) must publish independently; no new verification row was
mixed into those pending ledger edits. Remaining #210 gates: verified semantic
artwork/room IDs, broader native-only renderer capabilities, matched-reference
visual/parallax evidence, a scene-visible authored edit, production room
transitions/re-entry and remaining compatibility retirement. Cache benchmark/
CLI validation belongs to the parent; no cache performance claim is made here.
Publication scope: the existing Content BackgroundLayout/ConvertedImages files,
Game Background core/live APIs, Engine TexturePages core/live APIs, FAR/field/
image/texture-page/event tests, FarConversion, the background schema and these
FAR documentation sections. No new files or project registration are required.
Keep the parent source-cache files/project changes and its documentation hunks
separate, as well as the independently publishable verification-reference repair.

## Typed FAR layout slice (#210), 2026-11-23

Base slice published as `75bae48f3cfbd0e6955f6982d1b562e6e0b28012`;
**not full #210 completion**. Historical tests and
captures identify `2764a890b1b1f9f8a42d0f0164c1016f21e0442a` with the FAR and
parent boundary-repair edits dirty. The parent repair/policy documentation is
now published separately as `5dc172f1f7a9b9b38026ca35f8c6cd7a880c02ea`; this FAR
base milestone's CI follow-ups are tracked on #210. Successful exact-revision
CI remains a publication gate; the historical measurements below are unchanged.

- `far-layout-json` v1 / background-layout schema v1 writes
  `Native\data\backgrounds\001OP01A.background.json`,
  `002OP01A.background.json`, `007OP01A.background.json`,
  `008OP01A.background.json`, plus manifest registrations. Images and original
  FAR/TIM sources were unchanged. All generated content/evidence remains ignored.
- `background.import.bg.<stem>` references `image.bg.<stem>` (lowercase stems).
  Verified bindings: 001 → MAP001/MAP215 (8×4); 002 → MAP002/MAP216 (16×4);
  007 → MAP007 (8×12); 008 → MAP008 (8×12). All four are referenced; the old
  007-unreferenced claim was disproved against the configured disc.
- Full Debug/Release x64 builds passed with `VS_JOBS=4`, MSBuild `/m:2`.
  `[far-layout],[field-background]` passed **32 cases / 2,012 assertions in
  each configuration**, including all 288 cells through the production typed
  reader, offline exact reconstruction, authored edits, refusal and lifecycle
  cases. All four outputs validate against the committed JSON Schema.
- `VS.Extract backgrounds Native\data` converted four; the repeated command
  retained four with byte-identical JSON/manifest. Project generation was also
  stable on its second run. Scoped codec/converter/test/runtime tidy passed.
- Debug prologue: `VS.Desktop.exe --frames 6000 --scripted --press
  1000:Start,1300:Circle --capture build\issue-210-native-debug --at
  2200,3200,4200,5900 --no-gpu-validation --speed 8 --save-dir
  build\issue-210-native-save`. Native JSON 001 loaded at tick 2705; normal
  cancellation/exit at 6000, zero error log entries. Release produced the same
  requested capture ticks under `build\issue-210-native`; detailed logs are
  disabled in Release, so the Debug run supplies the load/exit evidence.

Evidence: `build\issue-210-*-build.log`, `build\issue-210-*-far-tests.log`,
`build\issue-210-convert-{first,repeat}.log`,
`build\issue-210-{tidy,runtime-tidy}.log`,
`build\issue-210-native-debug.log`, `build\issue-210-bindings.json`.
The canonical verification record is `VR-FAR-TYPED-LAYOUTS`.

**Remaining at this base checkpoint:** semantic artwork/room identities and
readability; authored image-ID resolution through upload (addressed by the
follow-up above); native-only extensibility beyond the bounded renderer envelope;
matched-reference placement/gradients/seams/parallax; an edited-layout room
demonstration and production transitions/re-entry. The runtime uses typed owned
cells without FAR reconstruction, but dormant module pointer declarations and
image/page rendering compatibility remain. Do not retire the broader EC-04
bridge or close #210 from this checkpoint.

**Publication scope:** the FAR Content codec/header/schema, Extract converter
and CLI, Field Background core/live APIs, session reset, FAR/background/event
tests, converter registration, scaffold/content-ledger registrations, the
generated Content/Extract/Tests projects, both related ledgers and the
BUILDING/ASSET_PIPELINE/HANDOFF updates. Exclude the already-published parent
text repair. Content-ledger sync also adds the previously omitted
`catalogs/menu/import/combination-blade.json` listing for `MENU/BLADE.SYD`;
the existing manifest already listed that file, dated 2026-11-23 20:10:57.
No SYD implementation or data was changed.

## Owner correction and measured startup baseline, 2026-11-23

The owner explicitly distinguishes **offline imported-byte preservation** from
**production typed-definition adoption**. Runtime repacking is a temporary
T-shim, even when the input is native CSV/JSON/glTF or a compiled SQLite table.
The existing plan's EC-04 register now lists eight active asset/layout bridges
with their retirement issues. All 21 open AM format issues and #520 have the
stronger typed-adoption gate; prior reconstruction results are not completion.
FAR work was instructed to consume typed cells directly, not rebuild FAR bytes.

Measured **before implementing either cache**, using published `2764a890b`
executables/libraries, Windows x64, native data with `znd-tables` v2:

| Measurement | Observed Release median |
|---|---:|
| Complete one-tick scripted launch, three processes | 2,778.588 ms |
| `CatalogDatabase::Load` | 74.565 ms |
| Typed recipe construction from SQLite | 19.097 ms |
| Entire `DataStore::Open` (includes catalog work) | 178.705 ms |
| `RequireConversions` | 732.754 ms |
| Source-file reads, isolated same source/dependency set | 122.967 ms |
| SHA-256, isolated same set | 516.496 ms |

The deduplicated set is **3,323 files / 103,428,746 bytes**; every isolated
digest matched the manifest. One actual Debug trace reached its first logical
frame at 18,855.842 ms, with approximately 14,679.606 ms between store-open
completion and conversion-verification completion. These are observed/warm
filesystem-cache runs, not a cold/clean-machine certification.

Source verification is the first optimization target; SQLite caching is smaller
today. #524 records atomic publication, invalidation, content-hashed CSV/schema
inputs, ordered layer fingerprints, `--verify-all`, timestamp limitations and
read-only asset-tree behavior. **No cache or performance gain is implemented
or claimed yet.** Evidence: `build\startup-baseline-release.json`,
`build\startup-components-release.log`,
`build\Debug\game-logs\1795491820893310-0`.

Current integration: `2764a890b` CI failed because Content's text codec included
Engine.Render. The repair moves shared encoding constants into Core.String and
keeps renderer aliases; the boundary check passes without ledger exceptions.
Full Debug/Release builds and the `[game-text-codec],[render-font],[core][text]`
selections passed (38 cases / 3,424 assertions). Parent-scoped source quality
also passes. The typed FAR slice is separately verified locally and awaits
its own scoped publication; do not certify either publication until its
fixing revision's CI passes.

## Persistent compiled catalog cache (#524)

Worktree based on `e2dab4d12`. The validated workshop catalog now persists as
`.cache/catalog.sqlite`. Fingerprints cover actual CSV/schema content, the
serialized compiler schema contract and version, layer configuration, ordered
roots and all selected layer files. Compilation uses an immutable snapshot of
the fingerprinted text; fingerprints are checked again before publication.
Timestamp-preserving authoring edits cannot bypass this check.

Cache hits open SQLite read-only, check its integrity/contract and retain an
immutable native snapshot. The disk handle is released so a newer generation
can replace the file atomically on Windows while older stores remain alive.
Conflict provenance survives cache reloads. Only fully validated typed records
publish; damaged caches recompile, and unavailable cache storage does not
invalidate otherwise usable assets.

- Full Debug/Release builds and **68 cases / 7,980 assertions per configuration**
  passed for `[catalog-cache],[catalog-layers],[source-verification],[asset-catalog],[content-tables][workshop]`.
- Tests cover real SQLite output, unchanged hits, same-size/timestamp CSV edits,
  external/compiled schema changes, compiler-version changes, layer additions,
  deletion and reordering, conflict restoration, malformed caches, rejected typed
  records, input changes before publication and unavailable cache storage.
- The real database is **3,141,632 bytes**. Release median catalog load:
  **90.676 ms forced compilation -> 17.762 ms cache hit**; unchanged sources
  still report 3,323 stamp hits and zero bytes hashed.
- Three completed Release launches with both caches: **1,967.302 ms median**,
  versus the original observed 2,778.588 ms baseline. These are warm observations,
  not a cold/clean-machine claim. Evidence: `build\issue-524-persistent-*`.
- Scoped cache/test tidy and changed-file quality pass without new findings.
  Both production Debug replays match unchanged baselines (2,400 and 900 frames);
  evidence is `build\issue-524-persistent-replays.log`.
  The final hazard reconciliation records the FAR divisor as constexpr 64 and
  classifies filesystem path `/` operators as non-arithmetic; no safety rule was
  disabled. Keep cache code and its generated hazard report together.

The two #524 cache slices and #525 integrity fix are now published with successful
exact-revision CI and closed issues (see the checkpoint above). Cache optimization
does not retire any remaining asset T-shim.

## Source verification cache and physical-size integrity (#524/#525)

Worktree based on `1124d0e8b`, alongside the separately owned FAR image-binding
slice. Required original sources now reuse validated size/time/SHA stamps from
`.cache/source-verification.json`; Desktop and Combat Lab expose `--verify-all`,
and launcher preparation always verifies fully. Native CSV/schema authoring
never uses these timestamp stamps. A malformed or unwritable cache does not
conceal invalid data or invalidate an otherwise usable read-only asset tree.
Cache failures use explicit warnings and a full check.

The #525 reproduction proved the old checker accepted a nine-byte physical
source whose manifest declared four bytes: it hashed only the prefix. The new
checker rejects physical length mismatches before cache reuse/hash, then checks
the stamp again after a full read/hash. Cache publication happens only after
all source/dependency/output checks succeed. The shared atomic text writer is
also used by asset-catalog import, preserving old targets on failed replacement.

Verification:
- Debug/Release solution builds passed, `VS_JOBS=4`, `/m:2`.
- Combined FAR/image/source-cache/catalog selections passed **136 cases /
  18,215 assertions per configuration**; the first parent-only Debug selection
  passed **35 cases / 7,265 assertions**.
- Tests cover zero-hash hits, full checks, same-size/timestamp-preserving edits,
  changed stamps/expected hashes, corrupt cache, missing sources/outputs,
  truncated/overlong sources, write failure and atomic replacement cleanup.
- The exact synthetic append reproduction now rejects `9` versus `4` bytes:
  `build\issue-525-fixed-repro.log`.
- Release direct verification median: **765.402 ms full -> 106.021 ms cached**,
  with **3,323 hits, zero files/bytes hashed** on unchanged inputs.
- Three actual one-tick Release launches per mode: median **2,778.943 ms full
  -> 2,001.929 ms cached**. One Debug pair: first frame **17,177.892 ms full
  -> 4,910.715 ms cached**; verification trace gap **13,073.644 -> 815.111 ms**.
  These are observed/warm runs, not cold-machine certification.
- Desktop full/cached flags and Combat Lab `--verify-all --catalog-only`
  exited successfully. Evidence: `build\issue-524-*.log/json`.

Persistent compiled SQLite is still #524; this source-cache slice cannot close
that whole issue. Caching does not retire EC-04 adapters or the temporary
original-source-tree requirement.

## Reversible English name codec (#500/#520), 2026-11-23

Verified worktree based on `6716fac3d` on `main`. The new Content game-text codec
shares the existing English glyph mapping with asset labels. It round-trips
Unicode, named symbol/button glyphs, escaped braces, page/confirm/newline and
signed nominal spacing. Unsupported controls/glyphs, malformed markup/UTF-8
and missing terminators fail with string identity and byte position.

E6 means controller confirmation, not a timed wait. FA parameters F0..FF mean
-16..-1; other parameters remain positive. The decoder's consumed count includes
E7, allowing the importer to preserve post-terminator bytes without treating
them as text. All-zero fixed slots remain classified empty import records.
Font/palette/speaker/formatting controls and the complete string catalog,
charmap/font asset and ID-based runtime adoption remain #500; no completion
claim is made for those.

- Full Debug/Release builds passed, `VS_JOBS=4`, `/m:2`, x64.
- Both configurations: **33 cases / 3,750 assertions**, selection
  `[game-text-codec],[asset-labels],[workshop-authoring]`.
- Actual 1,422 name slots: **1,419 nonempty names** round-trip with every
  suffix byte preserved; three all-zero slots are explicitly classified.
  Their only embedded controls are 809 FA:6 spacing operations.
- Tests cover every mapped English glyph, all 256 spacing parameters, Unicode,
  braces, multi-character level glyph versus literal `Lv.`, malformed input and
  a terminator-valued parameter. Existing label/authoring tests are unchanged.
- New codec/test tidy and complete changed-file quality: zero new findings.
  Project generation is stable on its second run. No native data changed.
- Evidence: `build\issue-500-codec-*.log`.

Continue with authoritative string definitions and their real consumers, then
the remaining semantic identities/domains and per-format adoption. A codec
foundation alone does not close #500 or #520.

## Named zone fields and domains (#199/#520), 2026-11-23

Verified worktree based on `2276a0a6a` on `main`. `znd-tables` version 2
names physical types, classes, affinities and DP-splash destination limbs.
Enemy class and weapon/shield/limb-armor materials use shared schema domains.
The misleading enemy `material` header is now `movement_class`, with its
unresolved packed/body-load semantics explicitly documented.

Readers accept schema-declared `legacy_name` aliases and reject ambiguous
headers. The shared normalizer retains every other cell, label and row order;
workshop authoring reuses it. `authoring` upgrades existing zone tables and
publishes the converter version last after comparing all decoded records.
Non-force zone conversion migrates version-1 tables before deciding whether
source reconversion is needed, so native edits are not lost to a version bump.
Unknown enum codes remain raw-codec-preservable but fail native authoring.
Synthetic authoring fixtures now use valid domain codes; a separate case checks
unsupported codes still round-trip through the raw codec and are refused by
the native table reader.

Verification:
- Full Debug/Release x64 builds passed (`VS_JOBS=4`, `/m:2`).
- Both configurations: **91 cases / 10,326 assertions**, selection
  `[catalog-layers],[content-tables],[workshop-authoring],[asset-labels],[asset-catalog]`.
  This includes all 256 extracted zones loading as their exact source bytes.
- Actual data: **39 renamed columns, 4,095 named values, 205,770 checked cells**,
  with no unexplained differences across 6,205 rows. IDs, labels, topology,
  encoded names, stats and storage tails are unchanged.
- Repeat authoring: zero columns/values/files changed; labels also change zero
  files. The actual native tree is now version 2 and both builds can consume it.
- Seed-199 known-field cold reads cover five enemy, limb and part rows each.
  This is not the full family readability gate: positional IDs, packed flags,
  encoded text and remaining opaque fields still block it.
- Both Debug production replays match unchanged baselines: 2,400 and 900 frames;
  run directories end in `1795489399740241-0` and `1795489466978296-0`.
- New shared/migration helper tidy and complete changed-file quality: zero new
  findings. Expanding optional tidy to the old zone importer also reports its
  existing layout literals, large reader/test and old fixture optional access.
  The unchanged reader/byte helpers/fixture helpers were compared to `2276a0a6a`;
  no quality baseline or test expectation was weakened to hide those findings.
- Generated projects are stable on a second run. Evidence:
  `build\issue-199-fields-*.log`, `build\issue-199-fields-broad-tidy.json`;
  the 13 original table/schema/manifest files remain in
  `build\issue-199-fields-before`.

All broad asset migration issues remain open until their full adoption,
authoring, lifecycle, reference and CI requirements pass. Next dependencies
include authoritative strings/semantic variant identities, remaining part
domains/flags, relational enemy definitions and adoption of layers outside
recipes. Do not infer equipment materials from the movement/body selector.

## Layered SQLite workshop catalogs (#499/#520), 2026-11-23

Verified worktree based on `f4312a2d2`, on `main`. The shared catalog compiler
implements plan section 9.6 partial CSV merge, add, replace and remove by the
schema's semantic/composite key. Optional `catalog-layers.json` lists patch/mod
roots in increasing precedence. Base data is never written. See BUILDING.md
section 5.1 for supported syntax and the nine-table scope.

Production `DataStore::Open` compiles workshop tables into an owned in-memory
SQLite database, then queries it to construct the recipe catalog used by the
live workshop loader. Named/numeric domains and legacy result aliases normalize
before matching. Duplicate resolved keys, bad operations/values, missing
references, unsupported layer tables and missing/repeated roots fail explicitly.
Conflicts report both source files; failed opens preserve the previous store.
The importer retains its base-only preservation path. SQLite is a pinned vcpkg
dependency, included in the generated executable links and tester packaging.

Verification:
- Full Debug and Release x64 builds passed (`VS_JOBS=4`, `/m:2`).
- Both configurations passed **85 cases / 10,168 assertions**, selection
  `[catalog-layers],[content-tables],[workshop-authoring],[asset-labels],[asset-catalog]`.
- Tests cover partial edits, directed recipe additions through the production
  live recipe loader, remove/replace, alias and enum keys, source diagnostics,
  conflict precedence, Unicode/space paths, repeat compilation, base-file
  preservation and failed-store publication.
- The actual authored workshop corpus reloads identically through SQLite,
  including every result, material rule and directed outcome.
- Both Debug production replays match unchanged baselines: `silver_wolves`
  2,400 frames, `first_lever_room` 900 frames. Run directories end in
  `1795487514732269-0` and `1795487588404587-0` under
  `build\Debug\combat-lab\runs`.
- New compiler/test scoped tidy and complete changed-file quality: zero new
  findings. Project generation is stable on a second run; generator syntax,
  release script syntax and the configuration schema were checked.
- Evidence: `build\issue-499-layers-*.log`. No disc-derived files changed.

This is an adopted foundation, not closure of #499/#520. Remaining work includes
the other table families, shared item definitions, full text/flags/field
semantics and actual controller-driven edited-workshop/action acceptance.

Next enemy-table trace: `CharacterInit.material` is **not** the equipment
material enum. `ActorSpawning.cpp:150` forwards it as `gait.movement_class`,
and the body asset request also consumes it; real values include 8, 16 and 64.
Do not label those as equipment materials. Equipment classes start at index 0
(six active entries), affinities at index 0 (seven active entries), and physical
types use 1..3. The verified distinction is recorded on #199 before migration.

## Named recipe domains (#499/#520), 2026-11-23

Base revision: `22b0be713` on `main`. The shared CSV schema/reader now supports
named enum domains with explicit numeric import aliases. Material rules, blade
categories, damage types and cost resources are written by name by the authoring
pass. Readers preserve the original numeric runtime values and normalize keys,
so mixed numeric/named aliases cannot create duplicate material rules.

The cost mapping is now traced end to end:
`EquipmentStats::BuildWeaponStatsFromParts` -> `WeaponLoadout.action_type` ->
`ActionCostsLive::Resources` -> `ActionCosts`: 1 MP, 2 Risk, 3 HP, 4 weapon PP,
or 0/0 for no payment. It is not `ActionDefinition::type`. Unsupported codes
remain losslessly decodable/re-encodable by the import codec but are refused
as authoring values; `none` requires zero cost.

Verification on this slice:
- Full Debug and Release solution builds passed (`VS_JOBS=4`, `/m:2`, x64).
- Debug authoring/label/recipe selection: **35 cases / 680 assertions**.
- Final Release authoring/label/recipe/catalog selection:
  **61 cases / 7,847 assertions**.
- Actual data: **12,897 numeric aliases** became their schema names across
  **15,499 recipe rows**. Every underlying code, quantity, identity and reference
  is unchanged. Repeating authoring changes zero files/values.
- Both Debug production replays match the unchanged baselines (2,400 and 900
  frames). Scoped tidy and changed-file quality report zero new findings.
- A seed-520 five-record cold read records semantic IDs, names, categories,
  damage, resource costs and stats in `build\issue-520-domains-cold-read.log`.
  It is only the known-domain check: remaining import-only/unnamed fields still
  block the full family readability gate.
- Evidence: `build\issue-520-domains-*.log`; original rows are retained locally
  in `build\issue-520-domains-before`.

Still open: the other catalog families, flags and numbered field families,
readable instruction/track structures, the authoritative string pipeline,
one-copy shared item definitions and remaining per-format adoption/verification.
No complete migration issue closes on this bounded step.

## Semantic recipe identity slice (#499/#520), 2026-11-23

Base revision: `a144ae986`, on `main`. This is the first section 4 identity
migration, not completion of the full authoring contract.

- `VS.Extract authoring <datadir>` now migrates generated recipe IDs and all
  directed references; `tables` and full `extract` also run it. Existing custom
  identities, labels and value edits survive. Schemas document legacy aliases,
  which readers normalize through `legacy_id`; ambiguous aliases and duplicate
  resolved directed pairs are errors.
- Real-data inspection found two Footman's Maces: categories 4 (Axe & Mace) and
  7 (Heavy Mace). The owner approved category-qualified IDs. They are now
  `blade.footmans-mace.axe-mace` and `blade.footmans-mace.heavy-mace`.
- The owner also approved quarantining the five stock, unreferenced `untitled`
  tail rows. They are no longer item definitions; their exact record bytes are
  in `catalogs/menu/import/combination-blade.json`, tracked in the conversion
  manifest. Only offline preservation export reads those bytes. Edited,
  referenced or explicitly named records are not silently discarded.
- The original shield/armor copies differ in gem slots for 15 records.
  `combineArmor` does not read model/gem-slot bytes, while `combineShields` does
  (upstream `src/MENU/MENUC.PRG/168.c:5017-5112`, native `Combination.cpp`).
  Their context-specific bytes remain exact; shared category/stats/legacy
  identity are checked, rather than forcing unequal records to be identical.
- Normal incremental table conversion retains the import file and authored
  edits. Missing tracked import provenance is refused unless the user explicitly
  requests source reconversion with `--force`.

**Evidence on the uncommitted slice based on `a144ae986`:**
- Full Release and Debug solution builds passed, `VS_JOBS=4`, `/m:2`, x64.
- Release final selection
  `[workshop-authoring],[asset-labels],[content-tables][workshop],[asset-catalog]`:
  **58 cases / 7,814 assertions passed**.
- Debug identity/recipe selection: **20 cases / 464 assertions passed**.
  An earlier Debug schema comparison used the old committed description; the
  schemas were refreshed from the native formatter and that selection passed.
- Both Debug production replays match their unchanged baselines after migration:
  `silver_wolves` 2,400 frames and `first_lever_room` 900 frames.
- Actual data: **189 semantic result IDs, 37,356 directed references**, and
  **five archived records**. Every retained result value/label and material table
  is unchanged; all directed pairs resolve to the same original result IDs.
  Archived bytes equal their exact source records. A repeated `tables` run
  converts **0** sources, keeps **3** recipe filesets and **256** zones, removes
  **0** files, and changes **0** IDs/references/labels.
- Scoped tidy and complete changed-file quality: **0 new findings**.
  Generated projects are stable on their second generation.
- Logs: `build\issue-520-authoring-*.log`, `build\issue-520-semantic-*.log`,
  `build\issue-520-import-*.log`; before-state data stays local under
  `build\issue-520-authoring-before`.

**Still open:** machine-readable named enum/flag domains, meaningful numbered
column families, authoritative readable text/markup, removal of remaining
import-only fields from authoring rows, and one-copy shared item definitions.
The field currently called `cost_type` needs its actual consumers traced; do
not equate it with `ActionDefinition::type` just because another intermediate
field is named `action_type`. Other asset families retain their section 7.2
readability and original runtime/preservation gates. #499/#520 stay open.

## Readable-label preparation and semantic authoring decision (#520), 2026-11-23

Base revision: `242ae3e2b` on `main`. The owner explicitly replaced the PR
workflow with scoped commits/pushes on `main`; that documentation-only commit
passed CI `45259252641`.

**Destination: ASSET_PIPELINE.md section 4, not labels over opaque dumps.**
The owner clarified this while the label pass was in progress. D26 now requires
semantic IDs, schema-backed named values/flags, meaningful fields/arguments and
explicit provenance for legacy identities/unused bytes. The new section 7.2
readability gate was appended to all 21 open asset-format issues. #520 was
expanded and remains open. The proposed table generator and charmap are now
accurately marked unimplemented; no new implementation is claimed for them.

**Preparatory code delivered**
- `VS.Extract labels <datadir> [--force]` and successful conversion commands
  add optional `display_name` metadata. CSV puts it second without shifting
  runtime semantic columns; JSON uses an early member and glTF uses
  `asset.extras.vs`. Other fields/IDs remain unchanged in this command.
- Bounded English names decode accents/punctuation, stop before terminator
  padding and retain unknown codes as visible markers. Item names label recipe
  results and parts; verified archetype references propagate shared aliases.
- Existing labels/author edits survive; `labels --force` refreshes only labels.
  Unknown fields and old numeric identities still present are migration debt.

**Measured local data evidence**
- All **21,709 rows in 15 CSV tables** preserve every original cell; the new
  label is column 2. Examples include Guildenstern, Muellenkamp Soldier (with
  the source accent), Battle Knife and Scramasax.
- **333 glTF and 2,751 JSON documents** are labeled. Across **13,525 non-disc
  files**, canonical non-label JSON/CSV values and every binary hash are
  unchanged. Schemas differ only by the optional annotation column.
- The repeat command reports **0 updated / 3,114 unchanged files**.
- No disc-derived data was committed. Native data was annotated only after
  rebuilding consumers and confirming no native application was running.

**Validation**
- Full Debug and Release solution builds passed (`VS_JOBS=4`, `/m:2`,
  `/p:Configuration=<Debug|Release> /p:Platform=x64`).
- Release `[asset-labels]`: **12 cases / 183 assertions**.
- The broader Release asset selection passed 132/133 cases; its only failure
  compared catalog text before/after optional annotations. The test now compares
  every non-label JSON field and verifies annotation types, not byte formatting.
  That exact case then passed **5,568 assertions**.
- Debug `[asset-labels],[asset-catalog]`: **38 cases / 7,350 assertions**.
- Both existing Debug production replays, run through `regression.run_replay`
  and `compare_replay`, match the unchanged baseline: `silver_wolves` 2,400
  frames and `first_lever_room` 900 frames. The full suite was not repeated for
  this annotation-only checkpoint.
- Scoped tidy and the mixed changed-file quality pass report **0 new findings**.
  The generated projects are stable on a second generation; all 15 CSV schemas
  were produced by the native formatter and are stable on repeated annotation.
- Evidence: `build\issue-520-*.log`, `build\issue-520-asset-before.json`.
  Exact pushed-revision CI is recorded on the issue after publication.

**Next:** implement the first actual section 4 authoring slice in workshop
catalogs: semantic result IDs/references and verified named domains. Preserve
directed combinations and original mappings, and classify unknown bytes from
their real consumers rather than guessing. This checkpoint does not close #520,
#499, #199 or any complete asset-format issue.

**Owner playtest feedback:** positive FMV/demo/rain/facial/manor-fire observations
are recorded on #149. New reports are #521 (VKP cigarette flicker), #522 (Debug
logging choppiness), #523 (window drag pauses playback). They are owner evidence;
tested revision and causes remain unverified. No speculative fixes were made.

## Converted actor-asset read failures (#518), 2026-11-23

Starting revision: `0530c5dc8` on `imp/asset-migration`, PR #519. The owner
finished the Debug playtest before Debug rebuilding and regression began.

The converted-read failure was logged in #518 before implementation.
`BeginFileRead` now distinguishes a started read, a full queue and a failed
read. Only a full queue can use the existing synchronous fallback, whose
result is also checked. Failed SHP, WEP, SEQ/ESQ and ETM reads free the
unpublished allocation from the correct heap end and return a failed request
before parsing or texture upload. A failed player sequence does not queue its
dependent weapon sequence.

Tests cover all four loaders, high/low sequence allocations, failed synchronous
fallback, recovery and live owned SHP/WEP ticket failure followed by a valid
queued request. The preview cancellation fixtures now load actual converted
models with the real event-stride table instead of relying on a missing store
being accepted as a zero-filled model. The shared model fixture reuses its
existing checked-result helper to satisfy tidy without weakening assertions.

Validation on the nine-file dirty changeset based on `0530c5dc8`:
- Debug and Release MSBuild `VagrantStory.Native.sln
  /t:VS_Tests;VS_CombatLab;VS_Desktop /m:2 /p:Configuration=<configuration>
  /p:Platform=x64 /v:minimal /nologo`, `VS_JOBS=4`: passed.
- `python tools\test\regression.py check --config Debug`: **9,896 tests /
  13,074,943 assertions; 2 replays; 0 regressions**. `silver_wolves` (2,400
  frames) and `first_lever_room` (900 frames) match the unchanged baseline.
- Final Release `VS.Tests.exe
  "[actor-assets],[asset-ownership],[actors-spawning],[encyclopedia-preview],[actors][sequence],[actors][texture],[content-models]"
  --reporter compact`: **302 cases / 19,405 assertions passed**.
- Scoped quality/tidy on the changed loader and test translation units found
  two test-size findings; both were corrected. The affected translation unit
  was rechecked with tidy: **0 new findings**. No baseline was changed.
- Evidence: `build\issue-518-debug-build.log`,
  `build\issue-518-debug-regression.log`,
  `build\issue-518-release-final-build.log`,
  `build\issue-518-release-final-tests.log`,
  `build\issue-518-release-final-console.log`, and
  `build\issue-518-quality{,-retest}.log`.

This also supplies fresh Debug regression evidence for the preceding #48
request-state milestone. #48 still has the limits below; neither change
completes asset modernization. Exact fixing-commit/branch-CI evidence belongs
on #518 and PR #519 after publication. The PR closes #518 only on owner merge.

## Per-request actor parser state (#48), 2026-11-23

Starting revision: `23279779c`, clean worktree. `imp/asset-migration` was
fast-forwarded from the merged branch to `origin/main`; work continues through
an owner-reviewed PR, not a direct push to `main`.

- Actor, weapon and sequence parser phases, buffers and cursors now live with
  each queued request's identity and record-memory resolver. Queue movement,
  cancellation and clearing carry or reset them together.
- Module parser globals are output-only diagnostic snapshots for those loaders.
  Tests overwrite the snapshots before and during a real preview parse and
  before cancellation: the parse still finishes or frees its partial geometry.
- The battle initializer relies on the queue reset rather than clearing
  individual parser globals.
- This is not #48 completion: the unowned ETM phase/buffer, serialized file
  streams, in-band shared-record index, unowned producers and deliberate
  immutable sharing remain. G00064's ledger note records a write-only snapshot,
  not compatibility retirement.

Validation on the seven-file dirty changeset based on `23279779c`:
- `VS_JOBS=2`; MSBuild `VagrantStory.Native.sln /t:VS_Tests /m:2
  /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`: passed.
- `build\Release\VS.Tests.exe
  "[actor-assets],[asset-ownership],[actors-spawning],[encyclopedia-preview],[actors][sequence],[actors][texture]"
  --reporter compact`: **240 cases / 2,197 assertions passed**.
- `quality.py --files` the changed `ActorAssetsLive.cpp/.hpp`,
  `ModelRenderingLive.cpp` and `EncyclopediaPreviewTests.cpp`: **0 new findings**.
- Evidence: `build\issue-48-release-build.log`,
  `build\issue-48-release-tests.log`, `build\issue-48-quality.log`.

At that checkpoint the owner was playtesting the Debug build. Its binaries,
logs and `Native\data` were not changed; Debug builds and gameplay replays
were deferred. Fresh integrated Debug evidence is now recorded above.
The latest starting CI
`45208337293` passed both jobs; exact-revision branch CI belongs in the issue
checkpoint after publication.

Follow-up: #518 records a source-confirmed error-propagation defect found while
tracing these loaders: `BeginFileRead` ignores a converted read's failure and
lets zero-filled content continue. Its fix and verification are recorded above.

## E207 reference mode (#507) and ZND zone tables (#517), 2026-11-23

Starting revision: `68c317d04`, clean worktree (the `resources/rood-reverse`
submodule checkout is at `f6bf8402`, not the recorded `c9a869b8`; it was left
alone, so a local `provenance.py --check` reports stale citations that CI does
not).

**#507 (closed, `58c6d497c`)**
- The MAP001 window glow is not drawn too faintly. The DuckStation reference
  was captured in its default True Color mode, which blends at 8 bits. E207's
  ~70 additive layers of a dark FBC palette (vertex colour 63, dither off)
  truncate to almost nothing in a 5-bit frame, on hardware and in the port.
- A replay of the traced primitives reproduces the native frame exactly in 5
  bits and the reference's yellow-green glow at 8 bits.
- The reference setup now uses DuckStation's software renderer with unscaled
  dithering. A new hardware-mode capture of `build\ds_map001` was not made:
  the capture script sends system-wide keys.

**#517 (slice of #199)**
- `bde93eb06` adds the lossless ZND codec. The follow-up adds the zone and
  enemy CSV tables (`Content/ZoneTables.hpp`) and the `znd-tables` converter
  (`VS.Extract tables`, now after `models`).
- The zone texture images live under `textures\zone\`. The room loader's zone
  read rebuilds each zone from the tables (`Content/ConvertedZones.hpp`).
  `EncodeTim`, `OriginOf` and `TimFromImage` moved from VS.Extract to
  VS.Content so the runtime rebuild and the importer share them.
- The launcher pipeline version is 9. ASSET_PIPELINE.md §6.9 has the format
  and limits.
- **Evidence:**
  - All 256 zones convert, rebuild and load back as their exact bytes.
  - A native New Game loads ZONE001 from the tables at tick 1385.
  - With the disc read forced, both replays match the old baseline.
    Converted, every frame after frame 39-40 equals the disc run shifted by
    2 frames. Baselines re-recorded: Debug and Release 9,889 tests /
    13,074,767 assertions, 2 replays, 0 regressions.
- The relational species, variant and loot tables, page depth and palette
  binding, and production enemy-edit evidence remain open on #199.

## Actor request ownership and identity (#48/#55), 2026-11-22

Starting revision: `ff4b3713d`, clean worktree. Commit
`fd50525fad2609929a326acc3818c663982b4712` extends the generation-checked
actor identity into room-enemy asset loading. Each ownable request is ticketed
to that actor; the spawn waits for all results before publishing it, cancels
queued requests when a later submission fails, and releases partial model
resources on load or publication failure. The actor owner is an opaque ID, not
a fixed enum. Follow-up `42a110beb` handles cancellation refusal by draining
the owned requests to completion before releasing the unpublished actor.
These are bounded producer-to-publication slices, not completion of either
issue.

Debug and Release passed `[actors-spawning]` (49 cases / 437 assertions),
`[asset-ownership]` (13 / 132), and `[encyclopedia-preview]` (43 / 684).
`quality.py --since origin/main` reported 0 new findings before the push.
Ledger gates, modernization, hazards, headers and static regression checks
reported 0 violations. Exact-revision Actions run
[45014081945](https://github.com/JEschete/VS-NativePort/actions/runs/45014081945)
passed both Source Quality and Tool and ledger controls.
The cancellation-refusal follow-up passed both jobs in exact-revision run
[45014737849](https://github.com/JEschete/VS-NativePort/actions/runs/45014737849).

The follow-up `quality.py --files Native\Game\src\Actors\ActorSpawning.cpp
Native\Tests\src\ActorSpawningTests.cpp --tidy --jobs 1` passed with 0 new
findings. An earlier serial tidy scan including the changed shared headers found
two diagnostics in unchanged `AnimationControl.cpp` functions about swappable
integer parameters; no new tidy findings were in the edited files. The ordinary
changed-source quality check passed.

Later slices: `ac397a138` owns scripted model requests; `4e1de0464` removes
actors by their instance definitions and releases each claim once. The
ticketed-load slice (`Actors/ActorLoads.hpp`) tracks every actor-owned load
until it ends: `SettleLiveActorLoads` runs each frame after the queue update
and logs each outcome, and removal or the spawn drain releases the actor's
tickets. The untracked owned-request API is retired. Release door run
(`--door-from z009-r000-d000 --door-limit 20`) matched the 15 passed / 5
blocked baseline with 20/20 teardowns, 0 engine errors and 45 settled loads
with no failed, lost or prematurely released tickets.

Record-memory slice: an owned request names no memory.
`SubmitActorAssetRequest` refuses an owned request with a destination; the
owner passes an `ActorRecordMemory` resolver with each live submission, which
the queue keeps per request and calls at bind time with the stored owner. A
spawned actor binds only its own body, second body, weapon and shield records
(`OwnedActorRecordPart`, generation-checked); the enemy preview resolves its
model buffer parts (`EnemyPreviewRecordMemory`); the equipment menu's reloads
bind through Ashley's handle. A kNone memory is a counted bind refusal, so the
load fails visibly. The Release door run again matched 15 passed / 5 blocked,
20/20 teardowns, 0 engine errors and 85 settled loads. Still open for #48
criterion 2: the head-owned `ActorAssetLoadState` parser state is global, the
shared record index rides in `request.data`, immutable sharing waits on
#203/#205, and the unowned menu `ActorPreview` and gimmick model requests
still name destinations.

Shape slice (#55): native readers of the actor's allocation kind word (0x40)
now read the spawn definition (`LiveActorShape`, installed or pending): the
action presentation's bystanders, AI special-actor registration and the AI
creation's actor record lookup. The spawn host still writes the word for the
PS1 layout; no native code reads it. An actor the spawn host did not allocate
has no shape, so it is no bystander, takes no special slot and has no record.
`InstallLiveActorInstance` defines actors whose storage the host does not own.

Both #48 and #55 remain open. Original global waits (`ScriptWaitForObjectLoad`,
the equipment menu's queue-busy check) stay as original timing; banks and
material sequences stay unowned; broader actor/resource migration (slot
addressing, the combined `ActorRecord`) remains. No hardware verification is
claimed.

## Connected-controller handover (#438), 2026-11-18

Starting revision: `aa41806e6`, clean worktree. The active-controller removal
handler now closes the old pad, resets its rumble cache, enumerates SDL's
remaining gamepads and opens the first available one. Initial connection and
handover share the same open/logging path; enumeration/open failures are logged.
Inactive-controller removal does not change the selected pad.

The two initial regression cases failed on the old implementation: input and
rumble were not handed over. Final SDL virtual-device tests cover real queued
removal events after both ADDED events have already been consumed, replacement
buttons/stick/edges, inactive removal, last-pad removal, reconnect, unchanged
rumble levels on the replacement and rumble shutdown. The existing removal test
now actually detaches its device: SDL no longer delivers rumble callbacks after
detachment. Connected-pad shutdown still checks the zero-motor callback.
Automatic fixtures allow only their synthetic VID/PID so rescans cannot select
or rumble a physical controller; the opt-in hardware test remains separate.

Validation on the uncommitted changes subsequently committed for #438:

- MSBuild `VagrantStory.Native.sln /t:VS_Tests;VS_Desktop;VS_CombatLab /m
  /p:Configuration=<Debug|Release> /p:Platform=x64 /v:minimal /nologo` passed
  in both configurations. After test-only refinements, rebuilt
  `Native\Tests\VS.Tests.vcxproj` with the same configuration/platform options.
- `build\<Debug|Release>\VS.Tests.exe "[input]" --reporter compact` passed
  **62 tests / 797 assertions** in each configuration.
- `quality-native.cmd --files Native\Engine\src\Input\Pad.cpp
  Native\Engine\include\Input\Pad.hpp Native\Tests\src\VibrationTests.cpp
  --tidy` passed with **zero new findings**, including header dependents.

Evidence: `build\issue-438-before-tests.log`,
`build\issue-438-<debug|release>-build.log`,
`build\issue-438-<debug|release>-final-build.log`,
`build\issue-438-<debug|release>-tests.log`, and
`build\issue-438-quality.log`. Exact-revision CI/closure evidence belongs on #438.
No new physical-controller model or gameplay certification is claimed.

## Transient-tooling retirement (#440), 2026-11-18

Starting revision: `d654bad79`, clean worktree. Retired 255 campaign files:
historical naming inputs/outputs/report/duplicate decisions, 20 batch-specific
generators, three one-off scripts, and the completed 54-slice and 900-function
promotion trackers. Generic review tools remain; their scratch outputs are now
ignored. Shared layout/citation support and its tests moved out of `sym_gen`
into `tools\naming\symbol_support.py` and `test_symbol_support.py`.

The completed queue's scheduling checks are gone, not the canonical ownership,
promotion, behavior or compatibility-retirement gates. All retained native files
and canonical CSV ledgers match their pre-cleanup hashes. `FileMenuLive.cpp`
and other live bindings are unchanged. Only the 62 quality-baseline entries for
deleted tool paths were removed; all 4,995 retained entries are unchanged and
no new findings were accepted.

Local validation: 48 naming tests, 35 modernization tests, 226 encounter/copier
tests and two symbol-ledger tests passed. Modernization and gate checks report
zero violations; changed-source quality reports zero new findings. Both copier
runs reproduce all 486 pre-cleanup output hashes without touching owned files.
Two solution generations are stable. Native builds/replays were not repeated
for this tooling-only cleanup because every native source/header/project hash
is unchanged. Exact-revision CI closure evidence belongs on #440.

Evidence: `build\transient-cleanup\before.json`, `verification.json`,
`removed-baseline-entries.json`, `copy-1.log`, `copy-2.log` and the project/test
logs in the same directory. Retired scripts and snapshots remain in Git history.

## Owner-requested cleanup, 2026-11-18

- Promotion tooling retired in `cb1e04334`; do not run promotion checks.
- Changed-file CI: `185758389`, verified on follow-up `8cd4a839a` in run
  `44235628223`. Exactly two changed Python files were selected: source gate
  approximately 0.6 seconds, source job 53 seconds, whole workflow approximately
  103 seconds versus 407 seconds originally. #437 is closed. Rules/scope changes
  and manual dispatch retain full audits; partial clone checks compare only the
  selected files, not unchanged sources.
- LibDs trim: `cda4b6424`, CI `44236497503`. Six uncalled functions and unused
  declarations are removed. Two referenced compatibility hooks and three layouts
  remain; the sector-addressed loading adapter is still EC-04 debt.
- Portable game state and backup cleanup: `d47e9bc0b` (#439, #436). Debug and
  Release solution builds passed `[launcher],[disc-queue],[save]`: 187 tests,
  6,100 assertions in each configuration. These builds used the final native
  source later committed as `d47e9bc0b`. Changed-source quality and focused
  clang-tidy on `Setup.cpp` and `LauncherTests.cpp` reported no new findings.
  CI found the renamed first-launch test's old inventory name; the follow-up
  changes that single name, preserving all inventory entries and results.
  Exact-revision closure evidence is recorded on #439 and #436.
- The real default launcher migration moved 11,410 save/asset files into
  `build\Debug`, with every SHA-256 unchanged. Assets were reused without
  extraction; old AppData state and its obsolete asset backups are gone.
  Explicit-state and self-check probes created no AppData. A native run using
  the moved assets and a nonexistent disc path reached 1,500 ticks and produced
  its requested frame (`build\portability-smoke\frame_1500.bmp`). This verifies
  startup/loading, not combat or all gameplay.
- Development state: `8cd4a839a`, 208 Player tests and CI passed. 8,829 Player
  files moved to `build\player` and 3,631 archive files to
  `build\evidence-archive`; all hashes matched and SQLite integrity checked OK.
- Controller support remains SDL3 mapping-based; no new physical-controller
  certification was performed. Already-connected pad handover is covered by
  the #438 checkpoint above.

Local evidence is ignored under `build`: `portability-debug-final-build.log`,
`portability-release-build.log`, `portability-release-tests.log`,
`portable-first-launch.log`, `legacy-state-hashes.json`,
`player-migration-hashes.json`, and `evidence-archive-migration-hashes.json`.
Do not delete migrated saves, assets, Player state or retained evidence as build
cache cleanup. Explicit launcher/runtime path overrides remain supported.

Maintained guidance:
- [Native source port plan](NATIVE_SOURCE_PORT_PLAN.md)
- [Ledgers](ledgers/)
- [Ordered work tracker](https://github.com/JEschete/VS-NativePort/issues/4)
