# F-drive systems: disassembly, decompilation, and porting strategy

Research date: 2026-10-14 (US Eastern).

## Scope and evidence

Read-only inspection found **28 system directories** at the top level of
`F:\`. No ROMs, disc images, save files, or game-title lists were opened.
Directory presence is not a claim about file contents, revisions, legality,
working emulator configurations, or project readiness.

The table below is a **research routing map**, not a compatibility certification.
CPU descriptions are orientation; custom instructions, additional processors,
memory maps, and hardware must be validated for each selected title.

- **Documented tool-family fit:** upstream explicitly describes the relevant
  platform or instruction family; a local pilot is still required.
- **Candidate:** plausible starting point, not verified game/core/debugger
  coverage.
- **Unresolved:** do not promise a complete workflow without further research.

## Not just PlayStation, and not Linux-only

[REA](REA.md#is-it-linux-only) has workflows on Windows, macOS, and Linux.
Its provider-specific input checks are much narrower than "all architectures
supported by Ghidra." Moving to Linux does not automatically add a console
loader, mapper, or hardware runtime.

The recommended broad Windows stack is:

1. **Ghidra plus carefully validated loaders/languages** for the persistent
   static-analysis database.
2. **Rizin/Cutter** for a second decoder, older-CPU breadth, and scripted
   binary inspection.
3. **Platform-appropriate emulator debuggers**, not one universal runtime
   debugger.
4. **m2c, objdiff, and matching tools** where their ISA, ABI, original compiler,
   and project objective fit.
5. **Specialized recompilers/runtimes**, such as N64Recomp, only for the
   systems and deliverables they actually target.
6. **ImHex/Kaitai** for data layouts and reproducible parsers across systems.

See the [static-analysis guide](CROSS_SYSTEM_STATIC_TOOLS.md) and
[debugger guide](MULTI_SYSTEM_DEBUGGERS.md) for source-backed capabilities and
Windows constraints.

## System-by-system routing

The runtime entries are starting candidates. "MAME candidate" specifically
means verify the machine driver, selected game's behavior, CPU device, and
debugger usefulness first; MAME support is not universally complete.

| Observed directory | CPU / mapping concerns | Static/decompilation starting point | Runtime investigation candidate |
| --- | --- | --- | --- |
| `3DS` | ARM11/ARM9, modules, ARM/Thumb, system services | Ghidra ARM with a verified executable loader; m2c for supported CPU routines | Azahar GDB candidate; selected Windows build's feature availability unresolved |
| `Amstrad CPC` | Z80, banked memory, machine/disk variants | Ghidra/Rizin Z80; labelled reassemblable disassembly first | MAME candidate |
| `Atari 2600` | 6507/6502-family, cartridge banks, cycle-sensitive video | 6502-aware static work plus explicit cartridge mapping | BizHawk or MAME candidate; check tracing/bus-cycle evidence |
| `Atari 7800` | 6502-family CPU, banks and graphics/DMA behavior | 6502-aware analysis with a correct machine map | BizHawk or MAME candidate |
| `Atari Jaguar` | 68000 plus custom GPU/DSP instruction sets | Separate CPU/device models; generic 68k covers only part | MAME candidate, completeness and custom-device debugging unresolved |
| `Atari Lynx` | 65SC02-family plus custom hardware | Validate 65C02-family variant and hardware mapping | BizHawk or MAME candidate |
| `Gamecube` | Gekko PPC, paired singles, DOL/REL, DSP | decomp-toolkit + Ghidra PPC + objdiff; m2c as a candidate aid | Dolphin |
| `GB` | SM83, cartridge mappers; not stock Z80 | Rizin GameBoy backend or a validated Ghidra extension | Mesen, mGBA, or BizHawk; verify per-core tools |
| `GBA` | ARM7TDMI, ARM/Thumb, ABI, DMA/audio | Ghidra ARM; m2c's explicit `gba` target | mGBA; MesenCE/BizHawk alternatives |
| `GBC` | SM83, mapper/VRAM/WRAM banks | Same CPU caution as GB; retain bank identity | Mesen, mGBA, or BizHawk; verify per-core tools |
| `N64` | VR4300, 64-bit registers, RSP, overlays, microcode | spimdisasm/Rabbitizer + matching tools; N64Recomp for a separate port route | BizHawk core-dependent candidate; validate needed CPU/RSP visibility |
| `NDS` | ARM9 + ARM7, ARM/Thumb, overlays and shared memory | Ghidra per processor/module; m2c where ABI/ISA fits | DeSmuME Windows debugger pilot; BizHawk core-dependent; melonDS for behavior comparison |
| `NEC TurboGrafx-16` | HuC6280 and bank mapping; not plain 6502 | Validate HuC6280 language/backend before using generic 6502 analysis | Mesen or BizHawk; confirm core features |
| `NEC TurboGrafx-CD` | HuC6280 plus CD loading and changing RAM code | Same CPU validation plus track/file/load mapping | BizHawk CD support is listed; Mesen CD debugging remains to verify |
| `NES` | 2A03/2A07, cartridge mappers, banked code/data | 6502-aware analysis with explicit mapper context | Mesen or BizHawk |
| `PS2` | EE, IOP, VUs, DMA and GS; not one generic MIPS CPU | spimdisasm/Rabbitizer EE; m2c `mipsee` candidate; separate VU work | PCSX2 debugger |
| `PSP` | Allegrex/VFPU, executable modules/relocations, HLE | spimdisasm/Rabbitizer Allegrex + validated Ghidra import | PPSSPP standalone/WebSocket candidate |
| `PSX` | R3000A, GTE, delay slots, PRG overlays | Existing matching pipeline + PS1-aware Ghidra | PCSX-Redux; DuckStation/no$psx cross-checks |
| `Sega 32X` | Two SH-2 CPUs plus Genesis CPUs and shared hardware | Ghidra SH-2/68k/Z80; m2c `sh2` only for appropriate CPU routines | BizHawk or MAME candidate; verify all CPU visibility |
| `Sega CD` | Main/sub 68000, Z80, RAM code and CD transfers | Ghidra/Rizin m68k/Z80 with separate spaces/lifetimes | BizHawk or MAME candidate |
| `Sega Dreamcast` | SH-4, ARM sound CPU, graphics/services | Ghidra SH-4 + ARM; do not assume m2c `sh2` fits | Flycast; optional GDB build must be verified |
| `Sega Game Gear` | Z80, mapper and handheld-specific hardware | Ghidra/Rizin Z80 plus cartridge map | Mesen/BizHawk/MAME candidates |
| `SEGA Genesis` | 68000 + Z80, coprocessor interaction | Ghidra/Rizin m68k/Z80; separate sound CPU | BizHawk or MAME candidate |
| `Sega Master System` | Z80 and cartridge mapping | Ghidra/Rizin Z80 plus mapper | Mesen/BizHawk/MAME candidates |
| `Sega Saturn` | Dual SH-2, sound 68000, SCU DSP, multiple graphics processors | Ghidra SH-2/68k; m2c `sh2`; DSP and timing separate | BizHawk or MAME candidate; verify selected core/device capabilities |
| `SNES` | 65C816, SPC700, variable-width state, cartridge coprocessors | Rizin SNES/SPC700; verified Ghidra extensions where needed | Mesen or BizHawk |
| `Wii` | Broadway PPC, paired singles, REL, DSP and IOS/ARM services | decomp-toolkit + Ghidra PPC/ARM; separate platform contracts | Dolphin |
| `WonderSwan` | NEC V30MZ, segmented addressing, banking | Validate exact NEC CPU extensions; generic x86 is only a starting point | MesenCE/BizHawk; MAME candidate |

These are platform-level leads. No claim is made that all 28 systems have a
complete, tested Windows disassembly/decompilation/porting pipeline.

## Choose the deliverable before the tools

| Deliverable | Meaning | Appropriate success measure |
| --- | --- | --- |
| Documented disassembly | Labelled instructions/data with a correct map | Reassembly where feasible, byte identity, useful symbols/comments |
| Matching decompilation | C/C++ reproduces original compiled output | Fresh comparison under the correct original toolchain |
| Functional reimplementation | Newly written implementation of understood behavior | Explicit feature/state/behavior tests; not called matching |
| Static-recompilation port | Machine instructions translated to host-compiled code plus runtime | CPU equivalence and complete system/gameplay contracts |
| Reconstructed-source port | Understood source adapted to a host platform | Behavior tests plus correct replacement of hardware/OS dependencies |

Many older titles were written largely in assembly. A good disassembly may be
the right long-term foundation; forcing arbitrary pseudocode into C is not
automatically better. Conversely, a static-recompilation project need not wait
for 100% idiomatic matching C, provided it is described honestly and its runtime
is tested.

VS-NativePort keeps its own stricter decompilation-first policy. This broader
research does not change that policy or authorize new port code here.

## Architecture-specific risks

- **Banked cartridges:** a CPU address alone is not a globally unique symbol.
  Store bank, mapping, and load state with references.
- **Variable CPU modes:** 65816 width flags and ARM/Thumb state can change
  instruction decoding. Wrong mode creates plausible but false disassembly.
- **Multiple processors:** sound CPUs, RSP, VUs, SCU DSP, and custom GPUs need
  their own code/data maps and synchronization observations.
- **Relocated/runtime code:** a disc file's offset is not its live RAM address;
  decompression, overlays, and copied code must be identified.
- **Original compilers:** IDO, historical GCC, CodeWarrior, and other toolchains
  are not interchangeable just because their output targets the same CPU.
- **Ports:** translated CPU code still needs graphics, sound, input, storage,
  timing, threading, and platform services.

## Practical rollout

Do not attempt to integrate 28 systems at once. Pilot representative families:

1. NES/GB or another banked 8-bit title: map banks and reproduce assembly.
2. GBA: ARM/Thumb routine and mGBA observation.
3. N64: MIPS metadata, matching or an explicitly separate recompilation route.
4. GameCube: DOL/REL splitting and one CodeWarrior-matched function.
5. Saturn/32X or Dreamcast: validate SH-2 versus SH-4 and multiple processors.
6. PS2/3DS/Jaguar: separate research spikes for the custom or less-established
   pieces instead of pretending the generic CPU backend solves the system.

Create separate title-specific repositories/workspaces. Keep local original
inputs immutable and ignored; retain only appropriate original annotations,
configuration, fixtures, and build tooling. No software was installed and no
F-drive data was modified for this research.

## Sources

The companion guides link primary sources and their platform/build caveats:

- [Static tools and CPU/backend evidence](CROSS_SYSTEM_STATIC_TOOLS.md)
- [Multi-system emulator and debugger evidence](MULTI_SYSTEM_DEBUGGERS.md)
- [MIPS variant evidence](MIPS_TOOLKIT.md)
- [m2c target definitions](M2C.md)
- [N64 recompilation/runtime boundaries](N64_RECOMP.md)
- [GameCube/Wii decompilation and CodeWarrior](GAMECUBE_WII_DECOMP.md)
- [PSP/PS2/Dreamcast source-level debugging evidence](PSP_PS2_DREAMCAST_DEBUGGERS.md)
- [REA provider and host boundaries](REA.md)
