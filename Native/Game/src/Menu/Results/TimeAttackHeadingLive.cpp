// Hand-maintained (tools/port/owned.csv).
// Binds the typed time-attack heading, countdown and palette pulse
// (TimeAttackHeading.cpp) to Menu_Results_disMap, the time-attack course and
// best times, Menu_Results_clutBuf, the live atlas draws and LoadImage.
#include "Menu/Results/TimeAttackHeadingLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Log.hpp"
#include "Legacy/Psx.h"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Menu/Results/TimeAttackHeading.hpp"
#include "Render/DisplayControl.hpp"
#include "Render/TextureRegionDrawLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/Results.h"
#include "Render/Images.h"
#include "State/Progress.h"
#include "State/StateFlags.h"
}

namespace VS::Menu::Results {
namespace {
constexpr std::uint32_t kTrialRoundShift = 24;
constexpr std::uint32_t kTrialDifficultyShift = 31;
static_assert(sizeof(u_short) == sizeof(std::uint16_t));
static_assert(std::size(Menu_Results_clutBuf) == kResultsClutWords);

std::uint64_t refusals = 0;

void Report(ResultsDrawResult result) {
  if (result == ResultsDrawResult::kInvalidTexture &&
      std::has_single_bit(++refusals)) {
    VS_LOG_WARN("menu.results",
                "Time-attack drawing refused a course or glyph outside the "
                "atlas or best-time table ({} refusals)",
                refusals);
  }
}

class LiveTimeAttackHost final : public TimeAttackDrawHost {
 public:
  void DrawFlashFade(ResultsPoint origin, std::size_t texture_index,
                     const TimeAttackColors& colors) override {
    std::array<CVECTOR, 2> vectors{};
    std::ranges::transform(
        colors, vectors.begin(), [](const Render::RegionColor& color) {
          return CVECTOR{
              .r = color.r, .g = color.g, .b = color.b, .cd = color.step};
        });
    Render::DrawLiveTextureFlashFade(Menu_Results_disMap, origin.x, origin.y,
                                     static_cast<int>(texture_index), vectors);
  }
  void DrawFade(ResultsPoint origin, std::size_t texture_index,
                std::int32_t brightness) override {
    LiveDrawFadingTexture(origin.x, origin.y, static_cast<int>(texture_index),
                          brightness);
  }
  void DrawFadeClut(ResultsPoint origin, std::size_t texture_index,
                    std::int32_t brightness, std::int32_t clut) override {
    LiveDrawFadingTextureClut(
        origin.x, origin.y, static_cast<int>(texture_index), brightness, clut);
  }
  void DrawTime(ResultsPoint origin, std::int32_t timer,
                std::int32_t time) override {
    LiveDrawTime(origin.x, origin.y, timer, time, 0);
  }
};

class LiveClutTarget final : public ResultsClutTarget {
 public:
  void Upload(const Render::ScreenRect& area,
              std::span<const std::uint16_t> colors) override {
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    // LoadImage only reads the colors; the core passes the whole buffer.
    LoadImage(&rect, reinterpret_cast<u_long*>(
                         const_cast<std::uint16_t*>(colors.data())));
  }
};

std::uint8_t LiveCourse() { return Audio_Seq_unusedStateFlags.timeAttackBoss; }

// Each course's first-place trial score, packed as the original word.
std::array<std::uint32_t, kTimeAttackCourses> LiveFirstPlaceScores() {
  std::array<std::uint32_t, kTimeAttackCourses> scores{};
  for (std::size_t course = 0; course < scores.size(); ++course) {
    const State_TrialScore& score =
        Audio_Seq_unusedScoredata.bossTimeTrialScores[course][0];
    scores.at(course) =
        score.time |
        (static_cast<std::uint32_t>(score.round) << kTrialRoundShift) |
        (static_cast<std::uint32_t>(score.difficulty) << kTrialDifficultyShift);
  }
  return scores;
}
}  // namespace

// decomp: MENUF.PRG _renderTimeAttackHeader (src/MENU/MENUF.PRG/3B8.c:1146),
// 0x8010459C
void LiveDrawTimeAttackHeading(int x, int y, int timer) {
  LiveTimeAttackHost host;
  Report(DrawTimeAttackHeading(LiveResultsAtlas(), x, y, timer, host));
}

// decomp: MENUF.PRG _renderCourse (src/MENU/MENUF.PRG/3B8.c:1175), 0x80104650
void LiveDrawCourseNumber(int x, int y, int timer) {
  LiveTimeAttackHost host;
  Report(DrawCourseNumber(LiveResultsAtlas(), x, y, timer, LiveCourse(), host));
}

// decomp: MENUF.PRG _renderBossName (src/MENU/MENUF.PRG/3B8.c:1195), 0x801046F8
void LiveDrawBossName(int x, int y, int timer) {
  LiveTimeAttackHost host;
  Report(DrawBossName(LiveResultsAtlas(), x, y, timer, LiveCourse(), host));
}

// decomp: MENUF.PRG _renderBestTimeHeader (src/MENU/MENUF.PRG/3B8.c:1226),
// 0x801047D4
void LiveDrawBestTime(int x, int y, int timer) {
  LiveTimeAttackHost host;
  const auto scores = LiveFirstPlaceScores();
  Report(DrawBestTime(LiveResultsAtlas(), x, y, timer, LiveCourse(), scores,
                      host));
}

// decomp: MENUF.PRG _renderStartCountdown (src/MENU/MENUF.PRG/3B8.c:1248),
// 0x8010489C
void LiveDrawStartCountdown(int x, int y, int timer) {
  LiveTimeAttackHost host;
  Report(DrawStartCountdown(LiveResultsAtlas(), x, y, timer, host));
}

// decomp: MENUF.PRG _pulseTimeAttackEnd (src/MENU/MENUF.PRG/3B8.c:1256),
// 0x80104914
void LivePulseTimeAttackClut(int blend_weight) {
  LiveClutTarget target;
  PulseTimeAttackClut(std::span(Menu_Results_clutBuf), blend_weight, target);
}
}  // namespace VS::Menu::Results
