# Assembly-backed native gap audit

Audit of the current local reference snapshots; no network update, conversion,
native-code change, or resource-file write is performed.

## Results

- Catalog rows checked: **4,207**.
- Port-target rows reconciled: **3,762**.
- Open native entries: **91** (90 stubs and 1 partial implementations).
- Matching C/C++ reference bodies located: **50**.
- Assembly located, but no matching C body located: **31**.
- Assembly-backed declarations with no local assembly located: **10**.

[ASSEMBLY_GAPS.csv](ASSEMBLY_GAPS.csv) contains **every open entry**, with its
catalog ID, native status/location, source classification, original symbol,
original declaration, located assembly, proposed provenance comment, C references
and purpose. Filter `source_status` for the assembly-only subset.

[ASSEMBLY_GAP_COVERAGE.csv](ASSEMBLY_GAP_COVERAGE.csv) also records assembly-backed
entries already implemented, aliases, and excluded/non-copied entries. These are
not counted as new gaps. Templates are checked against their actual `.inl` bodies
and instantiation bindings rather than counted as missing C functions.

## Assembly-only entries with a local source

| ID | Native name | Original source |
| --- | --- | --- |
| F01802 | `Battle::Ai::HasActorNavigationState` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\7C7E0.s:127` |
| F01803 | `Battle::Ai::CopyActorNavigationOverrideState` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\7C7E0.s:159` |
| F03568 | `Core::IdleInstruction` | `resources\rood-reverseWin\build\splat-artifacts\SLUS_010.40\build\src\SLUS_010.40\matchings\main\_asmNop.s:2` |
| F00761 | `Core::Math::BuildEulerRotationMatrix` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:1499` |
| F00762 | `Core::Math::LookupRotationSineCosine` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:1597` |
| F00763 | `Core::Math::MultiplyRotationMatrixInPlace` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:1664` |
| F00764 | `Core::Math::CosineFixedPoint` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:1731` |
| F00765 | `Core::Math::SineFixedPoint` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:1734` |
| F01017 | `Core::Math::LookupRotationTrigQuadrant` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\48C7C.s:188` |
| F01772 | `Core::Math::SquaredTileDistanceFromComponents` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\7BDD4.s:35` |
| F03399 | `Core::Math::LookupSineCosinePair` | `resources\rood-reverseWin\src\SLUS_010.40\31724.s:147` |
| F03400 | `Core::Math::Cosine` | `resources\rood-reverseWin\src\SLUS_010.40\31724.s:9` |
| F03401 | `Core::Math::Sine` | `resources\rood-reverseWin\src\SLUS_010.40\31724.s:12` |
| F03405 | `Core::Math::InverseSquareRootParts` | `resources\rood-reverseWin\src\SLUS_010.40\32154.s:79` |
| F03406 | `Core::Math::MultiplyMatrixIntoLeft` | `resources\rood-reverseWin\src\SLUS_010.40\32154.s:275` |
| F03409 | `Core::Math::SquareVectorComponents` | `resources\rood-reverseWin\src\SLUS_010.40\32154.s:24` |
| F03414 | `Core::Math::NormalizeVectorRegisters` | `resources\rood-reverseWin\src\SLUS_010.40\32154.s:162` |
| F02458 | `Effects::ScreenDistortion::WarpPolygonVertices` | `resources\rood-reverseWin\src\GIM\SCREFF2.PRG\3D8.s:9` |
| F00766 | `Field::CallRoomEntryPoint` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:1770` |
| F01833 | `Field::IsPointOutsideSolidVolumes` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\7E05C.s:283` |
| F00745 | `Render::DrawRoomGeometryPolygons` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:104` |
| F00746 | `Render::DecodeRoomPolygonVertices` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:506` |
| F00747 | `Render::ProjectAndDecodeRoomPolygon` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:548` |
| F00748 | `Render::SubdivideRoomTriangle` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:612` |
| F00749 | `Render::SubdivideRoomQuad` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:752` |
| F00750 | `Render::InterpolateRoomVertexMidpoint` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:921` |
| F00751 | `Render::IsRoomPolygonOnScreen` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:969` |
| F00752 | `Render::LinkRoomPolygonByDepth` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:1036` |
| F00770 | `Render::TintRoomPrimitiveColor` | `resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\2EA3C.s:2011` |
| F03404 | `Render::ApplyDepthCueColor` | `resources\rood-reverseWin\src\SLUS_010.40\32154.s:65` |
| F03408 | `Render::SetFogNearDistance` | `resources\rood-reverseWin\src\SLUS_010.40\32154.s:9` |

## Assembly not locally extracted

The following declarations are still gaps. Their `INCLUDE_ASM`
directives exist, but the corresponding instruction listings were not
found in any inspected reference. Do not invent a C implementation or
write splat output into a resource folder; extract any needed assembly
to an ignored build directory first.

| ID | Native name | Original declaration |
| --- | --- | --- |
| F01986 | `Effects::Plg074::UpdateTiledRing` | `src/EFFECT/PLG074.BIN/F8.c:3` |
| F01987 | `Effects::Plg074::UpdateRingArc` | `src/EFFECT/PLG074.BIN/F8.c:5` |
| F01988 | `Effects::Plg074::GenerateTrailOrigins` | `src/EFFECT/PLG074.BIN/F8.c:7` |
| F01989 | `Effects::Plg074::GenerateTrailTargets` | `src/EFFECT/PLG074.BIN/F8.c:9` |
| F01990 | `Effects::Plg074::GenerateTrailControlPoints` | `src/EFFECT/PLG074.BIN/F8.c:11` |
| F01991 | `Effects::Plg074::InitializeTrails` | `src/EFFECT/PLG074.BIN/F8.c:13` |
| F01992 | `Effects::Plg074::UpdateTrails` | `src/EFFECT/PLG074.BIN/F8.c:15` |
| F01993 | `Effects::Plg074::RenderTrails` | `src/EFFECT/PLG074.BIN/F8.c:17` |
| F01994 | `Effects::Plg074::UpdateTrailEmitter` | `src/EFFECT/PLG074.BIN/F8.c:19` |
| F01995 | `Effects::Plg074::UpdateRotatingSurface` | `src/EFFECT/PLG074.BIN/F8.c:21` |

## Tagged file: BATTLE/BATTLE.PRG/7E05C.s

All **21** catalog entries are accounted for: **8 already implemented**, **12 open with C counterparts**, and **1 open with assembly only**.

| ID | Original symbol | Native status | Source availability |
| --- | --- | --- | --- |
| F01825 | `func_800E685C` | implemented | c-or-cpp-body-available |
| F01826 | `func_800E6898` | stub | c-or-cpp-body-available |
| F01827 | `func_800E68A0` | stub | c-or-cpp-body-available |
| F01828 | `func_800E68EC` | stub | c-or-cpp-body-available |
| F01830 | `func_800E6A6C` | stub | c-or-cpp-body-available |
| F01831 | `func_800E6B24` | stub | c-or-cpp-body-available |
| F01832 | `func_800E6B4C` | stub | c-or-cpp-body-available |
| F01835 | `func_800E6EAC` | stub | c-or-cpp-body-available |
| F01836 | `func_800E6EB0` | stub | c-or-cpp-body-available |
| F01837 | `func_800E6F1C` | implemented | c-or-cpp-body-available |
| F01838 | `func_800E6F9C` | implemented | c-or-cpp-body-available |
| F01839 | `func_800E71DC` | implemented | c-or-cpp-body-available |
| F01840 | `func_800E72D0` | implemented | c-or-cpp-body-available |
| F01841 | `func_800E7370` | implemented | c-or-cpp-body-available |
| F01842 | `func_800E7454` | implemented | c-or-cpp-body-available |
| F01843 | `func_800E75EC` | stub | c-or-cpp-body-available |
| F01844 | `func_800E7608` | implemented | c-or-cpp-body-available |
| F01845 | `func_800E7660` | stub | c-or-cpp-body-available |
| F01829 | `func_800E6974` | stub | c-or-cpp-body-available |
| F01833 | `func_800E6BA0` | stub | assembly-only |
| F01834 | `func_800E6C34` | stub | c-or-cpp-body-available |

## Provenance and modernization

Keep the repository's existing per-function provenance format immediately
above each future implementation, for example:

```cpp
// decomp: BATTLE.PRG func_800E6974 (src/BATTLE/BATTLE.PRG/7E05C.s:114).
```

The CSV gives actual located assembly entry lines instead of relying on
catalog line zero. Shared/tail-entry control flow still needs assembly review.

Many counterparts are in `resources/rood-boi/pc/src`, not the upstream
decomp's main `src` tree. They are **behavior references**, not an approved
drop-in implementation: guest pointers, raw offsets, scratch state, partial
branches, and host-specific assumptions must be checked against the original
assembly before creating native C++ with typed state and ownership.
Test/probe mocks are excluded. A renamed static sorting helper is matched by
its explicit original-symbol provenance, not mistaken for absent C.

M2C is available at `C:\Tools\Base\Decompilation\m2c\m2c.py`.
No conversion was run during this inventory. For a genuine assembly-only
entry: retain M2C output and diagnostics in an ignored build directory,
review it against the full assembly/control-flow context, then implement
owned C++ with focused behavior tests and the provenance above.

Some open compatibility entries may already be bypassed by higher-level
native replacements. This is an entry-point coverage audit, **not** proof
that each entry is a live gameplay defect or should be translated rather
than retired. It does not certify implementations without `VS_UNPORTED`
as behaviorally complete. No native gameplay issue is closed by this audit.

## Reference snapshot pins

| Reference | Revision | Production source files scanned | Source SHA-256 |
| --- | --- | --- | --- |
| `rood-reverseWin` | `fd291cfe29b45656e00ff1e198f53e7730b59a19` | 383 | `bf50c6066554b4d602d390f2494f91ec4aedaf41399352e7d62a227a31c76468` |
| `rood-reverse` | `d4f7285631e6bc68e213fa304c662228240bfee7` | 382 | `611d70a7f4feba067403989b80c4acb06ab3dd396191a449128fcd49a5b8d17a` |
| `rood-reverse-noprompt` | `7339d4e86d99d5e848468ed1a5955b9782ec6ff5` | 389 | `12a7572a90a7007de6aab831b54fd90227230bbdd664390c85a1cea8fa415803` |
| `rood-boi` | `af51d41ceef7b1b9c3f9ecef152742ae8e97ca40` | 723 | `02fd12720a5b45b84f1b210f6085490656ae2374a5d40425cc58d1382fd3af59` |

`rood-reverseWin` is a tracked tree inside the parent repository; its
listed revision is the parent revision, not an independent submodule HEAD.
The other references are independent repositories. Restricted read-only
Git status was unchanged before/after inspection. No fetch or checkout ran.

The raw source-assembly label check found **0 uncataloged game glabels**
and **30 SDK/library labels**, excluded using the existing ledger rule.
SDK/platform replacement completeness remains a separate audit.

Rerun `python tools\port\audit_assembly_gaps.py --publish` after an authorized
reference update to identify newly available C bodies. Compare the recorded
revisions/fingerprints before reconciling any earlier assumptions.
