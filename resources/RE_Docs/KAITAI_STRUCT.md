# Kaitai Struct: testable binary-format descriptions

Research date: 2026-10-14 (US Eastern). Upstream documentation reviewed;
no schema was compiled and no retail asset was parsed for this research.

## What it is

[Kaitai Struct](https://kaitai.io/) describes binary layouts in declarative
`.ksy` files. Its compiler generates parsers for supported languages; the
corresponding runtime is a separate dependency. It is a format-analysis tool,
not a decompiler or an emulator.

**Recommendation: reuse selectively for data-layout investigation.** The root
[requirements file](../../requirements.txt) already declares
`kaitaistruct>=0.11,<1`; the imported snapshot also declares the runtime.
That establishes a declared dependency, not that the schema compiler or
visualizer is installed.

## Useful project applications

- Make header fields, table lengths, alignment, offsets, and byte order explicit
  when investigating extracted game files.
- Compare an asset generator's interpretation with an independently written
  schema, especially around palette/header/pixel boundaries.
- Turn a discovered data format into small regression fixtures instead of
  retaining only handwritten offset notes.
- Describe nested records or offset-based tables without duplicating parsing
  code across one-off scripts.

Start by finding and understanding the imported Kaitai material; do not create
a competing format definition for something the project already describes.

## Proposed Windows workflow

1. Identify an unresolved format question, not an entire archive to "decode."
2. Choose the compiler release and follow its official Windows distribution
   instructions. The Python runtime alone cannot compile `.ksy` files.
3. Work with local copies and source-owned minimal fixtures. Keep retail files,
   generated asset dumps, and exploratory parser output out of Git.
4. Express known fields and constraints, and label unresolved interpretations.
   Generate a parser into an ignored research/build directory.
5. Check known-good, truncated, invalid-length, out-of-range-offset, and
   endianness cases against independently calculated expected values.
6. Compare with existing parsers/generators before proposing a schema or parser
   change to the maintained source.

The public web IDE is not the recommended place for private or retail data.
Use a local compiler and local inspection tools; do not upload game files to
web services.

## Important limitations

- A parser that accepts a file does not prove that the field names or meanings
  are correct. Correlate fields with game-code accesses.
- Generated parsing is not automatically safe for arbitrary untrusted inputs.
  Bound allocations, offsets, counts, and decompression in the surrounding
  workflow, and test malformed data.
- A format parser does not reproduce timing, loading order, pointer relocation,
  GPU behavior, or game logic.
- Do not add Kaitai runtime dependencies to the native executable just because
  they are convenient for research. Offline investigation and production
  runtime architecture are separate decisions.
- The compiler, individual language runtimes, and shared format collection
  have separate licensing information; review the actual components reused.

## Sources

- [Official overview and downloads](https://kaitai.io/)
- [Upstream overview and compiler/runtime distinction](https://github.com/kaitai-io/kaitai_struct/blob/master/README.md)
- [User guide](https://doc.kaitai.io/user_guide.html)
- [Compiler](https://github.com/kaitai-io/kaitai_struct_compiler)
- [Python runtime](https://github.com/kaitai-io/kaitai_struct_python_runtime)
