# N64Recomp and the N64 Modern Runtime

Research date: 2026-10-14 (US Eastern). Upstream documentation reviewed;
no ROM processed and no port built.

## Recommendation

**High-value porting candidate for the N64 collection**, when the chosen goal
is static recompilation rather than first recovering matching, idiomatic C.

[N64Recomp](https://github.com/N64Recomp/N64Recomp) translates identified binary
functions into literal C implementations. It is not a generic decompiler or a
one-command port for an unknown ROM. It needs symbols/metadata and a runtime.

## What the pieces do

| Component | Role | Remaining obligation |
| --- | --- | --- |
| N64Recomp | MIPS instruction-to-C translation, function calls, configured metadata, overlay-related support | Correct code boundaries, relocation information, dynamic targets |
| ultramodern | Much of libultra: threads, controllers, audio, messages, timers, RSP-task handling, VI timing | Platform I/O callbacks and a graphics renderer |
| librecomp | Bridge from generated code; overlay handling, ROM DMA, save-media support | Per-game integration and verified behavior |
| Renderer | Implements graphics execution/output | Correct microcode and rendering behavior for the selected game |

The runtime recommends RT64, but that recommendation does not establish that
every game's graphics path is supported without work.

## Windows facts

N64Recomp documents a C++20/CMake build and generated C tested with MSVC, GCC,
and Clang. N64 Modern Runtime requires C++20, was developed using Clang, and
states that recent MSVC/GCC should work but are not regularly tested.

These are upstream claims, not a successful build on this machine. Do not
copy generic build commands into the PS1 repository or change its toolchain.
An N64 project should have its own repository, dependency pins, and validation.

## Metadata and hard cases

The reviewed N64Recomp README describes ELF as the current metadata input path.
The normal route starts from disassembly/decompilation metadata; it does not
infer all function boundaries safely from an arbitrary compressed ROM.

The documentation covers statically linked and relocatable overlays. It also
identifies limitations around TLB relocation support and RSP overlays. Check
the selected revision rather than assuming planned work is implemented.

Other integration risks include indirect branches, runtime-generated/copied
code, nonstandard microcode, timing, save transactions, and custom hardware
access. Literal CPU translation does not remove those contracts.

## First evaluation

1. Pick one legally obtained, well-understood title/revision with usable
   symbols or disassembly metadata.
2. Decide explicitly that this is a static-recompilation port, not a matching
   decompilation. Keep separate progress metrics.
3. Recompile a bounded function/subsystem and compare input/output state.
4. Integrate the runtime and selected renderer without masking missing hooks
   behind successful no-op stubs.
5. Establish reproducible tests for boot, input, audio, graphics, overlays,
   saving, and representative gameplay before making a port-completeness claim.

Generated C can be useful executable infrastructure without being readable
original source. Neither a successful translation nor a title screen proves a
complete port.

## Sources

- [N64Recomp: translation, metadata, overlays, RSP, building](https://github.com/N64Recomp/N64Recomp/blob/main/README.md)
- [N64 Modern Runtime: responsibilities and compiler expectations](https://github.com/N64Recomp/N64ModernRuntime/blob/main/README.md)
- [Example integration: Zelda 64 Recompiled](https://github.com/Zelda64Recomp/Zelda64Recomp)
- [Runtime-recommended renderer](https://github.com/rt64/rt64)
