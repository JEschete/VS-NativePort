# no$psx: optional manual debugger cross-check

Research date: 2026-10-14 (US Eastern). Vendor documentation reviewed;
not downloaded or tested.

## Recommendation

**Optional independent manual investigation tool**, after the primary
PCSX-Redux workflow is established.
[no$psx](https://problemkaputt.de/psx.htm) is a Windows PS1 emulator/debugger.
Its official feature list includes a disassembler, debugger, profiler, code
and memory breakpoints, assembler, I/O map viewer, VRAM viewer, polygon viewer,
and TTY debug console.

The integrated viewers and profiler make it worth considering for a narrowly
defined graphics or execution question, rather than adding another emulator
without a concrete experiment.

## Windows and integration considerations

The vendor distributes a Windows 32-bit application. That is a host-tool
property, not a requirement to change this project's compiler architecture.
Use the official distribution independently of the build toolchain.

The reviewed page does not establish a scripted remote-control contract
equivalent to PCSX-Redux's documented GDB/web interfaces. Plan a manual pilot;
do not promise unattended capture or VS Code integration without checking it.

The tool is distributed as freeware, not as a permissively licensed source
component. Do not assume permission to modify, redistribute, or bundle it.

## Useful first experiment

Choose a small, reproducible question already observed in another emulator:
for example, a particular GPU packet, polygon sequence, or hardware-register
access. Record tool version, disc/BIOS identity, configuration, overlay, and
the exact breakpoint/event. Compare the relevant state or viewer output with
the original observation.

Use disposable memory cards, keep local captures out of Git, and avoid the
assembler/patching features during baseline observation. Cross-emulator
agreement is useful evidence, not proof of physical-hardware correctness.

## Limits

This does not replace the matching compiler/object pipeline, supply recovered
C, or establish native-port acceptance. Its hardware reference is related to
[PSX-SPX](PSX_SPX.md); consult that guide's source-quality and provenance
cautions before reusing reference material.

## Sources

- [Official no$psx feature list and Windows distribution](https://problemkaputt.de/psx.htm)
- [Original hardware reference](https://problemkaputt.de/psx-spx.htm)
