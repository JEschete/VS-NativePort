# Ghidra and the PS1 loader

Research date: 2026-10-14 (US Eastern). Source-verified capabilities, not a
tested installation or a successful Vagrant Story import.

## Recommendation

**High-priority static-analysis pilot.** Use
[Ghidra](https://github.com/NationalSecurityAgency/ghidra) with the
[PS1 loader extension](https://github.com/lab313ru/ghidra_psx_ldr) to investigate
remaining functions, data structures, cross-references, and overlay boundaries.
Evaluate it manually before adding an MCP bridge.

Ghidra provides disassembly, decompilation, graphing, scripting, and automated
analysis on Windows. Its MIPS language definitions include little-endian
32-bit analysis. The PS1 extension adds PSX-specific loading and analysis; its
README documents overlay handling, PSYQ signatures, and GTE macro support.

## Why it fits this project

- Inspect a difficult unmatched function alongside its callers and data users.
- Apply known structures and function signatures to improve the readability
  of indirect calls and address arithmetic.
- Separate SDK/library code from game-specific code using cautiously checked
  PSYQ signature matches.
- Investigate GTE-related routines with PS1-aware analysis rather than treating
  coprocessor instructions as unexplained generic operations.
- Keep analysis projects independent of the maintained C snapshot.

This is a semantic aid. Decompiled pseudocode is neither original C nor code
that necessarily recompiles into the retail instruction sequence.

## Windows setup approach

1. Choose a released Ghidra version and an extension build targeting it.
   The loader README documents building through Eclipse/GhidraDev and installing
   the resulting ZIP using Ghidra's extension manager. Confirm the actual
   release/build requirements; do not assume an old extension ZIP works with a
   newer Ghidra.
2. Install the full JDK required by that Ghidra release. At research time the
   Ghidra development README requests JDK 25; other integrations document
   Ghidra 12.1.4 with older JDK requirements. Do not mix those instructions.
3. Launch the Windows distribution with `ghidraRun.bat`.
4. Create a local analysis project outside the maintained upstream snapshot.
   Do not commit its database or imported retail bytes.
5. Start with one known function and compare its decoded instructions against
   the repository's trusted disassembly before analyzing harder code.

No Ghidra, JDK, Eclipse, or extension was installed for this writeup.

## Critical PS1 import rules

- Use the verified executable load address and entry point. A file offset, a
  runtime RAM address, and a host pointer are not interchangeable.
- Import overlays as distinct programs or correctly isolated overlay blocks.
  Different files can occupy the same RAM range at different times.
- Retain the overlay identity with every address-based note. Never merge
  symbols just because their numeric addresses match.
- Check little-endian MIPS-I decoding, branch/load delays, and GTE output
  against known examples. A readable decompilation is not sufficient.
- Verify SDK signature hits against surrounding instructions and the expected
  library version. A recognized name is a hypothesis, not permission to replace
  game code with a guessed SDK implementation.
- For old Ghidra projects, the extension documents special reanalysis steps
  for stored GTE instruction/p-code information. Prefer a fresh pilot project
  rather than assuming an inherited database has current semantics.

## A bounded first experiment

Choose one already-matched function and one nearby unmatched function from the
same overlay. Record file identity, load mapping, function range, tool versions,
known symbols, and any imported types. Confirm that the matched function's
disassembly agrees with the accepted reference.

For the unmatched function, export only local notes about control flow, field
offsets, signedness, and unresolved assumptions. Test proposed C changes through
the existing compiler and object-comparison workflow. If a conclusion depends
on runtime state, pair it with [PCSX-Redux](PCSX_REDUX.md) evidence.

Success means a trustworthy analysis setup and a falsifiable hypothesis.
Only fresh matching verification can turn a source change into accepted
decompilation progress.

## Sources

- [Ghidra README and Windows/JDK requirements](https://github.com/NationalSecurityAgency/ghidra/blob/master/README.md)
- [Official Ghidra releases](https://github.com/NationalSecurityAgency/ghidra/releases)
- [MIPS language definitions](https://github.com/NationalSecurityAgency/ghidra/blob/master/Ghidra/Processors/MIPS/data/languages/mips.ldefs)
- [PS1 loader setup, overlays, signatures, and GTE notes](https://github.com/lab313ru/ghidra_psx_ldr/blob/master/README.md)
- [PSYQ signature project referenced by the loader](https://github.com/lab313ru/psx_psyq_signatures)
