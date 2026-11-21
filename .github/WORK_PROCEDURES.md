# Work Procedures

How work in this repository is carried out, step by step. The rules come from the owner and from
[copilot-instructions.md](copilot-instructions.md); this file is the operating checklist. The current work item, if
any, is in [HANDOFF.md](../Docs/HANDOFF.md).

## Ground rules

- Commit and push at verified milestones without asking. Never add AI or assistant attribution: no
  `Co-authored-by` trailers, tool names or model names in commits, issues, PRs, comments or files.
- Never force-push or rewrite history. Preserve unrelated edits in the worktree.
- Never delete Ghidra, `tools\Base` or `tools\PSX`. Keep the repository small: delete scratch files in `build\` when
  done, and keep disc-derived data, captures, dumps and BIOS files out of commits.
- Work in the order set by tracker [#4](https://github.com/JEschete/VS-NativePort/issues/4). Log a newly found defect
  as an issue, with evidence, before fixing it.
- A passing build or baseline is not proof of behavior. Close an issue only with the fixing commit, the exact-revision
  CI run and concrete test or replay evidence.

## Session start

1. Read [HANDOFF.md](../Docs/HANDOFF.md) and the next open item in tracker #4.
2. Check CI on the last pushed commit: `gh run list --repo JEschete/VS-NativePort --limit 4`. Track and repair a
   failure instead of refreshing a baseline to hide it.
3. Check `git status`; finish or commit any verified work left from the previous session before starting new work.

## Making a change

C++ promotion (#276) is complete; its differential and promotion-check tooling was retired and remains in Git
history. Verify each change with the smallest builds, Catch2 tags, replays and quality checks that cover it.

### 1. Analyze

- Define the behavior change and measurable acceptance criteria before editing.
- Read the original source and assembly (decomp `src\`, Ghidra) before the generated copy. Note every field offset,
  width, sign and the calls each function makes. Trace callers, producers, consumers and reset paths.
- Batch independent reads and edits; hand long mechanical work to a background agent while doing something else.

### 2. Implement

- Keep typed cores separate from checked live bindings. A live binding refuses a missing pointer or an out-of-range
  index instead of reading through it.
- Add `// decomp: [OVERLAY ]<symbol> (src/<path>:<line>)` provenance to translated code, citing the symbol's
  definition in `resources\rood-reverse`; after a decomp rename write `(src/<path>:<line>; was <old name>)`.
  `python tools\ledger\provenance.py --check` validates the citations ([QUALITY.md](../Docs/QUALITY.md)).
- The copier is retired (plan D24): edit copied code in place and keep its provenance. Write new headers as C++
  ([CPP_HEADERS.md](../Docs/CPP_HEADERS.md)). Never hand-edit `.vcxproj` files; run
  `python tools\scaffold\gen_solution.py`.
- Stage new tool files before generating projects: the tools solution inventory uses tracked files. Restage the
  scoped files after fixes before committing; staging does not establish verification.

### 3. Test

- Write Catch2 tests for the changed behavior under a tag, covering each branch and refusal. Build Debug and run the
  tag with `build\Debug\VS.Tests.exe "[tag]"`; build Release as well for shared code.
- Keep each `TEST_CASE` to about 9 assertions. Every Catch2 assertion expands to several statements, and tidy's
  `readability-function-size` limit (80 statements, lambdas included) can still fail callback-heavy cases.
  Guard optional reads in the same function or a guarded value-returning helper; `.value()` or a Catch2 `REQUIRE`
  alone is not an explicit guard for tidy. Do not initialize throwing namespace-scope statics, compare structs by
  value or `std::bit_cast` bytes rather than `memcmp`, and keep LF line endings.
- For gameplay changes, run `python tools\test\regression.py check --config Debug`, which runs the tests and scenario
  replays against the baseline.

### 4. Quality

- Format with the repository's clang-format
  (`.venv-3\Lib\site-packages\clang_format\data\bin\clang-format.exe --style=file -i <file>`), then run
  `python tools\lint\quality.py --files <changed first-party files>`; add `--tidy` for new test worlds or
  callback-heavy cases. After mixed C++/Python edits, run the `--changed` pass over the complete changeset.
- Changing a widely included header widens tidy to every translation unit that includes it. Avoid editing shared
  headers when a local helper would do.
- Do not replay the CI workflow locally; CI runs tool tests and changed-file quality checks on the pushed revision.
  Rule/scope changes and manual dispatch run a full source scan.

### 5. Record, publish and continue

- Keep function, data and type status in the canonical [ledgers](../Docs/ledgers/). Record only what was
  demonstrated.
- Commit only the change's files. Write the message to a UTF-8 file without BOM and use `git commit -F <file>`;
  no attribution. Push, then `gh run watch <id> --exit-status`.
- After CI passes, post a concise issue checkpoint with the commit, CI run and evidence location. Close an issue only
  when all its acceptance criteria are verified, then continue the next ordered item.
- Update [HANDOFF.md](../Docs/HANDOFF.md) at a package boundary, an owner pause or a genuine blocker. Include the exact
  current revision, next action and any unresolved limits.
- Retain referenced verification evidence; clean only disposable scratch intermediates.

## Stopping

When the owner asks to stop: finish or shelve the step in progress, commit and push any verified work with its limits
stated, update [HANDOFF.md](../Docs/HANDOFF.md) with the exact state and the next step, and report the status.
