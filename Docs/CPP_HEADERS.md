# C++ headers

Every native file compiles as C++23 (plan D5), and every header is real C++: a `.hpp` with `#pragma once`, its
declarations in a `VS::<System>` or `VS::<Module>` namespace and no C idioms. The owner decided on 2026-11-18 that
headers are rewritten, not renamed or relabeled; the migration (#441) is complete. Its dated retirement logs, owner
decisions and per-header procedure are in `git show b0383a255:Docs/CPP_HEADERS.md`.

[HEADER_LEDGER.csv](ledgers/HEADER_LEDGER.csv) has one row per file that needed work. `tools\ledger\headers.py` measures
every row from the source, so the ledger cannot claim progress the code does not have. CI fails on a stale ledger, on a
`done` row that regresses, on a new C-style header or `.cpp`, and on C linkage or a retired criterion (`RETIRED_CRITERIA`
in `headers.py`) returning to any tracked file.

## Writing a header

- Name it `.hpp`, use `#pragma once` and declare in its system or module namespace with the ledgers' names: a
  `final_name` such as `Field::zoneContext` becomes `VS::Field::zoneContext`. Break include cycles with forward
  declarations.
- Use `<cstdint>` types; the original's plain `char` was unsigned, so its bytes are `std::uint8_t`. Put an original
  layout's sizes and offsets in a `layout` namespace of named constants, each with its `static_assert`.
- Check the basename first: a private `src\Field\X.hpp` next to a public `include\Field\X.hpp` resolves to the
  public one, and cpplint's `build/include_subdir` rejects a bare `"X.hpp"` include from the root of a `src` directory.
- Run `python tools\scaffold\gen_solution.py` for a new file, then `python tools\ledger\headers.py --check`.

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
| `typedefs` | `typedef` | `struct X { ... };`, `using F = ...;` |
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

## Status

<!-- headers:begin (generated by tools/ledger/headers.py --sync) -->

**351 of 435 headers done**, 0 remaining, 84 retired. **790 of 813 sources free of C linkage**, 0 remaining, 23 retired.

| Header origin | Headers | Done | Remaining | Retired |
| --- | ---: | ---: | ---: | ---: |
| generated | 286 | 222 | 0 | 64 |
| owned | 139 | 123 | 0 | 16 |
| legacy | 10 | 6 | 0 | 4 |

| Header criterion | Headers failing | Sites remaining |
| --- | ---: | ---: |
| `.hpp` extension | 0 | |
| `generated` (in `copy_manifest.json`) | 0 | |
| `guard` (`#pragma once`) | 0 | |
| `c_linkage` | 0 | 0 |
| `static_asserts` | 0 | 0 |
| `typedefs` | 0 | 0 |
| `void_params` | 0 | 0 |
| `gnu_attributes` | 0 | 0 |
| `macros` | 0 | 0 |
| `global_decls` | 0 | 0 |
| `c_wrapped` | 0 | 0 |
| `baseline` | 0 | 0 |

| Source origin | Sources | Done | Remaining | `c_linkage` sites remaining |
| --- | ---: | ---: | ---: | ---: |
| generated | 198 | 179 | 0 | 0 |
| owned | 609 | 607 | 0 | 0 |
| legacy | 6 | 4 | 0 | 0 |

| Project | Headers done | Headers remaining | Sources done | Sources remaining |
| --- | ---: | ---: | ---: | ---: |
| Content | 5 | 0 | 9 | 0 |
| Engine | 68 | 0 | 84 | 0 |
| Game | 258 | 0 | 425 | 0 |
| Tests | 20 | 0 | 269 | 0 |
| Tools | 0 | 0 | 3 | 0 |

<!-- headers:end -->
