# Windows Working Copy

This is an editable source snapshot for Windows-specific development. It is a
regular directory in the parent repository, not another Git submodule.
The reference checkout at `../rood-reverse` remains unchanged and pulls from
`ser-pounce/rood-reverse`. It is reference material only. The Windows snapshot
now integrates `noprompt/rood-reverse`'s `decomp/verified-functions` branch,
with incoming source taking precedence over local overlaps by explicit user
direction. Earlier local match reports remain historical; native-port
implementation has not started as an accepted deliverable.

Upstream provenance does not authorize overwriting local work. Build verification
is read-only and reports differences as warnings. Only explicit synchronization
may replace a pristine recorded snapshot; local additions, edits, and deletions
are refused.

## Authoritative Fork Import: 2026-10-12

Integrated commit `3b3f35fb318178a5778a46db4dc93777e534c042` from the separate
reference at `../rood-reverse-noprompt`. Applied 333 changed source/configuration
files and restored nine namespace-coupled files to the fork versions. Their
implementation won every overlap; previous maintained source is backed up under
`build/fork-import-xxz7nexe/backup`. The original reference remained untouched.

Windows compatibility changes are confined to its adapter/verification layer
and independently verified symbol/size metadata. All 289 C units pass the
isolated historical compiler sweep; selected canonical function and read-only
table checks pass. This does not certify a complete overlay or native runtime.
See [Local Progress](DECOMP_PROGRESS.md) for exact scope and evidence.

## Fork Update: 2026-10-14

Imported all 36 modified `src/` and `config/` files from the clean
`decomp/verified-functions` reference at
`062096da30508e96fcb343cd7a4ce255632e1b9a`, relative to the previously
imported `3b3f35fb318178a5778a46db4dc93777e534c042`. Each copied file was
hash-checked against the reference; `upstream.json` now records the new revision
and incoming hashes. Both reference directories remained read-only.

The imported `func_8008AC78` now matches its complete 1,412-byte retail range.
An isolated WSL build of the updated source/config trees completed with
`All files match`; the BATTLE overlay is 577,828 bytes with SHA-256
`ad6914c81be92a008af8dece6ad394ce39c5aa42360542b5531716ebd00bba12`.
The build emitted 185 nonfatal compiler warnings and zero errors. This verifies
the historical PSX build and retail outputs, not the Windows native runtime.

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
controls, the historical trajectory candidates, and the subsequent accepted
trajectory reconstruction in the current unit.

## Source Revisions

| Source | Revision |
| --- | --- |
| [noprompt/rood-reverse](https://github.com/noprompt/rood-reverse/tree/decomp/verified-functions) | `062096da30508e96fcb343cd7a4ce255632e1b9a`, also recorded in `upstream.json` |
| [ser-pounce/rood-reverse](https://github.com/ser-pounce/rood-reverse) | Read-only original reference and source ancestry |
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
