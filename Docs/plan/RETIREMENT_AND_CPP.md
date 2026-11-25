# Native Source Port Plan: Retirement strategies and C++

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md)

## 7. Retirement strategies, in detail

| Strategy | When | Mechanism | Tracking |
|---|---|---|---|
| **R: rewrite at copy** | The replacement is mechanical and 1:1 (math, pad reads, crt, rand, VSync, scratchpad) | Call sites were rewritten into the modern API at copy time (the copier is retired, D24) | `[x]` only when the original dependency is actually gone; a renamed GTE/scratch/timing adapter remains tracked by its EC issue |
| **T: temporary shim** | The replacement needs design work at each call site (draw primitives, SPU, Ds, Press, Card) | A `Legacy\` function with the SDK name that translates into the modern API | `[ ]` → `[A]` shim exists → `[M]` all call sites migrated → `[x]` deleted |
| **D: delete** | No native meaning (critical sections, events, DrawSync, SetDispMask, ResetGraph, InitGeom, debug font, SPU IRQ setup, Ds ready system) | The call was removed at copy time, and any needed effect is reproduced by the system | `[x]` at copy |

Ordering follows §14: title input/lifecycle in M3, rendering in M4, audio in M5, movies in M6, field/actors in M7,
battle in M8 and menus/saves in M9. Content and memory retirement follow each slice; M10 closes remaining coverage.
Phase 2e is the final audit, not the start of retirement. Keep verified comparison paths during replacement, but
retaining one in the shipping runtime is a separate open decision (§13.3), not an automatic exemption.

## 8. C vs C++

### 8.1 Why C++ (answer to the owner's question)

The decomp stays as a reference in C, so the port gains nothing from staying in C. C++23 gives:

| Benefit | Detail |
|---|---|
| Type safety for IDs | `enum class`, `Handle<T>`, `ItemId` instead of `u_char`, which also removes the 255 cap |
| Value types with operators | `Fixed12`, `Vec3`, `Mat3` (the GTE replacement reads like math) |
| RAII | Textures, files, voices and loaders clean themselves up, with no leak paths through module resets |
| Real containers | `std::vector`, `std::span`, `std::array`, flat maps, instead of fixed arrays plus counters |
| Compile-time checks | `constexpr` tables, `static_assert` on formats, `consteval` validation of constant data |
| Namespaces/classes | Map 1:1 onto systems and modules, with no `vs_battle_` prefixes |
| Variants/visitors | Draw items, effect kinds, event commands |
| Exceptions-free error handling | `std::expected` for decoders and loaders |
| Testability | Systems are objects that can be built in isolation |

Costs: compile times (mitigated by PCH and static libs), and the temptation to over-abstract (mitigated by "data for
parameters, code for behaviors").

### 8.2 Promotion checklist (sets `cpp = yes`)
1. Compile with the strict settings (every source does since #483).
2. Move it into `namespace VS::<System>`. Free functions become methods where they operate on one state object.
3. Replace raw IDs with typed IDs/handles, and fixed arrays with `std::array`/`std::span`.
4. No remaining shim calls (`modernized = yes` is a prerequisite).
5. Full warning set (`-Wall -Wextra`, warnings as errors) is clean.
6. Passes a test or canonical-state replay/reference check for the migrated behavior; attach evidence before closure.

Each step is one column of [CPP_PROMOTION.csv](../ledgers/CPP_PROMOTION.csv), re-verified by
`tools\ledger\gates.py --check`. Steps 1, 2 and 5 are measured and synchronized by the tool from the generated projects,
source and quality baseline. Steps 3, 4 and 6 are explicit reviewed claims that the tool can only falsify: a signature
free of SDK integer typedefs and `void*` is necessary for a typed interface but does not prove one, and a linked record
proves a test exists, not that it covers everything. A `.cpp` suffix, renamed function, forwarding wrapper or
declaration cannot produce `cpp=x`, and any remaining consumer of the old symbol blocks it, including one in generated
code. Compiling as C++ is not promotion: generated code keeps global names and untyped signatures (its C linkage was
retired in #441), so a function with generated callers cannot reach `cpp=x` until those callers migrate. See
[the ledger summary](../ledgers/SUMMARY.md) for the current inventory, [MODERNIZATION.md](../MODERNIZATION.md) for the
per-function work and regression rules, and [PLATFORM_HAZARDS.md](../PLATFORM_HAZARDS.md) for what PS1 hardware
tolerated.
