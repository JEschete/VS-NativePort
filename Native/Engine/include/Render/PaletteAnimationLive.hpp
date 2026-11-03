#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Legacy/Psx.h"
#include "Render/PaletteAnimation.hpp"

// Live binding for the scene's animated colours (PaletteAnimation.cpp): one
// PaletteAnimation for the game, its VRAM uploads through LoadImage and its
// background colours and tints through the room and actor consumers. The
// generated callers keep the original call shapes below; a null colour
// source, an actor below zero, a mode past a byte, a duration past a
// halfword, or a row or span the original would have run past its arrays, is
// refused, counted and reported instead of reaching the core.
namespace VS::Render {
// vs_main_loadClut (main.c:11470).
void LoadLiveClutColors(const u_short* colors, u_int row, u_int first_color,
                        u_int count);
// vs_main_commitClut (main.c:11621).
void CommitLiveClutRows();
// vs_main_clutInit (main.c:11634).
void InitializeLiveClutAnimation();
// func_80047FFC (main.c:11229).
void UpdateLiveBackgroundGradientTransition();
// func_800481C0 (main.c:11287).
void UpdateLiveModelTintTransitions();
// func_800483FC (main.c:11345).
void UpdateLiveClutTransitions();
// func_80048EC4 (main.c:11604): reads the room's background colours.
void CaptureLiveRoomBackgroundColors();
// vs_main_setClutState (main.c:11632).
void SetLiveClutAnimationEnabled(int enabled);
// func_8007B10C (146C.c:8241): only the red offset's low 16 bits matter, as
// in the original.
void TransformLiveRoomPalettes(int mode, int duration, int red_offset,
                               std::int16_t green_offset,
                               std::int16_t blue_offset);
// func_8007B1B8 (146C.c:8248).
void TransformLiveSceneColors(int mode, int duration, std::int16_t red_offset,
                              std::int16_t green_offset,
                              std::int16_t blue_offset);
// func_8007B29C (146C.c:8257).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void TransformLiveActorColors(int mode, int duration, int actor,
                              std::int16_t red_offset,
                              std::int16_t green_offset,
                              std::int16_t blue_offset);
// func_8007B344 (146C.c:8265).
void TransformLiveBackgroundColors(int mode, int duration,
                                   std::int16_t red_offset,
                                   std::int16_t green_offset,
                                   std::int16_t blue_offset);

// LoadClutColors on the game's animator; false when refused.
bool LoadLiveClutRow(std::uint32_t row, std::uint32_t first_color,
                     std::span<const std::uint16_t> colors);
[[nodiscard]] const PaletteAnimation& LivePaletteAnimation() noexcept;
// Refused requests and delta-table steps the animator held, since start-up.
[[nodiscard]] std::uint64_t RefusedLivePaletteRequestCount() noexcept;
}  // namespace VS::Render
