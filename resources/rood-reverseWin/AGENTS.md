# Decompilation Rules

## Mandatory Byte-Exact Acceptance Gate

- Byte-exact matching is required before accepting decompilation progress.
  Ghidra evidence, confidence, compiler success, and Windows behavioral tests
  cannot substitute for a matching result.
- Before reconstruction, establish that the isolated WSL historical build works
  with the configured compiler, flags, headers, assembler, and linker settings.
  Validate the checker against a known matching reconstructed routine and a
  deliberately mismatching candidate. If either control fails, stop before edits.
- Change one function at a time. After every meaningful implementation edit,
  immediately build and compare its complete original byte range with correctly
  resolved relocations. Do not advance to another function until it passes.
  Shared helper/header edits invalidate matches of affected functions; recheck them.
- Accept only a fresh canonical `MATCH` report tied to current source/input hashes,
  exact toolchain/settings, binary hash, CPU/file range, command, and report path.
  Missing/stale reports, build failure, `DIFF`, timeout, unresolved relocations,
  partial comparisons, and `PROBE_MATCH`/`PROBE_DIFF` mean `NOT ACCEPTED`.
- On failure, diagnose only the current function or verification infrastructure,
  rerun the same check, or report `BLOCKED`. Do not bypass the gate or continue
  implementing other functions. Record actual outcomes in DECOMP_PROGRESS.md.
- After function matches, require byte-exact source-unit and linked-overlay
  comparisons before claiming their completion. Report each verified scope
  separately; function matches do not establish an overlay match.
- Never manufacture a match with original assembly inclusions, modified retail
  bytes, narrowed comparison ranges, ignored differences, invented symbol
  addresses, or changes to expected toolchain settings made merely to pass.
- Matching-compatible PSX source must pass first. Windows-native adaptations are
  separate work and cannot replace or invalidate accepted matching source. If
  readability, no-inline-assembly, and exact-matching requirements cannot all be
  satisfied, report `BLOCKED`; do not silently relax any requirement.
- Use isolated WSL snapshots only; do not rewrite canonical sources from WSL,
  automatically reset/sync, or overwrite user work to make checks pass.
- These are mandatory workflow instructions, not an implemented CI guarantee.
  Until a validated fail-closed check and CI gate exist, do not claim automated
  enforcement. See [DecompRules_Resources.md](DecompRules_Resources.md) for details.

## Existing Decompilation Requirements

- The deliverable is meaningful, human-readable C++ for the Windows native port.
  Prefer descriptive names, explicit types, understandable algorithms, and
  established modular boundaries. Do not present raw decompiler output as the
  finished implementation or disguise instruction transcription as C++.
- Inline assembly is NOT acceptable. Do not add `asm`, `__asm__`, assembly
  wrappers, embedded opcodes, or new assembly inclusions as an implementation.
  Do not use them as a shortcut for handwritten MIPS or GTE instructions.
  Existing upstream assembly is unfinished reference material, not permission
  to introduce more; preserve it when restoring the baseline.
- Use Ghidra to analyze each target before implementing it. Inspect decompiled
  control flow, functions, references, and data types, then verify conclusions
  against original instructions/data and their consumers. Record binary offsets
  and CPU addresses. If Ghidra is unavailable, report the blocker instead of
  silently substituting assembly transcription.
- Reconstruct hardware-dependent behavior as readable C++ using suitable shared
  platform services. Preserve fixed-point arithmetic, rounding, truncation,
  saturation, flags, layouts, and calling semantics where verified; do not invent
  names or semantics for unresolved behavior.
- Work on one source unit at a time and update DECOMP_PROGRESS.md as work occurs.
  Separate readable reconstruction, behavioral validation, function matching,
  and full-overlay matching. Never count syntax checks as behavior proof.
- Windows remains the development, native-build, and runtime platform. WSL is
  permitted ONLY for isolated historical-compiler matching verification. It must
  not modify canonical sources or become a dependency of the native build.
- WSL byte-exact verification must be performed after every routine implementation.
- Keep original game assets and binaries local and ignored. DataCrystal, data,
  discs, and SLUS-01040 are references, not files to overwrite or commit.
- Build preparation must never synchronize or replace source. The root verifier
  is read-only and explicit sync refuses local changes. Preserve user work and
  require explicit direction before synchronization or changes outside this
  working directory; never bypass these safeguards to make a build pass.
