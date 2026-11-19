#pragma once
// EFFPURGE.BIN's purge tables (src/EFFECT/EFFPURGE.BIN/0.c): for each effect,
// the CLUT (FBC), texture page (FBT), package and plugin files it loads and
// its renderer entry points. PurgeTables.cpp defines them;
// LiveEffectPurgeTables (EffectResourcesLive.hpp) views them as
// EffectPurgeTables.

#include <cstddef>
#include <cstdint>

#include "Effects/EffectResources.hpp"
#include "Effects/Runtime.h"

namespace VS::Effects {
// fbcClutRows, fbcFiles, fbcSlots, fbtCounts, pFiles and
// effectEntryPointIndices: one entry per effect.
inline constexpr std::size_t kEffectCount = 400;
// fbtFiles, and fbtSlots with its one trailing entry.
inline constexpr std::size_t kTextureFileCount = 755;
inline constexpr std::size_t kTextureSlotCount = 756;
// plgFiles: effects past it have no plugin.
inline constexpr std::size_t kPluginFileCount = 302;
// effectEntryPoints: 343 entries, 314 of them set.
inline constexpr std::size_t kEntryPointCount = 343;

// decomp: fbcClutRows (src/EFFECT/EFFPURGE.BIN/0.c:125): the CLUT rows of
// each effect's FBC; nonzero counts the FBC as a texture asset.
extern std::uint8_t fbcClutRows[kEffectCount];
// decomp: effectEntryPointIndices (src/EFFECT/EFFPURGE.BIN/0.c:424): each
// effect's first entry point; the next effect's ends it.
extern std::uint16_t effectEntryPointIndices[kEffectCount];
// decomp: effectEntryPoints (src/EFFECT/EFFPURGE.BIN/0.c:459).
extern EffectRenderer effectEntryPoints[kEntryPointCount];
// decomp: fbcFiles (src/EFFECT/EFFPURGE.BIN/0.c:160).
extern EffectFileExtent fbcFiles[kEffectCount];
// decomp: fbcSlots (src/EFFECT/EFFPURGE.BIN/0.c:141).
extern std::uint8_t fbcSlots[kEffectCount];
// decomp: fbtCounts (src/EFFECT/EFFPURGE.BIN/0.c:219).
extern std::uint8_t fbtCounts[kEffectCount];
// decomp: fbtFiles (src/EFFECT/EFFPURGE.BIN/0.c:265): every effect's FBTs
// in effect order.
extern EffectFileExtent fbtFiles[kTextureFileCount];
// decomp: fbtSlots (src/EFFECT/EFFPURGE.BIN/0.c:235).
extern std::uint8_t fbtSlots[kTextureSlotCount];
// decomp: pFiles (src/EFFECT/EFFPURGE.BIN/0.c:378).
extern EffectFileExtent packageFiles[kEffectCount];
// decomp: plgFiles (src/EFFECT/EFFPURGE.BIN/0.c:564).
extern EffectFileExtent pluginFiles[kPluginFileCount];
}  // namespace VS::Effects
