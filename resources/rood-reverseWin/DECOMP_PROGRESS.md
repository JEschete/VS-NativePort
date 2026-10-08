# Local Decompilation Tracking

## Latest Session: 2026-10-07

This section supersedes the dated input hashes and report timestamps below.
The earlier sections remain as historical evidence. Local accepted progress
is still two functions; this session did not accept a new reconstruction.

### Reference Update

- Read all eleven Markdown files under this working directory, including the
  vendored tool guides; reread the updated README after importing changes.
- Imported 85 maintained source, asset-description, configuration, tool, and
  README changes from reference commit
  `ef3f027a64bf861bbf5ad3efa30851788d499ce4`, using recorded ancestor
  `7bae70f6651cf70180852dd243d39d4a703aa589`.
- The reference checkout was read-only throughout. No fetch, pull, reset,
  checkout, staging, or source write occurred there; its HEAD and clean status
  were verified after the import.
- Three overlapping files were merged against the recorded ancestor:
  [exports.txt](config/BATTLE/BATTLE.PRG/exports.txt),
  [40564.c](src/BATTLE/BATTLE.PRG/40564.c), and
  [4A0A8.c](src/BATTLE/BATTLE.PRG/4A0A8.c). Incoming implementations and
  declarations retain the independently verified local names. The export list
  includes both local and incoming additions and remains sorted.
- The reverse patch preflight passed for the other imported changes. Hash
  checks preserved 989 unrelated local files, including this unit and its
  shared actor header. The manifest records 719 committed upstream files;
  it does not relabel merged local work as upstream.
- Retired Linux make recipes were not restored. Their new `.c.o`, `.s.o`,
  `.img.png.o`, and `.vsString.yaml.o` object names were carried into
  [windows.mk](tools/make/windows.mk) and the TITLE override in
  [Makefile.windows](Makefile.windows). A Windows dry-run of
  `build/src/BATTLE/BATTLE.PRG/38C1C.c.o` selects GCC `2.7.2-cdk`, the configured
  warning flags, and `-no-pad-sections`. This is not a Windows historical build;
  the historical Windows compiler executables remain unavailable.

### Matching Infrastructure And Fresh Controls

- The unchanged func_800A249C control initially failed: 212 compiled bytes
  versus 216 retail bytes, with incorrectly selected GCC `2.7.2-psx`.
  [matching.py](tools/verification/matching.py) now queries the upstream
  `.c.o` target instead of the retired `.o` name. The same control then matched
  all 216 bytes with the independently configured GCC `2.7.2-cdk` flags.
  Comparison ranges, retail bytes, relocation checks, and acceptance criteria
  were not changed.
- The new upstream fixed-length-string extension imports
  `tools.dev.dumpFixedLenStrings`. Its import succeeded in the canonical
  Windows tree but failed in the isolated splat copy because `tools/dev` was
  absent. [splat_artifacts.py](tools/verification/splat_artifacts.py) now copies
  that dependency; the original failed split then passed.
- Recorded table addresses `D_800E90AC` and `D_800E90B4` in the BATTLE symbol
  map only after checking the actual retail loads and the bytes described
  below. Regenerated splat artifacts before checking target relocations.
- Reran the complete control set against the final checker and splat inputs:
  canonical func_800A291C and func_800A249C returned `MATCH`/exit 0; an identical
  candidate returned `CANDIDATE_MATCH`/exit 3; changing only the X tile divisor
  from 128 to 256 returned `CANDIDATE_DIFF`/exit 1 at exactly `+0x44` and `+0x4C`.
  func_800A1C10 was refused as `INCLUDE_ASM`, and func_800A1B28 was refused for
  lacking a retail address, both with verified native exit code 2. An invalid
  CLI option from an interrupted helper run was not counted as a refusal.
- Refreshed both previously accepted local matches after the upstream import
  and metadata changes. These remain function-only results, not a source-unit,
  linked-overlay, behavioral, or native-runtime claim.

Fresh reports are under `~/.cache/vagrant-matching/reports` in Ubuntu WSL.
The report timestamps below are UTC, crossing into 2026-10-08 while the local
session date was 2026-10-07.

| Function | Role | Bytes | Report | Generated UTC |
| --- | --- | --- | --- | --- |
| func_800A2790 | Existing local match | 396 | `38C1C-func_800A2790.json` | 2026-10-08T03:41:41.766573Z |
| func_800A25EC | Existing local match | 420 | `38C1C-func_800A25EC.json` | 2026-10-08T03:42:09.623839Z |
| func_800A291C | Reference positive control | 132 | `38C1C-func_800A291C.json` | 2026-10-08T03:42:37.835091Z |
| func_800A249C | Reference positive control | 216 | `38C1C-func_800A249C.json` | 2026-10-08T03:43:06.022088Z |

All four have status `MATCH`, canonical mode, zero differences, source SHA-256
`55cdbd92aa9eec12716dc633c7f6d23a579c2780ed8cbe6adeaec5ca8a168da5`,
and snapshot SHA-256
`c7039e0fb760f20ef6cf5c4452256c93b2b7981fefd37a235d12af055ed343fe`.
Compiler, assembler, preprocessing, linker settings, binary hash, address,
complete range, dependencies, and exact invocation are retained in each report.

Current input SHA-256 values:

- Checker: `66c6bf156535ce927d16b020150f9136e6db897877d802ab5ae2864f51b6b32b`.
- Splat generator: `ffedd769de564d02691b37e828f27968fb84de46e860cbecd3731576c78ef77a`.
- Splat manifest: `9fe169b7924d49e6255d7afffab39ee409e9e923f0fc1cd7982f5adc6e88c886`.
- BATTLE symbol map: `5c60c39134ca08c060e50efa803e39e8f9e2c9e325d1b53624db1755b8f5662d`.
- Actor header, unchanged: `3d11a0102538d507a193338edd02d27d298bfd583aa4f6be262abe3ae167ec0c`.

### Current Function: func_800A208C Is BLOCKED

- Worked on this function only, in the existing source unit
  [38C1C.c](src/BATTLE/BATTLE.PRG/38C1C.c). Canonical source remains unchanged
  and still contains its `INCLUDE_ASM`; no candidate is accepted or integrated.
- Fresh bounded Windows Ghidra analysis completed in
  `build/ghidra/20261007-225349-852`. The saved `stdout.txt` contains the
  decompiled function and its original instructions; the evidence export
  completed on the hash-verified BATTLE and main-executable inputs.
- Full target range: BATTLE CPU `0x800A208C..0x800A249C`, file
  `0x3988C..0x39C9C`, 1,040 bytes, end exclusive. Retail call references are
  `0x800B63F0` and `0x800B6490`; their argument forwarding was checked against
  the neighboring source. The saved DataCrystal actor-data page describes a
  different record and was not used to invent meanings for this model record.
- Verified behavior includes the offset-8 state guard, speed minimum 18,
  packed destination word, existing position resolver, separate nonpositive
  and positive height branches, signed divisions, halfword motion stores,
  frame count, animation call, state update, and facing call. Unknown flags
  and the external resolver's existing stack-pointer defect remain unresolved;
  no fallback or unrelated repair was introduced.
- Retail table loads at `0x800A223C..0x800A2254` and
  `0x800A2338..0x800A2354` independently establish addresses `0x800E90AC` and
  `0x800E90B4`. Read-only binary inspection confirmed six bytes at file
  `0x808AC`: `5,5,5,10,12,12`, and six at `0x808B4`: `63,63,63,30,42,42`.
  No table extent beyond these used entries is claimed.
- The readable C candidate uses no new assembly, opcodes, or inclusions. Its
  local retry label prevents a non-retail loop-strength reduction; its unused
  two-word frame declaration preserves the eight unwritten local stack bytes
  seen in retail, following this unit's existing frame convention.
- Every meaningful candidate change was immediately checked in an isolated
  WSL snapshot. Two bounded variation runs tested local load/temporary forms
  without changing canonical source, expected settings, or comparison rules.
- Final result: `CANDIDATE_DIFF`, 1,040 compiled and 1,040 retail bytes, with
  28 differing words. Remaining differences include position-load scheduling,
  `a0`/`a1` register allocation around the table/velocity variables, and a
  halfword reload versus register sign extension. Equal size is not acceptance.
- Retained candidate:
  [direct-False-False-True.c](build/trajectory-variants-lddy9x_x/direct-False-False-True.c),
  SHA-256 `832c80cfedeb2d331dd425277cdb17aecbfcf2d3e9693c8040c5c704fe3b666d`.
  Full current report:
  `~/.cache/vagrant-matching/reports/candidates/38C1C-func_800A208C.json`.
  Continue only this function until a fresh canonical `MATCH` is obtained or
  the blocker is explicitly redirected. No other function was reconstructed.

## Current Project Status

Status updated on 2026-10-05 against the source on disk.

- The decompilation is unfinished. The Linux/upstream checkout is reference
  material only; its matching results and percentages are not Windows progress.
- Windows decompilation has two accepted local function-level matches beyond
  the imported reference: func_800A2790 and func_800A25EC. No source unit or
  linked overlay match is claimed.
- 161 BATTLE.PRG placeholder symbols now carry verified names (see
  [DataCrystal Symbol Pass](#datacrystal-symbol-pass)). Names do not change
  compiled code and are not decompilation progress.
- The native port has not started as an accepted implementation. Existing
  native code, probes, and generated executables are experimental leftovers,
  not completed port features or a playable game. Root native CMake excludes
  this unit, so the new functions are not natively integrated.

## Verifier Baseline And Controls

Established 2026-10-05 before any reconstruction, then rerun with the final
checker after both edits. All checks ran in isolated WSL snapshots; canonical
sources stayed unchanged during every run.

- Checker: [matching.py](tools/verification/matching.py), SHA-256
  `608a72df26ca1ef6a978a1970ccd07dae0dfd60a9482b391221e229815575208`.
  Toolchain: old-gcc 0.17 `2.7.2-cdk` cc1 SHA-256
  `90db2db7bcfe372244ca4d9785a7bfbf7613d711f69ee3bfe6c91d82a85e611d`, Ubuntu
  `mipsel-linux-gnu` cpp 12.4.0, as/ld 2.42, GNU Make 4.3, maspsx
  `--aspsx-version=2.77 --macro-inc`.
- Canonical assembler macros come from splat 0.50.0 run on Windows in an
  isolated copy by [splat_artifacts.py](tools/verification/splat_artifacts.py).
  Its manifest (SHA-256
  `30f8b445069dbd7e2f1c8ff61186af1ba957a7fafbf1dde4db0987c6764cf5c7`) records
  config, symbol-map, retail-binary and output hashes; stale artifacts are rejected.
  After the symbol-map renames in the DataCrystal Symbol Pass, the artifacts
  were regenerated (2026-10-05T06:54:07Z); the current manifest SHA-256 is
  `75285ca70b6854644d9eb7eb7a1719f825c920058383a67e663f73e0f00a08f7`.
- Positive controls on reconstructed reference C: func_800A291C (external call)
  and func_800A249C (global data plus external call) returned canonical `MATCH`
  with exit code 0.
- Candidate controls: an unchanged noncanonical copy returned `CANDIDATE_MATCH`
  with exit 3 (never acceptance). A copy changing one division in
  func_800A291C returned `CANDIDATE_DIFF` with exit 1, reporting exactly the two
  changed instructions at +0x44 and +0x4C.
- Fail-closed controls: an `INCLUDE_ASM` target is refused with exit 2; a target
  absent from the configured symbol map (func_800A1B28) is refused.
- Verifier fixes made to reach this baseline: the canonical `--macro-inc` flag
  and splat macros were missing; relocations outside the target no longer block
  checks while every relocation inside it must resolve to a verified address;
  the target links at its retail address without section-alignment padding;
  only `R_MIPS_26` jumps that stay inside the target may be section-relative.
  Reports now record dependency hashes, a whole-snapshot hash, flags, linker
  arguments, tool versions, command, path and time. Stale reports are deleted
  before each run.
- Rerun after each naming pass below, most recently against the regenerated
  manifest, with identical results: four canonical `MATCH` results (exit 0),
  `CANDIDATE_MATCH` (exit 3), `CANDIDATE_DIFF` at +0x44/+0x4C (exit 1), and
  both refusals (exit 2).

## Current File: BATTLE/BATTLE.PRG/38C1C.c

- Current source SHA-256, changed since acceptance by names only (see Names):
  `55cdbd92aa9eec12716dc633c7f6d23a579c2780ed8cbe6adeaec5ca8a168da5`.
- Current [3A1A0.h](src/BATTLE/BATTLE.PRG/3A1A0.h) SHA-256, also changed by
  names only:
  `3d11a0102538d507a193338edd02d27d298bfd583aa4f6be262abe3ae167ec0c`.
- Remaining: three `INCLUDE_ASM` bodies and two inline-assembly blocks in the
  reference C of func_800A141C. Those blocks are unfinished reference material.
- func_800A1C10 and func_800A141C need GTE operations. With no-inline-assembly
  and exact matching both required, they are `BLOCKED` until an approved
  non-assembly matching approach exists. func_800A1DE8 and func_800A208C still
  need analysis against the same constraints.
- The retained 63-group result belongs to a removed candidate with different
  source/header hashes. It does not validate current source. The leftover
  harness references removed helpers and incompatible signatures and stays retired.
- Original module: file offsets `0x38C1C..0x3A1A0`, CPU addresses
  `0x800A141C..0x800A29A0` (end exclusive). Retail BATTLE.PRG SHA-1
  `d53aaccc3b3a2fc057d05e0dcea92f7182bc72a9`, SHA-256
  `ad6914c81be92a008af8dece6ad394ce39c5aa42360542b5531716ebd00bba12`.

| Routine | CPU Address | File Offset | Current Source | Local Acceptance |
| --- | --- | --- | --- | --- |
| func_800A141C | 0x800A141C | 0x38C1C | Reference C with two inline-assembly blocks | BLOCKED: GTE |
| func_800A1C10 | 0x800A1C10 | 0x39410 | INCLUDE_ASM | BLOCKED: GTE |
| func_800A1DE8 | 0x800A1DE8 | 0x395E8 | INCLUDE_ASM | None |
| func_800A208C | 0x800A208C | 0x3988C | INCLUDE_ASM | None |
| func_800A25EC | 0x800A25EC | 0x39DEC | Readable C | Accepted canonical MATCH, 420 bytes |
| func_800A2790 | 0x800A2790 | 0x39F90 | Readable C | Accepted canonical MATCH, 396 bytes |

### Names

Placeholders were replaced on 2026-10-05 only where instructions, data, and
consumers establish the meaning; DataCrystal served as corroboration only.
Names do not change code: all 291 C units that compile in an isolated snapshot
produced byte-identical historical cc1 output before and after. The sweep could
not compile TITLE 22C.c: its Makefile makes the object depend on a generated
asset object and derives `-D MENUBGSZ` from that object's size, and the
verifier's flag lookup supports neither. None of the 25 tracked files that unit
reads differs from the last commit, so names cannot affect it. The lookup also
skips `config/MENU/Makefile` and `config/EFFECT/Makefile`, so MENU and EFFECT
units not pinned by their own overlay Makefile were compiled with GCC 2.7.2-psx
instead of 2.8.1-psx, identically in both runs.

| Old Name | New Name | Evidence |
| --- | --- | --- |
| `D_800F4538_unk0.unkF`, `D_800F45E0_t.unkF` | `index` | Object setup stores `D_800F45E0[arg0->index]`, then copies `arg0->index` here. func_800AE68C passes `&vs_battle_actorModels[i]->unk0`; func_800AE4FC reads `vs_battle_actorModels[index]` back. Weapon models use `index * 2`, like this unit's slot argument. |
| `D_800F4538_unk0.unk68`, `D_800F45E0_t.unk68` | `model` | Heap block freed when the slot is cleared. Its layout matches SHP data from file offset `0xF8`: AKAO, group, vertex, and polygon offsets at `+0x30..+0x3C`, then 16-byte joint records at `+0x40` (func_800A1C10 indexes `<< 4` from `0x40`). 44F14.c passes `akaoOffset` to sound calls. |
| `_armature_t.unk0` | `length` | SHP joint word 0 is the bone size. func_800A141C and func_800A1C10 negate it (full) or halve it as an X offset along the bone; func_800A17BC and func_800A11D8 return it with the bone matrix. |
| `D_800F4538_t.unk65A` | `targetSphereOffsetY` | The SHP header copy starts at actor `0x5E8`, as named fields confirm (`texMapOverlays` `0x5F4` = SHP `0xC`, `shadowRadius` `0x64C` = SHP `0x64`, `btSeqLbas` `0x664` = SHP `0x7C`). SHP `0x72` is the target-sphere Y position; selector `0xFA` adds it, scaled, to the actor's Y. |
| `D_800F4538_t.unk1848` | `movement` | func_800A4E68 dispatches on its first byte to the position-stepping func_800A25EC (2), func_800A2790 (3), and func_800A291C (4). |
| `D_800F4538_unk1848.unk0` | `mode` | That dispatch byte. func_800A249C stores 6; func_800A525C resets it to 1, or 0 for slot 0. |
| `D_800F4538_unk1848.unk6` | `facing` | Passed as the target angle to func_800AA850, which subtracts `unk0.facing` and wraps to +/-2048 (also func_800A6660). |

- Parameters and locals in this unit use data-flow names: `actorIndex`,
  `bone`, `key`, `field`, `limb`, `selector`, `offsetMode` (0 joint, 1 full
  length, otherwise half), `screenPosition`, `matrix`, `length`, `position`,
  `delta`. `limb` follows 146C.c, which finds limb i's bone with field 2 and
  stores that bone's field 0 as `limbs[i].nameIndex`.
- Function and global names come from `symbol_addrs.txt` and also name splat's
  generated assembly. The [DataCrystal Symbol Pass](#datacrystal-symbol-pass)
  renamed two that this unit uses: `D_800F4538` is now `vs_battle_actorModels`
  and func_800A0204 is now `vs_battle_animateCharacter`. The unit's own
  functions and its other placeholders, such as `D_800F45E0`, `D_800F49B8`, and
  func_800AA850, keep their names.
- Unchanged because meaning is not yet established:
  armature bytes `unk6`, `unk7`, `unk8`; `movement.unk8` and `movement.unk10`;
  `unk183C`, `unk646`, `unk181A`, `unk34`, `unk5D`; and the `unk0` state
  bits. DataCrystal calls actor byte `0xA` a standing/jumping/falling/climbing
  state, consistent with func_800A25EC storing 3 in its low three bits before
  animation `0x2F`, but individual values are unverified.
- The retired harness still uses the old names. It already references removed
  helpers, so it was not updated.

### Accepted: func_800A2790

- Range: file `0x39F90..0x3A11C`, CPU `0x800A2790..0x800A291C`, 0x18C bytes.
  Sole retail caller: func_800A4E68 at `0x800A50AC`, passing the
  `vs_battle_actorModels` actor pointer when byte `0x1848` is 3.
- Evidence: Ghidra project `build/ghidra/20261004-222441-885` (instructions and
  decompiler output) and splat's disassembly. The checker compared the same
  bytes and resolved both calls: func_800AA850 and
  `vs_battle_animateCharacter` (`0x800A0204`).
- Behavior: multiplies the signed SVECTOR at `0x1858` by the word at `0x1850`,
  divides each wrapped product by `2^24` toward zero, adds the low halfwords to
  the position, stores X/Z tiles (`/128` toward zero), sets bit 21 of the word
  at offset 8, faces toward `0x184E`, then selects animation 1, `0x20`, `0x1F`,
  or `0x22`, adding 6 when the scale is at least 12.
- Bit 21 of offset 8 is header field `unk0.unkA_5`, but the header declares it
  in a byte unit while retail uses a 32-bit read-modify-write. The source uses
  the documented word access (DECOMP_NOTES shape rule 7) without changing the
  shared header. Names `displacement`, `scale`, and `animation` describe
  verified data flow only.

### Accepted: func_800A25EC

- Range: file `0x39DEC..0x39F90`, CPU `0x800A25EC..0x800A2790`, 0x1A4 bytes.
  Retail callers: func_800A4E68 at `0x800A4F78` and `0x800A4FB8`, passing
  the actor pointer when byte `0x1848` is 2.
- Behavior: clears bit 21 at offset 8, queries func_800A6EE8 with the X/Z offset
  at `0x1858/0x185C`, and returns if the signed result is -3000. Otherwise it
  offsets the destination by the position and returns if func_800A8D64 returns
  null. It then commits X/Z. If offset-8 bits 16..18 are set, it calls
  func_800AC690. Otherwise a rise of 64 or more clears `unk34`, starts animation
  `0x2F`, clears `0x181A`, clears bits 14 and 19..20, and stores 3 in bits
  16..18; smaller rises and descents store the height. Tiles update and `0x5D`
  is cleared on every committed move.
- `height` is the signed halfword result used as the new Y. Bits 14, 16..18,
  19..20 and 21 correspond to header fields unk9_6, unkA_0, unkA_3 and unkA_5;
  the original again accesses them as one 32-bit word.

### Accepted Reports

| Function | Report (WSL) | Generated UTC | Snapshot SHA-256 |
| --- | --- | --- | --- |
| func_800A2790 | `~/.cache/vagrant-matching/reports/38C1C-func_800A2790.json` | 2026-10-05T06:55:40Z | `b42dc823abb6c7f5701b1e30ce69d2934005254d67239edbc340b3f25a8ca220` |
| func_800A25EC | `~/.cache/vagrant-matching/reports/38C1C-func_800A25EC.json` | 2026-10-05T06:56:00Z | `b42dc823abb6c7f5701b1e30ce69d2934005254d67239edbc340b3f25a8ca220` |

Both reports: status `MATCH`, mode canonical, zero differences, source SHA-256
`55cdbd92...68da5`, splat manifest `75285ca7...0a08f7`, compiler `2.7.2-cdk`, flags
`-G0 -O2 -Wall -quiet -fno-builtin -funsigned-char -Wno-unused -Wno-uninitialized`,
cpp `-nostdinc -I include/psx -I src/include -I . -D__attribute__(x)= -D_LANGUAGE_C -DOBJDIFF`,
as `-I include -G0 -no-pad-sections`. Reproduce from the workspace root:

```powershell
wsl.exe -d Ubuntu --exec /home/judee/.cache/vagrant-matching/venv/bin/python /mnt/d/Dev/NativePort/VS-NativePort/resources/rood-reverseWin/tools/verification/matching.py check --source src/BATTLE/BATTLE.PRG/38C1C.c --function func_800A2790
```

Reports go stale when the source, any listed dependency, the splat manifest,
the checker, or the toolchain changes. Function scope only: other `INCLUDE_ASM`
bodies are omitted under `OBJDIFF`, so this is neither a unit nor an overlay match.

## Required Approach

- Follow AGENTS.md: meaningful, human-readable C++; inline assembly is not
  acceptable. Use Ghidra before reconstructing each function and check findings
  against actual instructions, data, and callers.
- Iterate on noncanonical candidates outside canonical source with
  `--candidate`; apply only a `CANDIDATE_MATCH` to canonical source, then
  require a fresh canonical `MATCH`. Rerun the controls after checker changes.

## Verified Analysis Setup

- Run from the workspace root:
  `& ./resources/rood-reverseWin/tools/verification/Probe-Ghidra.ps1`.
- Ghidra 12.1.4 remains preferred, but its PSX extension is not active. The
  already-installed user extension for 12.1.2 reports version 12.1.2 and
  supplies `PSX:LE:32:default`. No installation was changed.
- The probe checks the retail SHA-1 before import, uses a fresh ignored project,
  caps automatic analysis at 60 seconds and each decompilation at 10 seconds,
  and terminates only its own process tree after 180 seconds by default. The
  complete-unit/dependency run used the explicit bounded `-Seconds 300` option.
- Input: workspace `data/BATTLE/BATTLE.PRG`; SHA-256
  `ad6914c81be92a008af8dece6ad394ce39c5aa42360542b5531716ebd00bba12`.
  File offset zero maps to CPU `0x80068800`. The imported initialized block is
  `0x80068800..0x800F5923` inclusive; raw import includes bytes that splat labels
  as BSS/junk, so it is not a verified runtime memory image.
- Ghidra adds a separate, uninitialized synthetic `GTEMAC` block at
  `0x20000000..0x20000263`; those addresses are analysis helpers, not game code.
- Compiler specification: `default`, whose calling-convention label is
  `__stdcall`. Actual argument/return usage must be recovered from MIPS
  registers and callers, not inferred from that label or generated signatures.
- Complete-unit project: `build/ghidra/20261004-222441-885/BattleUnit.gpr`
  relative to this working directory. `evidence.txt` records instruction bytes,
  references, and memory metadata; `stdout.txt` is the authoritative saved
  decompiler output. Do not assume the script log contains every decompiler
  line. Outputs remain ignored and do not redistribute the overlay.
- The probe also checks main executable `data/SLUS_010.40` against SHA-1
  `fababcfd4325d42f350d95b3472874affeb0e48c` and maps its file offset `0x800`
  to CPU `0x80010000` in a separate block of the same isolated analysis project.
  Exported function records identify which binary owns each CPU/file address.

## DataCrystal Symbol Pass

Applied 2026-10-05 to BATTLE.PRG. A DataCrystal label was adopted only where
the code agrees with it; contradicted or unverifiable labels left the
placeholder in place. 161 symbols were renamed: 147 script-opcode handlers,
12 engine functions, and 2 data tables. Saved DataCrystal pages were the label
source; every claim was checked against the handler or function body and its
callees (C where the unit has C, otherwise splat's assembly).

### Method

- DataCrystal's script-opcode page matches retail BATTLE.PRG exactly: the 256
  handler words of `_opcodeFunctionTable` at `0x800E9D30` and the 256 length
  bytes at `0x800E9C30` agree entry for entry.
- INITBTL's table at `0x800FAF7C` is the skip table. `vs_battle_runScript`
  switches to its heap copy (`D_800F4C28`) when START skips a scene
  (`D_800F4C2C == 2`). The 39 entries that differ from BATTLE's table point at
  the two shared no-op handlers or `vs_battle_invalidOpcode`.
- Handler return codes, from `vs_battle_runScript`: 0 advances by the opcode
  length, 1 yields, 2 finishes, 3 terminates (clears the script pointer), 4
  advances then yields, and any other value is a jump target.
- A handler takes DataCrystal's label, in this project's
  `vs_battle_script_<lowerCamelCase>` style, only when its body supports it.
  The other 122 handlers are named after their first opcode,
  `vs_battle_script_op0A` through `vs_battle_script_opF7`. That records
  verified table data, not behavior. Existing upstream names were kept.

### Named Handlers

| Opcode | Address | New Name | Evidence |
| --- | --- | --- | --- |
| 0x43, 0x44; most skip entries | `0x800B66E4` | `vs_battle_script_nop` | Returns 0 and reads no operand. |
| Skip table only | `0x800B66EC` | `vs_battle_script_nopYield` | Returns 4: advance, then yield. |
| 0x17 | `0x800B7014` | `vs_battle_script_splashScreenLoad` | Fills a GIM display slot, which func_800C8550 initializes. |
| 0x18 | `0x800B70E8` | `vs_battle_script_splashScreenShow` | Yields while the slot's transition runs; otherwise sets position, scale, and alpha deltas plus a duration. |
| 0x19 | `0x800B71C4` | `vs_battle_script_splashScreenHide` | Calls func_800C86AC, which unloads the GIM display slots. |
| 0x20 | `0x800B7574` | `vs_battle_script_modelLoad` | func_8007C928 allocates and initializes `vs_battle_actors[id]` from the next operand (DataCrystal's SHP id) and four flag bytes. |
| 0x22 | `0x800B76A8` | `vs_battle_script_modelAnimate` | Calls `vs_battle_animateCharacter` with an animation id from `D_800E9B58` plus an operand. |
| 0x26 | `0x800B792C` | `vs_battle_script_modelPosition` | Passes an absolute or relative position and a facing angle to func_800A9988; the operand layout matches DataCrystal's ModelPosition signature. |
| 0x28 | `0x800B7B58` | `vs_battle_script_modelMoveTo` | func_800A9D90 moves the actor by the difference between the destination operand and its current position. |
| 0x2F | `0x800B80B4` | `vs_battle_script_modelFree` | Calls `_removeActorAtIndex` (through func_8007D340) for each selected actor (DataCrystal "ModeFree"). |
| 0x33 | `0x800B8290` | `vs_battle_script_modelRotate` | Applies a relative (func_800A9F64) or absolute (func_800A9FD0) rotation vector over an optional duration. |
| 0x38 | `0x800B8510` | `vs_battle_script_modelLookAt` | func_8009FC60 stores a target actor and turn speed as the actor's look-at target. |
| 0x3B | `0x800B86E8` | `vs_battle_script_waitForFile` | Yields while the FIFO file-load queue `vs_battle_objectDataSlots` (SHP, WEP, SEQ, ETM) holds entries; func_8007CCAC returns its first free index. |
| 0x3E | `0x800B8788` | `vs_battle_script_modelIlluminate` | A nonzero RGB operand (bytes 7-9) makes the renderer draw the model a second time through func_800B3CF0 with that color, offset by bytes 3-5 and depth-biased by byte 6; zero turns it off. The visual result is unverified. |
| 0x58 | `0x800B92D0` | `vs_battle_script_setHeadsUpDisplayMode` | Through func_8007BD8C, sets `D_800EB9AF` (read by the HUD renderer) and `D_800F1CD6` to opposite values chosen by the operand. |
| 0x68 | `0x800B98C0` | `vs_battle_script_loadRoom` | Builds the zone/room door section, starts the load, and yields until `D_800F4BE2` is 2. |
| 0x6D | `0x800B9B18` | `vs_battle_script_displayRoom` | Operand bit pairs switch the GPU display mask (`SetDispMask`), a room flag in `vs_battle_roomData`, and whether every actor is drawn (`unk0.skip`). |
| 0x70 | `0x800B9C58` | `vs_battle_script_modelColor` | Applies CLUT color transforms (`vs_main_transformClutSlot`) with RGB offsets to selected actors. |
| 0x80 | `0x800BA2E0` | `vs_battle_script_playSfx` | Calls `vs_main_playSfx` (DataCrystal "SoundEffects0"). |
| 0x92 | `0x800BA5E8` | `vs_battle_script_playMusic` | Stores its slot operand in `D_800F4C69`, which 0x93-0x96 then use, and passes it to SLUS func_80045000; the callee itself is unverified. |
| 0xC0-0xDB | `0x800BC2E8` | `vs_battle_script_setCamera` | Writes camera state at `D_800F4BA4`; DataCrystal labels its cases CameraDirection, CameraLookAt, CameraPosition, and CameraHeight. |
| 0xE0 | `0x800BB41C` | `vs_battle_script_cameraWait` | Yields until the operand bits are set in func_800BD610's camera-transition completion flags. |
| 0xEA-0xEF | `0x800BDC9C` | `vs_battle_script_cameraZoom` | Sets up the camera's `projectionDistance`, `nearClip`, and `farClip` interpolations (`D_800F4BA4` offsets `0x180..0x1A1`); DataCrystal "CameraZoom", "CameraZoomScalar". |
| 0xF0 | `0x800BB474` | `vs_battle_script_wait` | Counts the operand's frames down in `D_800F4C58`, yielding each frame. |
| 0xFE, 0xFF | `0x800BB604` | `vs_battle_script_return` | Dismisses all eight text boxes, then returns 2 (finish). |

### Rejected Opcode Labels

| Opcode | DataCrystal | Code | Name Used |
| --- | --- | --- | --- |
| 0x1A | SplashScreenFadeIn | Only waits for transitions started by other opcodes. | `op1A` |
| 0x23 | ModelSetAnimations | Sets per-model display flags (skip, visible, freeze) through func_800A087C. | `op23` |
| 0x29 | ModelMoveTo2 | Attaches the actor to a bone of another actor (func_8009FF08 stores the parent and attach point and zeroes the local position) or, for ids from `0x2000`, detaches it (func_8009FFA8). | `op29` |
| 0x30 | ModelLoadAnimationsEx | Waits on actor state flags. | `op30` |
| 0x3A | ModelLoadAnimations | Loads weapon or ETM data into object slots. | `op3A` |
| 0x44 | SetEngineMode | Shared no-op. | `nop` |
| 0x54 | BattleOver | Plays an actor's sound through func_8009DBDC. | `op54` |
| 0x69 | LoadScene | Starts and waits for a screen transition. | `op69` |
| 0xA1 | SplashScreenEffects | Starts MPD block-8 effect entry `operand` through func_800CEF74; no GIM display-slot access. | `opA1` |
| 0xA2 | CameraZoomIn | Waits until the effect state from func_800CF48C equals its operand; no camera field. | `opA2` |

- Upstream names were kept where upstream already named a handler, such as
  `loadGim` (DataCrystal SplashScreenChoose, 0x16) and `loadMusicSlot`
  (MusicLoad, 0x90). Two DataCrystal labels conflict with upstream's
  code-derived names and were rejected: ModelControlViaScript on 0x42
  (`jumpFwd`) and ModelControlViaBattleMode on 0x50 (`setFlagImm`).
- Still unnamed: shared handlers func_800BD444 (0xE2, 0xE3) and func_800BD6C4
  (0xB9, 0xBA, 0xBF, 0xE4-0xE7, 0xED) have no DataCrystal label and serve
  several opcodes.

### Engine And Data Symbols

| Address | Old Name | New Name | Evidence |
| --- | --- | --- | --- |
| `0x800A0204` | func_800A0204 | `vs_battle_animateCharacter` | DataCrystal "AnimateCharacter". Stores the animation id at actor `0x5B8` (`animationId`), then calls `vs_battle_loadAnimation`. |
| `0x800A48CC` | func_800A48CC | `vs_battle_animateCharacterJump` | DataCrystal "JumpAnimateCharacter". Starts animation `0x2F`, which func_800A25EC also starts after a height change of 64 or more, and sets state bits. |
| `0x800AD008` | func_800AD008 | `vs_battle_loadAnimation` | DataCrystal "LoadAnimation". Reads the actor's animation id and resolves its animation header. |
| `0x800BFBB8` | func_800BFBB8 | `vs_battle_runScript` | DataCrystal "Script Opcode Decoder". Runs handlers from `_opcodeFunctionTable` or the skip table and applies the return codes above. |
| `0x800BFE34` | func_800BFE34 | `vs_battle_getOpcodeLength` | No DataCrystal label; returns `_opcodeLengths[opcode]`. |
| `0x800E9C30` | D_800E9C30 | `_opcodeLengths` | DataCrystal "Script Opcode Lengths"; all 256 bytes agree. |
| `0x800C97BC` | func_800C97BC | `vs_battle_execMenu` | DataCrystal "ExecMenuRoutine". Loads the menu overlay with `vs_battle_loadMenuPrg` and runs `vs_mainMenu_exec`. |
| `0x800CA2DC` | func_800CA2DC | `vs_battle_renderHeadsUpDisplay` | DataCrystal "ShowHeadsUpDisplay". Draws HP/MP/RISK bars with `_renderStatBar` and limb status from `vs_battle_getLimbStatus`, and reads `D_800EB9AF`. |
| `0x800CB83C` | func_800CB83C | `vs_battle_renderTextBoxes` | DataCrystal "ShowInformationText" is narrower: the routine draws all eight `vs_battle_textBoxes`, including the information box. |
| `0x800CD3E4` | func_800CD3E4 | `vs_battle_renderDialogBalloon` | DataCrystal "ShowDialogBalloon". Draws the box frame with `vs_battle_addTile` and the UV-flip helper func_800CD3A0. |
| `0x80089DC0` | func_80089DC0 | `_finishLoadMpd` | DataCrystal "LoadMpdFileMain". Waits for the MPD read queued by `_loadMpd` (queuing it if needed), loads the room, loot, cleared/script/door, and enemy sections, and frees the file; mirrors upstream `_finishLoadZnd`. |
| `0x80088554` | func_80088554 | `_tickStats` | DataCrystal "Replenish HP, MP and RISK". Timed HP, MP, RISK, PP, and limb recovery plus knockout detection for every actor. |
| `0x80076784` | func_80076784 | `_applyCharInitData` | DataCrystal "ResetCharacterNewGame" is too narrow: its only caller initializes `vs_battle_actors[id]` for any slot, player or enemy, then calls it to copy name, HP (with random variance), MP, RISK, and stats from `vs_battle_charInitData`. |
| `0x800F4538` | D_800F4538 | `vs_battle_actorModels` | DataCrystal "Actors SHP list pointers?". 17 actor model-record pointers, cleared at battle init and indexed by actor slot; parallels `vs_battle_wepModels` at `0x800F4588`. |

The underscore names follow upstream conventions: the three routines are
referenced only inside 146C.c, and `_opcodeLengths`, like upstream's
`_opcodeFunctionTable`, is read only by BATTLE's script units.

### Rejected RAM-Map Labels

| Address | DataCrystal | Reason |
| --- | --- | --- |
| `0x800CD3A0` | ShowDialogBalloonTail | Only flips UV bytes for the balloon renderer. |
| `0x800CB66C` | ExitMenuRoutine | Returns a pointer to the menu state at `D_800F4E98`. |
| `0x8007980C` | Master Spell Or Break Art Display Yellow Magic Effect | Calls func_8006FA20, which shows the unlock message; the effect was not traced. |
| `0x800784AC` | Launch Attack | Upstream already names it `_handleCombatCameraZoom`. |
| `0x80079030` | Chain Attack Timing Succeeded | Only calls func_8007138C, which camera code also uses. |
| `0x800793C0` | Eventually calls Main Item Drop | Assembly body; claim not traced. |
| `0x800C0990`, `0x800C1DC4` | ShowTargetSphere, ShowTargetBodyPart | Assembly bodies give no conclusive evidence. |
| `0x8006E158`, `0x8007357C`, `0x800735F8` | CameraControl, EnterMenu, ExitMenu | Circumstantial; `0x800735F8` also executes the chosen ability. |
| `0x80078248`, `0x80078748`, `0x800787F0` | Target Sphere Routine, Use Item, Cast Spell | Consistent but circumstantial. |
| `0x80079050`, `0x800790BC`, `0x800797BC`, `0x8007983C` | Chain Attack Timing Failed, Final Chain Attack, Item Drop Screen, Master Spell Or Break Art Displays Message | Consistent but circumstantial. |
| `0x800F1AB0` | Zone ID | Plausible but partial: it is an `_mpdRoomDoorSection_t` whose zone and room ids are copied into the zone context. SLUS references it without a symbol-map entry, so a rename needs cross-overlay plumbing. |

`D_800F45E0`, the parallel object table, has no DataCrystal label and keeps its
placeholder.

### Plumbing And Proof

- [symbol_addrs.txt](config/BATTLE/BATTLE.PRG/symbol_addrs.txt): 122 entries
  renamed (functions marked `// type:func`) and 39 added (35 handlers and 3
  engine functions that had no entry, plus `_opcodeLengths`), so splat's data
  tables and relocations use the new
  names. [exports.txt](config/BATTLE/BATTLE.PRG/exports.txt), the symbols
  INITBTL and the MENU overlays link against, was updated to match.
- Sources, headers, the INITBTL skip table, inline-assembly strings, and the
  native replacements in `native/platform` and `native/tests` use the new
  names. The retired harness keeps the old ones.
- The Windows native build compiles the renamed sources; its stub step still
  stops at the existing unresolved-symbol gate. The 117 renamed symbols in
  that list are exactly the ones the last pre-gate stub file stubbed under
  their old names, so the rename neither added nor resolved native gaps.
- Proof: 291 C units compiled with byte-identical historical cc1 output
  before and after once new names were mapped back to old ones (limits under
  Names); TITLE 22C.c reads no changed file. Regenerated splat
  assembly contains none of the old names. The canonical checks and controls
  above were rerun against the regenerated manifest.

## Historical Analysis Notes

These findings concern BATTLE.PRG, not another overlay at the same CPU addresses.
They are retained research from the rejected attempt, not implemented behavior,
fresh validation, or byte-match proof. Recheck them against instructions and
consumers before using them in a new implementation.

### func_800A1C10

- Range: file `0x39410..0x395E8`, CPU `0x800A1C10..0x800A1DE8`, end exclusive.
  Register arguments are actor slot (`a0`), bone selector (`a1`), output
  halfwords (`a2`), and offset mode (`a3`); `v0` returns zero or minus one.
- Actor lookup uses tables at `0x800F4538` and `0x800F45E0`. With neither
  object present, the routine returns minus one without output stores. The
  primary object's model pointer load at `0x800A1C50` precedes the null branch;
  this incidental original read is not permission to dereference null in C++.
- The call at `0x800A1C88` obtains a matrix from func_800A1DE8. Mode zero copies
  the low 16 bits of translation words at offsets `0x14`, `0x18`, and `0x1C`.
- Nonzero modes select a model armature. A selector with `(selector & 0xF0) ==
  0x40` uses the first weapon-model slot at `0x800F4588 + actorSlot * 8`, loads
  its model pointer at offset `0x1C`, and subtracts `0x3F` from the selector.
  A missing weapon returns minus one, but the matrix call occurs first.
- Mode one uses `lhu` at `0x800A1D0C`, negates that unsigned 16-bit value, and
  stores only its low halfword as the local X offset. Other nonzero modes use
  `lw` at `0x800A1D1C`, wrap negation to 32 bits, divide by two toward zero,
  then retain the low halfword. Ghidra's signed-short guess is not the source
  of truth for the mode-one load. Y and Z offsets are zero.
- CPU `0x800A1D38..0x800A1DA0` loads rotation/translation, executes instruction
  `0x4A480012` (MVMVA, shift 12, rotation matrix, V0, translation, signed IR
  limits), and reads MAC1..3. This is not RTPS/projection, and the result is not
  read from saturated IR registers. Final stores retain each low halfword;
  they do not write the fourth output halfword/padding.
- Consumers verified in both source and original instructions:
  func_800A190C calls at `0x800A1AC4` with an SVECTOR destination;
  func_800A1AF8, func_800A1B28, and func_800A1B9C propagate negative status.

### Rejected Candidate And Leftover Tests

- The removed candidate used native GTE services and was reported to pass
  63 controlled behavior groups. It never passed canonical byte comparison.
- Its source/header hashes are recorded in the historical WSL section below
  solely to distinguish it from the restored baseline.
- [test_battle_position.cpp](tools/verification/test_battle_position.cpp) and
  its retained executable/log are leftovers from that candidate. They must not
  be presented as passing current-source tests or accepted progress.
- The isolated test project must remain inactive until its test/source contract
  is repaired and independently validated. Do not resurrect a removed candidate
  or suppress reference assembly merely to make its tests pass.

### Matrix And Projection Evidence

- func_800A1DE8 spans file `0x395E8..0x3988C`, CPU
  `0x800A1DE8..0x800A208C`, end exclusive. At `0x800A1DEC`, `a2` is overwritten
  with the actor slot from `a0`. The third MATRIX pointer in the existing
  declaration is therefore not a verified input or destination.
- Normal paths copy a selected 32-byte actor/weapon matrix to scratchpad
  `0x1F8003C0`, then use `D_800F49B8` at `0x800F49B8` and matrix routines in the
  main executable. Selector `0xFF` takes a separate construction path. Missing
  actor/object or weapon paths return null. Return address and input/output
  scratchpad fields were examined during the rejected attempt, not validated
  by an active current-source matrix test.
- SLUS CPU `0x80041C68`, file `0x32468`, left-multiplies rotation columns in
  order 2, 1, 0 using rotation-only MVMVA and saturated IR1..3. It is not a
  simple transpose. SLUS `0x80041598`/file `0x31D98` loads rotation;
  `0x80041578`/file `0x31D78` loads translation. SLUS `0x800417EC`/file `0x31FEC`
  transforms a short vector and stores MAC1..3 plus flags. The provider uses
  zero translation and signed low-halfword differences from the view matrix.
- BATTLE `0x800B07DC`, file `0x47FDC`, is an external actor-transform dependency
  still assembly-only in another unit. The provider passes its special anchor
  through scratchpad `0x1F8003B0`, then returns `0x1F8003C0` with sign-extended
  output halfwords as translation. The rejected candidate used a fixture for
  this dependency; no current-source or hardware validation is claimed.
- Projection `0x800A141C..0x800A152C`, file `0x38C1C..0x38D2C`, uses RTPS and
  stores SXY2 and raw SZ3 at output offsets zero and four. It must not call the
  existing quarter-depth storage helper. Rejected-candidate tests are historical
  observations only, not validation of the restored routine.

### Movement And Layout Evidence

- Trajectory planner: BATTLE CPU `0x800A208C..0x800A249C`, file
  `0x3988C..0x39C9C`. Original table loads at CPU `0x800A223C..0x800A2254` and
  `0x800A2338..0x800A2354` use unsigned bytes. Retail file `0x808AC..0x808B2`
  contains six flight offsets `5,5,5,10,12,12`; file `0x808B4..0x808BA` contains
  six height corrections `63,63,63,30,42,42`, end exclusive. These are retained
  research notes; the current routine remains an assembly inclusion.
- Terrain movement: CPU `0x800A25EC..0x800A2790`, file `0x39DEC..0x39F90`.
  Clears bit 21 at actor offset `0x08` even on rejection. Height return is
  truncated to signed 16 bits before comparing minus 3000. X/Z commit only
  after a non-null terrain lookup. The 64-unit rise boundary and active-state
  callback path were exercised only in the rejected candidate's fixtures.
- Scaled movement: CPU `0x800A2790..0x800A291C`, file `0x39F90..0x3A11C`.
  Signed halfword displacement at `0x1858/0x185A/0x185C` multiplies the word at
  `0x1850`; only the wrapped low word is divided by `2^24` toward zero. Position
  stores wrap to halfwords; X/Z tile division truncates toward zero.
- The discarded [3A1A0.h](src/BATTLE/BATTLE.PRG/3A1A0.h) candidate changed the
  `0x5AC` flag word to uniform 32-bit storage to address a Win32 C++ layout
  mismatch. That edit is absent from current source. Its historical assertions
  do not validate the restored header's offsets or ABI.
- DataCrystal's saved actor-data page describes a different linked actor-state
  structure and is not evidence for this render/movement actor layout. The
  verified layout above comes from original loads/stores and consumers.

### Remaining Preconditions

- Metadata getter func_800A1648 is verified only for selectors 0, 1, and 2.
  Other selectors reach a path dependent on incoming MIPS `v1`; the inherited
  C reconstruction has no defined value there. No invented fallback is claimed.
- Bone slots and indices must be valid, and matrix-copy wrappers require a
  non-null matrix result. Some original call paths dereference a null result;
  the focused tests do not claim those invalid calls are safe or repaired.
- The external destination resolver func_800A4A24 has an existing stack-pointer
  lifetime defect in its source outside this unit. Its observed pointer contract
  is controlled in tests; that unrelated dependency was not rewritten.

## Historical Verification Failures

The attempts below used a discarded candidate, not the current restored source.
They produced no accepted match and must not be described as current validation.

### WSL Build Attempt: Failed

- Requested and executed on 2026-10-04 in Ubuntu WSL, using isolated temporary
  snapshots under `/home/judee/.cache/vagrant-matching`, never the root build or
  upstream synchronization. Each check was bounded by `timeout 180`.
- The check selected this unit's configured GCC `2.7.2-cdk`, with flags
  `-G0 -O2 -Wall -quiet -fno-builtin -funsigned-char -Wno-unused -Wno-uninitialized`.
  Its installed compiler SHA-256 was verified against the initialized toolchain:
  `90db2db7bcfe372244ca4d9785a7bfbf7613d711f69ee3bfe6c91d82a85e611d`.
- Initial verifier setup defects were corrected in
  [matching.py](tools/verification/matching.py): exclude unused ambiguous symbols
  without choosing an address, reject any actual ambiguous target/relocation,
  and include `tools/libdata` in the snapshot for the string transformer.
  BATTLE `_nop1` at `0x8006C478` and main-executable `_nop1` at `0x80042734`
  are not interchangeable.
- Canonical attempt:

  ```powershell
  wsl.exe -d Ubuntu -- timeout 180 /home/judee/.cache/vagrant-matching/venv/bin/python /mnt/d/Dev/NativePort/VS-NativePort/resources/rood-reverseWin/tools/verification/matching.py check --source src/BATTLE/BATTLE.PRG/38C1C.c --function func_800A1C10
  ```

  Preprocessing failed at `#include <psx_gte.h>`: the native GTE interface is
  outside the historical PSX include tree. This did not reach historical GCC.
- A second, explicitly noncanonical compatibility probe used
  `VAGRANT_MATCH_NATIVE_HEADERS=1`, copied the actual native header tree into
  the snapshot, and allowed the installed MIPS standard headers. It reached
  historical `cc1`, which exited with code 33:

  ```text
  In function func_800A208C:
  38C1C.c:507: syntax error before int
  horizontalX undeclared
  horizontalZ undeclared
  ```

  The reported declarations follow statements, which this historical C
  compiler rejects. Modern-header/line-directive warnings were also emitted.
  No game source was changed to work around the failure.
- Neither attempt produced the target object, a relocation comparison, or a
  MATCH/DIFF report. The checker compiles the whole unit even when selecting a
  single function, so this failure blocks every selected function in the unit.
  Native service calls also lack established retail relocation targets;
  arbitrary addresses or replacement assembly must not be supplied to make
  the check appear to pass.
- The probe cannot certify canonical matching: any eventual result from its
  noncanonical headers is labeled `PROBE_MATCH`/`PROBE_DIFF`, never `MATCH`.
  Ghidra interpretation and focused Windows tests do not establish byte identity.
- Game-source preservation hashes for this attempt:
  [38C1C.c](src/BATTLE/BATTLE.PRG/38C1C.c), SHA-256
  `8f291aec7e3bcc9c7e47d20b2cb900118cd2c7b5f1e9711cc231fff251d3f4c8`;
  [3A1A0.h](src/BATTLE/BATTLE.PRG/3A1A0.h), SHA-256
  `8805e14d5b7cf15f41e35fd3aac7c63aad3324fdb61ac39650e859d3abe0780f`.

### Other Limits

- Reconstruction is not a byte-match claim. Historical Windows GCC is absent;
  modern MIPS GCC can check syntax/ABI but cannot establish original matching.
- WSL-only compiler setup was authorized and completed on 2026-10-04: old-gcc
  0.17 archives for 2.7.2-psx, 2.7.2-cdk, and 2.8.1-psx passed published SHA-256
  checks and compiler smoke tests in `/home/judee/.cache/vagrant-matching`.
  The checker was later validated by the 2026-10-05 controls recorded above.
- Build verification is read-only: it reports manifest/source/revision
  differences as warnings and never invokes synchronization. Explicit sync
  refuses modified, added, deleted, or unmanifested populated source.
- The upstream manifest and historical root progress.json remain reference
  provenance, not new Windows matching results.
