# ImHex: local binary inspection

Research date: 2026-10-14 (US Eastern). Documentation research only; no
installation or game-file experiment was performed.

## Recommendation

**Useful optional desktop tool for data research.**
[ImHex](https://github.com/WerWolv/ImHex) supplies a graphical hex editor,
typed data views, byte/string search, bookmarks, and a C++-like pattern
language for describing file structures. Official Windows builds are available.

This complements [Ghidra](GHIDRA.md): use Ghidra for code and references, ImHex
for concrete byte layout, and [Kaitai Struct](KAITAI_STRUCT.md) when a format
description should become a generated parser or regression fixture.

## Concrete uses here

- Check whether a symbol-derived table actually contains the expected number
  and width of entries.
- Inspect image headers, palettes, alignment padding, and adjacent pixel data
  without conflating a file offset with a PS1 RAM address.
- Compare candidate record layouts using signed/unsigned values and explicit
  byte order rather than only interpreting a raw hex string.
- Bookmark relevant ranges while investigating disc-extracted data files.
- Use custom text encodings when plain ASCII search cannot explain a region.

These are proposed uses, not evidence that an existing asset interpretation is
wrong.

## A small, safe trial

1. Select a reviewed stable Windows release from the official project.
   Use desktop/local operation rather than opening retail files in the web
   version.
2. Open a disposable local copy of a small extracted file. Preserve the
   original hash, size, and provenance.
3. Find a known header or table already referenced by the project's code.
   Confirm the offset and byte order independently.
4. Apply a small, reviewed pattern to that range. Record which fields are
   demonstrated and which are still guesses.
5. Export only original schema/notes appropriate for version control, not
   retail bytes or copyrighted text.
6. If the layout becomes an input to tooling, add programmatic tests through
   the existing parser/generator workflow. An editor screenshot is not a
   durable regression test.

## Limits and cautions

ImHex can patch bytes and manage patches. That capability must not be used to
alter the retail comparison target, make a mismatch disappear, or change an
authoritative extracted file during inspection.

Patterns are executable analysis logic, not automatically trusted annotations;
review third-party patterns/plugins before loading them. A plausible pattern
display does not prove format semantics, pointer validity, decompression
behavior, or runtime hardware behavior.

Do not add ImHex as a game-build dependency. It is an optional analyst tool.
It does not resolve missing compilers, reconstruct matching C, or provide PS1
runtime traces.

## Sources

- [Upstream README and feature list](https://github.com/WerWolv/ImHex/blob/master/README.md)
- [Official releases](https://github.com/WerWolv/ImHex/releases)
- [Official documentation](https://docs.werwolv.net/imhex)
