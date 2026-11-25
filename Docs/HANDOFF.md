# Current Handoff

This file is the current state only. **Replace it, do not append**, at a package boundary, an owner pause or a
blocker, and keep it under 12 KB (`python tools\lint\doc_budget.py --check`). Evidence (commits, CI runs, test
counts, captures) goes once, on the issue; link to it here. Earlier handoffs are in Git history; the last long
version is `git show 215cdd4d9:Docs/HANDOFF.md`.

## State

- `main` at `215cdd4d9` (action catalog, #499) plus the documentation trim that replaced this file. Source
  Quality run 45547829269 on `215cdd4d9` succeeded.
- Work stream: asset migration, ordered by roadmap #217 under plan decisions D25 and D26. Tracker #4's ordered
  gameplay list (#1-#16) is complete.

## In progress: gameplay catalogs (#499)

Published slices, each with its evidence on #499: materials, bestiary (with #500), learning requirements, status
rules and shared timing, action categories, actions. The contracts are ASSET_PIPELINE §4.2-§4.7.

Open limits; none of the slices claims them:

- No controller or combat capture of an authored edit exists for any #499 slice; gameplay runs exercise only the
  default values.
- Actions: the top-level `effect_id` is an unnamed integer, the hit mediators are code, and Learning's `action`
  column still names break arts by slug. About 110 references in 62 files read `ActionDefinition` through the
  EC-04-T-ACTION bridge, so the #58 gate is not claimed.
- Action categories: the AI category sets (`PlanningPrimitives.cpp`), preview lists (`ActionPreview.cpp`) and
  `DamageDistribution` (`DamagePolicies.cpp`) are keyed by category number, and the four-bit field caps the
  catalog at 16 categories.
- Status rules: action, limb and AI consumers read the typed rules through compatibility lookups.
- Bestiary: preview-table indices, the ZUD/SHP/SEQ actor bridges, the BATTLE cursor bookmark and defeat flags keep
  their compatibility limits.

Next: items and text (#500) are separate slices, then the remaining executable catalogs.

## Other open streams

| Issue | Open gates |
|---|---|
| #210 FAR backgrounds | Matched-reference placement, gradients, seams and parallax; geographic room transitions and re-entry; MPD/ARM semantic room bindings; EC-04 retirement. Reference parity is blocked on state alignment: the next observation is an approved control path that selects New Game and pauses at "Have you found Sydney?", with the reference room, event and camera state. |
| #500, #54 text | The offline codec decodes the F8-FF controls. MCMAN's extended Japanese glyph records (47 location labels) are unsupported. Full catalogs, call-site argument bindings, font and charmap assets and typed UI consumers remain. |
| #520 readable authoring | Applies to every AM issue (ASSET_PIPELINE §1.2, §7.2). |
| #199, #200, #203, #55, #48 | Zones, actor archetypes, actor shapes, actor identity and content-request ownership. |
| #58 | Item definitions and instances; gated by the action bridge above. |
| #489 | EC-07: retire the legacy controller view. |

## Cautions

- Offline preservation of imported bytes is not production adoption. Runtime repacking from native data is a
  T-shim even when its input is CSV, JSON, glTF or compiled SQLite (owner, 2026-11-23; plan §15, EC-04).
  Reconstruction results alone do not complete an AM issue.
- The local `resources\rood-reverse` checkout (`f6bf8402`) is ahead of the CI pin (`c9a869b8`), and its
  `ignore = all` hides this from `git status`. `provenance.py --check` then fails locally: check against an export
  of the pinned source (`--decomp <dir>`), and never run a broad `--sync` against the mismatched checkout.
- Tests that open the real `Native\data` need locally extracted data.
- Proof roots under `build\evidence` contain read-only asset junctions; do not clean them recursively. Do not
  delete migrated saves, assets, Player state or cited evidence as cache cleanup.
