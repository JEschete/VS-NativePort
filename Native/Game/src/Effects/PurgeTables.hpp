#pragma once
// EFFPURGE.BIN's purge tables (src/EFFECT/EFFPURGE.BIN/0.c): for each effect,
// the CLUT (FBC), texture page (FBT), package and plugin files it loads and
// its renderer entry points. PurgeTables.cpp defines them;
// LiveEffectPurgeTables (EffectResourcesLive.hpp) views them as
// EffectPurgeTables.

#include <cstddef>
#include <cstdint>

#include "Effects/EffectResources.hpp"
#include "Effects/Runtime.hpp"

namespace VS::Effects {
// kFbcClutRows, kFbcFiles, kFbcSlots, kFbtCounts, kPackageFiles and
// kEffectEntryPointIndices: one entry per effect.
inline constexpr std::size_t kEffectCount = 400;
// kFbtFiles, and kFbtSlots with its one trailing entry.
inline constexpr std::size_t kTextureFileCount = 755;
inline constexpr std::size_t kTextureSlotCount = 756;
// kPluginFiles: effects past it have no plugin.
inline constexpr std::size_t kPluginFileCount = 302;
// kEffectEntryPoints: 343 entries, 314 of them set.
inline constexpr std::size_t kEntryPointCount = 343;

// decomp: fbcClutRows (src/EFFECT/EFFPURGE.BIN/0.c:125): the CLUT rows of
// each effect's FBC; nonzero counts the FBC as a texture asset.
extern const std::uint8_t kFbcClutRows[kEffectCount];
// decomp: effectEntryPointIndices (src/EFFECT/EFFPURGE.BIN/0.c:424): each
// effect's first entry point; the next effect's ends it.
extern const std::uint16_t kEffectEntryPointIndices[kEffectCount];
// decomp: effectEntryPoints (src/EFFECT/EFFPURGE.BIN/0.c:459).
extern const EffectRenderer kEffectEntryPoints[kEntryPointCount];
// decomp: fbcFiles (src/EFFECT/EFFPURGE.BIN/0.c:160).
extern const EffectFileExtent kFbcFiles[kEffectCount];
// decomp: fbcSlots (src/EFFECT/EFFPURGE.BIN/0.c:141).
extern const std::uint8_t kFbcSlots[kEffectCount];
// decomp: fbtCounts (src/EFFECT/EFFPURGE.BIN/0.c:219).
extern const std::uint8_t kFbtCounts[kEffectCount];
// decomp: fbtFiles (src/EFFECT/EFFPURGE.BIN/0.c:265): every effect's FBTs
// in effect order.
extern const EffectFileExtent kFbtFiles[kTextureFileCount];
// decomp: fbtSlots (src/EFFECT/EFFPURGE.BIN/0.c:235).
extern const std::uint8_t kFbtSlots[kTextureSlotCount];
// decomp: pFiles (src/EFFECT/EFFPURGE.BIN/0.c:378).
extern const EffectFileExtent kPackageFiles[kEffectCount];
// decomp: plgFiles (src/EFFECT/EFFPURGE.BIN/0.c:564).
extern const EffectFileExtent kPluginFiles[kPluginFileCount];
}  // namespace VS::Effects
