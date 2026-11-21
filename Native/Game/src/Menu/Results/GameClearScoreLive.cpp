// Hand-maintained (tools/port/owned.csv).
// Binds the typed game-clear draws (GameClearScore.cpp) to the results atlas
// (kDisMap), the results statics, the live atlas draws and the sound effects.
#include "Menu/Results/GameClearScoreLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

#include "Audio/SfxLive.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Results/GameClearScore.hpp"
#include "Menu/Results/Results.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Render/Images.hpp"
#include "Render/TextureRegionDrawLive.hpp"

using VS::Core::Gte::CVECTOR;

namespace VS::Menu::Results {
namespace {
std::uint64_t refusals = 0;

void Report(ResultsDrawResult result) {
  if (result == ResultsDrawResult::kInvalidTexture &&
      std::has_single_bit(++refusals)) {
    VS::Core::LogWarn(
        "menu.results",
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
    Render::DrawLiveTextureTintFade(kDisMap, origin.x, origin.y,
                                    static_cast<int>(texture_index), vectors);
  }
  void DrawFlashFade(ResultsPoint origin, std::size_t texture_index,
                     const GameClearColors& colors) override {
    const auto vectors = ToCvectors(colors);
    Render::DrawLiveTextureFlashFade(kDisMap, origin.x, origin.y,
                                     static_cast<int>(texture_index), vectors);
  }
  void DrawWipe(ResultsPoint origin, std::size_t texture_index,
                const GameClearColors& colors,
                std::int32_t wipe_edge_x) override {
    const auto vectors = ToCvectors(colors);
    Render::DrawLiveTextureWipe(kDisMap, origin.x, origin.y,
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

}  // namespace

// decomp: MENUF.PRG renderCongratulations (src/MENU/MENUF.PRG/3B8.c:1345; was
// _renderCongratulations)
void LiveDrawCongratulations(int x, int y, int timer) {
  LiveGameClearHost host;
  Report(DrawCongratulations(LiveResultsAtlas(), x, y, timer, host));
}

// decomp: MENUF.PRG renderScore (src/MENU/MENUF.PRG/3B8.c:1370; was
// _renderScore)
void LiveDrawScore(int x, int y, int timer, int /*unused*/) {
  LiveGameClearHost host;
  Report(DrawScore(LiveResultsAtlas(), x, y, timer, score, host));
}

// decomp: MENUF.PRG renderIncrementalScore (src/MENU/MENUF.PRG/3B8.c:1422; was
// _renderIncrementalScore)
void LiveDrawCountingScore(int x, int y, int timer, int /*unused*/) {
  LiveGameClearHost host;
  CountingReadout readout{.shown = onScreenScore,
                          .counting = incrementingScore};
  Report(
      DrawCountingScore(LiveResultsAtlas(), x, y, timer, score, readout, host));
  onScreenScore = readout.shown;
  incrementingScore = readout.counting;
}

// decomp: MENUF.PRG renderMapCompletion (src/MENU/MENUF.PRG/3B8.c:1499; was
// _renderMapCompletion)
void LiveDrawMapCompletion(int x, int y, int timer, int /*unused*/) {
  LiveGameClearHost host;
  Report(
      DrawMapCompletion(LiveResultsAtlas(), x, y, timer, mapCompletion, host));
}

// decomp: MENUF.PRG renderIncrementalMapCompletion
// (src/MENU/MENUF.PRG/3B8.c:1540; was _renderIncrementalMapCompletion)
void LiveDrawCountingMapCompletion(int x, int y, int timer, int /*unused*/) {
  LiveGameClearHost host;
  CountingReadout readout{.shown = onScreenMapCompletion,
                          .counting = incrementingMapCompletion};
  Report(DrawCountingMapCompletion(LiveResultsAtlas(), x, y, timer,
                                   mapCompletion, readout, host));
  onScreenMapCompletion = readout.shown;
  incrementingMapCompletion = readout.counting;
}

// decomp: MENUF.PRG renderRiskbreakerRankHeader (src/MENU/MENUF.PRG/3B8.c:1610;
// was _renderRiskbreakerRankHeader)
void LiveDrawRiskbreakerRankHeading(int x, int y, int timer) {
  LiveGameClearHost host;
  Report(DrawRiskbreakerRankHeading(LiveResultsAtlas(), x, y, timer, host));
}

// decomp: MENUF.PRG renderRiskbreakerRank (src/MENU/MENUF.PRG/3B8.c:1630; was
// _renderRiskbreakerRank)
void LiveDrawRiskbreakerRank(int x, int y, int timer) {
  LiveGameClearHost host;
  Report(DrawRiskbreakerRank(LiveResultsAtlas(), x, y, timer, rank,
                             kRiskbreakerRanks, host));
}
}  // namespace VS::Menu::Results
