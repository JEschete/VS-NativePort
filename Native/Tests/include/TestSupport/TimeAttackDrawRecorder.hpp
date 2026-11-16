#pragma once
// Recording host, live frame fixture and replay shared by the MENUF.PRG
// time-attack drawing tests (TimeAttackHeadingTests.cpp,
// TimeAttackRatingTests.cpp).

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Legacy/Psx.h"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Menu/Results/TimeAttackHeading.hpp"
#include "Menu/Results/TimeAttackRating.hpp"
#include "Render/DisplayControl.hpp"
#include "Render/TextureRegionDrawLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Images.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Progress.h"
#include "State/StateFlags.h"
}

namespace VS::test_support::time_attack {
namespace results = VS::Menu::Results;

enum class Kind : std::uint8_t { kFlash, kFade, kFadeClut, kTime, kWipe };

struct Call {
  Kind kind = Kind::kFlash;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::size_t index = 0;
  std::int32_t brightness = 0;
  std::int32_t clut = 0;
  std::int32_t time = 0;
  bool record = false;
  std::int32_t edge = 0;
  results::TimeAttackColors colors{};

  [[nodiscard]] bool At(std::int32_t at_x, std::int32_t at_y,
                        std::size_t at_index) const {
    return x == at_x && y == at_y && index == at_index;
  }
};

// Records every time-attack draw in order.
class RecordHost final : public results::TimeAttackResultsDrawHost {
 public:
  std::vector<Call> calls;

  void DrawFlashFade(results::ResultsPoint origin, std::size_t texture_index,
                     const results::TimeAttackColors& colors) override {
    calls.push_back({.kind = Kind::kFlash,
                     .x = origin.x,
                     .y = origin.y,
                     .index = texture_index,
                     .colors = colors});
  }
  void DrawFade(results::ResultsPoint origin, std::size_t texture_index,
                std::int32_t brightness) override {
    calls.push_back({.kind = Kind::kFade,
                     .x = origin.x,
                     .y = origin.y,
                     .index = texture_index,
                     .brightness = brightness});
  }
  void DrawFadeClut(results::ResultsPoint origin, std::size_t texture_index,
                    std::int32_t brightness, std::int32_t clut) override {
    calls.push_back({.kind = Kind::kFadeClut,
                     .x = origin.x,
                     .y = origin.y,
                     .index = texture_index,
                     .brightness = brightness,
                     .clut = clut});
  }
  void DrawTime(results::ResultsPoint origin, std::int32_t timer,
                std::int32_t time) override {
    DrawRankedTime(origin, timer, time, false);
  }
  void DrawWipe(results::ResultsPoint origin, std::size_t texture_index,
                const results::TimeAttackColors& colors,
                std::int32_t wipe_edge_x) override {
    calls.push_back({.kind = Kind::kWipe,
                     .x = origin.x,
                     .y = origin.y,
                     .index = texture_index,
                     .edge = wipe_edge_x,
                     .colors = colors});
  }
  void DrawRankedTime(results::ResultsPoint origin, std::int32_t timer,
                      std::int32_t time, bool is_new_record) override {
    calls.push_back({.kind = Kind::kTime,
                     .x = origin.x,
                     .y = origin.y,
                     .brightness = timer,
                     .time = time,
                     .record = is_new_record});
  }
};

inline constexpr std::size_t kLivePrimitiveWords = 0x20000 / sizeof(u_long);
inline constexpr std::size_t kLiveOrderingWords = 0x2088 / sizeof(u_long);
inline constexpr std::size_t kLiveSceneHead = 16;
inline constexpr std::size_t kLiveOverlayHead = 2;
inline constexpr u_long kLiveEnd = 0xFFFFFF;

// A frame whose primitives land in `storage`, and the time-attack state the
// Live entries read, restored afterwards.
struct LiveTimeAttackFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  void* saved_ordering = Render_legacyOrderingTables[0];
  int saved_framebuffer = Render_frameBuf;
  u_char saved_course = Audio_Seq_unusedStateFlags.timeAttackBoss;
  State_ScoreData saved_scores = Audio_Seq_unusedScoredata;
  int saved_time = Menu_Results_timeTrialTime;
  int saved_new_slot = Menu_Results_newTimeSlot;
  std::vector<u_long> storage = std::vector<u_long>(kLivePrimitiveWords, 0);
  std::vector<u_long> ordering =
      std::vector<u_long>(kLiveOrderingWords, kLiveEnd);

  LiveTimeAttackFixture() { Reset(); }
  ~LiveTimeAttackFixture() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_legacyOrderingTables[0] = saved_ordering;
    Render_frameBuf = saved_framebuffer;
    Audio_Seq_unusedStateFlags.timeAttackBoss = saved_course;
    Audio_Seq_unusedScoredata = saved_scores;
    Menu_Results_timeTrialTime = saved_time;
    Menu_Results_newTimeSlot = saved_new_slot;
  }
  LiveTimeAttackFixture(const LiveTimeAttackFixture&) = delete;
  LiveTimeAttackFixture& operator=(const LiveTimeAttackFixture&) = delete;
  LiveTimeAttackFixture(LiveTimeAttackFixture&&) = delete;
  LiveTimeAttackFixture& operator=(LiveTimeAttackFixture&&) = delete;

  void Reset() {
    std::ranges::fill(storage, 0);
    std::ranges::fill(ordering, kLiveEnd);
    Render_frameBuf = 0;
    Render_legacyOrderingTables[0] = ordering.data();
    Render_legacyPrimitiveBuffers[0] = storage.data();
    Render_frameScratch = {};
    Render_frameScratch.primitiveCursor = storage.data();
    Render_frameScratch.orderingTable = ordering.data() + kLiveSceneHead;
    Render_frameScratch.overlayOrderingTable =
        ordering.data() + kLiveOverlayHead;
  }
  // The primitives written so far followed by the whole ordering table, so a
  // replay must also link them into the same entries.
  [[nodiscard]] std::vector<u_long> Frame() const {
    const auto* const cursor =
        static_cast<const u_long*>(Render_frameScratch.primitiveCursor);
    std::vector<u_long> words(storage.data(), cursor);
    words.insert(words.end(), ordering.begin(), ordering.end());
    return words;
  }
  [[nodiscard]] bool Empty() const {
    return Render_frameScratch.primitiveCursor == storage.data();
  }
};

inline std::array<CVECTOR, 2> Cvectors(
    const results::TimeAttackColors& colors) {
  return {{{colors[0].r, colors[0].g, colors[0].b, colors[0].step},
           {colors[1].r, colors[1].g, colors[1].b, colors[1].step}}};
}

// Draws a host's recorded calls with the shared live atlas draws.
inline void Replay(const RecordHost& host) {
  for (const auto& call : host.calls) {
    const auto index = static_cast<int>(call.index);
    switch (call.kind) {
      case Kind::kFlash:
        VS::Render::DrawLiveTextureFlashFade(
            Menu_Results_disMap, call.x, call.y, index, Cvectors(call.colors));
        break;
      case Kind::kFade:
        results::LiveDrawFadingTexture(call.x, call.y, index, call.brightness);
        break;
      case Kind::kFadeClut:
        results::LiveDrawFadingTextureClut(call.x, call.y, index,
                                           call.brightness, call.clut);
        break;
      case Kind::kTime:
        results::LiveDrawTime(call.x, call.y, call.brightness, call.time,
                              call.record ? 1 : 0);
        break;
      case Kind::kWipe:
        VS::Render::DrawLiveTextureWipe(Menu_Results_disMap, call.x, call.y,
                                        index, Cvectors(call.colors),
                                        call.edge);
        break;
    }
  }
}
}  // namespace VS::test_support::time_attack
