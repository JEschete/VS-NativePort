# Windows Working Copy

This is an editable source snapshot for Windows-specific development. It is a
regular directory in the parent repository, not another Git submodule.
The reference checkout at `../rood-reverse` remains unchanged and pulls from
`ser-pounce/rood-reverse`. It is reference material only. The Windows snapshot
has two accepted function-level matches beyond that reference; native-port
implementation has not started as an accepted deliverable.

Upstream provenance does not authorize overwriting local work. Build verification
is read-only and reports differences as warnings. Only explicit synchronization
may replace a pristine recorded snapshot; local additions, edits, and deletions
are refused.

## Reviewed Update: 2026-10-07

The explicitly requested update imported 85 maintained changes from the
read-only reference HEAD `ef3f027a64bf861bbf5ad3efa30851788d499ce4` using the
previously recorded ancestor. Three overlapping BATTLE files were merged to
preserve local verified names alongside incoming code. The reference remained
clean and unchanged; no fetch or pull was run there. Retired Linux make recipes
were not restored, and their new object naming was carried into the maintained
Windows adapter instead.

`upstream.json` records hashes of the committed reference files, not the locally
merged source. The source replacement guard was not bypassed or weakened.
See [Local Progress](DECOMP_PROGRESS.md) for preservation checks, fresh matching
controls, and the still-unaccepted trajectory candidate in the current unit.

## Source Revisions

| Source | Revision |
| --- | --- |
| [rood-reverse](https://github.com/ser-pounce/rood-reverse) | See `upstream.json` for the active authoritative commit |
| [maspsx](https://github.com/mkst/maspsx) | `746b895f02929ecd148af7b1f4ff05b69f973878` |
| [decomp-permuter](https://github.com/simonlindholm/decomp-permuter) | `059609d4aec73eb0650726772954e1ad575825f8` |

Both helper tools are included as source snapshots without nested Git history.
Local build outputs, extracted game data, and disc images were not copied.

`Makefile.windows` is the maintained matching-build entry point. The original
Linux Makefile, Dockerfile and copied Linux CI recipes are retired. Windows
build adapters are kept outside the authoritative replacement set.
Historical Windows compiler executables are still required for byte matching;
old-gcc 0.17 has no Windows compiler release. No Linux fallback is used.

Full Git reset cleanup is disabled because it would target the parent repository.
See `../../docs/WINDOWS_MIGRATION.md` for setup and verification limits.
