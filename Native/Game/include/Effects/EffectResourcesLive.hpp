#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <optional>
#include <span>

#include "Effects/EffectResources.hpp"

struct Effects_RendererSlot;

// Live binding for the effect resources (EffectResources.hpp) over the
// battle's package records (Effects_package, Effects_actionPackage,
// Effects_secondaryPackage, Effects_savedPackage), the loader state
// (EffectResourcesLiveState.h), the renderer table (Effects_effectRenderers)
// and the EFFPURGE.BIN purge tables. The host reads and uploads through the
// disc queue, the high heap, the overlay arena, VRAM and the texture pages;
// sound blocks bind through the room sound banks.
//
// The block walk keeps each package's typed block views alongside the legacy
// pointers it publishes, so the typed readers (curves, transforms, gravity)
// see a block only while the package still points at it. The walk accepts
// only the loaded package buffer (bounded by its file size and heap payload)
// and the built-in empty package; other pointers are refused with a
// diagnostic. The C-shaped entries keep the original signatures for the
// copied effect runtime.
namespace VS::Effects {
// The current package's view of a bound block, or an empty view when the
// package is unbound, lacks the block or no longer points at it. Texture
// tables are bound by TextureBindings.hpp and have no view here.
[[nodiscard]] std::span<std::byte> LiveBoundPackageBlock(PackageBlockType type);

// Walks `package` into the current package (func_800D8060), publishing the
// legacy block pointers and the curve and particle extents; nullopt
// (refused) without a current package.
std::optional<PackageWalk> BindLiveEffectPackage(std::span<std::byte> package);

// The loader state and the package context mirrored from the globals.
[[nodiscard]] EffectResourceState LoadLiveEffectResourceState();
void StoreLiveEffectResourceState(const EffectResourceState& state);
[[nodiscard]] EffectResourceContext LoadLiveEffectResourceContext();
// Stores the current and saved packages; the action and secondary packages
// are fixed records.
void StoreLiveEffectResourceContext(const EffectResourceContext& context);
// The EFFPURGE.BIN tables (D_800FAE4C-D_800FB34C).
[[nodiscard]] EffectPurgeTables LiveEffectPurgeTables();

// func_800D7814 (6F014.c:446).
void LiveInitializeEffectResources();
// func_800D7890 (6F014.c:460).
void LiveSelectEffectResourceContext(int use_primary_context);
// func_800D78B8 (6F014.c:471).
void LiveSaveEffectResourceContext();
// func_800D78CC (6F014.c:473).
void LiveRestoreEffectResourceContext();
// func_800D78F0 (6F014.c:477).
void LiveUpdateParticleGravityAndDrag();
// vs_battle_loadEffPurge (6F014.c:570).
int LiveLoadEffectPurgeOverlay();
// func_800D7BF8 (6F014.c:580).
int LiveLoadEffectPackage();
// func_800D7C5C (6F014.c:592).
void LiveReleaseEffectPackageResources();
// func_800D7CFC (6F014.c:612).
int LiveUpdateEffectTextureLoading();
// func_800D7EF4 (6F014.c:681).
int LiveUpdateEffectPluginLoading();
// vs_battle_setEffectExec (6F014.c:725): installs plugin entry `entry` as
// renderer `index`; false (refused) past either table.
bool LiveSetEffectRenderer(PluginEntry entry, int index);
// func_800D8060 (6F014.c:746): the package buffer or the empty package.
void LiveBindEffectPackageBlocks(void* package);
// func_800F9800 (EFFPURGE 0.c:590): the renderer shape the entry-point
// table uses; the arguments are ignored, as in the original.
int LiveInvokeAllPluginUpdates(Effects_RendererSlot* slot, unsigned int command,
                               int packed);
// vs_effpurge_exec (EFFPURGE 0.c:911): the action request's effect.
void LiveConfigureEffectResources();
}  // namespace VS::Effects
