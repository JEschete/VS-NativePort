# Current Handoff

This file is the current state only. **Replace it, do not append**, at a package boundary, an owner pause or a
blocker, and keep it under 12 KB (`python tools\lint\doc_budget.py --check`). Evidence (commits, CI runs, test
counts, captures) goes once, on the issue; link to it here. Earlier handoffs are in Git history.

## State

- `main` at `aef2e1e47` (action volumes, #520; Source Quality run 45766202131 succeeded). The owner
  authorized direct work on `main`; each slice is committed, pushed and CI-confirmed before its evidence comment
  on #520.
- Work stream: asset migration, ordered by roadmap #217 under plan decisions D25 and D26. Tracker #4's ordered
  gameplay list (#1-#16) is complete.
- Verification policy (owner): focused Catch2 tags and scoped checks per slice; the wide Debug run (~24 min) only
  at batch boundaries. The last wide run, on `8dab23223`, passed 10,153 cases.

## In progress: semantic readable authoring (#520)

ASSET_PIPELINE §4 and the §7.2 readability gate. Shipped slices, each with evidence on #520:

- Workshop recipe domains and archived reserved bytes; zone part domains and split shape bits.
- Zone enemies: status immunities as status flags, limb action slots, quarantined template reserved bytes,
  equipment indices and class/affinity tail bytes, ZUD index derived from slot, named AI settings, described
  columns and units, enemy names as text, enemy kinds in an enemy-kinds table, gem effects by behavior, part
  categories by part kind (selected enums).
- Actions: hit effects, hit formulas, hit prerequisites and shield PP by behavior; effect packages by asset id
  (`asset.effect.eNNN.p`; semantic effect names wait for #216); behavior flags defined in ACTION_HITS §4.7.2 and
  range/area volume shapes and placement in §4.7.3 (`aef2e1e47`).
- Bestiary: previews by archetype, defeat ranges by enemy kind; limb labels by body part.
- Materials: catalog version 2 keeps only the columns MENUC reads (strength, intelligence, agility); class
  points, affinities and base DP are archived as named fields in the import provenance (`694b6714c`).

Remaining:

- §7.2 cold reads (five seeded records per family, seed `520:<family>`): materials passed; actions reads 4 and 5
  were partly readable. Their volume, flag, prerequisite and AI-reach findings are fixed in ACTION_HITS; camera
  framing, damage split across limbs and effect timing stay undocumented. Still unread: the JSON families
  (`models\zud`, `models\animation-sets`, audio banks and AKAO streams, backgrounds, text) and glTF.
- Blocked on their consumers: zone-enemy `movement_class`/`gait_class` (semantics unnamed; schemas list only the
  bits consumers test) and `zones.music_id` (selects `MUSIC/MUSIC<n>` and `SOUND/WAVE<n>`; needs #501).
- Deferred: room semantic names, JSON `raw_scale`, an optional typed `GemEffect` enum, and typed runtime
  adoption with EC-04 T-shim retirement.
- Checked and kept as columns: action-hit `hit_function` picks every checked hit's chance formula (only direct
  hits skip it), and blade `attack_shape` is copied with the whole `vs_Range` into inventory and saves
  (`146C.c:637`, `MENUC 168.c:4974`), so it cannot move to import provenance as the unread material fields did.

## Other open streams

| Issue | Open gates |
|---|---|
| #499 catalogs | No controller or combat capture of an authored edit exists for any slice. Actions: ~110 references in 62 files read `ActionDefinition` through EC-04-T-ACTION, so #58 is not claimed. Action categories stay keyed by number (AI sets, previews, `DamageDistribution`; 16-category cap). Status rules and bestiary consumers read through compatibility lookups. |
| #210 FAR backgrounds | Matched-reference placement, gradients, seams and parallax; room transitions and re-entry; MPD/ARM room bindings; EC-04 retirement. Reference parity needs an approved control path to "Have you found Sydney?" with the reference room, event and camera state. |
| #500, #54 text | MCMAN's extended Japanese glyph records (47 location labels) are unsupported. Full catalogs, call-site argument bindings, font and charmap assets and typed UI consumers remain. |
| #199, #200, #203, #55, #48 | Zones, actor archetypes, actor shapes, actor identity and content-request ownership. |
| #58 | Item definitions and instances; gated by the action bridge above. |
| #489 | EC-07: retire the legacy controller view. |

## Cautions

- After pulling a converter version bump, run `build\Debug\VS.Extract.exe authoring Native\data` before tests
  that open the real data; a second run must report no changes. Do not migrate while a wide run reads the data.
- Offline preservation of imported bytes is not production adoption. Runtime repacking from native data is a
  T-shim even when its input is CSV, JSON, glTF or compiled SQLite (owner, 2026-11-23; plan §15, EC-04).
- The local `resources\rood-reverse` checkout is ahead of the CI pin, and its `ignore = all` hides this from
  `git status`. Check provenance against an export of the pinned source (`provenance.py --check --decomp
  <dir>`), and never run a broad `--sync` against the mismatched checkout.
- `quality.py --files ... --tidy` reports existing findings as new; compare with the diff. CI runs
  `quality-native.cmd --since github` without tidy.
- Proof roots under `build\evidence` contain read-only asset junctions; do not clean them recursively. Do not
  delete migrated saves, assets, Player state or cited evidence as cache cleanup.