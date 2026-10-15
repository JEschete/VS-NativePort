# GameCube/Wii: decomp-toolkit and porting research

Research date: 2026-10-14 (US Eastern). Upstream documentation reviewed;
no disc opened and no build or port tested.

## Recommendation

**Use decomp-toolkit as the first matching-decompilation candidate.**
[decomp-toolkit](https://github.com/encounter/decomp-toolkit), commonly `dtk`,
is explicitly designed for GameCube/Wii decompilation. Its
[template](https://github.com/encounter/dtk-template) provides a project
structure instead of requiring a new build system from scratch.

## Documented strengths

- DOL analysis and splitting into relocatable objects.
- REL inspection/handling and analysis-oriented combinations with the main
  executable.
- CodeWarrior symbol demangling and map-file inspection.
- Disc inspection/extraction/verification and common archive/compression tools.
- Integration with objdiff and matching-project workflows.
- A portable single-binary tooling goal.

This is much closer to a platform-aware decompilation workflow than feeding a
raw PowerPC image into a generic agent.

## Matching requirements

The upstream explanation explicitly identifies CodeWarrior as the usual
GameCube/Wii matching compiler. Correct compiler/linker versions, flags,
translation-unit boundaries, relocations, and library behavior still matter.

The toolkit does not grant compiler or SDK redistribution rights. Use legally
available inputs and do not package proprietary SDK material.

For Windows, review the chosen release's assets and the template's exact host
requirements. A portable analyzer does not prove every matching compiler and
build helper works natively without additional setup. No installation was
tested here.

## Static analysis and runtime evidence

Use [Ghidra](CROSS_SYSTEM_STATIC_TOOLS.md) with the correct Gekko/Broadway
instruction semantics, format loading, REL identities, and address mappings.
Use [Dolphin debugging](MULTI_SYSTEM_DEBUGGERS.md) to correlate the static model
with actual execution.

GameCube and Wii are not merely generic PowerPC: paired-single instructions,
graphics, audio/DSP, OS behavior, DMA, and dynamic modules need attention. Wii
also has important IOS/ARM-side interactions that a PPC-only translation does
not replace.

## Recompilation watch list, not an accepted solution

[NWiiRecomp](https://github.com/BlackLineInteractive/NWiiRecomp) describes
DOL/ELF-to-C++ translation and its own runtime/HLE stack. Its README contains
strong universality claims while also describing partial boot milestones and
an interpreter fallback for runtime code.

Treat it as an **experimental project to inspect**, not evidence that arbitrary
GameCube/Wii games can already be ported automatically. Validate instruction
coverage, HLE contracts, renderer behavior, saves, and actual gameplay on a
selected title before adopting it. Do not repeat universality claims as
verified results.

A static CPU translation with substantial hardware/runtime emulation is a
different architecture and deliverable from a reconstructed source port.

## Bounded first evaluation

Start a separate title-specific research repository. Use dtk to establish the
executable/module layout and a reproducible baseline. Match a known small
function through the correct compiler and objdiff. Capture a controlled
Dolphin observation for a difficult routine. Only then choose between pursuing
matching source and experimenting with a recompilation/runtime route.

## Sources

- [decomp-toolkit README and commands](https://github.com/encounter/decomp-toolkit/blob/main/README.md)
- [dtk project template](https://github.com/encounter/dtk-template)
- [Dolphin](https://github.com/dolphin-emu/dolphin)
- [NWiiRecomp claims and current implementation description](https://github.com/BlackLineInteractive/NWiiRecomp/blob/master/README.md)
