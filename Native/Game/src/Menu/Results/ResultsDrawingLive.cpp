// Hand-maintained (tools/port/owned.csv).
// Binds the typed results draws (ResultsDrawing.cpp) to Menu_Results_disMap
// and the current frame's primitive buffer and ordering tables.
#include "Menu/Results/ResultsDrawingLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Legacy/Psx.h"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/TextureRegionDraw.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiTexturedQuads.hpp"

extern "C" {
#include "Menu/Results/Results.h"
#include "Render/Images.h"
}

namespace VS::Menu::Results {
namespace {
// The quads link seven entries before the frame's ordering table (`p[1] -
// 0x1C`).
constexpr int kQuadOrderingSlot = -7;
static_assert(std::size(Menu_Results_disMap) == kResultsAtlasEntries);
constexpr std::size_t kTextBytes = 8;
static_assert(sizeof(u_long) == sizeof(std::uint32_t));

std::uint64_t refusals = 0;

void Refuse(const char* what) {
  if (std::has_single_bit(++refusals)) {
    VS_LOG_WARN("menu.results", "Results drawing refused: {} ({} refusals)",
                what, refusals);
  }
}

class LiveResultsTarget final : public ResultsDrawTarget {
 public:
  bool DrawQuad(const Render::UiTexturedQuad& quad) override {
    u_long* const entry = Render::LiveFrameOrderingEntry(
        Render::LiveOrderingBase::kScene, kQuadOrderingSlot);
    if (entry == nullptr) {
      Refuse("no ordering entry for the quad");
      return false;
    }
    u_long* const storage =
        Render::ReserveLivePrimitiveWords(Render::kUiTexturedQuadWords);
    if (storage == nullptr) {
      Refuse("primitive buffer full");
      return false;
    }
    Render::UiTexturedQuadPacket previous{};
    std::ranges::copy(std::span(storage, Render::kUiTexturedQuadWords),
                      previous.begin());
    Render::LivePacketQueue queue(entry);
    const auto packet =
        Render::BuildUiTexturedQuadPacket(quad, previous, queue.ChainLink());
    if (!queue.Emit(packet)) {
      Refuse("quad not queued");
      return false;
    }
    return true;
  }

  void DrawText(std::string_view text, std::int32_t xy,
                std::uint32_t color) override {
    std::array<char, kTextBytes> terminated{};
    if (text.size() >= terminated.size()) {
      Refuse("prompt text too long");
      return;
    }
    std::ranges::copy(text, terminated.begin());
    Render::DrawLiveCompactTextColor(terminated.data(), xy,
                                     static_cast<int>(color), nullptr);
  }
};

void Report(ResultsDrawResult result) {
  if (result == ResultsDrawResult::kInvalidTexture) {
    Refuse("texture index outside the atlas");
  }
}
}  // namespace

// Copied code builds with unsigned plain char (/J); read the atlas bytes as
// unsigned whatever this translation unit uses.
std::array<Render::TextureRegion, kResultsAtlasEntries> LiveResultsAtlas() {
  std::array<Render::TextureRegion, kResultsAtlasEntries> atlas{};
  std::ranges::transform(Menu_Results_disMap, atlas.begin(),
                         [](const Render_TextureRegion& entry) {
                           return Render::TextureRegion{
                               .u = static_cast<std::uint8_t>(entry.x),
                               .v = static_cast<std::uint8_t>(entry.y),
                               .width = static_cast<std::uint8_t>(entry.w),
                               .height = static_cast<std::uint8_t>(entry.h),
                               .texture_page = entry.tpage,
                               .clut = entry.clut};
                         });
  return atlas;
}

// decomp: MENUF.PRG _renderTextureFadeIn (3B8.c:1877).
void LiveDrawFadingTexture(int x, int y, int texture_index, int brightness) {
  const auto atlas = LiveResultsAtlas();
  LiveResultsTarget target;
  Report(DrawFadingTexture(atlas, x, y, texture_index, brightness, target));
}

// decomp: MENUF.PRG _renderTextureFadeInClut (3B8.c:1914).
void LiveDrawFadingTextureClut(int x, int y, int texture_index, int brightness,
                               int clut) {
  const auto atlas = LiveResultsAtlas();
  LiveResultsTarget target;
  Report(DrawFadingTextureClut(atlas, x, y, texture_index, brightness, clut,
                               target));
}

// decomp: MENUF.PRG _renderTime (3B8.c:1951).
void LiveDrawTime(int x, int y, int timer, int time, int is_new_record) {
  const auto atlas = LiveResultsAtlas();
  LiveResultsTarget target;
  Report(DrawTime(atlas, x, y, timer, time, is_new_record != 0, target));
}

// decomp: MENUF.PRG _renderPressButtonPrompt (3B8.c:2117).
void LiveDrawPressButtonPrompt(int x, int y, int slide_timer,
                               int bounce_timer) {
  LiveResultsTarget target;
  Report(DrawPressButtonPrompt(x, y, slide_timer, bounce_timer, target));
}
}  // namespace VS::Menu::Results
