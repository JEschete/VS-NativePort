# Multi-system and Nintendo-focused emulator debuggers

Research date: 2026-10-14 (US Eastern). Primary documentation/source reviewed;
no emulator installed, game opened, debugger connected, or capture produced.

## Recommendations at a glance

| Tool | Relevant scope | Research value | Boundary to verify |
| --- | --- | --- | --- |
| Mesen / MesenCE | NES, SNES, GB/GBC/SGB, GBA, PC Engine, SMS/Game Gear, WonderSwan | Integrated disassembly, graphics/memory views, trace logging, Lua | Exact release, CD variant, and debugger API behavior |
| BizHawk | Broad multi-system collection | Reproducible inputs, memory tools, Lua, core-dependent debugging | Supported system does not guarantee full debugger parity |
| MAME | Many machines, with a device-oriented debugger | Multi-CPU/device/address-space investigation | Specific driver, selected game, device completeness, build |
| mGBA | GBA, GB/GBC | CLI debugger, genuine GDB remote support, Lua | CPU/core and script-to-debugger feature availability |
| DeSmuME | DS | Windows ARM7/ARM9 disassembly and documented GDB flags | Selected build/features; platform manuals can differ |
| melonDS | DS/DSi emulation | Independent behavior comparison | Reviewed README still lists debugger tools as TODO |
| Dolphin | GameCube/Wii | Guest PowerPC debugging and GDB stub | Debug Mode, selected build, CPU/DSP mode |
| Azahar | 3DS, Citra-derived | Source-confirmed conditional GDB stub | Whether the chosen Windows build enables it |

These are tools for **guest-machine evidence**, not native game ports.
Debugging a game inside an emulator is different from attaching a host debugger
to reconstructed native code.

## Mesen and the maintained MesenCE continuation

The reviewed `SourMesen/Mesen2` README identifies it as an archived snapshot
and points to the community continuation
[MesenCE](https://github.com/nesdev-org/MesenCE). The successor README lists
Windows, Linux, and macOS and the systems in the table above.

The Mesen documentation describes a substantial integrated suite: debugger,
assembler, event viewer, memory tools, profiler, PPU viewer, Lua script window,
text hooker, and trace logger. This is an especially good first investigation
tool for cartridge-era systems rather than trying to infer banked execution
using a generic flat-binary importer alone.

For a trial, label one known routine, identify the current cartridge bank,
trace a bounded call sequence, and correlate a memory/graphics event. Keep
the assembler/editor features away from the immutable comparison input.

Use a reviewed release from the chosen lineage. Native-packaged and
.NET-dependent builds have different runtime prerequisites; inspect its
release instructions instead of assuming every ZIP has identical requirements.
The precise TurboGrafx-CD/CD-ROM add-on debugging path was not established
in this research, so do not equate base PC Engine support with a validated
CD workflow.

## BizHawk: breadth with explicit per-core caveats

[BizHawk](https://github.com/TASEmulators/BizHawk) documents a broad platform
list spanning the Atari families, Nintendo handhelds/consoles, PC Engine/CD,
several Sega systems, PS1, and WonderSwan. The reviewed README also lists 3DS;
that alone is not a recommendation to use its 3DS core as a complete debugger.

TASVideos describes unified debugging/scripting **with exceptions**. Therefore:

- Confirm the actual core selected for a system.
- Verify memory domains, read/write callbacks, tracing, CPU visibility,
  stepping, and breakpoint support individually.
- A memory-watch window or Lua API is not proof of full instruction-level
  debugger access for every core.
- Compare deterministic input replay only within a recorded
  build/core/settings/firmware configuration.

The documented Lua API includes communication and memory-related automation,
making this a good candidate for repeatable experiments. Do not send private
data through its HTTP/communication functions.

Windows release/runtime requirements are version-specific; current upstream
describes x64 releases and redistributable prerequisites. Do not assume old
legacy builds offer the features of current releases.

## MAME: useful when a machine is more than one CPU

[MAME's debugger](https://docs.mamedev.org/debugger/index.html) targets the
emulated system and is enabled with the `-debug` option. Its documentation
describes disassembly/memory windows, registers, and a command console.

Its strongest cross-system feature is explicit device/address-space selection:
commands can address CPU/device tags and distinct program/data/I/O/opcode
spaces. This is a useful model for multi-CPU and custom-hardware research.

However, generic debugger capability does not establish that a particular
Jaguar, Saturn, 32X, Lynx, or Amstrad driver is complete or exposes everything
needed for a given game. Validate the selected driver first. The debugger UI
also depends on operating system and build options.

Use it as a candidate for less-covered systems, not as a promise that one MAME
installation solves all 28 folders.

## mGBA: a strong GBA starting point

[mGBA](https://github.com/mgba-emu/mgba) documents GB/GBC as well as GBA,
Windows support, a command-line debugger, Lua scripting, and GDB remote
debugging with Ghidra/IDA interoperability as an upstream claim.

Its source contains an actual socket-based GDB stub with configurable
port/bind-address parameters. Verify the appropriate guest CPU and client;
ARM GDB support does not itself prove Game Boy CPU remote-debugger parity.

The reviewed changelog places deeper scripting integration for breakpoints
and watchpoints under a future section. Do not assume Lua scripts in a selected
release can invoke every debugger operation just because both features exist.

## DS: DeSmuME versus melonDS

The [DeSmuME README](https://github.com/TASEmulators/desmume/blob/master/desmume/README)
explicitly warns that host ports have different features. Its Windows source
has separate ARM7/ARM9 disassembly views, memory tools, and a Lua console.
The Windows manual lists `--arm9gdb` and `--arm7gdb` options.

Those are concrete leads for a **Windows DS debugger pilot**. The manual
contains historical material, so confirm those flags and feature gates in the
actual selected build before constructing an automation client.

[melonDS](https://github.com/melonDS-emu/melonDS) is useful as an independent
DS/DSi behavior reference. Its reviewed README puts debugger/graphics-viewer
work in a TODO section. Do not advertise it as a verified shipped GDB/debugger
workflow on that basis; inspect a newer release/fork separately if needed.

## Dolphin: GameCube/Wii guest debugging

[Dolphin](https://github.com/dolphin-emu/dolphin) supports Windows and documents
`-d` / `--debugger` to expose debugger views. Source confirms PowerPC
breakpoints, a Qt breakpoint panel gated by Debug Mode, and a GDB stub with
Windows Winsock branches.

The guest PowerPC address space is not the host process address space. Audio
DSP and Wii IOS/ARM interactions also cannot be understood solely through one
PowerPC breakpoint view.

Record CPU execution mode and debugger configuration. This research did not
establish the exact breakpoint/JIT-block invalidation mechanism, so no claim
is made that a particular mode is required or that all modes have identical
stepping behavior. Test the known-routine control first.

## 3DS: conditional Azahar GDB candidate

[Azahar](https://github.com/azahar-emu/azahar) has a source-confirmed GDB stub
controlled by `ENABLE_GDBSTUB`, configuration keys for enabling it and choosing
a port, and calls from the emulation loop. That is a stronger lead than
assuming a Citra-derived name implies a working debugger.

Whether the chosen official Windows binary enables the stub, exposes the
required settings, and works with the desired client was not established.
Treat this as a conditional pilot, not a tested end-to-end 3DS pipeline. Do
not obtain keys, firmware, or proprietary SDK material from unauthorized
sources.

## Shared capture rules

Use a known small routine and controlled inputs before investigating unknown
code. Record game revision, tool/core/build, firmware, banks/overlays, processor,
execution mode, relevant registers/memory, and capture trigger. Disable cheats
and unrelated enhancements.

Debuggers often offer memory writes, patching, register edits, and execution
control. Keep remote listeners local/restricted and do not alter the baseline
while observing it. Use disposable saves and keep retail-derived captures local.

## Sources

- [Mesen2 archived lineage](https://github.com/SourMesen/Mesen2)
- [MesenCE systems, hosts, releases](https://github.com/nesdev-org/MesenCE)
- [Mesen debugger suite](https://www.mesen.ca/docs/debugging.html)
- [Mesen debugger details](https://www.mesen.ca/docs/debugging/debugger.html)
- [BizHawk supported systems and Windows requirements](https://github.com/TASEmulators/BizHawk)
- [TASVideos BizHawk scope/caveats](https://tasvideos.org/Bizhawk)
- [BizHawk Lua API](https://tasvideos.org/Bizhawk/LuaFunctions)
- [MAME debugger and device/address-space commands](https://docs.mamedev.org/debugger/index.html)
- [mGBA features](https://github.com/mgba-emu/mgba)
- [mGBA GDB stub interface](https://github.com/mgba-emu/mgba/blob/master/include/mgba/internal/debugger/gdb-stub.h)
- [mGBA future/released changelog](https://github.com/mgba-emu/mgba/blob/master/CHANGES)
- [DeSmuME Windows manual](https://github.com/TASEmulators/desmume/blob/master/desmume/README.WIN)
- [DeSmuME Windows ARM7/ARM9 disassembly views](https://github.com/TASEmulators/desmume/blob/master/desmume/src/frontend/windows/disView.h)
- [melonDS README/roadmap](https://github.com/melonDS-emu/melonDS)
- [Dolphin README](https://github.com/dolphin-emu/dolphin)
- [Dolphin GDB stub](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/PowerPC/GDBStub.cpp)
- [Dolphin breakpoint UI](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/DolphinQt/Debugger/BreakpointWidget.cpp)
- [Azahar GDB stub](https://github.com/azahar-emu/azahar/tree/master/src/core/gdbstub)
- [Azahar core build gates](https://github.com/azahar-emu/azahar/blob/master/src/core/CMakeLists.txt)
