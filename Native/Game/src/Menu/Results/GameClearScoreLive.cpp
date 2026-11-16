// Hand-maintained (tools/port/owned.csv).
// Binds the typed game-clear draws (GameClearScore.cpp) to
// Menu_Results_disMap, the results statics, the live atlas draws and the
// sound effects.
#include "Menu/Results/GameClearScoreLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Audio/SfxLive.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"
#include "Menu/Results/GameClearScore.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Menu/Score/ScoreRank.hpp"
#include "Render/TextureRegionDrawLive.hpp"

extern "C" {
#include "Menu/Results/Results.h"
#include "Render/Images.h"
}

namespace VS::Menu::Results {
namespace {
std::uint64_t refusals = 0;

void Report(ResultsDrawResult result) {
  if (result == ResultsDrawResult::kInvalidTexture &&
      std::has_single_bit(++refusals)) {
    VS_LOG_WARN("menu.results",
                "Game-clear drawing refused a glyph outside the atlas or rank "
                "table ({} refusals)",
                refusals);
  }
}

std::array<CVECTOR, 2> ToCvectors(const GameClearColors& colors) {
  std::array<CVECTOR, 2> out{};
  std::ranges::transform(
      colors, out.begin(), [](const Render::RegionColor& color) {
        return CVECTOR{
            .r = color.r, .g = color.g, .b = color.b, .cd = color.step};
      });
  return out;
}

class LiveGameClearHost final : public GameClearDrawHost {
 public:
  void DrawTintFade(ResultsPoint origin, std::size_t texture_index,
                    const GameClearColors& colors) override {
    const auto vectors = ToCvectors(colors);
    Render::DrawLiveTextureTintFade(Menu_Results_disMap, origin.x, origin.y,
                                    static_cast<int>(texture_index), vectors);
  }
  void DrawFlashFade(ResultsPoint origin, std::size_t texture_index,
                     const GameClearColors& colors) override {
    const auto vectors = ToCvectors(colors);
    Render::DrawLiveTextureFlashFade(Menu_Results_disMap, origin.x, origin.y,
                                     static_cast<int>(texture_index), vectors);
  }
  void DrawWipe(ResultsPoint origin, std::size_t texture_index,
                const GameClearColors& colors,
                std::int32_t wipe_edge_x) override {
    const auto vectors = ToCvectors(colors);
    Render::DrawLiveTextureWipe(Menu_Results_disMap, origin.x, origin.y,
                                static_cast<int>(texture_index), vectors,
                                wipe_edge_x);
  }
  void PlaySfx(std::int32_t group, std::int32_t effect) override {
    Audio::LivePlaySfxCentered(group, effect);
  }
  void StopSfx(std::int32_t group, std::int32_t effect) override {
    Audio::LiveStopSfx(group, effect);
  }
};

// The rank table is declared without its bound; it holds one row per rank.
std::array<RiskbreakerRankRow, Score::kRiskbreakerRankCount> LiveRankTable() {
  std::array<RiskbreakerRankRow, Score::kRiskbreakerRankCount> rows{};
  const std::span<const u_char[kRiskbreakerRowBytes]> table(
      Menu_Results_riskbreakerRanks, rows.size());
  std::ranges::transform(table, rows.begin(),
                         [](const u_char(&row)[kRiskbreakerRowBytes]) {
                           RiskbreakerRankRow copy{};
                           std::ranges::copy(row, copy.begin());
                           return copy;
                         });
  return rows;
}
}  // namespace

// decomp: MENUF.PRG _renderCongratulations (3B8.c:1332)
void LiveDrawCongratulations(int x, int y, int timer) {
  LiveGameClearHost host;
  Report(DrawCongratulations(LiveResultsAtlas(), x, y, timer, host));
}

// decomp: MENUF.PRG _renderScore (3B8.c:1357)
void LiveDrawScore(int x, int y, int timer, int /*unused*/) {
  LiveGameClearHost host;
  Report(DrawScore(LiveResultsAtlas(), x, y, timer, Menu_Results_score, host));
}

// decomp: MENUF.PRG _renderIncrementalScore (3B8.c:1409)
void LiveDrawCountingScore(int x, int y, int timer, int /*unused*/) {
  LiveGameClearHost host;
  CountingReadout readout{.shown = Menu_Results_onScreenScore,
                          .counting = Menu_Results_incrementingScore};
  Report(DrawCountingScore(LiveResultsAtlas(), x, y, timer, Menu_Results_score,
                           readout, host));
  Menu_Results_onScreenScore = readout.shown;
  Menu_Results_incrementingScore = readout.counting;
}

// decomp: MENUF.PRG _renderMapCompletion (3B8.c:1486)
void LiveDrawMapCompletion(int x, int y, int timer, int /*unused*/) {
  LiveGameClearHost host;
  Report(DrawMapCompletion(LiveResultsAtlas(), x, y, timer,
                           Menu_Results_mapCompletion, host));
}

// decomp: MENUF.PRG _renderIncrementalMapCompletion (3B8.c:1527)
void LiveDrawCountingMapCompletion(int x, int y, int timer, int /*unused*/) {
  LiveGameClearHost host;
  CountingReadout readout{.shown = Menu_Results_onScreenMapCompletion,
                          .counting = Menu_Results_incrementingMapCompletion};
  Report(DrawCountingMapCompletion(LiveResultsAtlas(), x, y, timer,
                                   Menu_Results_mapCompletion, readout, host));
  Menu_Results_onScreenMapCompletion = readout.shown;
  Menu_Results_incrementingMapCompletion = readout.counting;
}

// decomp: MENUF.PRG _renderRiskbreakerRankHeader (3B8.c:1598)
void LiveDrawRiskbreakerRankHeading(int x, int y, int timer) {
  LiveGameClearHost host;
  Report(DrawRiskbreakerRankHeading(LiveResultsAtlas(), x, y, timer, host));
}

// decomp: MENUF.PRG _renderRiskbreakerRank (3B8.c:1618)
void LiveDrawRiskbreakerRank(int x, int y, int timer) {
  LiveGameClearHost host;
  const auto ranks = LiveRankTable();
  Report(DrawRiskbreakerRank(LiveResultsAtlas(), x, y, timer, Menu_Results_rank,
                             ranks, host));
}
}  // namespace VS::Menu::Results
