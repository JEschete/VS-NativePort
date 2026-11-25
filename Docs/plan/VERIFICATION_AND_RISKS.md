# Native Source Port Plan: Verification, licensing and risks

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md)

## 10. Verification

| Method | Used for |
|---|---|
| **DuckStation reference captures** (`tools\PSX\Reference\duckstation-src` built locally) | Screenshots/video of each 1b–1e checkpoint, compared with a tolerance metric |
| **Deterministic replays** | Input files + per-frame state hashes. Reference logs from the original (DuckStation) for key checkpoints |
| **Decoder tests** | Against GodHands, the Unity VS parser and jpsxdec outputs |
| **Math oracle** | Core.Math versus GTE vectors (ps1-tests, PsyCross) |
| **Catalog round-trip** | §9.8 |
| **Ledger CI** | `scan_native.py` keeps the ledgers honest; the boundary lint; the shim-use count never increases |

### 10.1 Evidence and failure rules

- Record build revision/configuration, input script, asset/extractor version, initial state, tick/checkpoint and
  expected result with each gate. Existing `--press`/`--scripted` capture support is extended by M2, not reimplemented.
- Compare canonical semantic state, not native/PS1 memory dumps: stable IDs, location, flags, inventory, HP, timers
  and RNG state. Exclude pointers, padding, platform handles and diagnostic clocks. Define field ordering and integer
  representation. A reference must expose comparable fields before claiming matching state hashes.
- Define image regions/tolerances and audio/event timing tolerances per checkpoint before evaluating results.
  Native repeatability and original-game parity are separate tests; neither follows from the other automatically.
- Data-dependent tests may skip without a user disc locally, but a skipped test does not satisfy a release gate.
  Use synthetic malformed-file fixtures for ordinary CI; keep proprietary payloads/captures git-ignored.
- Required missing data, unsupported opcodes and unimplemented dispatch on an exercised path fail the checkpoint.
  Optional paths must be identified explicitly, produce a visible diagnostic and remain unverified.
- For code packages: Debug/Release x64 builds, relevant unit/integration tests, boundary checks and generated-file
  ownership checks are required. Add memory/lifetime instrumentation where supported. Documentation-only changes
  require link/structure/diff checks, not claims of a new runtime pass.

## 11. Licensing

- Original project contributions are dedicated to the public domain under **CC0 1.0 Universal (CC0-1.0)**,
  except where otherwise noted and only to the extent the project contributors hold the relevant rights.
  See the [project licensing notice](../../README.md#license) and full [license](../../LICENSE).
- The decomp (rood-reverse) declares CC0 for rights held by its contributors. Preserve its license and
  provenance when adapting code; that declaration does not grant rights held by the original game's rights holders.
- rood-boi (CC0) and PsyCross (MIT) may be lifted with attribution in `THIRD_PARTY.md`. SDL3 is zlib. Runtime
  libraries are permissive: dav1d (BSD-2), libwebm (BSD-3), libFLAC (BSD-3), nlohmann-json (MIT), lodepng (zlib).
- Third-party material retains its respective license, public-domain dedication and required notices.
  The CC0 dedication for original project contributions does not revoke upstream permissions, affect
  patent or trademark rights, or grant rights in the original game's code, assets or trademarks.
- ffmpeg is used **only as an external executable at extract time**. It is never linked or shipped.
- GPL / non-commercial / unlicensed tools (see TOOLS_INVENTORY) are **reference only**. We read them to understand
  formats and never copy code.
- **No game data is committed.** Disc contents, extracted catalogs and captures stay git-ignored. Users supply their own disc.

## 12. Risks

| Risk | Mitigation |
|---|---|
| BATTLE naming quality (1,478 generic) | Parallel agents with call-graph context, confidence tags, re-queue of low-confidence rows, and name review at the 1c gate |
| Hidden layout dependence and incorrect pointer-table extents | M1 validates source bounds, aliases, signatures and native layouts; compile success and x64-safe OT handles alone do not establish correctness |
| Scratchpad use (1,034 hits) | Became typed owned workspaces; since #45 no code addresses the scratchpad, only provenance comments name its offsets (§6.8.2) |
| Readback-dependent effects (`StoreImage`/`MoveImage`) | Capture API in Engine\Render from 1b |
| Timing parity (30/20 fps logic, field timing) | FrameClock reproduces logical ticks; replays catch drift |
| Effects volume (480 instantiations, ~90 plugins) | Template + catalog design; previewer in 2d |
| Asm-only functions (191 + 25) | Rewritten from asm + Ghidra; marked in the catalog `copy = asm` |
| Enemy variants diverge in ways `extends` cannot express cleanly | The round-trip test ([ENEMY_DATA_SCHEME §3, archived](https://github.com/JEschete/VS-NativePort/blob/0e3ae7d8df169bc684c8155fa44b90e731bb4e69/Docs/ENEMY_DATA_SCHEME.md)) catches it; worst case, a variant is emitted in full |
| Compatibility implementation mistaken for modernization | Separate behavior/architecture status; close EC issues by dependency and output evidence, not SDK-name counts |
| Required stubs or unresolved pointers allow execution to continue | Per-checkpoint required/optional policy; fail before use and retain precise module/table/opcode diagnostics |
| Renderer rewrite regresses readbacks or original visual rules | M4 primitive/effect fixtures plus reference captures; preserve an isolated comparison path until replacement passes |
| Verification depends on wall-clock diagnostics or raw native layouts | M2 canonical state and explicit simulation clocks; test varied presentation cadence separately |
| External decoder or backend work blocks a whole phase | Name the dependency and decision in the package; deliver minimal verified format slices without claiming full conversion |
| Generated code changes are lost on regeneration | Retired with the copier (D24): generated files are edited in place, keeping their `// decomp:` provenance |
