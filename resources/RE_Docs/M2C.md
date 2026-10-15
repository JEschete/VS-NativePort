# m2c: MIPS assembly to candidate C

Research date: 2026-10-14 (US Eastern). Upstream capabilities reviewed;
no function was translated, compiled, or matched during this research.

## Recommendation

**Targeted pilot for difficult remaining functions.**
[m2c](https://github.com/matt-kempster/m2c) translates assembly into candidate
C, with matching decompilation among its explicit goals. It documents 32-bit
MIPS support, little-endian O32 (`mipsel`), and compiler-specific GCC heuristics.

Unlike [Ghidra](GHIDRA.md), it consumes assembly text rather than importing and
disassembling the game's executable. Upstream describes GNU assembler-style
input such as that emitted by spimdisasm, which makes it a plausible companion
to the repository's existing splat-based workflow.

## What it can help with

### Beyond PlayStation

m2c is not limited to MIPS. Its documented platform targets also include:

| Target | Documented model | Candidate systems from the F drive |
| --- | --- | --- |
| `mips` | MIPS O32 | N64 work where the actual ABI matches |
| `mipsel` | Little-endian MIPS O32 | PS1 and some PSP work, after checking extensions |
| `mipsee` | Little-endian MIPS eabi64 | PS2 EE experiments, with separate instruction/runtime validation |
| `ppc` | Big-endian PowerPC | GameCube/Wii candidate C, not automatic console support |
| `arm` | Little-endian ARM | DS/3DS CPU routines with the correct ISA/ABI/context |
| `gba` | Little-endian ARM with APCS | GBA routines |
| `sh2` | Big-endian SuperH | Saturn/32X CPU routines |

These are CPU/compiler starting points, not verified complete-system workflows.
In particular, `sh2` does not establish Dreamcast SH-4 support, and N64's 64-bit
register semantics require checks even when code uses a compatible calling
convention. Assembly-era 8-bit games are generally a disassembly problem before
they are a C decompilation problem.

- Produce an initial control-flow and expression hypothesis for a routine.
- Incorporate a context file of known types and declarations.
- Infer provisional structures and generate referenced global/data
  initializers.
- Iterate on stack-variable layout with documented stack-structure support.
- Provide a candidate that can be immediately challenged by the actual
  historical compiler and object comparison.

Upstream explicitly warns that output can be suboptimal, especially for loops.
Correct-looking output is neither a source recovery guarantee nor a match.

## Applicability and Windows constraints

PS1's little-endian MIPS-I CPU and this project's historical GCC pipeline make
the documented `mipsel` platform and `gcc` compiler family the sensible starting
choices. This is a project-specific inference, not a tested PS1 preset or a
claim that all GTE instructions and compiler quirks are supported.

Confirm the selected release's target syntax and interpreter/dependency
requirements. The expected combined target form is `mipsel-gcc-c`, but validate
it with that release's help before constructing a command. No Python
environment was changed for this writeup.

Native Windows execution and the exact historical GCC pairing were not tested.
Keep m2c in a separate analyst environment if evaluated; do not change the
matching compiler to accommodate it or introduce WSL as a native-build
dependency.

## First experiment

1. Select one matched control function and one unmatched neighbor in the same
   overlay. Use trusted generated assembly rather than pasted, normalized, or
   guessed instructions.
2. Supply a minimal context containing the relevant existing types, function
   declarations, and globals. Avoid importing conflicting declarations.
3. Record tool revision, target selection, context identity, and input assembly.
4. Evaluate the known function first. Check signedness, arithmetic width,
   branch/load-delay interpretation, and calls against the accepted source.
5. Translate the unknown function into an ignored scratch location, retaining
   explicit unknown fields instead of manufacturing authoritative names.
6. Compile the hypothesis using the correct historical compiler, flags, and
   assembly pipeline; inspect differences through
   [the matching workflow](MATCHING_WORKFLOW.md).
7. Rewrite only what evidence supports. Address type/control-flow differences
   before considering permutation search.

## Limits

- m2c does not identify load addresses, unpack archives, or manage overlay
  lifetime.
- An inferred structure is not proof of field semantics or bounds.
- Generated globals are suggestions, not replacements for verified retail
  tables or permission to add generic placeholder buffers.
- Compiler-family support does not establish compatibility with the project's
  specific 2.7.2/2.8.1 PSX compiler variants.
- Do not submit retail assembly or private project context to external scratch
  services as a substitute for local evaluation.

Upstream's license is GPLv3. Review actual licensing implications before
redistributing tool code or incorporating components; this recommendation
concerns local use as an analysis tool.

## Sources

- [m2c README: goals, targets, context, stack structures, limitations](https://github.com/matt-kempster/m2c/blob/master/README.md)
- [m2c license](https://github.com/matt-kempster/m2c/blob/master/LICENSE)
- [Existing project toolchain credits](../../README.md#credits)
