# Reverse-engineering tools: VS-NativePort and the wider console collection

Research date: **2026-10-14 (US Eastern)**.

This is a project-specific research catalog, not a claim that these tools have
been installed, integrated, or successfully run on Vagrant Story. It prioritizes
the unfinished PS1 matching decompilation; native-port investigation comes
later. The [project README](../../README.md) remains the status authority.

## Expanded scope: the systems on the F drive

The research now also covers disassemblies, decompilations, and future ports
across the **28 system folders** observed at the top level of the F drive.
Only directory names were inspected, not ROM contents or game-library lists.

**REA is not Linux-only.** It has Windows, macOS, and Linux workflows, but
provider/format support differs. See [REA's host-support explanation](REA.md#is-it-linux-only).
A Windows-capable MCP server is not automatically a console loader, CPU
implementation, debugger, or porting runtime.

Start with these broader guides:

| Guide | Contents |
| --- | --- |
| [Cross-system matrix and project strategy](CROSS_SYSTEM_MATRIX.md) | All 28 observed system folders, CPU/format pitfalls, tool candidates, and unresolved coverage |
| [Cross-system static analysis](CROSS_SYSTEM_STATIC_TOOLS.md) | Ghidra, Rizin/Cutter, Binary Ninja, IDA, and CPU-versus-console distinctions |
| [Multi-system debuggers](MULTI_SYSTEM_DEBUGGERS.md) | MesenCE, BizHawk, MAME, mGBA, DeSmuME/melonDS, Dolphin, Azahar, and coverage limitations |
| [MIPS toolkit](MIPS_TOOLKIT.md) | spimdisasm/Rabbitizer across N64, PS1, PSP, and PS2 |
| [N64 static recompilation](N64_RECOMP.md) | N64Recomp, Modern Runtime, metadata, hardware/runtime obligations |
| [GameCube/Wii decompilation](GAMECUBE_WII_DECOMP.md) | decomp-toolkit, CodeWarrior matching, and experimental recompilation caveats |
| [PSP/PS2/Dreamcast debugging](PSP_PS2_DREAMCAST_DEBUGGERS.md) | PPSSPP WebSocket interface, PCSX2 debugger, and optional Flycast GDB server |

For other projects, choose the deliverable explicitly: a reassemblable
disassembly, matching C/C++, functional reimplementation, static-recompilation
port, or reconstructed-source port. They are not interchangeable. The strict
PS1 acceptance rules below describe this repository, not an automatic rule that
all future ports must wait for complete matching decompilation.

## PS1-specific main finding

**REA is not currently the best first addition for this project.** Its
documented Windows Ghidra provider admits PE and raw 6502 targets, not PS1
executables. Its separate MIPS ELF support is not a PS1/MIPS-I import path.
The more promising combination is:

1. Keep the existing compiler and object-matching workflow as the acceptance
   gate.
2. Pilot PS1-aware Ghidra analysis for difficult remaining functions and data.
3. Capture controlled runtime evidence with a PS1 debugger when static
   analysis leaves an unknown.
4. Add an agent bridge only after the underlying analysis is trustworthy.

This is a recommendation from documented capabilities, not a benchmark.

## Guides and priorities

| Tool / resource | Proposed role | Priority and status |
| --- | --- | --- |
| [Ghidra + PS1 loader](GHIDRA.md) | Functions, types, references, GTE-aware static investigation | High-priority new pilot; compatibility must be tested |
| [PCSX-Redux](PCSX_REDUX.md) | PS1 execution, memory access, debugger-driven evidence | High-priority runtime pilot; captures are not hardware truth |
| [m2c](M2C.md) | MIPS assembly to a first C hypothesis | Targeted pilot; output is not matching C |
| [objdiff and matching companions](MATCHING_WORKFLOW.md) | Compiler feedback and instruction-level comparison | Existing toolchain; improve disciplined use, not replace it |
| [PSX-SPX](PSX_SPX.md) | Hardware contract reference for interpreting traces | High-value reference; not executable validation |
| [ImHex](IMHEX.md) | Local byte/table/header inspection | Optional analyst tool |
| [Kaitai Struct](KAITAI_STRUCT.md) | Explicit binary layouts and parser fixtures | Reuse existing dependency/material where appropriate |
| [GhidraMCP](GHIDRA_MCP.md) | Agent queries against a validated Ghidra workspace | Optional follow-up; PS1 extension interoperation unverified |
| [REA](REA.md) | General agent-facing analysis orchestration | Defer for retail PS1 inputs; reconsider supported host binaries |
| [DuckStation](DUCKSTATION.md) | Independent PS1 behavior comparison and visual diagnosis | Secondary evidence; disable enhancements for baseline work |
| [no$psx](NO_PSX.md) | Manual profiler, breakpoints, and graphics/I/O viewers | Optional independent spot-check; automation not established |

Each guide includes upstream sources, a bounded first experiment, Windows
considerations, and limitations. Tools already mentioned in the project are
identified as existing infrastructure rather than presented as new discoveries.

## Rules for useful evidence

- **Matching and behavior are different questions.** Fresh accepted byte-exact
  verification is required for reconstructed-source progress. Pseudocode,
  emulator observations, a native link, or a nonblack screen do not substitute.
- **No source mutation by research tooling.** Keep analysis projects, generated
  inputs, and temporary experiments separate from the maintained snapshot.
  Preserve unrelated local work.
- **Record identity.** Include the disc revision, file hash, overlay name,
  runtime mapping, tool release, settings, and exact function/range when
  applicable. The same address can name different code in different overlays.
- **No external uploads.** Keep retail files, assets, dumps, private code, and
  credentials off public scratch services and third-party model endpoints.
  Local MCP transport alone does not make inference local.
- **No real saves.** Use disposable memory-card fixtures and record the complete
  save/load transaction rather than treating enumeration as write correctness.
- **Do not change the reference to fit the result.** Preserve original bytes,
  emulator baselines, compiler flags, and explicit unsupported-tool errors.
- **Research does not authorize installation.** No software was installed,
  dependency manifest changed, server registered, or build configuration
  modified for these docs. Pin reviewed versions when conducting a trial.

## Suggested first evaluation

Use one known-matched function and one difficult unmatched function in the same
overlay. Establish correct Ghidra decoding on the matched control; investigate
the unmatched function without editing authoritative inputs. If necessary,
capture a bounded PCSX-Redux trace with reproducible inputs. Feed only supported
conclusions into a C candidate, then rebuild and compare using the project's
existing accepted verification path.

Stop if a missing historical compiler, incorrect memory map, wrong ISA, or
unverified tool integration prevents a trustworthy result. None of these tools
removes those prerequisites.

## Research scope

Primary upstream READMEs, platform/support documentation, and source-level
interfaces were preferred over promotional articles. Version statements are
snapshots of the cited material, not guarantees about installed tools or the
latest package registry release. Consult the selected release's documentation
before setup.

Related project references:

- [PS1/toolchain notes](../../docs/PSX_NOTES.md)
- [Windows migration and ownership](../../docs/WINDOWS_MIGRATION.md)
- [Native prerequisites and historical deviations](../../docs/NATIVE_DEVIATIONS.md)
- [Local decompilation progress](../rood-reverseWin/DECOMP_PROGRESS.md)
