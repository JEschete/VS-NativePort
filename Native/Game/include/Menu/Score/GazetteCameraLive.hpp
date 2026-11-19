#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.h"

// Binds the gazette camera and panels (GazetteCamera.hpp) to the frame's
// primitive buffer and scene ordering table (VS::Render::FrameScratchpad(),
// vs_main_frameBuf), the frame camera, the score screen's camera settings and
// backups, the score (Menu_Score_score), the GTE projection's screen offset,
// the compact font and the actor preview, in the shapes the copied score
// screen, records and encyclopedia call. A palette index outside 0-3, a
// banner of more than kMaxEnemyBannerLines lines, a null matrix, an ordering
// entry outside the frame's table and a call that does not fit the primitive
// buffer are refused, before anything is written, and reported on the
// "menu.score" channel.
namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
// MENU9.PRG (238.c).
void RestoreLiveCamera();
void BuildLiveViewerCamera(MATRIX* camera_matrix);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void DrawLiveEnemyBanner(int x, int y, int width, int height, int gradient);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void DrawLiveGradientPanel(int x, int y, int width, int height, int gradient);
void DrawLiveFooterBackground(int top);
void RenderLiveOverview(int animation_step);
void DrawLiveViewerEdgeFade();

// Inputs the binding refused since start-up.
[[nodiscard]] std::uint64_t LiveGazetteCameraRefusals() noexcept;
}  // namespace VS::Menu::Score
