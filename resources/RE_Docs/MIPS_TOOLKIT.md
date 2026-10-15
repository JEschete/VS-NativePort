# MIPS toolkit beyond PS1: N64, PSP, and PS2

Research date: 2026-10-14 (US Eastern). Upstream sources reviewed;
no binaries decoded or environments modified.

## Strong candidates

[spimdisasm](https://github.com/Decompollaborate/spimdisasm) and
[Rabbitizer](https://github.com/Decompollaborate/rabbitizer) provide
console-aware MIPS decoding beyond generic CPU-family recognition.

| Tool | Role | Explicitly documented console extensions |
| --- | --- | --- |
| spimdisasm | Matching disassembly, functions, data/rodata, symbols, splitting, pointer analysis | N64 RSP, PS1 GTE, PSP Allegrex, PS2 R5900 EE |
| Rabbitizer | Per-instruction decoder API and instruction classification | N64 RSP, PS1 GTE, PSP Allegrex, PS2 R5900 |

spimdisasm is mainly N64-focused but documents other MIPS platforms. Rabbitizer
is a lower-level decoder library, not a whole-program disassembler or loader.
The project already uses the splat ecosystem; inspect existing integration
before installing duplicate or conflicting versions.

## Uses for the collection

- **N64:** disassembly and symbol/section work feeding a matching project or
  [N64Recomp](N64_RECOMP.md). Treat CPU code and RSP microcode separately.
- **PSP:** verify Allegrex/VFPU-specific decoding rather than accepting output
  from a plain MIPS mode. Executable modules, relocations, and lifetime still
  need a correct format-aware workflow.
- **PS2:** decode EE-specific instructions, but do not interpret that as support
  for the entire EE/IOP/VU/GS system. VU programs and DMA-driven behavior require
  separate models and tools.
- **PS1:** continue to validate GTE, address mapping, and delayed operations
  against the project's trusted pipeline.

## Relationship to m2c

[m2c](M2C.md) consumes assembly to propose C and includes MIPS-related ABI
targets. Its output can help with functions, but decoding an instruction and
recovering valid higher-level semantics are separate milestones. Validate
custom instructions, register widths, calling conventions, and coprocessor
effects before promoting a candidate.

## Windows trial

Use an isolated, reviewed analyst environment rather than altering a game's
existing dependency pins. The upstream projects provide Python interfaces and
Rabbitizer also provides C/C++/Rust interfaces; this research did not establish
every current Windows wheel/build combination.

Start with known byte sequences for each console's extensions. Reassemble when
the matching workflow supports it. Explicitly test instructions generic MIPS
decoders commonly mishandle, and preserve original words when interpretation is
uncertain.

spimdisasm labels its same-VRAM overlay support experimental and warns that its
experimental ELF-object frontend is not guaranteed to produce matching or even
assemblable output. Choose the relevant frontend carefully.

## Sources

- [spimdisasm features, supported variants, and experimental boundaries](https://github.com/Decompollaborate/spimdisasm/blob/master/README.md)
- [Rabbitizer features, console variants, and bindings](https://github.com/Decompollaborate/rabbitizer/blob/master/README.md)
- [m2c targets](https://github.com/matt-kempster/m2c/blob/master/README.md)
