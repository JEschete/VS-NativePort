# PSX-SPX: hardware reference, not a validation tool

Research date: 2026-10-14 (US Eastern). Reference documentation reviewed;
no emulator or physical-hardware test was performed.

## Recommendation

**High-value reference for interpreting low-level code and captures.**
[PSX-SPX](https://psx-spx.consoledev.net/) collects information about the PS1
memory/I/O map, CPU, GPU, GTE, MDEC, SPU, DMA, timers, CD-ROM, BIOS, controllers,
and memory cards.

Use it alongside the project's [PS1 notes](../../docs/PSX_NOTES.md), trusted
disassembly, and measured execution. It is especially relevant to the later
native hardware layer, but also helps explain remaining decompiled routines.

## Concrete uses

| Question | Relevant reference area | Required follow-through |
| --- | --- | --- |
| What does this hardware-address access mean? | Memory and I/O maps | Check width, masking, and actual access sequence |
| Why do GTE outputs differ? | GTE operations, flags, precision | Capture complete input/output state and retained registers |
| Why does a transfer stall or reorder? | DMA, interrupts, CD-ROM, timers | Record state transitions and timing in a controlled run |
| Why does an image decode but look wrong? | GPU/VRAM and MDEC formats | Compare actual pixel/packet data, not just a frame count |
| Does a memory-card operation really persist? | SIO/controllers/cards and BIOS | Test full transaction and durable reload with disposable data |

The table is a project-specific application of the reference, not a claim that
all these contracts are already implemented.

## Source quality and provenance

Prefer the site's stable reference. At research time, the repository README
warns that development-branch content includes AI-written material that has
not all received human review. Record the exact page/version used and check
surprising claims against independent evidence.

The maintainers also disclose unresolved copyright/licensing provenance and
non-clean-room material in the reference's history. Linking to a reference
does not make its contents permissively reusable. Do not copy chapters,
tables, or code into this repository, and do not present it as a clean-room
specification or a settled legal basis for redistribution.

This writeup summarizes the resource's role; it does not reproduce the
underlying hardware documentation.

## Recommended use

1. Start from an observed instruction, address, or hardware command.
2. Find the relevant stable reference section and record the specific claim.
3. Separate documented behavior, emulator behavior, and unresolved assumptions.
4. Construct a bounded test with explicit preconditions and expected state.
5. Cross-check emulator-specific surprises and use physical hardware when
   required to establish hardware fidelity.
6. Preserve an original test/analysis explanation rather than copying reference
   prose into source comments.

A reference lookup is not a test pass. Neither PSX-SPX nor an emulator trace
can by itself establish complete hardware compatibility.

## Sources

- [Stable reference and provenance notice](https://psx-spx.consoledev.net/)
- [Maintained repository and branch-quality warning](https://github.com/psx-spx/psx-spx.github.io)
- [Original no$psx hardware reference](https://problemkaputt.de/psx-spx.htm)
- [Related manual debugger guide](NO_PSX.md)
