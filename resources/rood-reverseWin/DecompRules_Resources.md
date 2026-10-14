# Decompilation Rules And Resources

Inventory date: 2026-10-04. Workspace root:
`D:\Dev\NativePort\VS-NativePort`.

## Approved PSX GTE Exception: 2026-10-13

The user explicitly approved invoking existing SDK GTE primitives in the
PSX-only matching backend. Their SDK-owned hardware expansion is permitted
only there; higher-level algorithms remain readable C/C++, and Windows-native
generated inputs must use C/C++ software-GTE services for those operations.

This narrow exception does not permit new handwritten assembly, assembly
wrappers, opcode payloads, retail-body copies, new assembly inclusions, or
modified SDK primitives. All configured-toolchain, complete-range, fresh-report,
function/unit/overlay, and fail-closed acceptance gates remain unchanged.
Existing requirements are preserved below; this exception supersedes only
their prohibition of existing SDK GTE expansion in the PSX matching path.

## Required Deliverable

Meaningful, human-readable C++ for the Windows native port, not assembly
transcription, raw decompiler output, or code shaped only to satisfy a historical
compiler. Use descriptive names, verified types, understandable algorithms, and
existing modular platform services. Follow [AGENTS.md](AGENTS.md).

Matching-compatible PSX source must pass byte-exact verification first.
Windows-native adaptations are separate work and cannot replace or invalidate
accepted matching source. This boundary does not relax readability or the
no-inline-assembly rule. If all requirements cannot be satisfied for a target,
report `BLOCKED` instead of silently choosing a weaker deliverable.

## Mandatory Byte-Exact Acceptance Gate

Byte-exact matching is required, not optional. Ghidra analysis, compiler success,
behavioral tests, and confidence do not substitute for matching. This gate governs
decompilation progress and completion claims, including work described as
"source-complete."

1. **Establish A Working Baseline Before Editing.** Confirm that the isolated WSL
  historical build succeeds with the configured compiler, flags, headers,
  assembler, linker settings, and hash-verified retail binary. Validate the
  checker with a known matching reconstructed routine and a deliberately
  mismatching candidate. Both controls must produce the correct outcomes.
  Original assembly inclusions and compiler smoke tests are not positive controls
  for reconstructed source. If the build or either control fails, stop before
  reconstruction and report the blocker.
2. **One Function, Then Verify.** Work within one source unit, changing one
  function at a time. After every meaningful implementation edit, immediately
  build and compare that function against its complete original byte range,
  including correctly resolved relocations. Diagnose and rerun that same check
  until it passes or report `BLOCKED`. Do not advance to another function while
  matching is unresolved. A shared helper, header, or build-input edit invalidates
  existing reports for affected functions; those functions must be rechecked.
3. **Require Fresh, Reproducible Evidence.** Accept only a canonical `MATCH` report
  generated from the current source and relevant header/build-input hashes.
  Record compiler version/hash, complete compiler/assembler/linker settings,
  original binary hash, function and overlay identity, CPU address, file offset,
  complete comparison size, exact command, and report location in
  [DECOMP_PROGRESS.md](DECOMP_PROGRESS.md). A passing report becomes stale after
  any relevant input changes and cannot be reused as acceptance evidence.
4. **Fail Closed.** Build failure, `DIFF`, timeout, unresolved relocation, partial
  comparison, missing/stale report, or `PROBE_MATCH`/`PROBE_DIFF` means
  `NOT ACCEPTED`. A compatibility probe is not a canonical matching build.
  Record what actually ran and its failure stage; never replace matching with
  Windows tests, Ghidra confidence, or a completion claim to maintain momentum.
5. **Verify The Claimed Scope.** After all changed functions match, rebuild and
  compare the complete source unit and linked overlay, including relevant
  code/data, layout, padding, and relocations, before claiming completion at
  those scopes. Function matches alone do not establish unit or overlay matching.
  Do not claim a broader scope than the actual comparison establishes.
6. **Do Not Manufacture Matching.** Do not change retail bytes, expected ranges,
  comparison rules, symbol addresses, or expected toolchain settings merely to
  make a check pass. Do not mask differences or count assembled original bodies
  as reconstructed functions. Use only configuration and symbol provenance that
  are independently verified; ambiguous addresses remain blockers when needed.
7. **Preserve Both Source Boundaries And User Work.** Matching-compatible PSX
  source must pass before Windows-native adaptation. Native service calls must
  not be substituted into canonical matching source on the assumption that their
  behavior will produce retail instructions. WSL may build isolated snapshots
  only; it must not rewrite canonical sources or become a native dependency.
  Do not automatically reset, clean, replace, or synchronize edited trees.
8. **Stop On Incompatible Requirements.** Keep the readable-code and
  no-inline-assembly requirements. If they cannot be satisfied together with
  exact matching using verified tooling, report `BLOCKED` and obtain explicit
  direction. Do not silently relax a requirement or invent a workaround that
  only changes the reported result.

### Enforcement Status

These instructions require the workflow but do not implement a technical CI
gate. On 2026-10-05 the function checker passed positive, candidate, negative
and fail-closed controls (see the tracker); controls must be rerun after any
checker change. It verifies function scope only. Actual automated enforcement
still requires CI that rejects missing, stale, or unsuccessful reports, plus
source-unit and overlay comparison. Do not claim that this Markdown alone
guarantees matching or that those automation gates already exist.

## Non-Negotiable Rules

**No Inline Assembly.** Do not introduce `asm`, `__asm__`, assembly wrappers,
embedded instruction words, or new assembly inclusions as an implementation.
Existing upstream assembly is unfinished reference material, not a template
for new work.

**Use Ghidra.** Inspect decompiled control flow, references, callers, data types,
and original instructions before implementing each target. Ghidra output is
evidence to interpret, not finished C++ to paste unchanged.

**Verify Meaning.** Check interpretations against actual binary instructions,
data, and consumers. Record the binary, file offset, CPU address, and unresolved
questions. Documentation, symbol names, and decompiler guesses alone do not
establish behavior.

**Preserve Behavior.** Verify layouts, signedness, fixed-point arithmetic,
overflow, truncation, rounding, saturation, flags, and calling semantics.
Express hardware-dependent operations through readable shared services.

**One Source Unit.** Validate each meaningful change before proceeding and
update [DECOMP_PROGRESS.md](DECOMP_PROGRESS.md) as work occurs. Do not claim
completion for scaffolding, placeholders, or unverified code.

**Separate Evidence.** Keep readable reconstruction, behavioral validation,
function matching, and complete-overlay matching separate. A compiler accepting
code proves neither behavior nor byte identity.

**Windows First.** Windows is the development, native-build, and runtime platform.
WSL is allowed ONLY for isolated historical-compiler matching verification,
never as a native build dependency or a way to rewrite canonical sources.

**Preserve User Work.** Do not run destructive cleanup, automatic upstream
replacement, installers, or synchronization as an incidental part of analysis.

**Report Blockers.** Do not replace unavailable Ghidra analysis with inline
assembly or make unsupported claims about installed tools.

## Source And Data Boundaries

| Location, Relative To Workspace Root | Role | Handling |
| --- | --- | --- |
| `resources/rood-reverseWin` | Working decompilation and documentation | Make scoped, reviewed changes here. |
| `resources/rood-reverse` | Upstream reference checkout | Read-only for this work; do not reset or edit it. |
| `resources/DataCrystal` | Saved format, executable, and mechanics documentation | Corroborating reference; verify claims against bytes. |
| `data` | Original extracted game files | Read-only, local, Git-ignored. |
| `disks` | Original image, cue, and disc-layout metadata | Read-only, local, Git-ignored. |
| `SLUS-01040` | Additional original disc extraction | Read-only, local, Git-ignored. |
| `tools/Base` | General analysis and development tools | Local installations are ignored; the explicitly registered PSX loader submodule is tracked. |
| `tools/PSX` | PSX-specific tools, emulators, and SDKs | Local, ignored installations; do not modify incidentally. |

Keep original art, audio, videos, executable binaries, and any necessary copies
in ignored local folders. Tool availability does not authorize redistribution of
proprietary SDKs, game content, or bundled material. Document paths and provenance
instead of copying installations into tracked source.

## Safety And Current Status

- Root `tools/windows.py verify-upstream` is read-only: manifest, revision, or
  source differences are reported as warnings and local work is used as-is; it
  never synchronizes. Explicit sync refuses local additions, edits, deletions,
  or populated unmanifested source. Preserve and review work before updating
  the reference snapshot.
- The 2026-10-12 import makes the noprompt fork authoritative over overlapping
  local source, as explicitly requested. All 289 C units compile in the isolated
  historical sweep; selected canonical byte checks pass. Earlier local match
  counts are historical, not acceptance of replaced implementations. No full
  unit, overlay, native runtime, or gameplay completion is claimed.
- Imported source still includes assembly and GTE assembly macros. They remain
  reference material, not assembly-free reconstructed progress. The acceptance
  requirements above are unchanged; the old 63-group result remains stale.
- Matching runs through [matching.py](tools/verification/matching.py) in WSL
  with splat macros from [splat_artifacts.py](tools/verification/splat_artifacts.py).
  Use `--candidate` for noncanonical iteration; only a fresh canonical `MATCH`
  is acceptance. Do not report matching from setup smoke tests, Ghidra
  evidence, candidate results, or Windows tests.

## Installed Tool Inventory

All paths below are relative to the workspace root. "Present" means files or
directories were inspected, not that the tool was launched, configured, or
validated against this game. Unconfirmed versions are deliberately not guessed.

### General Tools: tools/Base

| Tool And Location | Observed Installation | Role In This Project |
| --- | --- | --- |
| Ghidra: `tools/Base/Analysis/Ghidra` | Version folders `11.4.2`, `12.1.2`, `12.1.3`, `12.1.4`; 12.1.4 is preferred, with its application metadata verified. | Required static analysis, disassembly, decompiler, cross-references, and type recovery. |
| PSX loader: `tools/Base/Analysis/Ghidra/Extensions/ghidra_psx_ldr` | Root Git submodule at release `2026.10.03`; nested PsyQ signatures initialized; compatible 12.1.4 archive prepared. | PSX executable loading, PSX processor/GTE analysis, overlay support, and PsyQ signature reference. Activation in 12.1.4 still requires verification. |
| WLA-DX: `tools/Base/Assemblers/WLA-DX` | `Release-10.7` and `Source` folders. | Multi-target assembly tooling for other supported architectures; not the historical PSX C compiler or a decompilation deliverable. |
| Noesis: `tools/Base/Asset-Analysis/Noesis` | `Noesis.exe`, `Noesis64.exe`, plugins, optional plugins, scenes, and ReadMe. Version unconfirmed. | Inspect/convert supported game assets; Vagrant Story format/plugin compatibility must be checked. |
| vgmstream: `tools/Base/Audio/vgmstream` | `vgmstream-cli.exe`, codec DLLs, README, USAGE, and COPYING. Version unconfirmed. | Identify, decode, and inspect supported streamed game audio; not a replacement for game sequencing or playback logic. |
| ares: `tools/Base/Emulators/ares-v148` | Folder label `v148`; `ares.exe`, databases, shaders, SDL3. | Multi-system emulation and behavioral reference where the selected core supports the target. |
| Mesen2: `tools/Base/Emulators/Mesen2` | `Mesen.exe`; file metadata reports `2.1.1`. | Other-platform emulation; not the PSX execution/debugging tool for this project. |
| Floating IPS: `tools/Base/Patching/Floating-IPS` | `flips.exe`, COPYING, COPYING.gpl3. Version unconfirmed. | Create/apply IPS/BPS patches to local copies; never patch the canonical retail reference in place. |
| cc65: `tools/Base/Toolchains/cc65` | Source checkout with compiler sources, libraries, configs, samples, tests, and Makefile. Runnable compiler not confirmed. | 6502-family toolchain; not a MIPS/PSX compiler. |
| devkitPro: `tools/Base/Toolchains/devkitPro` | `devkitA64`, `devkitARM`, `devkitPPC`, libraries, MSYS2, tools, and installation metadata. | Other console/platform development; not a substitute for the required historical MIPS compiler. |

### PSX Tools: tools/PSX

| Tool And Location | Observed Installation | Role In This Project |
| --- | --- | --- |
| jPSXdec: `tools/PSX/Asset-Tools/jpsxdec` | Source checkout plus `jpsxdec/jpsxdec.exe`; Java dependencies and successful launch not verified. | Inspect/index/extract supported PlayStation disc audio/video, files, and TIM images from local copies. |
| DuckStation: `tools/PSX/Emulators/DuckStation` | `duckstation-qt-x64-ReleaseLTCG.exe`; file metadata reports `0.1.11826`; supporting Qt/SDL3 DLLs present. | PSX execution and visual/audio behavior reference; debugger availability depends on the installed build. |
| PCSX-Redux: `tools/PSX/Emulators/PCSX-Redux` | Source checkout with `src`, `tests`, tools, Windows projects, and build recipes. Runnable emulator not confirmed. | PSX-focused debugging, execution tracing, memory inspection, and analysis once a working build is verified. |
| PSn00bSDK: `tools/PSX/Toolchains/PSn00bSDK` | Source checkout with `libpsn00b`, examples, tools, CMake files, and docs. SDK build/install not confirmed. | Modern open-source PSX development libraries/tooling; not historical GCC 2.7.2/2.8.1 byte-matching compatibility. |

Do not confuse these local installations with the maintained Windows build's
downloaded tools under `build/tools`, or with the WSL-only historical compilers
under `/home/judee/.cache/vagrant-matching`.

## Ghidra: Windows Analysis

Preferred existing installation:
`D:\Dev\NativePort\VS-NativePort\tools\Base\Analysis\Ghidra\12.1.4`.
The user added this installation on 2026-10-04. Its application metadata reports
`12.1.4`, public build dated 2026-09-21. This update leaves older installations
and their version-specific extension archives untouched.

| Entry Point Or Requirement | Verified Location Or Evidence |
| --- | --- |
| GUI | `tools/Base/Analysis/Ghidra/12.1.4/ghidraRun.bat` |
| Headless analysis | `tools/Base/Analysis/Ghidra/12.1.4/support/analyzeHeadless.bat` |
| Java | Bundled docs require a 64-bit JDK 21; Windows PATH resolves Java and javac under `C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot\bin`. |
| Optional Python integration | Bundled docs support Python 3.9 through 3.14 for PyGhidra/debugger support; this is separate from the WSL verifier environment. |
| Project storage | Use an ignored local output directory such as `resources/rood-reverseWin/build/ghidra`, outside the Ghidra installation and source checkout. |

Launch from the workspace root in PowerShell:

```powershell
& .\tools\Base\Analysis\Ghidra\12.1.4\ghidraRun.bat
```

Before reconstructing a function, verify the imported binary hash, load address,
memory map, processor, and calling conventions. Import symbols/types only when
their provenance is known. Analyze callers, references, and data accesses;
translate the understood behavior into readable C++, then validate it separately.
For overlays, preserve distinct overlay/address-space context rather than
silently replacing another program mapped at the same CPU address.

For BATTLE.PRG, the current [splat configuration](config/BATTLE/BATTLE.PRG/splat.yaml)
maps file offset zero to CPU address `0x80068800`; its configured SHA-1 is
`d53aaccc3b3a2fc057d05e0dcea92f7182bc72a9`. Do not reuse this base for every binary.
The PSX loader supplies processor ID `PSX:LE:32:default`; do not invent an R3000
language ID or assume generic MIPS analysis fully models GTE behavior.

Local Ghidra reference documentation:

- [Getting Started](../../tools/Base/Analysis/Ghidra/12.1.4/GettingStarted.md)
- [Headless Analyzer](../../tools/Base/Analysis/Ghidra/12.1.4/support/analyzeHeadlessREADME.md)
- [Application Metadata](../../tools/Base/Analysis/Ghidra/12.1.4/Ghidra/application.properties)
- [PSX Loader README](../../tools/Base/Analysis/Ghidra/Extensions/ghidra_psx_ldr/README.md)

### PSX Loader Submodule And Version Compatibility

Repository: <https://github.com/lab313ru/ghidra_psx_ldr>.
Registered in the root [.gitmodules](../../.gitmodules), at the version-independent
path `tools/Base/Analysis/Ghidra/Extensions/ghidra_psx_ldr`. This is the SOURCE
checkout, not an active extension installed into a running Ghidra instance.

| Component | Pinned Revision |
| --- | --- |
| Loader, release `2026.10.03` | `3050da64925b9aad299489468000dc62b835c09a` |
| Nested `data/psyq` signature submodule | `e9e46e7e133ef275a79bfce650924f98edb086bc` |

The broad `tools/Base` ignore rule still excludes local application binaries and
archives. The loader's explicitly staged Git gitlink remains tracked; no blanket
unignore was added. Git submodule addition stages the gitlink and root registration,
but no commit was made and no unrelated files were staged.

Initialize this loader and its nested dependency on another checkout:

```powershell
git submodule update --init --recursive -- tools/Base/Analysis/Ghidra/Extensions/ghidra_psx_ldr
```

**Current Version:** Ghidra 12.1.4 is installed and is now the preferred analysis
version. No further upgrade is needed for this loader release. Its release and
CI matrix also support 12.1.3, but each installation needs its matching extension
ZIP; do not reuse the 12.1.3 ZIP in 12.1.4 or vice versa. Source/API compatibility
across versions is not assumed.

The compatible release archive was downloaded to:
`tools/Base/Analysis/Ghidra/12.1.4/Extensions/Ghidra/ghidra_12.1.4_PUBLIC_20261004_ghidra_psx_ldr.zip`.
Its SHA-256 matches the published release checksum:
`32abacd90a5950b97a0756f67e991c3cc0f8a62edd5531890d91af68a9916255`.

**Activation:** Open Ghidra 12.1.4, choose File -> Install Extensions, select or
add the prepared PSX loader ZIP, enable it, and restart Ghidra when prompted.
Confirm the PSX loader and language are available before importing game binaries.
The ZIP is prepared and verified; activation in the new installation and
successful analysis have not been verified or claimed.

**Verified Analysis Fallback (2026-10-04):** The existing user-installed PSX
extension for Ghidra 12.1.2 is active and was successfully used for a bounded
Windows headless inspection of BATTLE.PRG. The retail hash, load address,
`PSX:LE:32:default` processor, memory blocks, original instructions, references,
and decompiler output were checked for the current source unit. See
[DECOMP_PROGRESS.md](DECOMP_PROGRESS.md) and
[Probe-Ghidra.ps1](tools/verification/Probe-Ghidra.ps1) for the evidence and
repeatable command. This does not activate 12.1.4, certify a reconstructed
routine, establish runtime parity, or claim historical matching.

Keep older Ghidra installations separate. Back up analysis projects before opening
or migrating them in 12.1.4, and do not overwrite the only copy. The application
upgrade was supplied by the user; this update changes the resource guide and
prepares the matching loader archive, not the pinned submodule revision.

- [Loader Release And Version-Specific Downloads](https://github.com/lab313ru/ghidra_psx_ldr/releases/tag/2026.10.03)
- [Official Ghidra 12.1.4 Release](https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.1.4_build)

### Other Tool Usage Notes

WLA-DX's 10.7 release contains `wlalink.exe`, `wlab.exe`, and multiple architecture
assemblers; no MIPS assembler was found in that release directory. cc65 targets
6502-family systems. devkitPro's observed toolchains target ARM, AArch64, and
PowerPC, not this game's historical MIPS compiler variants.

vgmstream's usage guide documents `-m` for metadata-only inspection and `-o` for
WAV conversion. Keep its codec DLLs beside the executable and write decoded audio
only to ignored outputs. jPSXdec's README also documents file/TIM extraction and
links its STR format reference; check its non-commercial-use license before
distribution or reuse.

Noesis's executables report generic Windows resource version `1.0.0.1`; this does
not establish its actual release version. Installed plugins and each game's
format compatibility need separate verification.

PCSX-Redux is available as a source checkout with Windows Visual Studio projects;
do not describe it as a launched/working debugger until a build is verified.
PSn00bSDK supplies useful PSX hardware/API reference and modern homebrew tooling,
but is not a drop-in PsyQ replacement. Its README flags provenance concerns in
parts of its GTE library; do not copy implementation code without reviewing that
provenance and its licenses.

Local references for these tools:

- [vgmstream Usage](../../tools/Base/Audio/vgmstream/USAGE.md)
- [jPSXdec README](../../tools/PSX/Asset-Tools/jpsxdec/README.md)
- [PCSX-Redux README](../../tools/PSX/Emulators/PCSX-Redux/README.md)
- [PSn00bSDK README](../../tools/PSX/Toolchains/PSn00bSDK/README.md)
