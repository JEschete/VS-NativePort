# Cross-system static analysis: Ghidra, Rizin/Cutter, and paid alternatives

Research date: 2026-10-14 (US Eastern). Sources reviewed; no tools installed
and no ROMs, disc images, or game executables opened.

## Recommendation

Use **Ghidra as the main analysis database**, then evaluate **Rizin/Cutter as a
second decoder and scriptable inspection tool**, especially for older CPUs.
Do not buy a commercial suite or adopt an MCP wrapper until one representative
input proves the CPU, format, memory map, and needed automation actually work.

The [system matrix](CROSS_SYSTEM_MATRIX.md) maps these layers to the F drive's
system folders. This guide is broader than the current PS1 project's acceptance
rules; a future project must choose whether it aims at matching source,
documented disassembly, or a functional port.

## Ghidra

[Ghidra](https://github.com/NationalSecurityAgency/ghidra) runs on Windows,
macOS, and Linux and provides disassembly, decompilation, graphing, types,
cross-references, scripting, and extensible loaders/processor languages.

Its upstream processor tree includes 6502, 68000, ARM, MIPS, PowerPC, SuperH,
SuperH4, Z80, x86, and other families. That is a useful base for much of the
collection, **not a claim that every console variant or file format works**.

Important verified examples:

- The built-in 6502 language definition lists 6502 and 65C02. It does not
  establish SNES 65816 or PC Engine HuC6280 coverage.
- The Z80 processor directory does not by itself establish Game Boy SM83
  compatibility. A Game Boy CPU is not simply a stock Z80.
- Generic MIPS/PPC/ARM support does not establish every console's custom
  coprocessor, instruction extension, ABI, or loader.
- Saturn SH-2 and Dreamcast SH-4 are different targets.
- An x86 language does not alone establish every NEC V30MZ-specific detail
  needed for WonderSwan.

Use a verified console loader or explicitly reconstruct the map in a research
copy. Preserve bank/overlay identity with every address. Test a small set of
known instructions and branch targets before relying on decompiler output.

[GhidraMCP](GHIDRA_MCP.md) can be considered after the manual import is correct.
It is a query/annotation bridge, not additional console support.

## Rizin and Cutter

[Rizin](https://github.com/rizinorg/rizin) is a portable CLI analysis framework.
Its README explicitly lists Windows, scripting through rzpipe, binary/hex
inspection, disassembly, and a broad architecture set including:

- 6502, Z80, GameBoy, SNES, SPC700.
- m68k, SuperH, ARM/ARM64, MIPS, PowerPC, and x86.
- N64 RSP among the listed architecture backends.

It also lists Game Boy (Advance), Nintendo DS ROM, and Nintendo 3DS FIRM file
formats. **A 3DS FIRM loader is not an arbitrary retail 3DS game loader.**
Likewise, a backend name is not proof of complete analysis, console memory
mapping, or decompilation coverage.

[Cutter](https://github.com/rizinorg/cutter) supplies a GUI on Rizin, with
official Windows ZIP releases and plugin support. Its README references the
rz-ghidra decompiler integration. Check the exact release's installed plugins
and supported languages; the disassembler's CPU list is not a guarantee that
its decompiler understands every architecture.

Useful experiments:

1. Decode known byte sequences using the exact CPU variant.
2. Compare instruction lengths, operands, endianness, and branches with a
   second trusted tool.
3. Test banked inputs as distinct mapped regions, not as one fictional flat
   address space.
4. Use CLI queries for repeatable extraction of symbols, references, and hashes.
5. Save an analysis project plus original annotations, not a patched reference.

This is a stronger candidate for older-CPU breadth than assuming one generic
MCP provider supports every machine.

## Binary Ninja

[Binary Ninja](https://binary.ninja/features/) is a commercial analysis
alternative with intermediate representations, APIs, and extensible
architectures/loaders. Its official requirements list Windows 10/11 x64,
macOS, and Linux. The feature page lists ARM, MIPS, PowerPC, x86, and other
families plus a community plugin ecosystem.

**Edition matters.** Architecture availability, automation/headless use, and
licensing must be checked against the actual edition before choosing it.
Do not assume the free edition is a full multi-console research stack, or that
a community CPU plugin has all the semantics needed for useful high-level IL.

Evaluate locally on a small known input before spending money. Do not upload
game/private code to cloud demos or analysis services.

## IDA and Hex-Rays

[IDA](https://hex-rays.com/) is another commercial option with a mature plugin
and processor ecosystem. Processor modules, file loaders, and Hex-Rays
decompilers are distinct capabilities and entitlements.

Do not read "IDA Free" as "free decompilation of all console CPUs." The reviewed
[product page](https://hex-rays.com/ida-free) distinguishes architectures and
cloud-only decompilation offerings. Verify offline availability and the exact
processor/decompiler purchase before using it for local-only console work.

For this collection, Ghidra plus Rizin/Cutter is the better no-purchase baseline.
Paid tools are optional alternatives when a concrete feature justifies them.

## Common failure modes

- Treating compressed data, graphics, or jump tables as code.
- Losing cartridge banks by naming functions with only a CPU address.
- Translating one CPU while ignoring another CPU, DSP, or graphics processor.
- Guessing structures because the decompiler prints plausible pseudocode.
- Calling an assembled disassembly a recovered original C codebase.
- Calling a host-linked CPU translation a complete game port.

For 8/16-bit assembly-heavy titles, a labelled, reassemblable disassembly may
be the correct first deliverable; there may be no original C to recover.

## Sources

- [Ghidra overview, Windows installation, scripting](https://github.com/NationalSecurityAgency/ghidra/blob/master/README.md)
- [Ghidra processor tree](https://github.com/NationalSecurityAgency/ghidra/tree/master/Ghidra/Processors)
- [6502/65C02 language definitions](https://github.com/NationalSecurityAgency/ghidra/blob/master/Ghidra/Processors/6502/data/languages/6502.ldefs)
- [Ghidra Z80 language files](https://github.com/NationalSecurityAgency/ghidra/tree/master/Ghidra/Processors/Z80/data/languages)
- [Rizin architecture, formats, Windows, and scripting lists](https://github.com/rizinorg/rizin/blob/dev/README.md)
- [Cutter Windows distributions and plugins](https://github.com/rizinorg/cutter/blob/dev/README.md)
- [Binary Ninja features](https://binary.ninja/features/)
- [Binary Ninja platform/edition requirements](https://docs.binary.ninja/about/requirements.html)
- [IDA product/edition comparison](https://hex-rays.com/ida-free)
