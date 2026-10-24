# Ledger Summary

Reconciled 2026-10-21 from the persisted ledgers and ownership registry in the
local worktree at `4777775f5d16163fc4d0f2740170bdb02fb4d175`, including
the published audio ledger/ownership edits. This is a report of recorded state,
not a new tracker, reference rescan, build or gameplay verification.

## Current inventory

| Record | Rows | Rows with `final_name` populated |
| --- | ---: | ---: |
| [Function catalog](FUNCTION_CATALOG.csv), all rows | 4,207 | 3,765 |
| Function catalog, port targets (`copy=yes`) | 3,762 | 3,762 |
| Function catalog, excluded (`copy=no`) | 445 | 3 |
| [Data ledger](DATA_LEDGER.csv) | 2,174 | 2,174 |
| [Type ledger](TYPE_LEDGER.csv) | 842 | 842 |
| [Field ledger](FIELD_LEDGER.csv) | 6,926 | 6,926 |

The catalog's `name` and `generic` describe the original source, not the native
name. The 2,266 port-target rows with `generic=1` therefore do **not** mean
2,266 native functions still need naming. A populated `final_name` also does
not establish the stricter `named` gate for parameters, locals and usage.
Excluded SDK rows are inventory decisions, not proof of completed retirement.

## Ownership and status are separate

[owned.csv](../../tools/port/owned.csv) registers **195 functions**, all with
catalog IDs, across **84 distinct source/header paths**. This measures
hand-maintained ownership only, not all first-party code or completed behavior.
The [copy manifest](../../tools/port/copy_manifest.json) separately identifies
generator-managed output.

Raw function status markers across all 4,207 rows:

| Column | `x` | `stub` | Blank |
| --- | ---: | ---: | ---: |
| `copied` | 3,522 | 236 | 449 |
| `compiles` | 3,758 | 0 | 449 |
| `named` | 0 | 0 | 4,207 |
| `understood` | 161 | 0 | 4,046 |
| `modernized` | 0 | 0 | 4,207 |
| `cpp` | 28 | 0 | 4,179 |
| `verified` | 7 | 0 | 4,200 |

All 2,174 data rows and 842 type rows record `copied=x` and `compiles=x`;
their `modernized`, `cpp` and `verified` columns are blank.

**Known bookkeeping limits:** 147 registered owned functions still carry a
legacy `copied=stub` marker. The raw 236 is not a current executable-stub count.
Use the source-backed [assembly gap audit](../ASSEMBLY_GAPS.md) and its coverage
records to distinguish implementations, aliases, partial bodies and remaining
gaps. Blank `named`, `modernized` or `verified` markers mean unrecorded gates,
not proof that all code is unnamed, unimplemented or untested.

The 28 `cpp=x` entries correspond to the owned audio slice. They are recorded
C++ markers, not evidence that every promotion/retirement prerequisite is closed;
their `modernized` gates remain blank. Five new `verified=x` markers cover the
menu wrappers' original command IDs/tags and actual open/change/select/leave/
refused-action dispatch, not every caller or perceptual parity. Stream/XA and
music-state rows record bounded tests and remaining listening scope in their
notes without blanket verification or retirement claims.

| Gate | Evidence needed |
| --- | --- |
| Ownership | Registered implementation/header and exclusion from generated output |
| Compilation | Successful builds tied to revision, dirty state and configuration; a stored marker is not a fresh build |
| C++ promotion | The [plan's promotion checklist](../NATIVE_SOURCE_PORT_PLAN.md#82-promotion-checklist-c--cpp-sets-cpp--yes), not just a `.cpp` suffix |
| Behavior verification | Exact tests/replays, inputs/assets, result and remaining limits; see [current handoff](../HANDOFF.md) and [historical evidence](../HANDOFF_HISTORY.md) |
| Architectural retirement | Applicable [EC gates](../EMULATION_COMPATIBILITY_AUDIT.md), migrated consumers and cleanup paths; tracked under #18 and its existing issues |

## Inventory snapshots versus current status

[SOURCE_FILE_LEDGER.md](SOURCE_FILE_LEDGER.md) and
[SDK_API_LEDGER.md](SDK_API_LEDGER.md) retain reference-inventory counts and
per-file/per-API gates. Their original-source counts and unchecked boxes are
not automatically synchronized with native ownership or issue acceptance.
The old summary's 3,679 game-function / 1,489 data-row snapshot is superseded
by the persisted-ledger counts above.

For a read-only recount in PowerShell:

```powershell
$functions = Import-Csv Docs\ledgers\FUNCTION_CATALOG.csv
$functions.Count
$functions | Group-Object copy | Select-Object Name, Count
foreach ($column in 'copied','compiles','named','understood','modernized','cpp','verified') {
    $column
    $functions | Group-Object $column | Select-Object Name, Count
}
foreach ($ledger in 'DATA','TYPE','FIELD') {
    $rows = Import-Csv "Docs\ledgers\${ledger}_LEDGER.csv"
    "$ledger rows: $($rows.Count)"
}
$owned = Import-Csv tools\port\owned.csv
$owned.Count
@($owned.path + $owned.header | Where-Object { $_ } | Sort-Object -Unique).Count
```

Refresh this report from the persisted ledgers after accepted ledger changes.
Do not run `build_ledgers.py` solely to refresh these numbers: that legacy
reference-inventory generator rewrites catalogs and emits a source-scan summary.
If intentionally regenerating inventories, preserve stable IDs, merged fields
and status edits, then reconcile this report against the resulting ledgers and
ownership registry. Do not infer missing gate evidence from aggregate counts.
