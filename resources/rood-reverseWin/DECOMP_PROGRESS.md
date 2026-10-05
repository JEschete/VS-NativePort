# Local Decompilation Tracking

## Current File: BATTLE/BATTLE.PRG/38C1C.c

- Attempt started and rejected: 2026-10-04. Assistant changes were rolled back;
  the source unit was restored to its pre-edit baseline before the new attempt.
  No second unit started.
- Analysis restarted: 2026-10-04. Windows Ghidra inspection now succeeds using
  the existing 12.1.2 PSX extension.
- Acceptance status: NOT ACCEPTED. Byte-exact verification is required by the
  user's acceptance criterion; zero routines from this attempt are verified
  byte matches. The earlier source-complete wording did not satisfy that gate
  and must not be treated as completed decompilation progress.
- Native candidate status: all 17 routines are represented in readable
  native source; all five assembly inclusions and the projection routine's
  inline assembly have been replaced. The whole file compiles directly as
  Win32 C++ with no assembly-discard macros. Sixty-three focused behavior groups
  pass and exercise every routine. Gameplay integration, external-service
  fidelity and hardware comparison remain unverified. The WSL historical
  matching build was attempted and failed; no byte comparison was produced.
- The native GTE service calls changed the historical build contract. Merely
  moving declarations to satisfy old C syntax would not establish byte identity
  or resolve those native service relocations. Ghidra evidence and these focused
  tests are not substitutes for per-function matching verification.
- Original module: file offsets `0x38C1C..0x3A1A0`, CPU addresses
  `0x800A141C..0x800A29A0` (end exclusive).
- Both local BATTLE.PRG copies match configured SHA-1
  `d53aaccc3b3a2fc057d05e0dcea92f7182bc72a9`.
- Evidence: raw MIPS instructions, nearby types/callers, DataCrystal executable
  map, and SLUS-01040 disc metadata. Original assets remain ignored.

| Routine | CPU Address | File Offset | Reconstruction | Validation |
| --- | --- | --- | --- | --- |
| func_800A141C | 0x800A141C | 0x38C1C | Reconstructed projection with packed screen position and raw SZ3 | Actor/object modes and saturation tests passed |
| func_800A1C10 | 0x800A1C10 | 0x39410 | Reconstructed local bone offset and MAC-result position | Real matrix-provider/GTE path tested |
| func_800A1DE8 | 0x800A1DE8 | 0x395E8 | Reconstructed selection, view composition and scratchpad return; unused parameter retained | Direct layout, matrix, selection and special-path contract tests passed |
| func_800A208C | 0x800A208C | 0x3988C | Reconstructed target decoding, trajectory tables, flight-time loops, flags and callbacks | Target forms, rising/falling, speed clamp and call-order tests passed |
| func_800A25EC | 0x800A25EC | 0x39DEC | Reconstructed terrain movement and state transition | Rejection, 63/64 height boundary and callback-order tests passed |
| func_800A2790 | 0x800A2790 | 0x39F90 | Reconstructed wrapped fixed-point movement and animation selection | Overflow, rounding, coordinate/tile and animation tests passed |

## Required Approach

- Follow AGENTS.md: meaningful, human-readable C++; inline assembly is not
  acceptable. Use Ghidra before reconstructing each function and check findings
  against actual instructions, data, and callers.
- No second source unit was started. The rejected assembly-based edits are not
  retained as candidates. The current native reconstruction is unaccepted;
  do not advance to another unit on the strength of its Windows tests alone.

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

## First Routine Findings

These findings concern BATTLE.PRG, not another overlay at the same CPU addresses.
They establish a reconstruction contract, not behavioral or byte-match proof.

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

### Implemented And Tested Unit

- [38C1C.c](src/BATTLE/BATTLE.PRG/38C1C.c) contains all 17 routines with the
  previously unfinished algorithms reconstructed. The legacy file name and
  exported symbol names are preserved. New routines use descriptive parameters
  and existing GTE services, not assembly/opcode dispatch or pasted decompiler
  output. A shared offset-mode helper captures the verified full/half behavior.
- The implementation uses the existing native software-GTE service at
  `native/platform/inline_gte.c`; its transform is explicitly
  `gte_op_mvmva(1, 0, 0, 0, 0)`, consistent with original instruction
  `0x4A480012`. MAC outputs and GTE register/flag side effects are retained.
- Wrapped 32-bit negation is handled explicitly for INT_MIN before signed
  division; negative odd halves truncate toward zero. Native halfword casts
  are checked under the verified 32-bit Windows compiler/ABI, not assumed
  portable to arbitrary platforms.
- DataCrystal's saved SHP-format joint section calls offset zero a four-byte
  bone-size field. This corroborates the word-sized field but does not establish
  signedness, stride, or mode behavior; those come from the BATTLE instructions
  and consumers. The existing 16-byte armature stride and `0x40` field base are
  checked with static assertions, as are actor/object model-pointer offset
  `0x68`, weapon model-pointer offset `0x1C`, and MATRIX size/translation offset.
- [test_battle_position.cpp](tools/verification/test_battle_position.cpp)
  compiles the whole actual source as C++, uses real game types and the actual
  software GTE, and maps the scratchpad at the required Windows address. The
  real reconstructed matrix provider is exercised. External terrain, special
  actor-transform, animation, audio and notification calls use controlled test
  fixtures/observers; their implementations are outside this unit and are not
  being claimed as reconstructed. No assembly is suppressed in the harness.
- Sixty-three passing behavior groups cover absent objects, translation
  low-halfwords, full and half offsets, negative/positive
  odd and even rounding, unsigned endpoint loads, word-sized half loads,
  INT_MIN wrap, object fallback, actor precedence, missing weapons,
  weapon translation-only behavior, weapon selector remapping, fractional
  rotation rounding, untouched output padding, MAC results despite IR
  saturation, GTE flags, raw SZ3, view-matrix composition, scratchpad return,
  trajectory flight-time loops, terrain rejection, movement-product wrapping,
  tile updates, animation/callback ordering, and the metadata/selector wrappers.
  Exercised routines also include func_800A152C, func_800A1648, func_800A1720,
  func_800A17BC, func_800A190C, func_800A1AF8, func_800A1B28, func_800A1B9C,
  func_800A249C, func_800A2574, and func_800A291C.
- Repeat from the workspace root without the root build/synchronizer:

  ```powershell
  cmake -S resources/rood-reverseWin/tools/verification -B resources/rood-reverseWin/build/verification -G "Visual Studio 17 2022" -A Win32
  cmake --build resources/rood-reverseWin/build/verification --config Release --target test_battle_position
  ctest --test-dir resources/rood-reverseWin/build/verification -C Release -R "^BattlePosition$" --output-on-failure
  ```

- Verified with installed Clang 19.1.5 and a Win32 Visual Studio build. CTest
  passed one executable containing all 63 groups with a 10-second test limit.
  These tests establish focused native behavior under controlled dependencies,
  not original-hardware execution or the complete battle workflow.
- Integration remains separate: the root native CMake currently excludes
  `38C1C.c`. No root build, source synchronization, native-service modification,
  or historical compiler invocation was performed. The old third parameter of
  func_800A1DE8 is retained for ABI compatibility even though the binary ignores it.

### Matrix And Projection Evidence

- func_800A1DE8 spans file `0x395E8..0x3988C`, CPU
  `0x800A1DE8..0x800A208C`, end exclusive. At `0x800A1DEC`, `a2` is overwritten
  with the actor slot from `a0`. The third MATRIX pointer in the existing
  declaration is therefore not a verified input or destination.
- Normal paths copy a selected 32-byte actor/weapon matrix to scratchpad
  `0x1F8003C0`, then use `D_800F49B8` at `0x800F49B8` and matrix routines in the
  main executable. Selector `0xFF` takes a separate construction path. Missing
  actor/object or weapon paths return null. Return address and input/output
  scratchpad fields are now verified and checked by the direct matrix tests.
- SLUS CPU `0x80041C68`, file `0x32468`, left-multiplies rotation columns in
  order 2, 1, 0 using rotation-only MVMVA and saturated IR1..3. It is not a
  simple transpose. SLUS `0x80041598`/file `0x31D98` loads rotation;
  `0x80041578`/file `0x31D78` loads translation. SLUS `0x800417EC`/file `0x31FEC`
  transforms a short vector and stores MAC1..3 plus flags. The provider uses
  zero translation and signed low-halfword differences from the view matrix.
- BATTLE `0x800B07DC`, file `0x47FDC`, is an external actor-transform dependency
  still assembly-only in another unit. The provider passes its special anchor
  through scratchpad `0x1F8003B0`, then returns `0x1F8003C0` with sign-extended
  output halfwords as translation. The external transform's contract is tested
  with a fixture; its full implementation/hardware fidelity is not certified.
- Projection `0x800A141C..0x800A152C`, file `0x38C1C..0x38D2C`, uses RTPS and
  stores SXY2 and raw SZ3 at output offsets zero and four. It must not call the
  existing quarter-depth storage helper. Actor/object fallback and depth
  saturation tests verify this distinction.

### Movement And Layout Evidence

- Trajectory planner: BATTLE CPU `0x800A208C..0x800A249C`, file
  `0x3988C..0x39C9C`. Original table loads at CPU `0x800A223C..0x800A2254` and
  `0x800A2338..0x800A2354` use unsigned bytes. Retail file `0x808AC..0x808B2`
  contains six flight offsets `5,5,5,10,12,12`; file `0x808B4..0x808BA` contains
  six height corrections `63,63,63,30,42,42`, end exclusive. The reconstruction
  retains these verified algorithm constants, not unrelated asset data.
- Terrain movement: CPU `0x800A25EC..0x800A2790`, file `0x39DEC..0x39F90`.
  Clears bit 21 at actor offset `0x08` even on rejection. Height return is
  truncated to signed 16 bits before comparing minus 3000. X/Z commit only
  after a non-null terrain lookup. The 64-unit rise boundary and active-state
  callback path are checked separately.
- Scaled movement: CPU `0x800A2790..0x800A291C`, file `0x39F90..0x3A11C`.
  Signed halfword displacement at `0x1858/0x185A/0x185C` multiplies the word at
  `0x1850`; only the wrapped low word is divided by `2^24` toward zero. Position
  stores wrap to halfwords; X/Z tile division truncates toward zero.
- [3A1A0.h](src/BATTLE/BATTLE.PRG/3A1A0.h) needed a local ABI repair: the first
  eight bits of the `0x5AC` flag word previously mixed byte and integer storage
  types, placing subsequent fields four bytes late in the Win32 C++ verifier.
  The whole flag word now uses 32-bit storage with unchanged bit widths and
  meanings. Static assertions pin animation `0x5B8`, special anchor `0x84C`,
  scale `0x183C`, and movement `0x1848/0x1850/0x1858`, in addition to the earlier
  model/matrix offsets. This is an ABI correction, not a guessed field rename.
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

## Verification Limits

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
  The new function checker is still unvalidated; no matching is claimed.
- Root `tools/windows.py verify-upstream` silently replaces locally edited
  source trees. Do not invoke it, CMake preparation, or Decomp on local work.
  Preserve local changes as a patch outside the upstream replacement set.
- Explicit upstream sync remains upstream-wins. The upstream manifest and
  historical root progress.json are not modified to manufacture matching.
