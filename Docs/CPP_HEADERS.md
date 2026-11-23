# C++ headers

Every native file compiles as C++23 (plan D5), but most headers are still written in C: `.h` files with `#define`
constants and everything at global scope. Until 2026-11-18 they also used `extern "C"`, `typedef struct`,
`_Static_assert`, `f(void)`, GNU attributes, macro include guards and the copier's split type sections.
They work, but they are not C++, and GitHub's language bar reports them as C for that reason. The owner decided on
2026-11-18 that they are rewritten as real C++ headers rather than renamed or relabeled. Work is tracked in #441.

[HEADER_LEDGER.csv](ledgers/HEADER_LEDGER.csv) has one row per file that needs work. `tools\ledger\headers.py` measures
every row from the source, so the ledger cannot claim progress the code does not have. CI fails on a stale ledger, on a
`done` row that regresses, on a new C-style header or `.cpp`, and on C linkage or a retired criterion returning to any
tracked file.

## C linkage retired (2026-11-18)

C linkage was removed from every native file in one change rather than header by header. A name's linkage must agree in
every translation unit that declares it, so migrating one header's linkage would have changed all of its includers
anyway. Every file compiles as C++, and nothing outside the build looks up a native name by its C spelling: there are
no exports, `GetProcAddress` lookups or assembly files, and the Python tools load only system DLLs. Debug and Release
linked with no declaration that disagreed with its definition. The remaining `.h` files are still C-style in the other
criteria below; their declarations now have C++ linkage.

## C spellings retired (2026-11-18)

Four more criteria were cleared from every header by rewrites that keep the meaning, across all of `Native\`:

| Criterion | Rewrite |
| --- | --- |
| `static_asserts` | `_Static_assert(...)` became `static_assert(...)` |
| `void_params` | `f(void)` became `f()` |
| `gnu_attributes` | `__attribute__((unused))` on extern globals and one public field was removed; it has no effect there |
| `typedefs` | `typedef struct X X;` became `struct X;` and `typedef struct X { ... } X;` became `struct X { ... };` (unions alike). `typedef enum X X;` was removed: each followed its own `enum X { ... }`, which C++ already names. `typedef T N;`, including function pointers and arrays, became `using N = T;` |

Every tag already matched its typedef name, so no type changed its spelling. Enums stay unscoped: making one an
`enum class` changes its uses, so it belongs to that header's own migration. `RETIRED_CRITERIA` in `headers.py` holds
these four the way C linkage is held: a tracked header that regains one fails `--check`, even while other criteria
keep it remaining. `tools\ledger\hazards.py` reads struct definitions in either form, and its sites were unchanged by
the rewrite.

## Include guards retired (2026-11-18)

Every header now uses `#pragma once`, and `RETIRED_CRITERIA` holds `guard` to it.

- The 65 headers guarded by a whole-file `#ifndef X` / `#define X` / `#endif` were converted; no other file tested or
  defined any of those guard names.
- The 222 `split` headers were merged into one section each. The copier wrote a types section and a declarations
  section, and included type dependencies in `VS_TYPES_ONLY` mode, so an includer saw only their types. No source
  file set `VS_TYPES_ONLY`, and only one declarations section defined a macro (`SPU_PAN_TABLE_SIZE`), so making the
  declarations visible changes no expansion. It exposed one include cycle, `Effects/Runtime.h` ->
  `Battle/ActionResolution.h` -> `Battle/BattleTypes.h` -> `Effects/Runtime.h`. `ActionResolution.h` included
  `Action.h`, `BattleTypes.h` and `Hit.h` only for two pointer parameters, so it now forward-declares
  `Battle_ActionDefinition` and `Battle_HitResult` instead. Those are now `VS::Battle::ActionDefinition` and
  `VS::Battle::HitResult`, forward-declared in `Battle/ActionResolution.hpp`.

## Owner decisions (2026-11-18)

- **Real C++, not a rename.** Renaming `.h` to `.hpp` or overriding the language in `.gitattributes` would change the
  label but not the code. "I want this project correct, not just 'works'."
- **The copier is retired, file by file.** `tools\port\copy_to_native.py` reads the decomp under
  `resources\rood-reverseWin`, which the owner removed on purpose in `6b20016f0`, so it can no longer regenerate
  anything. `tools\port\copy_manifest.json` now lists files that have not been migrated yet. It no longer means the
  file can be reproduced. Generated files are edited directly. A migration that crosses a generated file, for example
  updating an include when a header becomes `.hpp`, edits it in place. A generated header
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

## Migrating a header

1. Pick a header whose dependencies are done or move in the same change. Start with leaves.
2. `git mv X.h X.hpp` and update every include of it. Check the new basename first: a private `src\Field\X.hpp` next to a
   public `include\Field\X.hpp` resolves to the public one. When a private header only adds a declaration or two to a
   public header of the same name, move them into the public header and mark the private row `retired` with a
   `merged into` note. A private header at the root of a `src` directory cannot be renamed in place either: cpplint's
   `build/include_subdir` rejects a bare `"X.hpp"` include.
3. Rewrite it against the criteria table, with `#pragma once`, its system or module namespace and the ledgers' names.
   A `final_name` such as `Field::zoneContext` becomes `VS::Field::zoneContext`. Define it with its qualified name at
   global scope (`ZoneContext VS::Field::zoneContext;`) or unqualified inside the namespace. Inside that namespace, uses
   drop the `VS::Field::` prefix unless a local or member of the same name would shadow it.
4. If it was generated, remove it from `copy_manifest.json` and resolve what that exposes. The first migration
   (`Event/Camera.hpp` and `Event/Screen.hpp`, #347) established these steps:
   - Delete the path's line from `copy_manifest.json`. Only membership matters now; nothing compares the hashes.
     Deleting or renaming a listed file without its line fails `tools\ledger\headers.py --check` (`manifest-stale`),
     because `tools\port\check_leftovers.py` reads every listed path (#468).
   - Run `tools\ledger\gates.py --check`. Headers carry no `owned.csv` rows, so none were reported, but a header
     that a registered function names as its `header` would be.
   - `tools\scaffold\gen_solution.py` gives only manifest `.cpp` files the cross-project include directories, so a
     header changes no list. Rerun it for the new file name.
   - The header is in the strict quality scope (every source is since #483): `lower_case` members, `<cstdint>` types (the original's plain
     `char` was unsigned, so its bytes are `std::uint8_t`), and no magic numbers. Put the original layout's sizes and
     offsets in a `layout` namespace of named constants, each with its `static_assert`.
   - A generated file that stays in the manifest and declares data of the migrated type forward-declares it, for
     example `namespace VS::Event { struct CameraState; }` before `extern VS::Event::CameraState* Event_camera;`.
5. Update ledger locations that name the old path, then run `tools\scaffold\gen_solution.py`.
6. Build Debug and Release, run the tests that cover the changed code, run the quality check on the changed files, and
   run `python tools\ledger\headers.py --sync`.

## Not part of this work

- **C-style code in `.cpp` bodies and the relaxed compile settings:** #248.
- **PS1 SDK types and hardware-shaped bridges** (`u_char`, `RECT`, packet macros): the EC register (plan §15) and
  [SDK_API_LEDGER.md](ledgers/SDK_API_LEDGER.md). A migrated header may still use them.
- **Removing the copier itself:** possible once `copy_manifest.json` is empty, which needs the generated `.cpp` files to
  leave it under #248.

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
