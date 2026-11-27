# Current Handoff

This file is the current state only. **Replace it, do not append**, at a package boundary, an owner pause or a
blocker, and keep it under 12 KB (`python tools\lint\doc_budget.py --check`). Evidence goes once on the issue.

## State

- Latest code revision on `main`: `30ef54d7151d98408b8ff599cc074b7e583f0938` (ETM queue-owned loader progress, #48).
  Exact-revision Source Quality CI passed: [run 45955613953](https://github.com/JEschete/VS-NativePort/actions/runs/45955613953).
- Current work: #48, actor/model request ownership, cancellation and completion. The issue remains open.
- Verified checkpoint evidence: [#48 comment](https://github.com/JEschete/VS-NativePort/issues/48#issuecomment-7212100242).

## In progress: #48

The live ETM adapter now imports legacy phase/buffer state once for raw queue entries; later diagnostic snapshots
cannot overwrite the entry's progress. The ETM producer itself remains unowned (#207).

Next: complete the lifecycle trace for the menu actor-preview producer—submission, record binding/publication,
partial failure, cancellation and release—then migrate a bounded producer path with tickets and deterministic
cleanup.

Open limits: file streams remain serialized; other producers still queue unowned requests; shared-record references
still point to mutable actor slots. Complete the selected producer-to-actor-publication path, cancellation/replacement
and partial-failure behavior, repeated entry, and the applicable Debug/Release and gameplay evidence before closing
#48. Coordinate final actor publication with #55 and follow the ordered implementation handoffs in #217.

## Cautions

- Keep `resources\rood-reverse` read-only and use the pinned decomp for provenance checks.
- Do not replay the full GitHub CI workflow locally; use focused tests and scoped quality checks.
- Preserve unrelated edits and keep disc-derived data, captures, memory dumps and BIOS files out of commits.
