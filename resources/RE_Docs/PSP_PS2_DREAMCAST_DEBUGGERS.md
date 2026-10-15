# PSP, PS2, and Dreamcast: platform-specific runtime tools

Research date: 2026-10-14 (US Eastern). Upstream README/source inspection
only; no emulators installed or debugger connections tested.

## PSP: PPSSPP

[PPSSPP](https://github.com/hrydgard/ppsspp) has Windows builds and an
HLE-based PSP runtime. In addition to local debugging features, its upstream
source describes a WebSocket debugger interface using the subprotocol
`debugger.ppsspp.org` and JSON messages with an `event` field.

The source registers handlers for memory, disassembly, stepping, breakpoints,
GPU inspection/recording, input, and HLE/kernel objects. It requests an initial
`version` event. This is concrete evidence of an automation candidate, not an
assumption that a GUI can be driven remotely.

Important boundaries:

- Match the protocol/client to the exact emulator revision; no cross-version
  stability guarantee was established here.
- The upstream header explicitly excludes this WebSocket debugger from the
  libretro build. Prefer the standalone application for this experiment.
- PPSSPP's HLE behavior is not necessarily every low-level hardware/firmware
  detail. Validate assumptions before using a syscall observation as a port
  contract.
- For static code, pair it with [Allegrex-aware tools](MIPS_TOOLKIT.md);
  generic MIPS decoding does not establish VFPU correctness.

## PS2: PCSX2

[PCSX2](https://github.com/PCSX2/pcsx2) documents Windows support and requires a
legitimately obtained BIOS. Its Qt debugger source includes run/pause,
step-into/over/out, analysis actions, a current-CPU interface, and separate
disassembly, memory, register, symbol, thread, and stack views.

Use this as a local evidence source for a particular processor/module. Do not
assume one view represents EE, IOP, vector units, DMA, and GS behavior together.
Check which CPU and address space a breakpoint belongs to.

The source proves debugger implementation exists; it does not prove a particular
download includes every feature or that a stable remote API is available.
Confirm the selected build's UI and execution-mode requirements before capture.
No remote protocol or VS Code integration is promised by this guide.

For decompilation, the [MIPS toolkit](MIPS_TOOLKIT.md) provides explicit EE
instruction decoding candidates. EE decoding alone does not produce a whole
PS2 port.

## Dreamcast: Flycast

[Flycast](https://github.com/flyinghead/flycast) documents Windows releases
and a Windows source-build entry point. Its CMake configuration includes
`ENABLE_GDB_SERVER`, **off by default**, and the source contains a GDB remote
server under `core/debug`.

This makes an SH-4 GDB experiment plausible, but do not assume ordinary release
downloads enable it. Establish the build option, a compatible SH debugger,
register model, connection settings, and a known breakpoint before planning
automation.

**Network caution:** the inspected server source constructs an IPv4 listener
from a port without a loopback address restriction. Do not assume localhost-only
binding. Isolate the experiment and restrict access before enabling it; do not
expose emulator debugger services to a LAN or the internet.

Use Ghidra's actual SH-4 language for static investigation. Saturn's SH-2 m2c
target is not evidence of Dreamcast support. The Dreamcast's ARM audio processor
and graphics/runtime services remain separate parts of any port.

## Shared first experiment

Choose one known routine and a reproducible input sequence. Verify loaded-module
identity, CPU mode, mapping, and instruction bytes. Capture pre/post registers
and a bounded relevant memory range. Repeat from a controlled state and compare
with static analysis or a second reference.

No reference patching, no real save files, no public dumps, and no claims of a
complete port based only on a successful boot or debugger attachment.

## Sources

- [PPSSPP overview and Windows development builds](https://github.com/hrydgard/ppsspp/blob/master/README.md)
- [PPSSPP WebSocket protocol and registered handlers](https://github.com/hrydgard/ppsspp/blob/master/Core/Debugger/WebSocket.cpp)
- [PPSSPP libretro exclusion](https://github.com/hrydgard/ppsspp/blob/master/Core/Debugger/WebSocket.h)
- [PCSX2 overview and Windows/BIOS requirements](https://github.com/PCSX2/pcsx2/blob/master/README.md)
- [PCSX2 debugger window](https://github.com/PCSX2/pcsx2/blob/master/pcsx2-qt/Debugger/DebuggerWindow.h)
- [PCSX2 debugger components](https://github.com/PCSX2/pcsx2/tree/master/pcsx2-qt/Debugger)
- [Flycast platforms and Windows build entry point](https://github.com/flyinghead/flycast/blob/master/README.md)
- [Flycast GDB build option](https://github.com/flyinghead/flycast/blob/master/CMakeLists.txt)
- [Flycast GDB server implementation](https://github.com/flyinghead/flycast/blob/master/core/debug/gdb_server.cpp)
