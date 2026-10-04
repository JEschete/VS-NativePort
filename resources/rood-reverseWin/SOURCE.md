# Windows Working Copy

This is an editable source snapshot for Windows-specific development. It is a
regular directory in the parent repository, not another Git submodule.
The reference checkout at `../rood-reverse` remains unchanged and pulls from
`ser-pounce/rood-reverse`. Its upstream source always takes precedence over
local edits to the authoritative Windows decompilation snapshot.

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
