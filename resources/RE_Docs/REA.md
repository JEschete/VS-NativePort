# REA: Reverse Engineer Anything

Research date: 2026-10-14 (US Eastern). Documentation research only; no
installation, MCP registration, or game-binary experiment was performed.

## What it is

[REA](https://github.com/morluto/rea) is an agent-facing CLI and Model Context
Protocol server. It exposes analysis workflows backed by other tools, including
Ghidra, Hopper, and IDA. The underlying engine does the disassembly and
decompilation; the agent interprets the results and proposes further work.
REA is not a new decompiler, a PS1 emulator, or a matching compiler.

## Is it Linux-only?

**No. REA has Windows, macOS, and Linux workflows.** Support is specific to
the operation, provider, input format, and package version, not a single
yes/no answer for the whole product.

| Workflow | Documented host boundary | Meaning for a Windows analyst |
| --- | --- | --- |
| Ghidra native analysis | Linux/macOS providers and an experimental Windows x64 P0 provider | Windows is supported, but its admitted inputs and mutations are narrower |
| Static JavaScript/Electron analysis | Documented cross-platform CLI workflow, including Windows paths | Not Linux-only; mostly irrelevant to console machine-code recovery |
| APK analysis through JADX | Linux/macOS/Windows x64 with a full JDK | A separate workflow, not support for arbitrary console ARM binaries |
| Some ELF diagnostics / recorded Linux crashes | Linux x64 with additional tools | These particular features are Linux-specific |

The [Ghidra provider contract](https://github.com/morluto/rea/blob/main/docs/ghidra-provider.md)
describes Linux/macOS read analysis and limited atomic function-name/entry-comment
edits; Windows P0 is read-only. It imports into an ephemeral database rather
than exposing an arbitrary persistent Ghidra GUI workspace.

For the wider collection of systems on the F drive, see the
[cross-system research matrix](CROSS_SYSTEM_MATRIX.md). Do not interpret REA's
raw 6502 support as automatic NES, Atari, or bank-switched cartridge support:
the documented lane is a bounded raw CPU image, not a console memory model.

The [MindStudio article](https://www.mindstudio.ai/blog/reverse-engineer-anything-mcp)
that prompted this investigation is a useful introduction, but its broad
"rebuild an app" description is not a platform-support guarantee. Upstream's
provider-specific admission rules are much more important for this repository.

## Important finding: not currently a direct PS1 solution

| Layer | Upstream documentation | Consequence here |
| --- | --- | --- |
| Windows Ghidra provider | Experimental Windows x64 support; admitted targets are native x86/x64 PE files and explicit raw NMOS 6502 images | Does not establish support for retail PS-X EXE files or PS1 overlays |
| Generic MIPS provider | Specific ELF32 MIPS32/MIPS32r2 profiles with detailed ABI restrictions | Does not establish R3000A/MIPS-I or PS-X EXE support |
| Raw-binary provider | Documented CPU choices are x86, x86_64, ARM, ARM64, and 6502 | MIPS is not in the documented raw import choices |
| Ghidra itself | Supports MIPS and extensions | Ghidra capability does not bypass REA's narrower admission boundary |

**Recommendation: defer REA for retail PS1 analysis.** First evaluate Ghidra
with a PS1 loader directly. Do not rename a PS-X EXE to ELF, falsify ELF flags,
or use a PSP profile to get past admission. Those approaches lose the very
architecture and memory-map evidence that analysis needs.

REA may be useful later for examining a generated Windows native executable,
provided its PE architecture is admitted by the selected release. That would
explain host-side code, not prove that the host implementation matches the
original game.

## What could still be useful

- Structured function analysis, strings, calls, references, and memory
  observations for supported inputs.
- Explicit unknowns and evidence records rather than an unqualified prose
  explanation.
- Reusable CLI/MCP workflows when a supported analysis provider is already
  installed.
- A possible future integration if upstream adds actual PS1/MIPS-I raw
  loading, correct address mapping, and overlay-aware analysis.

These are potential project uses, not measured improvements in this checkout.
REA's application/UI/browser features are not priorities for the remaining
game-code matching work.

## Windows prerequisites and version boundaries

At research time, upstream's package manifest identified version `6.5.0`, MIT
licensing, and Node.js ranges `^22.19.0 || ^24.11.0 || >=26.0.0`. That manifest
is not proof of the version available from npm.

The Windows Ghidra guide describes:

- Windows 10+ x64 with fixed local NTFS target and temporary volumes.
- An existing Ghidra 12.1.x distribution, verified upstream with 12.1.4.
- A full 64-bit JDK compatible with that Ghidra distribution.
- A matching packaged Windows native addon for filesystem/process controls.
- A bounded read-only analysis interface, not general Ghidra GUI parity or
  unrestricted database annotation.

Do not simply install the newest Ghidra and assume compatibility. The current
Ghidra development README and REA's documented Ghidra range differ. Choose a
mutually supported release, record versions, and follow that release's JDK
requirements.

## Evaluation plan, if support becomes relevant

1. Review a particular REA release and its provider documentation. Pin the
   package version; record the package source and analysis-engine version.
2. Test outside the maintained game-source snapshot using a source-owned
   fixture whose function, bytes, and load address are known.
3. Preview agent setup changes before approving them. Upstream setup can add
   MCP entries and bundled workflow instructions; those are separate changes
   that should not be accepted accidentally.
4. With an approved installation, inspect diagnostics:

   ```powershell
   rea doctor --json
   rea providers --json
   ```

5. Confirm the actual client's tool discovery and a successful analysis call.
   Copilot CLI support is not proof that this VS Code SDK session exposes the
   same MCP server.
6. Reject unsupported inputs explicitly. For a future PS1 lane, verify actual
   MIPS-I decoding, load-delay behavior, GTE handling, load address, source
   identity, and overlay separation before using conclusions.
7. Keep resulting explanations as hypotheses until checked against the
   existing matching build or controlled behavior captures.

Upstream documents `npx rea-agents setup` as its entry point. It was **not**
run for this research. Prefer a reviewed explicit version over executing a
moving package in this repository.

## Data handling and authority

Local analysis does not imply local-only inference: an agent may send returned
strings, pseudocode, or bytes to its model provider. Do not expose private code,
credentials, retail assets, or dumps to third-party systems. Keep analysis
projects and retail-derived artifacts local and out of version control.

An MCP tool is not a source-of-truth upgrade. A function explanation, successful
tool call, reconstructed UI, or host executable is not fresh byte-exact matching
progress. REA also does not supply the missing historical Windows compilers.

## Sources

Primary sources, accessed on the research date:

- [Upstream README](https://github.com/morluto/rea/blob/main/README.md)
- [Installation and reviewed setup](https://github.com/morluto/rea/blob/main/docs/installation.md)
- [Windows Ghidra boundary](https://github.com/morluto/rea/blob/main/docs/windows-ghidra-p0.md)
- [MIPS ELF admission rules](https://github.com/morluto/rea/blob/main/docs/ghidra-mips.md)
- [Raw-binary CPU choices](https://github.com/morluto/rea/blob/main/docs/raw-binary.md)
- [Client-specific limitations](https://github.com/morluto/rea/blob/main/docs/client-configuration.md)
- [Ghidra provider semantics and host differences](https://github.com/morluto/rea/blob/main/docs/ghidra-provider.md)
- [Package manifest](https://github.com/morluto/rea/blob/main/package.json)

The upstream main commit observed during research was
[`949228b6069d96766dba8545284ecbafd3cbe93f`](https://github.com/morluto/rea/tree/949228b6069d96766dba8545284ecbafd3cbe93f).
Main-branch links above can change; recheck the selected release before setup.
