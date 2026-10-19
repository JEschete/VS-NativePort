# Copilot Instructions

## Project Guidelines
- Never attribute changes to the assistant in commits. Do not add assistant authorship, co-author trailers, signatures, or AI attribution to commit messages or metadata.
- When about to work on a new feature, do web research to understand how it should work and review any related existing code or documentation.

## Architecture and Sources of Truth
- Follow owner decisions and implementation packages in [NATIVE_SOURCE_PORT_PLAN.md](../Docs/NATIVE_SOURCE_PORT_PLAN.md); use [HANDOFF.md](../Docs/HANDOFF.md) for recent evidence and remaining limits. Historical results are not fresh verification of the current worktree.
- Keep `resources\rood-reverseWin` and the upstream reference read-only. Native implementations belong in the existing systems; do not include or link reference code from `resources\`.
- Keep function/data/type status in the existing [ledgers](../Docs/ledgers/). Track behavior verification, C++ promotion and compatibility retirement separately; compiling or renaming a file does not establish all three.
- Treat existing hardware-shaped compatibility bridges as tracked migration debt. Follow the plan's typed native APIs and EC retirement gates rather than extending those bridges as the destination.

## Issue Tracking and Verification
- Use https://github.com/JEschete/VS-NativePort/issues/4 as the ordered work tracker. Complete maintained-code quality work before the listed gameplay priorities, and do not silently reorder them.
- Search existing issues before creating a tracker. Log newly discovered bugs with evidence and reproduction steps before implementing a fix; distinguish confirmed defects from suspected causes.
- Use GitHub CLI directly for authorized issue, comment and other GitHub updates that would otherwise open MCP approval forms. Never expose tokens or store credentials in the repository.
- Close an issue only after its acceptance criteria have been verified. Include the fixing commit and concrete tests or replay evidence; a successful exit or a passing baseline alone is not proof of gameplay completion.
- Inspect failed GitHub Actions quality checks on pushed commits when starting or resuming work and after any authorized push. Track and repair failures instead of refreshing the baseline to hide them. Keep CI issues open until the pushed fixing revision has a confirmed successful run.
- The owner authorizes commits and pushes for work through the remaining issues in ordered tracker #4 and related defects discovered during that work. Use the owner's configured identity with no additional attribution. Preserve unrelated edits; do not force-push or rewrite history without separate authorization.
- For each issue, implement and verify the acceptance criteria, commit and push the scoped fix, confirm the relevant CI checks, then close with the fixing commit and evidence. Continue to the next ordered issue; log new defects before fixing them. Stop only for a genuine blocker or an owner pause, recording the evidence and next observation needed.

## Working Loop and Productivity
- Start from the current worktree, ordered issue, affected handoff/plan sections and ledger IDs. Preserve unrelated edits. Define a bounded behavior change and measurable acceptance criteria before implementation.
- Trace affected callers, producers, consumers, callbacks and reset/cleanup paths. Reuse existing helpers and read the original source/assembly before guessing behavior or field meanings.
- When Serena is configured, prefer symbol outlines, bodies and references over repeated whole-file reads. Verify the C/C++ index covers the affected translation units; missing results from an incomplete or stale index are not proof of no callers.
- When Beads is configured, use `bd prime` and linked issue history to recover context, then explicitly claim work consistent with #4's priorities. Keep Beads for dependencies, discoveries, attempts and handoffs, not duplicate per-function status or a replacement priority tracker.
- Discover available tools before relying on them. Proposed harness operations in [Decomp Process.md](../Docs/Decomp%20Process.md) are not implemented commands. Do not initialize tools, rewrite agent guidance or configure outbound sync as an incidental step.
- If repeated attempts produce no new evidence, record the failed approaches, blocker and next observation needed. Preserve a concise handoff instead of repeating the same investigation or weakening the acceptance criteria.

## Validation and Handoff
- Use existing build, Catch2 test selections and [quality checks](../Docs/QUALITY.md). Run the smallest checks that cover the change; expand for shared interfaces, state or lifecycle. Do not weaken tests, expected outputs or quality baselines to make an unexplained failure pass.
- For generator changes, check output correctness, ownership preservation and stability on a second generation run. Do not regenerate over conflicting local edits.
- Verify the reported behavior, not a proxy: exercise actual controller actions for interactive combat; do not edit HP, bypass event flags or force completion to claim a gameplay fix. Align scene/event state for reference comparisons, not just timestamps.
- Record exact commands/selections, configuration, source revision and dirty state, outcomes, local evidence paths and remaining limits. Update relevant ledgers/issues only for what was demonstrated. Keep disc-derived data, captures, memory dumps and BIOS files out of commits and issue attachments.

## Local environment (development machine)
- **Platform:** Windows x64 with native Windows tools and PowerShell. Do not add Linux-only tools or workflows requiring WSL/Unix shells; verify Windows compatibility before introducing optional tooling.
- **Build:** `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe VagrantStory.Native.sln /m /p:Configuration=Debug /p:Platform=x64`; tests `test-native.cmd Debug`. Regenerate projects with `python tools\scaffold\gen_solution.py` (never hand-edit `.vcxproj`). See `Docs\BUILDING.md`.
- **Game data:** disc image `disks\SLUS-01040.cue/.bin`; extracted data `Native\data` (`build\Debug\VS.Extract.exe extract disks\SLUS-01040.cue Native\data`, needs FFmpeg on PATH: winget Gyan.FFmpeg).
- **External tools:** `C:\Tools` (shown in the solution as `Tools\C-Tools`; inventory in `Docs\TOOLS_INVENTORY.md`). Repository tools in `tools\` (`Tools\tools`).
- **DuckStation (reference emulator):** installed at `C:\Tools\Systems\PSX\Emulators\DuckStation\duckstation-qt-x64-ReleaseLTCG.exe`. Never run or reconfigure the installed copy for tests: `tools\test\duckstation_setup.ps1` makes a portable copy in `build\duckstation` (git-ignored) with its own `settings.ini`.
- **PS1 BIOS:** `F:\Emulators\PlayStation\bios\SCPH1001.BIN` (NTSC-U, used for SLUS-01040; `scph5501.bin` also there). Copies exist under `F:\Emulators\LaunchBox\Emulators\RetroArch\system\`. Never copy a BIOS into the repository.
- **Reference captures:** `tools\test\duckstation_capture.ps1 -SkipIntro -Keys '40:Down,42:X' -Seconds 45,50 -Out build\ds_x` (keys briefly take focus; Z = Cross, X = Circle, Return = Start).
- **Native test runs:** `tools\test\run_native.ps1 -NoCopy -SkipIntro -Frames 1800 -At '1500' -Press '1200:Down,1300:Circle'`. Ticks are 1/60 s. **Skip the intro movie by pressing Start (`-SkipIntro` = `--press 1000:Start`)**: the title menu is then up by tick ~1100 instead of ~3300. Crash stacks are in `build\Debug\vs-native.log`.
- **Menu testing:** use the original prologue skip: Start during the opening, then Start again at the A. J. Durai quote. Verified replay inputs are `--press 1000:Start,1300:Circle,3000:Start,4000:Start,7000:Triangle`: MAP009 loads at tick 4058 and MAINMENU.PRG at 7005. Avoid repeated Circle presses, which can toggle the battle sphere. Use the full prologue for combat troubleshooting and implementation; skipping it is not combat verification.

## Code Translation Guidelines
- When translating assembly into native implementations, add provenance comments identifying the original decomp symbol and source/assembly location so future upstream decomp progress can be cross-referenced. Use meaningful semantic names and preserve traceability.
- Make fixes to generated native code through the copy generator or a registered owned implementation, preserving original decomp symbol/source provenance; do not patch generated output directly.
- Separate on-disk offsets/layout from native pointers and ownership. Check bounds, element strides, aliases, arithmetic widths, dispatch signatures and repeat-entry resets when changing these contracts; see [PORTING_WATCHLIST.md](../Docs/PORTING_WATCHLIST.md).