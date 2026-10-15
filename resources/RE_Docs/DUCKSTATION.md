# DuckStation: independent behavior comparison

Research date: 2026-10-14 (US Eastern). Upstream documentation and debugger
source reviewed; no game run or debugger integration was tested.

## Recommendation

**Secondary emulator for controlled comparisons.**
[DuckStation](https://github.com/stenzek/duckstation) is useful for reproducing
a retail sequence independently of PCSX-Redux and for investigating visual
behavior. Its Qt debugger source includes stepping, breakpoints, register and
memory views, stack/call-stack models, and memory searching.

These implementation details establish that a debugger exists, not that every
packaged Windows build exposes the same UI or a stable automation API.

## Appropriate uses

- Compare a questionable observation with a second emulator implementation.
- Establish the expected retail sequence with controlled settings and inputs.
- Inspect a bounded instruction/memory state when a local debugger is available.
- Separate renderer-enhancement effects from behavior present in the retail
  baseline.

For a scripted trace pipeline, [PCSX-Redux](PCSX_REDUX.md) is the stronger first
candidate because its GDB and web interfaces have dedicated documentation.
DuckStation advertises remote debugging, but this research did not establish a
versioned remote-protocol contract or tested VS Code configuration. That is not
a claim that no remote interface exists.

## PGXP is not a baseline

DuckStation's PGXP implementation tracks additional precision for geometry and
rendering, with CPU/GTE-related hooks visible in source. It is an enhancement,
not a replacement for the PS1's original integer/fixed-point behavior.

Disable PGXP, widescreen hacks, overclocking, cheats, texture replacements, and
other enhancements for baseline investigations. Record renderer, resolution,
CPU mode, BIOS, region, disc identity, and timing-related settings.

Do not infer that PGXP necessarily changes the architectural CPU registers, or
that it requires a particular interpreter/recompiler mode, merely from the
presence of instruction hooks. Those questions require implementation-specific
verification. The reason to exclude it here is that enhanced output is not the
unmodified comparison target.

## First experiment

1. Use a reviewed official Windows release, local disc/BIOS material, and a
   disposable memory-card fixture.
2. Record a baseline configuration with enhancements disabled.
3. Reproduce one small sequence also observed in PCSX-Redux.
4. Compare the same event and relevant state, not only screenshots or elapsed
   time. Record when an overlay loads and which address space is being read.
5. Investigate differences in emulator configuration before attributing them
   to game code.
6. Preserve local notes and reproducible inputs. Treat discrepancies as
   evidence to investigate, not permission to select the more convenient result.

Save states are emulator/build-specific conveniences, not portable proofs or
substitutes for testing actual memory-card transactions.

## Licensing and limits

At research time, upstream README/source identify CC-BY-NC-ND-4.0 terms.
Do not assume permissive reuse, modified-build redistribution, or bundling is
allowed. Use official distributions independently; review current terms before
redistribution or integration.

DuckStation does not compile matching C, supply historical compilers, or certify
physical-hardware behavior. Neither enhanced rendering nor an apparently
correct title screen establishes accepted native-port progress.

## Sources

- [Upstream README: features, Windows distribution, and terms](https://github.com/stenzek/duckstation/blob/master/README.md)
- [Qt debugger interface](https://github.com/stenzek/duckstation/blob/master/src/duckstation-qt/debuggerwindow.h)
- [Qt debugger implementation](https://github.com/stenzek/duckstation/blob/master/src/duckstation-qt/debuggerwindow.cpp)
- [PGXP interface and provenance](https://github.com/stenzek/duckstation/blob/master/src/core/cpu_pgxp.h)
