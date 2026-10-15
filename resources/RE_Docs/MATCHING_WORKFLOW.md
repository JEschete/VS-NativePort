# objdiff and the existing matching toolchain

Research date: 2026-10-14 (US Eastern). Documentation research only;
no build, permutation search, or progress measurement was performed.

## Recommendation

**Keep this as the acceptance spine, not a competing toolchain.**
The project already credits splat, maspsx, old-gcc, objdiff, decomp-permuter,
decomp.me, and decomp.dev. The opportunity is to combine their roles
carefully with better static/runtime evidence, not to replace them with an
AI-generated functional reimplementation.

## Roles

| Tool | Appropriate role | What it does not establish |
| --- | --- | --- |
| splat | Existing binary splitting/disassembly configuration | Recovered source semantics by itself |
| maspsx | Existing PSX assembly/toolchain compatibility pipeline | A substitute for the original compiler |
| old-gcc | Historical compiler tooling/reference | Availability of the required Windows compiler executables |
| objdiff | Expected/current relocatable-object comparison and progress reporting | Correctness of stale inputs or authenticity of the compiler invocation |
| decomp-permuter | Local search over C variants near a match | A solution to unknown logic or incorrect types |
| decomp.me | Existing external scratch ecosystem | Permission to upload game code, assembly, or private context |
| decomp.dev | Existing progress presentation | Fresh verification or independently certified behavior |

The project README remains authoritative about accepted build paths and missing
historical Windows compiler prerequisites. Do not introduce unsupported compiler
fallbacks to make an analysis tool appear usable.

## objdiff

[objdiff](https://github.com/encounter/objdiff) explicitly lists PS1 in its MIPS
support. It compares relocatable objects, including functions and data, and
supports configured rebuilds, highlighted instruction/register comparisons, and
progress reporting.

Its `objdiff.json` configuration maps expected and current objects using
`target_path` and `base_path`; build/watch behavior is configurable. Reuse the
project's existing generated configuration and reports. Do not casually reverse
the objects or mark a unit complete just because the UI looks close.

For every accepted result, record:

- Expected input identity and source provenance.
- Compiler variant, flags, and assembler pipeline.
- The rebuilt source and current object identity.
- Overlay/unit/function identity and exact matched byte counts.
- The report generation time and whether the objects were rebuilt freshly.

Whole-output byte-exact verification remains a separate project gate where
required. Relocation-aware object similarity and a linked retail-output
comparison answer related but not identical questions.

## decomp-permuter

[decomp-permuter](https://github.com/simonlindholm/decomp-permuter) offers random
and explicit macro-directed C permutations scored against target assembly.
Upstream recommends it as a complement to manual matching, especially near the
end when register allocation is the main remaining difference.

Important caveats:

- MIPS support is described as IDO and "possibly GCC"; the project's historical
  PSX GCC variants need a real compatibility trial.
- Its normal import setup includes a compile script and object tools. Do not
  assume a repository containing `.sh` workflows runs unchanged on Windows.
- The distributed permuter@home mode explicitly does not work on native
  Windows. That is a restriction of that mode, not proof that every local
  search mode is unavailable.
- Default scoring can ignore stack differences; use final object/byte
  verification even when search reports score zero.
- A search cannot fix an invalid memory model, guessed data, wrong overlay,
  or an incorrect algorithm just by shuffling equivalent-looking expressions.

Keep experiments bounded in a scratch directory. Do not let search mutate the
maintained source or hide failed compiler invocations. Any candidate promoted
to source must be understood and freshly verified.

## Suggested combined loop

1. Identify a remaining mismatch from a fresh report.
2. Establish the routine's types/control flow using
   [Ghidra](GHIDRA.md), [m2c](M2C.md), and existing code.
3. Resolve runtime unknowns with [PCSX-Redux](PCSX_REDUX.md) if needed.
4. Compile a C candidate through the exact accepted toolchain.
5. Use objdiff to classify the remaining mismatch: semantics, types,
   instruction selection, scheduling, register allocation, or data layout.
6. Consider bounded permutation search only for a genuinely near-matching case.
7. Rebuild and run the required acceptance comparison; retain provenance and
   update progress only from the accepted result.

No new public scratch upload, cloud worker, dependency, build command, or
compiler substitution is authorized by this document.

## Sources

- [objdiff README, PS1 support, configuration, and licensing](https://github.com/encounter/objdiff/blob/main/README.md)
- [decomp-permuter README, scoring, intended use, and Windows caveat](https://github.com/simonlindholm/decomp-permuter/blob/master/README.md)
- [splat](https://github.com/ethteck/splat)
- [maspsx](https://github.com/mkst/maspsx)
- [old-gcc](https://github.com/decompals/old-gcc)
- [Project build requirements and acceptance rules](../../README.md)
