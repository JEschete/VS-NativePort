# C++ headers

Every native file compiles as C++23 (plan D5), but most headers are still written in C: `.h` files with `typedef struct`,
`_Static_assert`, `extern "C"`, macro guards, `#define` constants and everything at global scope. They work, but they
are not C++, and GitHub's language bar reports them as C for that reason. The owner decided on 2026-11-18 that they are
rewritten as real C++ headers rather than renamed or relabeled. Work is tracked in #441.

[HEADER_LEDGER.csv](ledgers/HEADER_LEDGER.csv) has one row per file that needs work. `tools\ledger\headers.py` measures
every row from the source, so the ledger cannot claim progress the code does not have. CI fails on a stale ledger, on a
`done` row that regresses, and on a new C-style header or a new `.cpp` with C linkage.

## Owner decisions (2026-11-18)

- **Real C++, not a rename.** Renaming `.h` to `.hpp` or overriding the language in `.gitattributes` would change the
  label but not the code. "I want this project correct, not just 'works'."
- **The copier is retired, file by file.** `tools\port\copy_to_native.py` reads the decomp under
  `resources\rood-reverseWin`, which the owner removed on purpose in `6b20016f0`, so it can no longer regenerate
  anything. `tools\port\copy_manifest.json` now lists files that have not been migrated yet. It no longer means the
  file can be reproduced. Generated files are edited directly. A migration that crosses a generated file, for example
  removing an `extern "C"` wrapper around a header that is being migrated, edits it in place. A generated header
  leaves the manifest when it is migrated itself, and from then on it is held to the strict quality checks like any
  other hand-maintained file.

## Reading the ledger

| Column | Meaning |
| --- | --- |
| `id` | `H` for headers, `S` for sources, numbered by path when the ledger was created |
| `kind` | `header` (`.h`/`.hpp`) or `source` (`.cpp`) |
| `origin` | `generated` (listed in `copy_manifest.json` at creation), `legacy` (the PS1 SDK layer under `Legacy\`) or `owned` |
| criteria columns | Measured from the source; see the next table. Blank for a source row except `generated` and `c_linkage` |
| `status` | `remaining`, `done` or `retired`; see below |
| `notes` | The only hand-written column |

Rows are created only when the ledger is first written: every `.h`, every `.hpp` that failed a criterion, and every
`.cpp` that had C linkage. A row follows a `.h` renamed to its sibling `.hpp`. A new file that fails a criterion is
rejected by `--check`; write it as C++ instead of adding it.

### Header criteria

A header is `done` when it is a `.hpp`, is not generated, uses `#pragma once` and every count below is 0.

| Column | Counts | C++ form |
| --- | --- | --- |
| `generated` | Listed in `copy_manifest.json` | Hand-maintained, inside the strict quality scope |
| `guard` | `macro` (`#ifndef X` / `#define X`) or `split` (the copier's `VS_TYPES_ONLY` sections) | `#pragma once`, with include cycles broken by forward declarations |
| `c_linkage` | `extern "C"` and `__cplusplus` | C++ linkage |
| `c_wrapped` | `extern "C"` blocks anywhere in `Native\` that include this header | A plain `#include` |
| `static_asserts` | `_Static_assert` | `static_assert` |
| `typedefs` | `typedef` | `struct X { ... };`, `enum class`, `using F = ...;` |
| `void_params` | `f(void)` parameter lists | `f()` |
| `gnu_attributes` | `__attribute__((...))` | Standard attributes such as `[[maybe_unused]]`, or removal |
| `macros` | `#define`, apart from guards and the permitted macros below | `inline constexpr`, `enum class`, inline functions, templates |
| `global_decls` | Declarations at global scope; `extern "C"` blocks count as global | Declarations in `VS::<System>` or `VS::<Module>`, named from the ledgers |
| `baseline` | Findings for the file in `tools\lint\baseline.json` | No findings |

Forward declarations of types (`struct SDL_Window;`) and `static_assert` are not counted in `global_decls`. A forward
declaration follows the header that defines its type, and that header is measured on its own. Namespaces and names
come from the ledgers: `final_name` in [FUNCTION_CATALOG.csv](ledgers/FUNCTION_CATALOG.csv), and the type and data
names in [TYPE_LEDGER.csv](ledgers/TYPE_LEDGER.csv) and [DATA_LEDGER.csv](ledgers/DATA_LEDGER.csv).

`PERMITTED_MACROS` in `headers.py` lists the macros C++ cannot replace, each with its reason. It has one entry,
`VS_ASSERT`, which turns its condition into text. A macro that a function, template or constant can replace does not
belong on that list.

### Source criterion

A source is `done` when its `c_linkage` count is 0: no `extern "C"` wrapper around an include, no `extern "C"`
definition or local declaration, and no `__cplusplus` test. Everything else in `.cpp` bodies, including the relaxed
compile settings, is part of #248.

### Status

| Status | Meaning |
| --- | --- |
| `remaining` | At least one criterion fails |
| `done` | Every criterion holds. `--sync` sets it, and it stays set: a later failure is reported as `header-regressed` or `source-regressed`, not recorded as `remaining` |
| `retired` | The file was deleted on purpose. Written by hand with a note saying why |

## Migrating a header

1. Pick a header whose dependencies are done or move in the same change. Start with leaves.
2. `git mv X.h X.hpp` and update every include of it.
3. Rewrite it against the criteria table, with `#pragma once`, its system or module namespace and the ledgers' names.
4. In the same change, remove every `extern "C"` wrapper around it and every `extern "C"` on definitions or local
   declarations of its functions. A function's linkage must agree in every file, so all includers change together.
5. If it was generated, remove it from `copy_manifest.json` and resolve what that exposes. The first migration
   (`Event/Camera.hpp` and `Event/Screen.hpp`, #347) established these steps:
   - Delete the path's line from `copy_manifest.json`. Only membership matters now; nothing compares the hashes.
   - Run `tools\ledger\gates.py --check`. Headers carry no `owned.csv` rows, so none were reported, but a header
     that a registered function names as its `header` would be.
   - `tools\scaffold\gen_solution.py` applies the copied-C settings only to manifest `.cpp` files, so a header changes
     no list. Rerun it for the new file name.
   - The header is now in the strict quality scope: `lower_case` members, `<cstdint>` types (the original's plain
     `char` was unsigned, so its bytes are `std::uint8_t`), and no magic numbers. Put the original layout's sizes and
     offsets in a `layout` namespace of named constants, each with its `static_assert`.
   - A generated file that stays in the manifest and declares C-linkage data of the migrated type keeps its linkage
     with a forward declaration inside its `extern "C"` block, for example
     `namespace VS::Event { struct CameraState; }` before `extern VS::Event::CameraState* Event_camera;`.
6. Update ledger locations that name the old path, then run `tools\scaffold\gen_solution.py`.
7. Build Debug and Release, run the tests that cover the changed code, run the quality check on the changed files, and
   run `python tools\ledger\headers.py --sync`.

## Not part of this work

- **C-style code in `.cpp` bodies and the relaxed compile settings:** #248.
- **PS1 SDK types and hardware-shaped bridges** (`u_char`, `RECT`, packet macros): the EC register (plan §15) and
  [SDK_API_LEDGER.md](ledgers/SDK_API_LEDGER.md). A migrated header may still use them.
- **Removing the copier itself:** possible once `copy_manifest.json` is empty, which needs the generated `.cpp` files to
  leave it under #248.

## Status

<!-- headers:begin (generated by tools/ledger/headers.py --sync) -->

**2 of 435 headers done**, 429 remaining, 4 retired. **0 of 813 sources free of C linkage**, 809 remaining, 4 retired.

| Header origin | Headers | Done | Remaining | Retired |
| --- | ---: | ---: | ---: | ---: |
| generated | 287 | 2 | 281 | 4 |
| owned | 138 | 0 | 138 | 0 |
| legacy | 10 | 0 | 10 | 0 |

| Header criterion | Headers failing | Sites remaining |
| --- | ---: | ---: |
| `.hpp` extension | 369 | |
| `generated` (in `copy_manifest.json`) | 281 | |
| `guard` (`#pragma once`) | 287 | |
| `c_linkage` | 127 | 292 |
| `static_asserts` | 118 | 362 |
| `typedefs` | 157 | 1,247 |
| `void_params` | 39 | 107 |
| `gnu_attributes` | 5 | 7 |
| `macros` | 69 | 10,521 |
| `global_decls` | 368 | 3,206 |
| `c_wrapped` | 357 | 5,681 |
| `baseline` | 23 | 581 |

| Source origin | Sources | Done | Remaining | `c_linkage` sites remaining |
| --- | ---: | ---: | ---: | ---: |
| generated | 199 | 0 | 195 | 195 |
| owned | 608 | 0 | 608 | 735 |
| legacy | 6 | 0 | 6 | 8 |

| Project | Headers done | Headers remaining | Sources done | Sources remaining |
| --- | ---: | ---: | ---: | ---: |
| Content | 0 | 13 | 0 | 12 |
| Engine | 0 | 81 | 0 | 90 |
| Game | 2 | 315 | 0 | 435 |
| Tests | 0 | 20 | 0 | 269 |
| Tools | 0 | 0 | 0 | 3 |

<!-- headers:end -->
