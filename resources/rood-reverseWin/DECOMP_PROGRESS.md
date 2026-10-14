# Local Decompilation Tracking

## Combined Loader Search: 2026-10-14

The user requested combinations of partial fixes for the four remaining loader
words. Ran **192 complete-function source combinations** in a hash-validated
isolated snapshot using unchanged expected settings, SDK preprocessing, retail
bytes, callback arguments, public ABI, and function range. No canonical source
was written. **No combination reduced the four-word gap without regressions.**

- Crossed four cursor representations (committed unsigned accumulator, separate
  word/byte accumulator, byte pointer, ID-table pointer), three accepted-result
  forms (word, byte, direct constant), eight shared-publication qualification
  patterns, and two equivalent rejection layouts. The existing count reads and
  receiver publication ordering were preserved. Every candidate received the
  normal complete instruction/relocation check, not a heuristic score.
- Outcomes: **24 cases had four differences, 24 had five, 72 had 21, and 72 had
  22**, all 212 bytes long. Pointer/separate-cursor forms fixed some early
  original offsets but introduced later register/order differences and were
  rejected. A valid lead had to be a strict subset of `+0x44/+0x50/+0x54/+0x58`
  with no new differing offsets. None met that condition.
- Additional reviewed command-state capture, callback-result provenance, and
  immutable count-snapshot combinations emitted the same four differences;
  ineffective forms were removed. Inputs, logs, complete reports, and aggregate
  results remain in ignored `build/pad-combinations-0cueng2l` and its isolated
  WSL cache counterpart. There was no empty loop, instruction padding, modified
  primitive, fixed-register binding, compiler change, masked byte, or partial
  acceptance.
- Restored and reverified the prior exact checkpoint. Final result remains
  **212/212 bytes, four words at `+0x44`, `+0x50`, `+0x54`, `+0x58`**; the
  earlier `+0x88` fix remains intact. Source SHA-256:
  `69e7ee771c8ce72897ccb58f29dcef590197acbe90e931354d597c05baf9563c`.
  [Current normal report](build/closure-pass-validation/PADCMD-_padLoadActInfo.json).
  Positive getter and deliberate-negative controls passed before experiments.
  The requested further gap closure is unresolved; no function or percentage
  increase was accepted.

## Next-Word Attempt: 2026-10-14

Attempted the next `_padLoadActInfo` difference, requiring a strict reduction
from the verified four-word checkpoint with no new mismatching offsets.
**No further word was resolved in this pass.** The previous one-word gain is
preserved exactly; the working source and four-word checkpoint still share
SHA-256 `69e7ee771c8ce72897ccb58f29dcef590197acbe90e931354d597c05baf9563c`.

- Fresh positive `_padGetActSize` matches 56 bytes; its deliberate mutation
  remains rejected only at `+0xC`.
- Typed byte/ID-table cursors, separate signed or constant word-address views,
  and equivalent compound-assignment direction fixed some early original words
  but introduced later register/order differences. Those regressions were
  rejected, not counted as the next-word gate.
- Independent arithmetic placement, split padding/shift operations, pointer
  addition, byte count snapshots, callback-status dispatch, accepted-value
  coupling, parameter/register hints, and five previously untested publication
  qualification combinations retained the same four differences. No-effect and
  regressing forms were removed. The five complete isolated qualification
  reports remain in WSL `pad-publications-y_xh6emp`.
- The protected original and imported fork both retain the same loader assembly
  placeholder; neither supplied a missing C implementation. They were read only.
- Final fresh normal result remains **212/212 bytes, differences only at
  `+0x44`, `+0x50`, `+0x54`, `+0x58`**, with original `+0x88` still matched.
  [Full current report](build/closure-pass-validation/PADCMD-_padLoadActInfo.json).
  Canonical source, ABI, expected toolchain, comparison range, and previous
  percentage measurements remain unchanged. The next-word request remains
  unresolved; the whole function is still `NOT ACCEPTED`.

## One-Word Completion Gate Met: 2026-10-14

The user set the immediate gate to resolving at least one of the five remaining
`_padLoadActInfo` instruction differences. **That gate is met** in the isolated
candidate, without adding any new mismatching offsets or changing compiler
settings, expected bytes, comparison range, public interfaces, or primitive
bodies. This is not whole-function acceptance or a percentage increase.

- Fixed original offset **`+0x88`**, CPU **`0x8002CE64`**. Retail and compiled
  code now both contain **`0x00052880`, `sll a1,a1,2`** there, rather than the
  misplaced success-constant load. Both complete original-settings checks
  produce **212/212 bytes with four differences**, strictly reduced from five.
- Remaining mismatches are exactly **`+0x44`, `+0x50`, `+0x54`, `+0x58`**.
  Every other original instruction remains matched. Canonical input hashes and
  timestamps remain unchanged. The positive 56-byte getter and its known
  negative mutation were checked before source changes.
- The source improvement couples the byte-buffer address to the committed
  ID-table pointer value and preserves the observed shared-access ordering:
  ID count read, receiver callback publication, actuator count read. Only those
  shared count reads and receiver-pointer write use explicit volatile views;
  there are no added memory accesses or instructions. The existing types,
  callbacks, complete initialization, guards, and return values are retained.
- Forty-five equivalent placements of independent local arithmetic were checked
  across the complete function in an isolated Linux snapshot; all retained the
  original five differences. Broader volatility and pointer-only alternatives
  introduced regressions and were not retained. No score-only improvement,
  empty loop, instruction padding, forced register, alternative accepted recipe,
  missing callback argument, or narrowed comparison was used.
- Current source SHA-256:
  `69e7ee771c8ce72897ccb58f29dcef590197acbe90e931354d597c05baf9563c`.
  [Verified source checkpoint](build/pad-storage-four-words.c) and
  [full fresh report](build/closure-pass-validation/PADCMD-_padLoadActInfo.json)
  retain the result. The working candidate contains the same source.
- Canonical `PADCMD.c` still retains the original function until the other four
  words match. No full reconstructed-function count, coverage percentage,
  native behavior, or gameplay completion is claimed for this one-word gate.

## Loader Compiler-Stage Evidence: 2026-10-14

Inspected the existing `_padLoadActInfo` candidate through raw compiler output,
RTL, first/second scheduling, allocation, and delay-slot filling rather than
repeating source permutations. Canonical implementations and expected recipes
remain unchanged; no new function was accepted.

- Bounded capture `pad-schedule-2486sb7d` retains the original raw compiler
  output and actual assembler input. Dump-enabled output differs only in the
  compiler-generated argument comment listing `-da -dumpbase`; after checking
  precisely that comment, all instruction text is identical. The verifier still
  receives the original normal compile, unmodified. Early capture attempts
  refused the comment difference; an assembler-version-query hook error was
  repaired before successful capture. No instruction difference was masked.
- The first scheduling pass moves the independent address-alignment chain to
  the front of the accepted block. The result has separate lowered definitions
  for the command-state constant and the returned constant. Delay-slot output
  places the storage-address increment at the callback branch, while retail
  places the returned one there. The raw compiler already contains the mismatch;
  PsyQ preprocessing is not introducing it.
- Explicit shared-outcome flows required an extra saved register or changed
  branch layout (220/216-byte variants versus the 212-byte target). A byte-valued
  return ABI probe, scoped busy-status temporary, and result derived from the
  command-state assignment did not close the five-word mismatch. Failed probes
  were removed; the public return remains `int` and the callback remains fully
  evaluated with its required port argument.
- A neighboring-SDK provenance hypothesis was checked strictly as a separate
  diagnostic: the already-used CDK 2.7.2 / `-mmips-as` recipe still yields five
  differences. That comparison does not authorize changing the expected recipe,
  and no canonical configuration was changed. Evidence is retained in ignored
  `build/pad-provenance-x2naydem`. A fresh direct normal check afterward confirms
  the original configured PSX compiler/settings and complete retail range.
- The retained candidate now stores aligned address bits as `u_int`, with an
  explicit wrapped unsigned addition, original signed right shift, and unsigned
  left shift. This preserves the 32-bit MIPS operations without relying on
  signed overflow or left-shifting a negative value; it does not change emitted
  historical instructions or constitute accepted native behavior validation.
- Final normal result remains **`CANDIDATE_DIFF`, 212/212 bytes, five differing
  instruction words** at `+0x44`, `+0x50`, `+0x54`, `+0x58`, `+0x88`.
  [Current complete report](build/closure-pass-validation/PADCMD-_padLoadActInfo.json).
  Positive `_padGetActSize` remains `MATCH`, 56 bytes; its deliberate mutation
  is rejected only at `+0xC`. No candidate was promoted, and last measured BATTLE
  **98.42288%** / overall **97.73727%** coverage remains unchanged.

## Loader Scheduling Diagnosis: 2026-10-13

Continued the same five-word `_padLoadActInfo` mismatch. Canonical source,
interfaces, expected compiler settings, SDK primitives, and retail comparison
ranges remain untouched. No function or completion increase was accepted.

- Fresh positive `_padGetActSize` matches 56/56 bytes; its deliberate rounding
  mutation remains rejected only at `+0xC`.
- A byte-sized accepted status, callback-status switch, meaningful single-pass
  rejection block, and ordinary unbound register hint did not close the gap.
  All exploratory forms were removed. The retained candidate is again ordinary
  structured validation followed by the verified initializer operations.
- Tested four compiler-pass variants strictly as **nonacceptance diagnostics**,
  not new expected settings. Full comparisons produced: first scheduling
  disabled, 212 bytes / 29 differing words; second scheduling disabled, 212 / 30;
  delay scheduling disabled, 216 / 38; post-loop CSE disabled, 212 / five.
  None matched. This does not prove every possible source reconstruction or
  compiler hypothesis impossible; it rules out these specific substitutes.
  Saved reports/disassemblies are under ignored `build/pad-codegen-xwbik4ip`
  and the corresponding isolated WSL cache folder.
- The diagnostic wrapper exited while starting its normal-settings restoration;
  no restoration success was inferred. Reestablished the original configured
  check directly, then repeated it after removing the final source hint.
  The final current report records the original flags and all 212 retail bytes.
  [Normal current report](build/closure-pass-validation/PADCMD-_padLoadActInfo.json).
- Final result remains **`CANDIDATE_DIFF`, 212/212 bytes, five instruction-word
  differences** at `+0x44`, `+0x50`, `+0x54`, `+0x58`, `+0x88`. The existing
  candidate hash remains
  `e046cabdfbadffcbfa29b830f696f9635af2abb6dbf07804f29987ac5c6c8e56`.
  It is **`NOT ACCEPTED` / scheduling `BLOCKED`** for this pass. Last measured
  BATTLE matched code remains **98.42288%**, overall **97.73727%**. No native,
  gameplay, whole-source completion, or diagnostic-only match is claimed.

## Actuator Loader Guard Refinement: 2026-10-13

Continued only `_padLoadActInfo` in the isolated current-source candidate.
The exact historical compiler, SDK preprocessing, headers, callback arguments,
retail range, and complete-byte acceptance gate remain unchanged.

- Fresh `_padGetActSize` positive control is `MATCH`, 56/56 bytes; the deliberate
  size-rounding mutation remains rejected at `+0xC` only.
- Replaced the explicit rejection label with structured short-circuit input
  validation. This removes the redundant callback argument move while preserving
  the required callback argument, pointer initialization, guard order, alignment,
  buffer partitions, state updates, and return values.
- Current [candidate](build/pad-storage-candidate.c) is **212/212 bytes with
  five differing instruction words**, at `+0x44`, `+0x50`, `+0x54`, `+0x58`,
  and `+0x88`. The remaining difference is the success constant versus alignment
  arithmetic scheduling. Candidate SHA-256:
  `e046cabdfbadffcbfa29b830f696f9635af2abb6dbf07804f29987ac5c6c8e56`.
  [Complete current report](build/closure-pass-validation/PADCMD-_padLoadActInfo.json).
  Equal length and one fewer difference are not accepted function progress.
- A finite, function-scoped existing-permuter search completed within its hard
  limits: one worker, at most 1,200 seeds, 180 seconds. Full-unit compilation
  used the actual `psyq-reorder` path and immutable compiler/input hashes; the
  generated target was checked for assembly before compilation. Both generated
  improvements were rejected: one inserted an unmotivated empty loop, and the
  other left the port pointer uninitialized and altered a callback declaration.
  Neither was copied into the implementation or treated as acceptance evidence.
  Inputs/logs/diffs remain in ignored `build/pad-storage-permuter-7hxpiuif` and
  the same isolated WSL cache folder.
- Normalized callback-result, raw input-alias, split-address, and nested-block
  probes did not close the gap; ineffective forms were removed. No synthetic
  no-op loop, forced register, omitted callback argument, modified primitive,
  altered compiler setting, or ignored instruction was introduced.
- The function remains **`NOT ACCEPTED` / scheduling `BLOCKED`** in this pass.
  Canonical source and interfaces are untouched; last measured BATTLE coverage
  remains **98.42288%** and overall matched code **97.73727%**. No native or
  gameplay validation is claimed. The previously verified complete retail
  binaries do not turn this candidate into an accepted reconstruction.

## Function Closure Pass: 2026-10-13

The user requested prioritizing actual function closures and completion gain
over further actor-transform scoring. Investigated smaller remaining script and
main/SDK functions one at a time, preserving each failed slice as unaccepted.
**No new function passed canonical acceptance in this pass.** No percentage gain
is claimed; last measured BATTLE coverage remains **98.42288%**, and overall
matched code remains **97.73727%**. Compiler success, equal size, diagnostic
scores, and complete binaries retaining original assembly were not counted as
new reconstructed progress.

- `func_800B9DB4`: current-source script candidate reaches 368/372 bytes but
  still differs in early return and value scheduling. Fresh adjacent positive
  control is 64-byte `MATCH`; the deliberate argument mutation is rejected
  exactly at `+0x20`. Queried-word helper interfaces are already correct.
  A finite full-context permuter run exposed an important diagnostic issue:
  its default scorer compared the entire compiled unit. The temporary runner
  was corrected to disassemble only this function, retaining complete-unit
  compilation and the unchanged normal acceptance checker. Corrected baseline
  passed; a one-worker, at-most-1,200-seed, 180-second search ended at its limit
  with one scored improvement (761), not a match. A named paired-animation base
  was reviewed and tested normally; the function remains `NOT ACCEPTED`.
  Inputs/logs remain in WSL `script-permuter-txyijd9z`; the earlier unscoped run
  `script-permuter-_6jhicaj` is diagnostic history, not usable acceptance evidence.
- SDK GTE leaves were inspected against actual instructions and unmodified
  primitive bodies. `SetGeomOffset` uses argument registers while its existing
  primitive uses different temporaries; matrix setters also have different
  load/transfer scheduling. Initialization includes COP0 setup, and the
  projection leaf has a different hazard interval. None was replaced merely
  to raise a percentage or by changing primitive bodies.
- `func_8001FA68`: complete SDK interrupt initializer candidate preserves
  initialization guards, volatile mask/status accesses, context save and stack
  slot setup, callback installation, and critical-section exit. Current result
  is 220/216 bytes; the final callback/base conversion remains nonmatching.
  `SetIntrMask` positive control matches 24 bytes, and an isolated bit mutation
  is rejected. Original instructions and fresh hash-verified Ghidra exports
  establish CPU `0x8001FA68..0x8001FB40`, file `0x10268..0x10340`.
- `_padLoadActInfo`: complete actuator-storage loader candidate is 212/212
  bytes with **six differing instruction words** (`+0x3C`, `+0x44`, `+0x50`,
  `+0x54`, `+0x58`, `+0x88`). Buffer alignment/partitions and callback/state
  updates are reconstructed with existing `PadPort` types; callback/result
  scheduling still differs. Positive `_padGetActSize` matches 56 bytes; its
  deliberate rounding mutation is rejected at `+0xC`. Setter candidates
  `_padSetMainMode` (140/152) and `_padSetVsyncParam` (52/44) remain unaccepted.
  Fresh Ghidra projects include `20261013-213458-829`, `20261013-220026-597`,
  and `20261013-221319-929`. No speculative SDK ABI form was promoted.
- Fixed a verified checker defect for main executable configurations whose
  first segment is a header list, not a code dictionary. The configured code
  segment is now selected explicitly; unnamed main code uses Splat's actual
  existing identifier `800`, while named BATTLE ownership remains unchanged.
  Six actual-configuration/fail-closed regressions pass. No address, expected
  instruction, toolchain setting, or comparison mask was changed.
- Annotated the existing `_padLoadActInfo = 0x8002CDDC` symbol with `type:func`.
  The prior control range erroneously included this next function. Actual
  linked instructions establish the getter's `JR` at `0x8002CDD4`, delay slot
  at `0x8002CDD8`, and next prologue at `0x8002CDDC`, independently of candidate
  length. Regenerated isolated Windows main and BATTLE split artifacts;
  stale artifacts were refused rather than their guards being suppressed.
- Current checker controls pass for main SDK and BATTLE; BATTLE's known
  negative remains rejected only at `+0xD8`. A bounded full historical rebuild
  verifies the complete 337,920-byte main executable (SHA-256
  `51dfdf15a58f7de8505742b5050de2951b133ed5ce801b08bff7f60b76e5d1bf`)
  and 577,828-byte BATTLE overlay (SHA-256
  `ad6914c81be92a008af8dece6ad394ce39c5aa42360542b5531716ebd00bba12`)
  remain byte-exact. The only changed staged maintained input was the verified
  symbol-type annotation. Canonical game implementations remain unchanged.
  [Regression and candidate reports](build/closure-pass-validation/validation.json).
  No native/gameplay validation or source-complete milestone is claimed.

## Historical Allocator Evidence: 2026-10-13

Continued the same unaccepted `func_800B28A8` candidate; canonical game source,
shared declarations, SDK primitive bodies, expected settings, and retail ranges
remain unchanged. Fresh positive and deliberate-negative controls pass.

- Separated the root-scale cache pointer lifetime from the per-bone cache
  traversal. The root cache now uses the original temporary-address flow.
  Restored the scratch-relative MAC translation destination and branch-local
  parent-scale lookups, and recovered header-before-traversal local-object
  ordering from the observed stack accesses. These are candidate refinements,
  not accepted function progress.
- Captured the exact historical compiler's RTL, liveness, local/global
  allocation, scheduling, and delay-slot dumps in isolated cache
  `actor-allocator-57b7xmek`, retained under the same ignored Windows build
  folder. A normal compile ran first; a separate dump-enabled compile was
  required to produce **byte-for-byte identical assembly**. Only the normal
  assembly was returned to the unchanged verifier. Dump flags are diagnostic
  only and are not new expected settings or acceptance evidence.
- The allocation dump shows the working scalar (pseudo-register 96) assigned
  hard register 18 (`s2`), conflicting with hard register 16 (`s0`). Several
  phase-disjoint pointer values, including saved-matrix/orientation views,
  occupy `s0`; the saved selector remains live across matrix composition.
  Retail instead uses `s0` for that scalar and recomputes copy-address
  temporaries. This explains a concrete allocation mismatch rather than
  attributing every difference to missing hardware or an unexplained score.
- An explicit complete matrix-copy intrinsic produced 4,460 bytes and did
  not solve the conflict; it was removed. Ordinary declaration-order changes
  also had no effect and were removed. No fixed-register assignment, new
  assembly, SDK-body change, ignored relocation, or comparison relaxation was
  introduced. A focused search found no named matrix-copy operation in the
  provided source/header/include files.
- Final normal result remains **`CANDIDATE_DIFF`, 4,268/4,292 bytes**, with
  **808 differing instruction words and ten differing table words**. Current
  candidate SHA-256:
  `b25d0b380a356b1cb4fc835997442339042623864a05f5b821e65695e2510db0`.
  [Full current report](build/actor-transform-diagnostic/49268-func_800B28A8.json).
  The 24-byte length gap does not describe the extent of remaining differences.
  The function remains **`NOT ACCEPTED`**, with an identified allocation and
  scheduling blocker, not a proof that all possible C reconstructions are
  impossible. Last measured accepted BATTLE coverage remains **98.42288%**;
  no native/gameplay validation or new canonical function is claimed.

## Actor Transform Layout Refinement: 2026-10-13

Continued only `func_800B28A8` in the ignored `49268.c` candidate. No canonical
game implementation, shared interface, primitive body, verifier setting, retail
range, or accepted coverage changed during this continuation.

- Fresh controls pass: canonical `func_800B26B8` is `MATCH`, 496/496 bytes;
  the deliberate saved-selector mutation is rejected at `+0xD8` only.
- Recovered explicit bone-loop continuation and independent cursor lifetimes,
  a retained parent-bone matrix pointer, the verified orientation target view,
  a word-aligned eight-byte actor-header snapshot, and the observed coordinate,
  color, and oscillation-clamp update order. A single untyped actor base with
  explicit field views removes an unintended duplicate argument spill.
- Enumerated every original stack access: the retail frame is `0x58` bytes,
  with no accesses to `sp+0x10..0x17`; header snapshot words occupy `+0x18/+0x1C`,
  and bone index, actor matrix offset, and world destination occupy `+0x20`,
  `+0x24`, and `+0x28`. The already-matching caller `func_800AEF94` establishes
  the repository's explicit unused-local-frame convention. Applying that
  convention for the verified eight-byte region restores this frame without
  adding instructions. The final single-base candidate reproduces the complete
  prologue; its first differing instruction is `+0x1B4` / CPU `0x800B2A5C`.
  This regional agreement is not partial acceptance or newly matched coverage.
- The current complete candidate is **`CANDIDATE_DIFF`, 4,260/4,292 bytes**:
  **833 instruction-word differences and ten read-only table-word differences**,
  checked through the complete original end at `+0x10C0`. It is not merely a
  32-byte discrepancy. Candidate SHA-256:
  `f42136d52bf72a4cd38a65e47126db52e61e8a59e50e7e169461fd8229fd6df3`.
  [Full report](build/actor-transform-diagnostic/49268-func_800B28A8.json).
- A separate retained
  [checkpoint](build/actor-transform-table-match.c) produces all 40 retail
  table bytes exactly but adds an unwanted argument spill and still differs
  in its 4,276-byte body. Its fresh
  [checkpoint report](build/actor-transform-diagnostic/table-match-checkpoint.json)
  remains `CANDIDATE_DIFF`; table agreement is not function acceptance.
- Ran one bounded existing-permuter search: at most 600 seeds, one worker,
  120-second search limit, hard outer/child limits. The compiler wrapper restores
  the generated body into the full current preprocessed unit and rejects any
  changed existing SDK assembly-node sequence. Two reference-setup failures
  were repaired with the recorded GTE macro context and removal of duplicate
  label macros; no candidate search ran during those failed setups.
  The valid baseline passed; the search reached its limit and returned four
  diagnostic improvements. Opaque pointer arrays and artificial constant
  temporaries were rejected rather than installed. A readable mutually
  exclusive record-cursor simplification was tested normally and did not
  produce a match. Logs/results remain in
  `build/actor-transform-diagnostic` and WSL cache `actor-permuter-nxlmr_qf`.
- The function remains **`NOT ACCEPTED` / code-generation `BLOCKED`** in this
  refinement pass. No fixed-register bindings, new assembly, manufactured
  instruction padding, altered flags, masked differences, or narrowed ranges
  were used. Canonical `49268.c` retains the original implementation. Last
  measured accepted BATTLE coverage remains **98.42288%**, five routines /
  8,160 bytes outstanding. No native or gameplay validation is claimed.

## Approved GTE Reconstruction Attempt: 2026-10-13

The user explicitly approved existing SDK GTE primitives in the PSX-only
matching backend. This supersedes earlier same-day entries stating that no
exception was approved; those entries remain below as historical evidence.
The exception is recorded additively in AGENTS.md and DecompRules_Resources.md.
No primitive body, expected compiler setting, retail byte, or acceptance range
was changed. Windows-native operations must still use C/C++ services.

- Positive control `func_800B26B8` remains `MATCH`, 496/496 bytes. Its deliberate
  saved-selector mutation remains `CANDIDATE_DIFF` at `+0xD8` only. Repeated both
  controls after the verifier correction described below.
- The unmodified legacy SDK spelling `gte_rtv0tr()` left marker word
  `0x0000027F` in the configured assembler pipeline. The already-existing
  symbolic compatibility binding `gte_rtv0tr2()` instead emitted actual
  `0x4A480012`, with the observed rotation/translation loads, delay NOPs, and
  MAC1..3 stores. This is the existing binding of the same primitive, not a
  newly introduced wrapper or opcode payload. Retained the
  [legacy diagnostic](build/actor-transform-diagnostic/sdk-legacy-result.json)
  and [symbolic diagnostic](build/actor-transform-diagnostic/sdk-symbolic-result.json).
  The historical hardware-lowering/policy blocker is resolved for this target.
- Reconstructed the complete `func_800B28A8` candidate in ignored
  [actor-transform-candidate.c](build/actor-transform-candidate.c): root and
  bone transforms, saved-bone and parent-bone paths, orientation cases 0..9,
  scale propagation, clipping, texture selection, color adjustment, render
  dispatch, and temporary header restoration. Ghidra and original instructions
  establish CPU `0x800B28A8..0x800B396C`, file `0x4A0A8..0x4B16C`, 4,292 bytes.
  The original implementation remains in canonical source.
- The complete comparison initially exposed a verifier lookup defect: YAML
  parses owner `49268` as an integer, while the source stem is a string.
  Normalized only that identifier in `retail_readonly_section`; the configured
  file range `0xDF4..0xE1C`, CPU `0x800695F4`, and all 40 table bytes remain
  mandatory. Three actual-config regressions pass for numeric, string, and
  unknown owners. No table bytes, relocations, or differences were ignored.
- Corrected the inconsistent third-argument declaration in
  [44F14.h](src/BATTLE/BATTLE.PRG/44F14.h): `char` to `int`. Retail explicitly
  saves `a2` with `SW` at `0x800B28E0` and reloads the word at `0x800B2A30`,
  `0x800B3530`, and `0x800B38FC`; `3A1A0.c` already declared the word argument.
  All existing render call sites remain byte-exact: `func_800AEF94` is
  1,876/1,876 bytes and `func_800A4828` is 164/164 bytes.
- Latest complete candidate result is **`CANDIDATE_DIFF`, 4,316/4,292 bytes**,
  with eight differing words in the complete ten-entry table. The remaining
  mismatch is ordinary-C code/layout work, not an unresolved GTE expansion.
  An ordinary unbound `register` hint made no difference and was removed.
  The [latest report](build/actor-transform-diagnostic/49268-func_800B28A8.json)
  is diagnostic evidence only. The function is **`NOT ACCEPTED`**.
- Refreshed eleven canonical function matches, including both callers, all
  three existing `49268.c` routines, the accepted dispatcher/tile functions,
  and their previously corrected helpers/callers. A first combined regression
  runner exited early after six matches; no completion was inferred. Finished
  the remaining checks separately and reran the rebuild with hash-validated
  current reports. The final
  [canonical regression](build/actor-transform-validation/validation.json)
  verifies complete linked `49268` text (7,940 bytes) and rodata (40 bytes),
  complete `146C` text (159,680 bytes) and rodata (1,716 bytes), and the full
  577,828-byte BATTLE overlay. Retail SHA-256 remains
  `ad6914c81be92a008af8dece6ad394ce39c5aa42360542b5531716ebd00bba12`.
  Unreconstructed original assembly still contributes to that reproduction;
  it is not new readable-source progress.
- No new reconstructed function was accepted. The last measured accepted
  BATTLE coverage remains **98.42288%**, five routines / 8,160 bytes outstanding.
  No Windows-native or gameplay test is claimed. The inactive CMake preset and
  pending native `gte_SetTransMatrix` binding remain separate validation work;
  the nonmatching candidate was not installed into native generated inputs.

## GTE Capability Diagnosis: 2026-10-13

The blocker is a historical-code-generation and acceptance-policy boundary,
not missing Windows hardware or an absent implementation of the required
transform operation. No policy exception has been approved.

- `49268.c` uses configured GCC `2.7.2-cdk`, compiler SHA-256
  `90db2db7bcfe372244ca4d9785a7bfbf7613d711f69ee3bfe6c91d82a85e611d`,
  with the existing `-G0 -O2 -fno-builtin -funsigned-char` settings.
  A bounded isolated C-interface probe compiled with those exact settings
  emits `JAL SetRotMatrix`, `JAL SetTransMatrix`, and `JAL RotTrans`, with
  no COP2 instructions. Retained
  [capability result](build/gte-c-api-capability.json) and
  [compiler output](build/gte-c-api-probe.s). This output is diagnostic
  evidence, not an assembly implementation or decompilation match.
- Inspected the pinned old-gcc 0.17 CDK build recipe and its PSX/endian patches.
  They configure the PSX/R3000 target and build compatibility; the inspected
  patches do not add a GTE builtin interface. The recipe references
  `https://github.com/decompals/old-gcc/releases/download/0.14/b18.tar.gz`.
  Downloaded that source archive into the isolated cache under size/time
  limits, actual SHA-256
  `de4cba44576a4de3b6f750a5a3559fe9e91de1a75ccdf02d41bcf71f76114144`.
  Extracted only `c-decl.c` and the three MIPS backend files.
- The inspected machine description/output routines and C builtin registry
  expose no COP2/GTE lowering path. The output routines visibly support
  COP1 transfers, providing a positive inspection control. These findings,
  the actual compiler probe, and the existing SDK `__asm__ volatile` macros
  provide the current-toolchain basis for the blocker; absence in a binary
  string search alone was not treated as proof.
- Enumerated actual GTE commands in `func_800B28A8`: three instances of
  `MVMVA(1,0,0,0,0)` at CPU `0x800B2960`, `0x800B2DEC`, and `0x800B2F38`.
  The existing Windows C adapter's `psx_inline_transform` invokes that exact
  selector tuple; its rotation/translation/vector loaders and MAC1..3 stores
  cover the observed operation boundary. This does not certify the complete
  routine or gameplay and is not new matching progress.
- Attempted to activate the focused native test through CMake Tools. Target
  and test discovery were empty; the build was refused because no configure
  preset is active. Retrieved diagnostics afterward. The workspace defines
  `windows` and `windows-debug`, but their presence does not imply selection.
  No native test was claimed as run, and no unvalidated test edit was made.
- Requested explicit approval for existing SDK GTE primitives only in a
  PSX matching backend, keeping higher-level game logic readable C and the
  Windows hardware backend C/C++. Also requested native preset selection.
  The response directed autonomous work but did not explicitly approve the
  rule exception or select presets. Therefore no assembly expansion,
  native-only reconstruction, altered toolchain, or relaxed matching gate
  was introduced.
- The viable next implementation decision is a narrowly approved PSX
  hardware-binding exception, or separately approved native-only behavioral
  work. Under unchanged strict C-only matching rules this routine remains
  `BLOCKED`; ordinary C library calls cannot substitute for the embedded
  COP2 stream. Accepted BATTLE coverage remains **98.42288%**, with five
  routines / 8,160 bytes outstanding. Canonical game source is unchanged.

## Transform Hardware Boundary: 2026-10-13

- Rechecked the blocked resolver's return hypothesis against its complete
  original body and the actual caller at `0x800D6388..0x800D63D0`. Retail
  calls `func_8006CB04` at `0x800D63B8` and immediately restores the frame;
  the caller does not consume `v0`. The resolver's exit does not copy its
  effect-applied flag into `v0`. Together with the maintained void declaration,
  this provides no basis to change the return contract merely to preserve
  that otherwise-unused flag. No resolver or caller source was changed.
- Requested a one-time move to the remaining unattempted routine while
  preserving the resolver as blocked. The response reported that the user
  was unavailable and directed autonomous work. Investigated
  `func_800B28A8` in [49268.c](src/BATTLE/BATTLE.PRG/49268.c), without rewriting
  acceptance requirements or introducing a new implementation.
- Fresh hash-verified Windows Ghidra project:
  `build/ghidra/20261013-013857-071`. Original range: CPU
  `0x800B28A8..0x800B396C` / file `0x4A0A8..0x4B16C`, 4,292 bytes.
  The actor-update consumer supplies a camera or parent-bone matrix and
  uses the resulting actor/bone transform data. The third argument is
  retained by retail and callers; Ghidra's two-parameter guess is not treated
  as the complete interface.
- Verified the opening hardware sequence directly: rotation/translation
  `CTC2` writes at `0x800B2900..0x800B2934`, vector `LWC2` reads from
  scratchpad `0x1F8003A8` at `0x800B2950..0x800B2954`, `MVMVA` word
  `0x4A480012` at `0x800B2960`, and MAC1..3 `SWC2` stores to
  `0x1F8003D4..0x1F8003DC` at `0x800B29A4..0x800B29AC`.
  Ghidra corroborates these values becoming the scratch matrix translation
  used by subsequent matrix composition and scale code. The saved SHP
  reference's joints, groups, and vertex records were consulted as context,
  not substituted for instruction/consumer evidence.
- The available SDK `gte_*` path in
  [inline_c.h](include/psx/inline_c.h) expands to `__asm__ volatile`.
  Earlier recorded library-call probes emitted `JAL` sequences instead of
  inline COP2 operations and did not match. No approved C-only intrinsic
  route was identified for this existing historical toolchain. Reusing the
  macro would introduce assembly into a new reconstruction, while calling
  the library would change the required bytes.
- `func_800B28A8` is therefore **`BLOCKED` under the combined no-new-assembly
  and byte-exact requirements**. This is a current toolchain/workflow blocker,
  not a proof that no future C-only backend could exist. No partial stub,
  copied assembly body, raw opcode payload, altered flag/range, or generated
  placeholder was presented as an implementation or accepted progress.
- Canonical game source and protected references remain untouched by this
  continuation. Accepted BATTLE coverage remains **98.42288%**, with five
  routines / 8,160 bytes outstanding. The Ghidra exporter now includes this
  target and completed within its established hard bound.

## Resolver Current-Source Continuation: 2026-10-13

- Returned to `func_8006CB04` in the active source unit after dispatcher
  acceptance. Created [battle-ability-current.c](build/battle-ability-current.c)
  from current canonical source and replaced only the resolver inclusion.
  Both accepted routines and their verified helper contracts are preserved;
  the older resolver copy was not restored over current code.
- Fresh positive control `func_80075554` matches all 2,936 code bytes and
  1,716 read-only bytes. The old negative copy referenced a now-removed
  original tile assembly input and was correctly refused. Rebuilt the
  [negative control](build/battle-ability-negative-current.c) from current
  source; its deliberate actor-condition change produces the expected
  complete-range mismatch, rather than a stale-input refusal.
- The plain current-source resolver still compares at **592 / 604 bytes**
  under the expected GCC `2.8.1-psx` configuration. The compiler removes the
  local effect-applied flag, while retail retains its initialization and
  assignment. This remains a local code-generation hypothesis, not an
  assertion about the exact original source.
- Tested meaningful, behavior-equivalent flag uses for the no-effects path:
  an early notification guard and four response-selection forms. They keep
  the flag but introduce non-retail control flow, producing 608..616 bytes,
  and none matches. Preserved variants and complete reports in
  `~/.cache/vagrant-matching/ability-flag-_jjf3b03/results.json`.
- A separate two-level optimizer diagnostic produced 592 bytes with `-O2`
  and 608 with `-O1`; neither matched. Results:
  `~/.cache/vagrant-matching/ability-optimizer-t9hu6iqk/results.json`.
  These runs were diagnostic only. Expected settings, canonical compiler
  configuration, retail data, and acceptance criteria were not changed.
- Removed the non-matching notification guard from the working candidate.
  The [retained full comparison](build/ability-current-report.json) remains
  **`CANDIDATE_DIFF`, 592 / 604 bytes**, with 133 differing instruction words
  and 205 differing read-only words. The canonical inclusion remains intact;
  this routine is still `BLOCKED` pending a valid matching C formulation.
- No new function was accepted. Canonical game source stayed unchanged during
  verification; BATTLE coverage remains **98.42288%**, with five routines /
  8,160 bytes outstanding. No new unit, overlay, native, or behavioral
  completion is claimed. Temporary probe runners were removed; candidates,
  control inputs, and diagnostic evidence remain available.

## Accepted Actor State Dispatcher: 2026-10-13

This entry supersedes the blocked dispatcher candidates and one-instruction
results below. The remaining difference came from an action-helper parameter
conversion being imposed at the caller instead of at the stored action index.

- Replaced `func_80075554`'s assembly inclusion in
  [146C.c](src/BATTLE/BATTLE.PRG/146C.c#L5659) with its complete readable C
  actor-state dispatcher. It scans the 16 actor slots, preserves the observed
  hold and pending states, movement/facing resolution, animation requests,
  focus requests, queued actions, and early tile-result return. Unresolved
  state values remain numeric. No inline assembly, embedded MIPS, forced
  register binding, padding, or expected compiler-setting change was added.
- Fresh canonical **`MATCH`: 2,936 / 2,936 bytes, zero differences**, configured
  GCC `2.8.1-psx`, CPU `0x80075554..0x800760CC` / file `0xCD54..0xD8CC`, end
  exclusive. The complete **1,716-byte read-only section also matches**.
  Canonical source SHA-256:
  `9a4d5e5de67a088c1291d2f9f3e395d8a2030fa7e456531cde516d1602d7cd82`.
  [Function report](build/dispatch-accepted-validation/func_80075554.json),
  timestamp `2026-10-13T04:34:09.511647+00:00`.
- Verified the action-helper consumer in both C and retail instructions:
  `func_80072EC4` accepts the incoming word and narrows it with a halfword
  store to `actionIndex`. Its maintained input now uses `u_int`, with the
  low-16-bit conversion explicit at that store. The existing word-valued
  caller in `func_800735F8` retains its original conversion explicitly.
  This is not a compiler-flag change or ignored caller difference.
- The focus helper `func_8006D028` now declares the unused second argument
  passed by retail. It has no other maintained C caller to update, and its
  complete 60-byte body remains unchanged. These two helper corrections are
  compatibility work, not additional newly decompiled functions.
- Fresh canonical checks pass for every affected or previously accepted
  slice: `func_80072EC4` 504 bytes, `func_800735F8` 288 bytes,
  `vs_battle_exec` 3,968 bytes plus all read-only data, `func_8006D028` 60 bytes,
  and `func_80074BAC` 1,548 bytes. One main-loop run detected a source
  hash/timestamp change and was discarded; its stable rerun passed without
  relaxing the guard or reverting edits. All current reports are retained in
  the [validation record](build/dispatch-accepted-validation/validation.json).
- Corrected a missing function classification on the already-recorded
  `_initBattleCameraTransition = 0x800730BC` boundary. Verified it against
  Splat's recorded function symbol and the retail return/delay-slot followed
  by the next prologue. This fixes an oversized helper-check range; the
  boundary was not inferred from candidate length. Regenerated hashed
  verification artifacts before checking the 504-byte helper range.
- Rebuilt current maintained inputs in the isolated historical workspace.
  The complete owning unit's **159,680 text bytes** and **1,716 read-only
  bytes** match their configured retail ranges in the linked ELF. The complete
  **577,828-byte BATTLE overlay matches retail**, SHA-256
  `ad6914c81be92a008af8dece6ad394ce39c5aa42360542b5531716ebd00bba12`.
  Full target comparison reports `All files match`.
  [Scope report](build/dispatch-accepted-validation/validation.json) and
  [build log](build/dispatch-accepted-validation/build.log) are retained.
- Regenerated progress over 424 units: BATTLE matched code is now
  **509,240 / 517,400 bytes, 98.42288%**, up from 506,304 / 517,400,
  97.85543%. This adds one accepted function and 2,936 bytes. Overall matched
  code is **1,892,772 / 1,936,592, 97.73727%**, with 4,083 / 4,156 matched
  functions. [Fresh progress](build/dispatch-accepted-validation/progress.json).
- **Five BATTLE routines / 8,160 bytes remain**: `func_8006CB04`,
  `func_8008AC78`, `func_800B28A8`, `func_800B9DB4`, and `func_800E3600`.
  Other unfinished assembly still contributes to binary reproduction, not
  readable reconstruction. No native integration, independent behavior,
  gameplay parity, assembly-free unit completion, or 100% completion is claimed.

## Actor State Dispatcher: 2026-10-12

### Final-Instruction Refinement

- Rechecked the current candidate and kept work confined to its initial
  state-byte read. A packed command-field view and an unbound C register hint
  produced the same redundant load; both probes were removed.
- Tested eight bounded cached-read forms over byte, halfword, and word types.
  The variants either retained the one-word mismatch or changed other code
  and table bytes. Generated inputs and full comparison reports remain in
  `~/.cache/vagrant-matching/dispatch-state-a2xqmg13/results.json`.
  The historical compiler requires declarations at the start of a block;
  generated loop caches were enclosed in a local block rather than changing
  compiler flags or the maintained language contract.
- Ran the existing permuter with each generated dispatcher compiled inside
  the entire recorded source-unit context, not only a standalone function.
  The run was capped at 600 seeds, one worker, 120 seconds of search, and
  short per-compile limits. It reached the search limit without an improving
  output. Preserved context, inputs, logs, and results:
  `~/.cache/vagrant-matching/dispatch-permuter-3b428ib5`.
  The owned search process tree was terminated at its limit; this is not a
  completed match or exhaustive proof that matching C is impossible.
- Reusing the existing word-sized flags local also produced a worse,
  constant-folded form. Removed all non-improving probes and refreshed the
  [retained comparison](build/actor-state-refinement-report.json): still
  **2,936 / 2,936 bytes, one differing instruction at `+0x8C`**, with
  **zero differences across all 1,716 read-only bytes**.
- Canonical positive and deliberate negative controls pass. No canonical
  source, shared header, helper interface, retail bytes, comparison bounds,
  or expected compiler settings changed. This routine remains `BLOCKED`;
  accepted BATTLE coverage is unchanged at **97.85543%**.

- The reconstruction and earlier evidence follow below.

- Continued the active source unit with `func_80075554`, retail CPU
  `0x80075554..0x800760CC` / file `0xCD54..0xD8CC`, 2,936 bytes.
  Fresh hash-verified Windows Ghidra project:
  `build/ghidra/20261012-215450-885`. Verified callers `0x80079E08` and
  `0x80079F28`, both switch tables, actor fields, helper interfaces, and the
  16-slot loop against original instructions and nearby C.
- Built a complete readable C
  [dispatcher candidate](build/battle-actor-state-layout.c) covering held and
  pending states, movement and facing resolution, animation requests,
  queued action IDs, focus requests, and early tile-result return. State names
  remain numeric where their meaning is unresolved. No new assembly, forced
  register binding, padding, or expected-toolchain change was introduced.
- The first candidate compiled at 2,864 / 2,936 bytes. Removed an extra live
  parameter pointer, used the actual retail case-group order, preserved
  independent normalization-state blocks, split direct-facing call setup,
  and corrected state 12 to share state 11's callback. The latter is a
  behavior correction in an unaccepted candidate, not acceptance of the
  earlier no-op interpretation.
- Retail passes an explicit second zero argument to `func_8006D028`, while
  the maintained function declares one parameter. The configured compiler
  rejects a two-argument call under that declaration. A candidate-only unused
  second parameter preserves all **60 / 60 helper bytes** and has no other
  maintained C caller to update. The helper and its new call are confined to
  the ignored candidate; canonical APIs were not changed or promoted.
- Kept the existing unsigned-short interface of `func_80072EC4`. Separating
  full-width movement speed from narrow action/status values eliminated the
  action argument's extra mask without changing that shared interface.
  The candidate also uses the SDK's correct `SVECTOR*` normalization input.
- Final retained result: **`CANDIDATE_DIFF`, 2,936 / 2,936 bytes**, with
  **one differing instruction word**, and **all 1,716 read-only bytes match**.
  At CPU `0x800755E0` / relative `+0x8C`, retail moves the already-read state
  from `v1` into `s0`; the candidate reloads the same state byte from actor
  offset `+8`. This difference was not ignored. The function remains
  `BLOCKED` at the byte-exact acceptance gate.
- Retained [dispatcher report](build/actor-state-candidate-report.json),
  generated `2026-10-13T03:19:54.531789+00:00` (UTC, October 12 locally), and
  [helper probe report](build/actor-state-helper-report.json).
  Dispatcher candidate SHA-256:
  `c32bd42b26315d74188afc5155c6948526251acb521444ceb7d7b3cb09b02d18`.
  Original-order trials remain in
  [battle-actor-state-candidate.c](build/battle-actor-state-candidate.c).
- Fresh canonical positive control `func_80074BAC` still matches all 1,548
  bytes, and the deliberate `func_8006CA20` mismatch is still rejected.
  Canonical source hashes/timestamps stayed unchanged throughout these
  checks. Accepted BATTLE coverage remains **97.85543%**, with six routines
  outstanding. No new function, source-unit, linked-overlay, native, or
  independent behavioral completion is claimed for this continuation.

## Accepted Tile Transition: 2026-10-12

This entry supersedes the blocked tile-candidate results below. The immutable
actor-table base separated from its selected slot resolved the final two
temporary-register differences without assembly or toolchain changes.

- Replaced only `func_80074BAC`'s assembly inclusion in
  [146C.c](src/BATTLE/BATTLE.PRG/146C.c#L5393) with the complete readable C
  tile-trigger, door-selection, encoded-destination, and fall-damage routine.
  Preserved its public `int, u_char*` parameter types and the candidate's user
  formatting. Added only required local forward declarations; no shared
  header, unrelated routine, or protected reference source was changed.
- Fresh canonical **`MATCH`: 1,548 / 1,548 bytes, zero differences**, configured
  GCC `2.8.1-psx`, CPU `0x80074BAC..0x800751B8` / file `0xC3AC..0xC9B8`,
  end exclusive. The function adds no inline assembly, assembly macro,
  embedded MIPS, forced register binding, or new assembly inclusion.
- Canonical source SHA-256:
  `81182b66e6f390f7c052fe8145fb6b6c0a5d488c1743b3977b4e23cd4a2eda05`.
  Canonical report timestamp: `2026-10-13T01:30:19.927405+00:00` (UTC;
  October 12 local date). Retained
  [function report](build/tile-accepted-validation/function.json).
- Rebuilt current maintained inputs in the isolated WSL historical workspace.
  The source unit's complete configured ranges match the linked ELF: **159,680
  text bytes** at CPU `0x80069C6C` / file `0x146C`, plus **1,716 read-only bytes**
  at CPU `0x80068800` / file `0x0`. The linker merges these ranges into
  `.BATTLE_PRG`; their bounds come from the retail split, not compiled length.
- The complete linked **BATTLE overlay matches all 577,828 retail bytes**,
  SHA-256 `ad6914c81be92a008af8dece6ad394ce39c5aa42360542b5531716ebd00bba12`.
  The full isolated rebuild's target comparison also reports `All files match`.
  Retained [scope validation](build/tile-accepted-validation/validation.json)
  and [build log](build/tile-accepted-validation/build.log).
- Fresh object progress over 424 units: BATTLE matched code is now
  **506,304 / 517,400 bytes, 97.85543%**, up from 504,756 / 517,400,
  97.556244%. This is one newly accepted function and 1,548 additional bytes.
  Overall matched code is 1,889,836 / 1,936,592, **97.585655%**; matched
  functions are 4,082 / 4,156. Retained
  [progress report](build/tile-accepted-validation/progress.json).
- **Six BATTLE routines / 11,096 bytes remain**: `func_8006CB04`,
  `func_80075554`, `func_8008AC78`, `func_800B28A8`, `func_800B9DB4`, and
  `func_800E3600`. The first three remain assembly inclusions in the active
  source unit. Other unfinished assembly is still part of exact reproduction;
  unit/overlay byte equality does not declare those routines decompiled.
- Positive and deliberate negative controls pass. Canonical hashes/timestamps
  stayed unchanged during verification. No native integration, independent
  behavioral test, gameplay parity, assembly-free unit completion, or 100%
  BATTLE completion is claimed. Earlier percentages remain historical below.

## BATTLE 100% Attempt: 2026-10-12

The fresh baseline is 97.556244% matched code, with 12,644 bytes across seven
remaining assembly inclusions: four in 146C.c (6,500 bytes), func_800B28A8 in
49268.c (4,292 bytes), func_800B9DB4 in 4A0A8.c (372 bytes), and func_800E3600
in 771A8.c (1,480 bytes). No assembly was reclassified as reconstructed C.

- Analyzed func_800B9DB4 with hash-verified Windows Ghidra project
  `build/ghidra/20261012-060905-947`, its original retail range CPU
  `0x800B9DB4..0x800B9F28` / file `0x515B4..0x51728`, and script table entry
  `0x800E9EF8` (opcode `0x72` in the saved DataCrystal dump).
- Validated the selected source unit's positive control func_800B9D74
  (`MATCH`, 64 bytes) and a deliberate argument-offset mismatch
  (`CANDIDATE_DIFF`, exactly `+0x20`) before reconstruction.
- Isolated readable-C state-machine candidates compile but still differ in
  early-guard/return code generation. Current candidate:
  [battle-script-candidate.c](build/battle-script-candidate.c),
  `CANDIDATE_DIFF`, 364 / 372 bytes. A bounded initialized-result variant
  produced 384 / 372 bytes and 57 differing words. Neither is accepted.
- Alternative compiler runs were explicitly diagnostic only; they produced
  no match and did not change expected canonical compiler settings. An
  external scratch API was unavailable. No candidate was promoted, no retail
  byte or expected range changed, and this routine remains `BLOCKED` pending
  further matching work rather than being declared complete.
- Verified linker support now hashes the generated auto-address maps and
  Splat's explicit retail symbol CSV. Ambiguous addresses still fail; target
  relocations and complete ranges are checked. The symbol dump is enabled
  only in an isolated copy, not the canonical split/compiler configuration.
- Work continues separately on the remaining non-GTE func_800E3600, one
  source unit and routine at a time. BATTLE has not reached 100% and its
  reported percentage remains unchanged until a new canonical match is
  accepted and the full report is regenerated.

### Subsequent Matching Results

- The movement controller in [771A8.c](src/BATTLE/BATTLE.PRG/771A8.c),
  `func_800E3600`, was analyzed with Windows Ghidra project
  `build/ghidra/20261012-064327-127` and the retail range CPU
  `0x800E3600..0x800E3BC8` / file `0x7AE00..0x7B3C8`.
  Its reconstructed positive control `func_800DF9A8` matched all 172 bytes;
  reducing its loop bound from eight to seven produced a deliberate difference
  at `+0x94`. The final [movement candidate](build/battle-movement-candidate.c)
  is `CANDIDATE_DIFF`, 1,292 / 1,480 bytes. It remains `BLOCKED`, not promoted.
- The ability-effect resolver in [146C.c](src/BATTLE/BATTLE.PRG/146C.c),
  `func_8006CB04`, was analyzed with Windows Ghidra project
  `build/ghidra/20261012-070234-772`, retail CPU `0x8006CB04..0x8006CD60` /
  file `0x4304..0x4560`, and its callers in
  [5BF94.c](src/BATTLE/BATTLE.PRG/5BF94.c#L8176).
  The existing header and callers establish a void routine with a target ID
  and effect-context pointer. The candidate uses the existing typed ability
  and per-target effect records; it introduces no assembly or new game types.
- Fresh controls for this unit pass: `func_8006CDB8` matches 32 / 32 bytes,
  while an intentionally altered argument differs at `+0xC`.
  `func_8006CA20` matches 228 / 228 bytes and all 1,716 read-only-section bytes.
  An intentionally altered actor condition fails both instruction and table
  comparisons. These are verifier controls, not newly decompiled functions.
- The previous valid [ability candidate](build/battle-ability-candidate.c) was
  `CANDIDATE_DIFF`, 584 / 604 bytes under configured GCC `2.8.1-psx`, with
  133 differing instruction words and 205 differing read-only words.
  Report: `~/.cache/vagrant-matching/reports/candidates/146C-func_8006CB04.json`.
  A bounded twelve-case test of valid C flag-lifetime/exit forms produced the
  same mismatch; results remain in
  `~/.cache/vagrant-matching/ability-variants-qsf2297n/results.json`.
  A volatile probe instead produced 612 bytes with extra stack traffic. A
  diagnostic valued return in the void routine did not resolve the difference
  and was removed. No failed or nonportable probe was promoted.
- Retail retains a local effect-applied flag that these ordinary C candidates
  optimize away. This is a current code-generation hypothesis, not proof of
  the original source or an independent behavioral test. This resolver is
  `BLOCKED` pending further valid-C matching work.

### Resolver Correction And Scene Controller Continuation

- Continued the same resolver after the user requested further work. Comparing
  the complete emitted instructions exposed an incorrect action lookup in the
  earlier candidate: retail CPU `0x8006CC28..0x8006CC34` reads four bits at
  action-header word bits 20..23, not the six-bit prerequisite field at action
  offset `+0x14`. The candidate now extracts the verified header field at the
  observed word width. This correction is to an unaccepted candidate, not an
  accepted function or proof of the earlier candidate's behavior.
- Retaining the verified no-effect `0x10` object category reproduces the
  retail object-dispatch range check. The current retained resolver candidate
  is `CANDIDATE_DIFF`, **592 / 604 bytes**, with 133 differing instruction
  words and 205 differing read-only words. The unused flag remains unresolved.
  Fresh same-unit positive and negative controls still pass.
- Bounded diagnostics compared all three available compiler versions; none
  resolved the mismatch and expected settings were not changed. Results:
  `~/.cache/vagrant-matching/ability-compilers-fzreuxa7/results.json`.
  Sixteen same-compiler control-flow variants and eight flag-type variants
  also produced no match. Evidence remains in
  `~/.cache/vagrant-matching/ability-control-flow-v5h44smp/results.json` and
  `~/.cache/vagrant-matching/ability-control-flow-wqlbdbhx/results.json`.
- Asked for a one-time move to another routine while retaining this resolver
  as blocked. The question response reported that the user was unavailable
  and directed autonomous work. Investigated only the next selected routine
  in the same source unit; no acceptance rule or requirement was rewritten.
- Analyzed the room/simple-map render controller `func_8008AC78`, retail CPU
  `0x8008AC78..0x8008B1FC` / file `0x22478..0x229FC`, with fresh Windows
  Ghidra project `build/ghidra/20261012-085344-167`. Verified its caller,
  room header and geometry/scene pointers, `simpleMap` setting bits, scratchpad
  camera angle, draw-area restoration, and border color branches against
  original instructions and existing SDK records.
- Added a complete readable C [scene candidate](build/battle-scene-candidate.c)
  using the existing `RECT`, `MATRIX`, `LINE_G4`, `LINE_G2`, and `DR_MODE`
  types. It compiles under configured GCC `2.8.1-psx`; the final comparison is
  `CANDIDATE_DIFF`, **1,368 / 1,412 bytes**. Its typed scratchpad view is checked
  for the verified 92-byte extent. It was not promoted or counted as progress.
- Retail's packet insertion at CPU `0x8008B058..0x8008B0C8` exactly follows
  the existing assembly helper in
  [insertTPage.h](src/include/insertTPage.h). The candidate instead expresses
  the `DR_MODE` packet and ordering-table links in C, producing different
  register usage and instruction scheduling. No new helper assembly, assembly
  macro expansion, embedded MIPS, or altered compiler settings were introduced
  to force a match. This is a concrete remaining compatibility blocker, not
  a proof that every possible valid C formulation is impossible.
- Both routines remain `BLOCKED`. No new canonical function, source-unit,
  linked-overlay, or native/gameplay completion is claimed; BATTLE is still
  **97.556244%** with seven routines outstanding.

### Tile Transition Matching Continuation

- Resumed the simple-map render candidate and tested an advancing primitive
  cursor instead of retaining its draw-area base. The result was
  `CANDIDATE_DIFF`, 1,372 / 1,412 bytes, and did not remove the extra saved
  register or assembly-helper mismatch. The candidate remains unaccepted.
- Under the previously recorded autonomous-work direction, kept that routine
  blocked and investigated `func_80074BAC` in the same source unit. Fresh
  Windows Ghidra project: `build/ghidra/20261012-154253-551`. Verified retail
  CPU `0x80074BAC..0x800751B8` / file `0xC3AC..0xC9B8`, caller `0x80079A90`,
  and the neighboring reconstructed `func_800751B8`.
- Reconstructed the complete actor/tile trigger, door-selection, encoded
  destination, and fall-damage branches in the ignored
  [tile candidate](build/battle-tile-candidate.c), using existing actor,
  terrain, position, and door-record definitions. The health halfword at
  actor-state `+0x18` is the existing `currentHP` field. Initial candidate
  compilation exposed and fixed a mistaken unknown-field name; the retained
  version compiles with the unchanged configured historical settings.
- The first complete comparison was 1,556 / 1,548 bytes. Reusing the verified
  disjoint actor-flag and encoded-destination phases in a signed scalar,
  retaining an early position alias, and capturing the repeated X-coordinate
  read brought the function to its exact original length. The explicit door
  null branch also aligned its zero-argument operands.
- Final retained result: **`CANDIDATE_DIFF`, 1,548 / 1,548 bytes, 47 differing
  instruction words**. Equal size is not a matching result or acceptance.
  Remaining differences are in actor-table address operands and packed
  destination-record scheduling. Report:
  `~/.cache/vagrant-matching/reports/candidates/146C-func_80074BAC.json`.
  Candidate SHA-256:
  `bfdacdd6f9e41a23c745d8a44bc2ef4880ac8f47bec47446d79d617d635ff6e8`.
- Bounded 24-case pointer/capture probes remain in
  `~/.cache/vagrant-matching/tile-variants-scqcef2_/results.json`.
  A separate 24-case declaration-order check produced the same 47-word
  mismatch throughout; results remain in
  `~/.cache/vagrant-matching/tile-variants-l8dv8f4t/results.json`.
  The non-improving alternate record view and lookup probe were discarded;
  the retained candidate uses the existing door type and original public
  parameter types. No generated variant was promoted.
- Improved the verifier's instruction-evidence companion to disassemble the
  entire executable section range even when imported symbol annotations make
  the default disassembler print raw bytes. Only evidence formatting changed:
  compared bytes, complete ranges, relocation validation, toolchain flags,
  and acceptance criteria did not. Fresh positive control `func_8006CA20`
  matches all 228 code and 1,716 read-only bytes; its deliberately wrong
  actor-condition control still fails both comparisons.
- This tile routine is still `BLOCKED` at the byte-exact gate. No canonical
  game function, header, retail input, or expected compiler setting was
  changed. BATTLE remains **97.556244%**, with seven routines / 12,644 bytes
  outstanding; no native, behavioral, unit, or overlay completion is claimed.

### Tile Operand Refinement And Bounded Permuter

- Re-read the user-edited candidate before changes and verified that its
  formatting edits retained the 47-word mismatch. Preserved those edits;
  no earlier full-file candidate was restored over them.
- Used the repository's existing decomp-permuter in isolated WSL copies, each
  limited to 800 seeds, one worker, a 120-second search timeout, and bounded
  child compiles. Canonical compiler hashes, flags, assembler settings, retail
  inputs, and the edited candidate were checked for preservation. The generic
  diagnostic target prelude was corrected from 64-bit to PSX 32-bit registers
  only in the isolated setup; canonical settings were not changed.
- The first search found paired Z-field assignment as a useful refinement.
  Reviewed that small C change rather than importing generated formatting or
  an entire output function. Applied the corresponding paired X-field
  assignment and independently verified the complete target each time.
  These assignments preserve the verified five-bit coordinate truncation
  and eliminated the destination constructor's instruction differences.
- An explicit typed actor slot with the tile-identity mask evaluated first
  reduced the final table lookup differences. The retained
  [tile candidate](build/battle-tile-candidate.c) now compares at
  **1,548 / 1,548 bytes with exactly two differing instruction words**:

  | CPU Address | Relative Offset | Remaining Difference |
  | --- | --- | --- |
  | `0x80074C64` | `+0xB8` | Actor-slot address sum uses `v0` rather than retail `v1` |
  | `0x80074C68` | `+0xBC` | Actor-pointer load uses that same `v0` address rather than `v1` |

- Final result remains **`CANDIDATE_DIFF`, not accepted**. No difference was
  masked, no comparison range narrowed, and no assembly, padding, forced
  register binding, or compiler-setting change was added. Non-improving
  volatile, stored-word, reused-query, and signed-offset probes were removed;
  the retained implementation uses the existing typed actor-slot pointer.
- Final report:
  `~/.cache/vagrant-matching/reports/candidates/146C-func_80074BAC.json`.
  Retained candidate SHA-256:
  `f72ae382d639089b732ad4ddc38f0919cfe9bc6e0c61bf8a4f5ea1c0a67e1332`.
  Search inputs, logs, and outputs remain in
  `~/.cache/vagrant-matching/tile-permuter-7syr4i3o`,
  `~/.cache/vagrant-matching/tile-permuter-iecsqm89`, and
  `~/.cache/vagrant-matching/tile-permuter-igyjpbnd`.
  Permuter scores are diagnostic only and are not byte-exact acceptance.
- Fresh positive and deliberate negative controls pass. Canonical game source,
  protected reference checkouts, headers, and retail inputs remain unchanged.
  Accepted BATTLE progress remains **97.556244%**; this continuation reduced
  the candidate mismatch, not the count of accepted functions.

### Layout Verification Updates

- The checker now uses local function boundaries from the hash-recorded retail
  Splat symbol export. This fixes the manual map's omitted neighboring
  boundary without deriving a range from candidate length.
- The explicit `--reference-assembly` option retains hash-recorded, unchanged
  inclusions from other routines solely to reproduce the unit's read-only
  layout. The selected target must still be reconstructed C; checking the
  canonical `func_8006CB04` inclusion is explicitly refused. Other assembly
  is not reconstructed progress. Complete target bytes and the entire
  read-only section are compared, with no masked differences.
- Fresh regression checks pass for imported `func_800A2CD4` (744 code bytes
  plus all 76 read-only bytes) and SDK `memcmp` (76 bytes, configured PsyQ
  preprocessing). Expected compiler settings, retail bytes, canonical game
  source, and protected reference checkouts were not changed by these attempts.
- No routine has been newly accepted in this 100% attempt. BATTLE remains at
  504,756 / 517,400 matched code bytes, **97.556244%**, with all seven routines
  and 12,644 bytes outstanding. No new source-unit, linked-overlay, native,
  gameplay, or assembly-free completion is claimed.

## Fresh WSL Build And Progress: 2026-10-12

WSL is authorized for isolated historical-compiler matching verification.
Missing Windows `cc1.exe` binaries do not prevent that verification. Windows
remains the native development/build/runtime platform; this evaluation adds
no Linux dependency to the native build and changes no canonical game source.

- Built the current imported source in a fresh isolated WSL copy at
  `~/.cache/vagrant-matching/progress-kl_egopc` using the configured historical
  compilers, imported Make recipes, and read-only local retail inputs.
- The build completed with exit 0 and its final retail comparison reported
  `All files match`. It generated 424 object units and a fresh objdiff 3.8.0
  progress report. This supersedes the stale tracked root progress report for
  evaluating the imported material.
- The scratch compiler location was corrected to the path expected by the
  Makefile. The scratch xdelta invocation unsets the command-line `XDELTA`
  override before invoking the binary: otherwise GNU make exports that path
  as the binary's extra-arguments environment variable. Original decode flags
  and canonical recipes were not changed. Failed partial reports were discarded.

### Measured Object Progress

| Measure | Current Result |
| --- | --- |
| Matched code bytes | 1,888,288 / 1,936,592, **97.51%** |
| Matched functions | 4,081 / 4,156, **98.20%** |
| Matched data bytes | 181,826 / 865,658, **21.00%** |
| Code in explicitly declared complete units | 399,716 / 1,936,592, **20.64%** |
| Explicitly declared complete units | 35 / 424 |

The declared-complete measures come from the manually maintained category list,
not an automated proof of native readiness. The code/function matching measures
are the repository's object-progress convention, which also includes assembly
source; they must not be presented as assembly-free C/C++ completion.

| Category | Matched Code |
| --- | --- |
| BATTLE main engine | 97.56% |
| Entry point and libraries | 77.39% |
| Bootstrapper and fullscreen effects | 100.00% |
| Menus | 100.00% |
| Title | 100.00% |
| Ability effects | 100.00% |
| Ending | 100.00% |

- 1,878,156 code bytes (**96.98%**) are associated with `.c.o` units; 58,436
  bytes (**3.02%**) with `.s.o` units. C-associated coverage is an upper bound
  on pure-C reconstruction because C files may still contain inline assembly,
  GTE macros, or original assembly inclusions.
- Reproducing every configured retail target exactly is not the same as
  decompiling every function: remaining original assembly can reproduce bytes
  without constituting readable portable C/C++.
- No Windows-native gameplay, runtime parity, assembly-free completion, or
  native-port completion percentage is inferred from these figures.

Fresh report:
[progress-20261012.json](build/progress-20261012.json).
Full isolated build log and generated report remain in the WSL directory above;
evaluation timestamp: `2026-10-12T09:20:19.765619Z`.

## Fork-First Source Integration: 2026-10-12

The user explicitly requested source integration, with the incoming fork taking
precedence wherever it overlaps local work. This entry supersedes earlier
current-source counts, names, hashes, and verification claims. Earlier entries
remain historical evidence for the backed-up local implementations.

- Source: `noprompt/rood-reverse`, branch `decomp/verified-functions`, commit
  `3b3f35fb318178a5778a46db4dc93777e534c042`; previous recorded source base
  `ef3f027a64bf861bbf5ad3efa30851788d499ce4`.
- Applied the maintained source/configuration/asset-description/tool delta:
  333 files, comprising 86 additions, 228 modifications, and 19 deletions.
  The fork's committed content won all 17 local overlaps. No local implementation
  was kept in preference to an overlapping incoming implementation.
- Restored nine additional fork files coupled to its unchanged shared symbol
  namespace, including the BATTLE exports, initialization table, and menu
  consumers. This removes stale local renames without adding compatibility
  aliases to the incoming game code.
- Preserved a complete pre-import maintained-source backup in ignored
  `build/fork-import-xxz7nexe/backup`, with before hashes and import records in
  the containing directory. The separate fork checkout and original ser-pounce
  reference were not modified. No commit or staging operation was performed.
- [upstream.json](upstream.json) records the authoritative fork revision and
  original committed file hashes. Windows-specific infrastructure remains
  outside its replacement set; deliberate verification metadata additions are
  local differences, not mislabeled fork bytes.

### Compatibility Fixes

- Preserved Windows make/verification tools and carried the fork's GNU-as
  PsyQ reorder preprocessing into
  [compile_windows.py](tools/verification/compile_windows.py) and
  [psyq_assembly.py](tools/verification/psyq_assembly.py). Ordinary game C still
  uses maspsx. The fork's exact move-to-addu rule is shared with historical
  verification, and Windows CRLF input is normalized; both LF/CRLF checks pass.
- Fixed owning-overlay lookup for nested SDK source paths and honored optional
  per-overlay Makefiles rather than requiring files absent from the original
  build. TITLE flag inspection no longer tries to rebuild its image object;
  its compiler sweep uses the existing Windows background-size calculation.
- Generated the imported BATTLE string headers in isolated splat preparation,
  fixing the missing `statusStrings.h` include without changing game source.
- Restored verified call-address metadata needed by the isolated function
  linker and the controller status global at `0x800F49F4`. No arbitrary address,
  retail-byte alteration, ignored relocation, or changed expected compiler
  setting was used.
- SDK `memcmp` has a verified complete 76-byte function range, followed by
  padding. Recorded its disassembled size independently of compiled output;
  the checker honors declared sizes only when they do not overlap the next
  verified function boundary.
- Linked the imported actor unit's anonymous read-only relocations only at the
  split's verified file/CPU location. The complete 76-byte read-only section is
  compared too; a table difference causes failure, not a masked function match.

### Validation

- Complete bounded isolated historical-compiler sweep: all 289 C source units
  compiled successfully, zero failures, with imported per-unit settings and
  required generated headers. Compilation is not behavioral or byte-match proof.
  Result: `~/.cache/vagrant-matching/import-compile-416qeq7z/results.json`.
- BATTLE and SLUS splits/generated inputs pass on Windows. Windows make dry-runs
  select the imported compiler flags and maspsx/PsyQ recipe as appropriate.
- Canonical function checks: func_800A3054 matches all 76 bytes; imported
  func_800A2CD4 matches all 744 bytes plus all 76 read-only-table bytes;
  SDK `memcmp` matches all 76 bytes with PsyQ reorder preprocessing.
- A fresh imported-source negative control changes only the first `0x3F`
  constant in func_800A3054 to `0x3E`. It returns `CANDIDATE_DIFF`, at exactly
  `+0x0` and `+0x10`, rather than acceptance.
- No full linked overlay, Windows-native build/runtime, gameplay, or
  assembly-free completion is claimed. The imported fork still includes
  unfinished assembly and GTE macros. Prior local match reports are historical
  because their source and namespace were superseded by explicit direction.

## Active Unit: 3A1A0.c, 2026-10-08

### Accepted: vs_battle_animateCharacterJump

- Replaced exactly this routine's `INCLUDE_ASM` in
  [3A1A0.c](src/BATTLE/BATTLE.PRG/3A1A0.c) with readable C. The implementation
  adds no inline assembly, opcodes, hardware macros, or assembly wrappers and
  uses the animation API already declared by the existing header.
- Fresh canonical `MATCH`: 344 compiled / 344 retail bytes, zero differences,
  GCC `2.7.2-cdk`. Full BATTLE range: CPU `0x800A48CC..0x800A4A24`, file
  `0x3C0CC..0x3C224`, end exclusive.
- Accepted local progress is five functions / 2,876 bytes, an increase of one
  function / 344 bytes. All four earlier functions in the separately blocked
  38C1C unit were freshly rechecked with the final symbol inputs.
- Neither source unit, the linked overlay, independent behavior, native
  integration, nor gameplay is declared complete. The remaining GTE blocker
  in 38C1C remains explicitly recorded below without relaxing either rule.

### Evidence And Controls

- Added CPU `0x800A48CC` to the existing bounded Windows Ghidra evidence
  exporter. The new ignored project `build/ghidra/20261008-013600-713` completed
  its hash-verified BATTLE/main-executable inspection, original-instruction
  export, references, and decompiler output. Retail callers of this entry
  are `0x8006EB30` and `0x8006EBB4`.
- Preserved the actor-state guards, existing active-motion call at
  `0x800B64A8`, zero-speed animation `0x2F`, startup-delay byte, nonzero
  fixed-point speed division by 4096 toward zero, byte threshold, original
  32-bit flag-word accesses, movement mode 7, low-halfword facing store,
  and original speed-word store. Unknown flags retain their verified masks
  rather than invented semantic names.
- Initial new-unit verification exposed four unmapped external calls outside
  the selected control. Added only their independently verified addresses,
  decoded from actual retail JAL words, then regenerated splat artifacts:

| Retail Caller | File Offset | Instruction Word | Verified Target |
| --- | --- | --- | --- |
| 0x800A3514 | 0x3AD14 | 0x0C023689 | 0x8008DA24 |
| 0x800A3BCC | 0x3B3CC | 0x0C023743 | 0x8008DD0C |
| 0x800A4A34 | 0x3C234 | 0x0C0234B0 | 0x8008D2C0 |
| 0x800A6F4C | 0x3E74C | 0x0C02371F | 0x8008DC7C |

- Before reconstruction, canonical func_800A3054 matched all 76 bytes in
  this source unit. An isolated copy changing its first `0x3F` constant to
  `0x3E` returned `CANDIDATE_DIFF`, 76 / 76 bytes, at exactly `+0x0` and
  `+0x10`. Both controls used the configured compiler and regenerated inputs.
- Candidate iterations were immediately checked over the complete routine.
  Direct original-word flag updates prevented a non-retail shared state-store
  tail; retaining the nonzero-path flag value and splitting its final
  AND/OR into two statements reproduced the retail register lifetimes.
  The full matching candidate was promoted only after `CANDIDATE_MATCH`,
  followed immediately by the canonical `MATCH`.
- No checker comparison logic, retail bytes, complete ranges, expected
  compiler settings, shared header, or canonical 38C1C implementation changed.
  The source resolver's pre-existing stack-pointer warning remains outside
  this routine and was not changed.

### Latest Canonical Reports

Reports reside under `~/.cache/vagrant-matching/reports` in Ubuntu WSL. All
five have status `MATCH`, canonical mode, zero differences, complete ranges,
verified relocations, dependency hashes, and reproducible tool settings.

| Function | Source Unit | Bytes | Generated UTC |
| --- | --- | --- | --- |
| vs_battle_animateCharacterJump | 3A1A0.c | 344 | 2026-10-08T05:52:42.872719Z |
| func_800A1DE8 | 38C1C.c | 676 | 2026-10-08T05:54:19.109958Z |
| func_800A208C | 38C1C.c | 1,040 | 2026-10-08T05:54:56.445942Z |
| func_800A25EC | 38C1C.c | 420 | 2026-10-08T05:56:04.064763Z |
| func_800A2790 | 38C1C.c | 396 | 2026-10-08T05:56:44.354529Z |

- New report: `3A1A0-vs_battle_animateCharacterJump.json`.
- Current 3A1A0.c SHA-256:
  `56e09c0283fa9e2d58f11b8142514b45ac73f538372d578ded99ee3dea858300`.
- Unchanged 38C1C.c SHA-256:
  `a8f7748bee0e6a006cedd6b19ca5006366ac9a7350d7e2a112680486905a9a27`.
- Common snapshot SHA-256:
  `d72bc431f1305b15efd5b95f33c5a1161d10efaa1d547a41ef386a2d1c882502`.
- Unchanged checker SHA-256:
  `66c6bf156535ce927d16b020150f9136e6db897877d802ab5ae2864f51b6b32b`.
- Current splat manifest SHA-256:
  `bba927856657f0d9247db9ef4f5b4ac84849839926b76bb57fb5e2348962223f`.

Reproduce from the Windows workspace root:

```powershell
wsl.exe -d Ubuntu --exec /usr/bin/timeout 180 /home/judee/.cache/vagrant-matching/venv/bin/python /mnt/d/Dev/NativePort/VS-NativePort/resources/rood-reverseWin/tools/verification/matching.py check --source src/BATTLE/BATTLE.PRG/3A1A0.c --function vs_battle_animateCharacterJump
```

## Remaining GTE Blocker: 2026-10-08

- Canonical [38C1C.c](src/BATTLE/BATTLE.PRG/38C1C.c) remains unchanged at
  SHA-256 `a8f7748bee0e6a006cedd6b19ca5006366ac9a7350d7e2a112680486905a9a27`.
  Its four accepted local matches are preserved; this investigation accepts
  no additional function or broader completion scope.
- Inspected the remaining func_800A1C10 retail range: CPU
  `0x800A1C10..0x800A1DE8`, file `0x39410..0x395E8`, 472 bytes, end exclusive,
  and its saved hash-verified Ghidra record. Retail directly executes COP2
  matrix/vector loads, `MVMVA`, and MAC result stores in the function.
- The SDK's `gte_*` implementations in
  [inline_c.h](include/psx/inline_c.h) expand to `__asm__ volatile`, not genuine
  C-only intrinsics. Inspecting exposed builtin names in the configured GCC
  `2.7.2-cdk` binary found no GTE/COP2 intrinsic. This does not prove that no
  future matching-compatible compiler implementation could exist.
- Tested an isolated, assembly-free C candidate using existing
  `SetRotMatrix`, `SetTransMatrix`, and `RotTrans` calls. It compiled and all
  target relocations resolved, but the full comparison returned
  `CANDIDATE_DIFF`: 404 compiled / 472 retail bytes. The generated candidate
  has library `jal` instructions at CPU `0x800A1D38`, `0x800A1D40`, and
  `0x800A1D50`; retail embeds the COP2 operations instead.
- Candidate:
  [38C1C-gte-library-candidate.c](build/38C1C-gte-library-candidate.c).
  Report: `~/.cache/vagrant-matching/reports/candidates/38C1C-func_800A1C10.json`.
  This is a failed matching experiment, not accepted reconstruction,
  behavioral validation, or Windows-native integration.
- The normal positive control matched func_800A249C; the negative control
  found exactly the intentional changes at `+0x44` / `+0x4C` in func_800A291C
  before the experiment. No checker, expected bytes, ranges, compiler flags,
  canonical implementation, or original binary was changed.
- func_800A1C10 and the projection routine func_800A141C remain `BLOCKED`
  under the combined no-inline-assembly and exact-matching requirements.
  Those requirements have not been relaxed. Work continues separately in
  the neighboring [3A1A0.c](src/BATTLE/BATTLE.PRG/3A1A0.c), one routine at a
  time; this unit is not declared complete.

## Latest Continuation: 2026-10-08

This entry supersedes the input hashes and three-function count in earlier
entries. Their implementation and verification history remains unchanged.

### Accepted: func_800A1DE8

- Replaced this function's `INCLUDE_ASM` in
  [38C1C.c](src/BATTLE/BATTLE.PRG/38C1C.c) with readable C and a typed view of
  the verified scratchpad layout. No new inline assembly, instruction words,
  assembly wrappers, or assembly inclusions were introduced.
- Fresh canonical `MATCH`: 676 compiled / 676 retail bytes, zero differences,
  GCC `2.7.2-cdk`. Complete BATTLE range: CPU `0x800A1DE8..0x800A208C`, file
  `0x395E8..0x3988C`, end exclusive.
- Local accepted progress is now four functions / 2,532 bytes, an increase of
  one function / 676 bytes. All three earlier local functions were rechecked
  against the final source and regenerated symbol inputs.
- This is function-level matching only. No source-unit, linked-overlay,
  independent behavior, Windows-native integration, or gameplay completion
  is claimed. Remaining unfinished code in this unit is func_800A1C10's
  assembly inclusion and func_800A141C's two projection assembly blocks.

### Matrix-Provider Evidence

- Analyzed the saved, hash-verified Windows Ghidra record from
  `build/ghidra/20261007-225349-852` and checked it against retail instructions.
  Verified callers are CPU `0x800A17E8` and `0x800A1C88`.
- Preserved actor, fallback-object, weapon, and selector-`0xFF` paths; null
  returns; full 32-byte matrix copying including padding; and scratch addresses
  `0x1F8003A8`, `0x1F8003B0`, and `0x1F8003C0`.
- Selector `0xFF` halves the signed coordinates at actor offsets
  `0x84C..0x850` with arithmetic right shift, scales them by the signed value
  at `0x183C` with division by 4096 toward zero, calls the existing actor
  transform helper, then sign-extends its output halfwords into translation.
  Verified local half-coordinate values were retained without extra reloads.
- Normal paths preserve the existing matrix-library calls at SLUS CPU
  `0x80041C68`, `0x80041598` (`SetRotMatrix`), `0x80041578` (`SetTransMatrix`),
  and `0x800417EC` (`RotTrans`). The first callee left-multiplies rotation,
  rather than transposing it. The translation path uses signed low-halfword
  differences and zero GTE translation. No native replacement was substituted.
- Kept the legacy third MATRIX pointer parameter, marked unused: retail
  overwrites `a2` at `0x800A1DEC`. It is not a verified input or destination.
- Consulted DataCrystal's saved SHP structure documentation for skeletal
  context; original instructions, actual types, and consumers establish the
  implemented layout and arithmetic. Unknown anchor semantics remain unnamed.
- Added only the independently established global addresses
  `D_800F45E0 = 0x800F45E0` and `D_800F49B8 = 0x800F49B8` to the BATTLE symbol
  map. Both were required by actual target relocations and checked against
  the retail load/address-construction sequences. Regenerated splat artifacts
  before comparison; no expected range, byte, checker, or toolchain setting
  was altered.
- Normal reconstructed-C positive control matched func_800A249C; the deliberate
  negative control differed only at `+0x44` / `+0x4C` in func_800A291C before
  reconstruction. Candidate iterations were checked immediately, then the
  full matching candidate was promoted and immediately checked canonically.

### Current Canonical Reports

Reports reside under `~/.cache/vagrant-matching/reports` in Ubuntu WSL. All
four have status `MATCH`, canonical mode, zero differences, current dependency
hashes, complete ranges, verified relocations, and exact commands/settings.

| Function | Bytes | Report | Generated UTC |
| --- | --- | --- | --- |
| func_800A1DE8 | 676 | `38C1C-func_800A1DE8.json` | 2026-10-08T05:15:48.133293Z |
| func_800A208C | 1,040 | `38C1C-func_800A208C.json` | 2026-10-08T05:16:39.935768Z |
| func_800A25EC | 420 | `38C1C-func_800A25EC.json` | 2026-10-08T05:17:34.568343Z |
| func_800A2790 | 396 | `38C1C-func_800A2790.json` | 2026-10-08T05:18:09.414459Z |

- Current source SHA-256:
  `a8f7748bee0e6a006cedd6b19ca5006366ac9a7350d7e2a112680486905a9a27`.
- Common snapshot SHA-256:
  `45900549decab261ba3947b861a450e8b78a0943c549ca948d472bb5fa0799b4`.
- Unchanged checker SHA-256:
  `66c6bf156535ce927d16b020150f9136e6db897877d802ab5ae2864f51b6b32b`.
- Current splat manifest SHA-256:
  `85000465a503dc592d9690e91ace7a68c25e6c84d5374d1ba68241eda562906d`.

Reproduce from the Windows workspace root:

```powershell
wsl.exe -d Ubuntu --exec /usr/bin/timeout 180 /home/judee/.cache/vagrant-matching/venv/bin/python /mnt/d/Dev/NativePort/VS-NativePort/resources/rood-reverseWin/tools/verification/matching.py check --source src/BATTLE/BATTLE.PRG/38C1C.c --function func_800A1DE8
```

### Work Order

[2EA3C.h](src/BATTLE/BATTLE.PRG/2EA3C.h) contains a two-field geometry-offset
type and three declarations for scene initialization and geometry-offset
access. It is an interface, not a pending executable function body. Offset-based
filenames do not require ascending-address decompilation. The active unit was
continued because its remaining bodies, callers, Ghidra evidence, and validated
function checks already provided a concrete, bounded implementation target.
The header was not changed.

## Latest Session: 2026-10-08

This entry supersedes the dated counts, hashes, and blocked status below.
Earlier sessions remain as historical evidence.

### Accepted: func_800A208C

- Replaced only this function's `INCLUDE_ASM` in
  [38C1C.c](src/BATTLE/BATTLE.PRG/38C1C.c) with readable C. The implementation
  introduces no assembly, instruction words, volatile accesses, replacement
  hardware wrappers, or new assembly inclusions.
- Fresh canonical result: `MATCH`, 1,040 compiled / 1,040 retail bytes, zero
  differences, GCC `2.7.2-cdk`. Complete BATTLE range: CPU
  `0x800A208C..0x800A249C`, file `0x3988C..0x39C9C`, end exclusive.
- Measurable local progress: three accepted functions, totaling 1,856 bytes,
  up from two functions / 816 bytes. The other two functions were freshly
  rechecked after this source change. No source-unit, linked-overlay,
  independent behavioral, or native-runtime completion is claimed.
- The unit still contains two unfinished assembly inclusions, func_800A1C10
  and func_800A1DE8, and the two projection assembly blocks in func_800A141C.
  No other function, shared header, checker, symbol map, or platform code was
  changed during this reconstruction.

### Evidence And Implementation

- Reused the hash-verified Windows Ghidra analysis in
  `build/ghidra/20261007-225349-852`, its original instruction records, and
  retail call references at `0x800B63F0` and `0x800B6490`.
- Consulted the saved DataCrystal Script Opcodes dump and nearby argument
  forwarding. It corroborates the movement context but does not document
  these argument details; unknown flags and fields retain their existing names.
  The actor-data page describes a different record and was not substituted
  for the verified model structure.
- Kept the verified state guard, minimum speed 18, packed destination word,
  existing resolver, table bytes at `0x800E90AC` / `0x800E90B4`, separate height
  paths, unsigned wrapped products, signed division, halfword stores, frame
  count, and animation/state/facing callback order.
- Reduced the candidate from 28 differing words to six by using one phase
  temporary for disjoint magnitude, table-index, and velocity phases; to two
  by correcting equivalent subtraction sequencing; then to zero by storing
  X/Z velocity before Y. The latter makes GCC reload the signed committed Z
  halfword at CPU `0x800A22DC`, rather than sign-extending the division result.
  The resulting machine instructions and complete range match retail exactly.
- The temporary is named `phaseValue`, not misleadingly named after one of
  its phases. The local retry label and original two-word unused frame space
  are retained. No altered compiler settings, arbitrary symbol addresses,
  masked differences, narrowed ranges, or original assembly body was used
  to obtain the match.
- Bounded exploration used a private WSL copy of unchanged, hash-verified
  inputs and the unmodified checker. Its reconstructed-C positive and
  deliberate negative controls passed, and its baseline reproduced the
  existing 28-word difference. Staged results were candidates only.
  The winning candidate then passed the normal current-Windows-input checker
  before promotion, followed immediately by the canonical check.
- Before edits, the normal positive control matched all 216 bytes of
  func_800A249C; the negative control reported exactly the deliberate changes
  at `+0x44` and `+0x4C` in func_800A291C. The checker and retail inputs remain
  unchanged. The external resolver's existing stack-pointer defect is outside
  this unit and was not repaired or assigned an invented fallback.

### Fresh Canonical Reports

All three reports are under `~/.cache/vagrant-matching/reports` in Ubuntu WSL,
status `MATCH`, canonical mode, zero differences. Each retains the complete
comparison, verified relocations, tool hashes/settings, linker invocation,
retail binary hash, dependency hashes, and command.

| Function | Bytes | Report | Generated UTC |
| --- | --- | --- | --- |
| func_800A208C | 1,040 | `38C1C-func_800A208C.json` | 2026-10-08T04:48:28.033154Z |
| func_800A25EC | 420 | `38C1C-func_800A25EC.json` | 2026-10-08T04:49:49.613914Z |
| func_800A2790 | 396 | `38C1C-func_800A2790.json` | 2026-10-08T04:50:33.068831Z |

- Current source SHA-256:
  `8a0c21ba8f5547d04269f1c80f434abdac539e4201c637a2e537ae6479b37990`.
- Common snapshot SHA-256:
  `1b055ba21fba011448bfbb4967c04dfdba6061e924b628da814adb28e7a6ab1b`.
- Unchanged checker SHA-256:
  `66c6bf156535ce927d16b020150f9136e6db897877d802ab5ae2864f51b6b32b`.
- Unchanged splat manifest SHA-256:
  `9fe169b7924d49e6255d7afffab39ee409e9e923f0fc1cd7982f5adc6e88c886`.
- Compiler SHA-256:
  `90db2db7bcfe372244ca4d9785a7bfbf7613d711f69ee3bfe6c91d82a85e611d`.

Reproduce from the Windows workspace root:

```powershell
wsl.exe -d Ubuntu --exec /usr/bin/timeout 180 /home/judee/.cache/vagrant-matching/venv/bin/python /mnt/d/Dev/NativePort/VS-NativePort/resources/rood-reverseWin/tools/verification/matching.py check --source src/BATTLE/BATTLE.PRG/38C1C.c --function func_800A208C
```

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
