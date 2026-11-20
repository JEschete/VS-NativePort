#pragma once
// Hand-maintained (tools/port/owned.csv).
// The room background globals as the copied room loader, weather effects,
// room state and snapshot read and write them. BackgroundLive.cpp defines
// them and mirrors them into the typed RoomBackgroundState
// (Field/Background.hpp) around each call.

#include <stdint.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "Content/DiscQueue.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Field {
inline constexpr std::size_t kBackgroundTileCorners = 4;

// decomp: vs_battle_backgroundTile (BATTLE/BATTLE.PRG 2842C.h:9): one tile of
// the tiled background's layout file, its texture rectangle and its corner
// colours.
struct BackgroundTileRecord {
  VS::Legacy::RECT texture_rect;
  std::array<VS::Core::Gte::CVECTOR, kBackgroundTileCorners> corner_colors;
};

// decomp: vs_battle_backgroundLayout (BATTLE/BATTLE.PRG 2842C.h:16): the
// layout file's header; width * height BackgroundTileRecords follow it.
struct BackgroundLayoutRecord {
  std::int32_t width;
  std::int32_t height;
  std::int32_t reserved08;
};

// decomp: D_800F1CE0 (BATTLE/BATTLE.PRG 146C.h:1113): zero vectors; [0] is
// the sky dome's turn outside a room.
extern VS::Core::Gte::VECTOR backgroundReferenceVectors[2];
// decomp: D_800F1D78 (BATTLE/BATTLE.PRG 146C.c:704): the five gradient
// colours last spread over the sky dome.
inline constexpr int kBackgroundGradientColorCount = 5;
extern int backgroundGradientColors[kBackgroundGradientColorCount];
// decomp: D_800F1DAC (BATTLE/BATTLE.PRG 146C.c:711): the tiled background's
// TIM read, or null.
extern VS::Content::DiscQueueSlot* backgroundTextureDiscRequest;
// decomp: D_800F1DB0 (BATTLE/BATTLE.PRG 146C.c:712): its layout read, or
// null.
extern VS::Content::DiscQueueSlot* backgroundLayoutDiscRequest;
// decomp: D_800F1DB4 and D_800F1DB6 (BATTLE/BATTLE.PRG 146C.c:713): the
// tiled background's scroll offset.
extern int16_t backgroundOffsetX;
extern int16_t backgroundOffsetY;
// decomp: D_800F1DB8 and D_800F1DBA (BATTLE/BATTLE.PRG 146C.c:715): the Q12
// parallax of the camera's yaw and pitch.
extern int16_t backgroundParallaxScaleXFixed12;
extern int16_t backgroundParallaxScaleYFixed12;
// decomp: D_800F1DBE (BATTLE/BATTLE.PRG 146C.c:718): how many texture pages
// the tiled background holds.
extern int16_t legacyBackgroundTextureReservationId;
// decomp: D_800F1DC6 (BATTLE/BATTLE.PRG 146C.c:722): 1 when the room's five
// colours match and a solid backdrop replaces the dome.
extern char isSolidBackground;
// decomp: D_800F1DC8 (BATTLE/BATTLE.PRG 146C.c:723): the tiled background's
// first texture page.
extern char legacyBackgroundTexturePage;
// decomp: D_800F1DC9 (BATTLE/BATTLE.PRG 146C.c:724): 1 while the tiled
// background is drawn.
extern std::uint8_t isTiledBackgroundEnabled;
// decomp: D_800F1DCC (BATTLE/BATTLE.PRG 146C.c:727): the tiled background's
// layout (high heap), or null.
extern BackgroundLayoutRecord* loadedBackgroundLayout;
// decomp: D_800F1DD0 (BATTLE/BATTLE.PRG 146C.c:728): its TIM until the
// upload (high heap), or null.
extern std::uint32_t* legacyBackgroundTimBuffer;
}  // namespace VS::Field
