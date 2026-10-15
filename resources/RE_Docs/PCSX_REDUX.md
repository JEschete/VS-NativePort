# PCSX-Redux: runtime evidence and automation

Research date: 2026-10-14 (US Eastern). Official documentation reviewed;
not installed or tested with this game's disc image.

## Recommendation

**High-priority debugger pilot.**
[PCSX-Redux](https://github.com/grumpycoders/pcsx-redux) explicitly targets PS1
development and reverse engineering. It offers an integrated debugger, a
documented GDB server, Lua scripting, and a web API.

Use it to answer questions static decompilation cannot settle: which routine
writes a field, which overlay is resident, what data reaches a hardware
register, or what state a difficult function receives and returns.

## Documented interfaces

| Interface | Relevant capabilities | Important constraint |
| --- | --- | --- |
| Built-in debugger | CPU debugging and separate GPU investigation | Most CPU features need Dynarec disabled; enable the debugger separately |
| GDB server | Remote breakpoints and read/write/access watchpoints; IDE connections | Requires a MIPS-capable debugger client and enabled emulator debugger |
| Lua | Console/editor, LuaJIT and FFI-based access | Runs on the main UI thread; long scripts stall the UI and FFI can crash the process |
| Web API | Execution state/control, RAM and VRAM access, symbol upload, screenshots, save states | HTTP without TLS; includes write/control operations, not just observation |

Upstream documents GDB connections on port 3333 and a web server on port 8080.
Confirm the chosen build's configuration rather than assuming either is running
or bound safely.

Selected documented HTTP operations include:

- `GET /api/v1/execution-flow`
- `GET /api/v1/cpu/ram/raw`
- `GET /api/v1/gpu/vram/raw`
- `POST /api/v1/assembly/symbols?function=upload`

Read the full API contract before using parameters or mutation endpoints.
The symbol upload can help correlate source-derived names with runtime
addresses, but an address-to-name map must be valid for the active overlay.

## Windows considerations

Upstream describes 64-bit builds, an OpenGL 3.0+ capable GPU/driver, and a
Visual Studio 2022 C++ source-build path. Its GDB documentation links a Windows
multiarch debugger distribution. Review binaries and versions before use.

A modern MIPS toolchain used to build emulator fixtures or OpenBIOS is **not**
the historical compiler needed for this project's byte-exact game rebuild.
Do not run remote installer snippets or replace the matching toolchain as a
side effect of debugger setup.

## First experiment

1. Use a legally obtained local image and disposable memory-card fixture.
   Record emulator version, BIOS identity, disc revision/hash, CPU mode, and
   relevant configuration.
2. Enable the debugger and disable Dynarec for the CPU-debugging experiment.
3. Reproduce a small known sequence. Set a breakpoint on an already-understood
   function, confirming the correct overlay is loaded.
4. Validate an address map against an actual instruction sequence. Then place
   one watchpoint on a specific unresolved field or buffer.
5. Capture the PC, instruction, relevant registers, memory range, and input
   state at the observed access. Record the event order and preconditions.
6. Repeat from a controlled start and compare the same event, not just the
   same wall-clock time. Save states can help reproduce an experiment but are
   version/configuration-specific.
7. Use the capture to test a C hypothesis or define a future host fixture.
   Do not call it accepted source progress until matching verification passes.

For GTE investigations, record all relevant input/output registers, flags, and
state carried between operations. The project's [PS1 notes](../../docs/PSX_NOTES.md)
already explain why delay slots and retained state matter.

## Limits and operational safety

- An emulator observation is an emulator observation, not physical-hardware
  certification. Cross-check surprising results with an independent reference
  and, where required, hardware.
- Keep GDB and web endpoints local, review bind settings, and disable them when
  not needed. The API can alter RAM, VRAM, and execution state.
- Lua's editor can persist/reload scripts; test scripts in a disposable setup.
- Do not treat a screenshot or successful boot as complete gameplay evidence.
- Record exact builds for automation; no stable API contract was established
  by this research.
- Keep captures local. Avoid committing raw RAM, retail assets, or real saves.

## Sources

- [Upstream project and Windows build requirements](https://github.com/grumpycoders/pcsx-redux)
- [Debugger introduction and Dynarec constraint](https://pcsx-redux.consoledev.net/Debugging/introduction/)
- [GDB server and Windows client instructions](https://pcsx-redux.consoledev.net/Debugging/gdb-server/)
- [Lua environment and safety limitations](https://pcsx-redux.consoledev.net/Lua/introduction/)
- [Web server and API](https://pcsx-redux.consoledev.net/web_server/)
