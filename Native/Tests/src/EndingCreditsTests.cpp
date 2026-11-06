#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Ending/Credits/StaffRollCore.hpp"
#include "TestSupport/ChannelWarnings.hpp"
#include "TestSupport/EndingDisplayRecorder.hpp"

namespace {
namespace Credits = VS::Ending::Credits;
namespace Ending = VS::Ending;
namespace Math = VS::Core::Math;
namespace Render = VS::Render;
using VS::test_support::ChannelWarnings;
using VS::test_support::EndingDisplayRecorder;
using VS::test_support::FrameBufferAllocation;
using VS::test_support::TimBlockSpec;
using VS::test_support::TimBytes;
using DisplayRecorder = EndingDisplayRecorder<Ending::DisplayHost>;

constexpr std::size_t kDecodedResource = 7;
constexpr std::size_t kWorkspaceBytes = 16;
constexpr std::int16_t kScrollSpeed = -128;
// Script opcodes.
constexpr std::uint8_t kOpWait = 1;
constexpr std::uint8_t kOpText = 2;
constexpr std::uint8_t kOpIllustration = 3;
constexpr std::uint8_t kOpSpecial = 4;
constexpr std::uint8_t kOpFadeIn = 5;
constexpr std::uint8_t kOpFadeOut = 6;
constexpr std::uint8_t kOpFreezeScroll = 7;
constexpr std::uint8_t kOpHoldFrames = 8;
constexpr std::uint8_t kOpEnd = 0xFF;
// The text palette (960, 480) and texture page.
constexpr std::uint16_t kTextPalette = 30780;
constexpr std::uint16_t kTextPage = 31;

// The staff roll's host: the display recorder plus its script, glyph widths,
// art, illustration reads and music, logged in `calls` beside the display
// calls.
class FakeCreditsHost final
    : public EndingDisplayRecorder<Credits::StaffRollHost> {
 public:
  std::vector<std::uint8_t> script;
  std::vector<std::uint8_t> glyph_widths;
  std::vector<std::vector<std::byte>> embedded_arts;
  std::vector<std::byte> workspace;
  std::map<std::size_t, std::vector<std::byte>> resources;
  std::vector<int> projection_depths;
  std::vector<std::array<Credits::ModelVertex, Ending::kQuadCorners>> projected;
  bool art_prepared = true;
  // IsIllustrationReadComplete reports each read pending this many times.
  int read_polls = 0;

  void BeginRippleProjection(int depth) override {
    projection_depths.push_back(depth);
  }
  // Projects each vertex straight onto its x and y.
  [[nodiscard]] std::array<Ending::ScreenPoint, Ending::kQuadCorners>
  ProjectQuad(const std::array<Credits::ModelVertex, Ending::kQuadCorners>&
                  vertices) override {
    projected.push_back(vertices);
    std::array<Ending::ScreenPoint, Ending::kQuadCorners> corners{};
    std::ranges::transform(vertices, corners.begin(),
                           [](const Credits::ModelVertex& vertex) {
                             return Ending::ScreenPoint{vertex.x, vertex.y};
                           });
    return corners;
  }
  void StartEndingMusic(int byte_count) override {
    calls.push_back("music " + std::to_string(byte_count));
  }
  void StopMusic() override { calls.emplace_back("stop music"); }
  [[nodiscard]] bool PrepareEmbeddedArt() override {
    calls.emplace_back("prepare");
    return art_prepared;
  }
  [[nodiscard]] std::span<const std::byte> EmbeddedArt(
      std::size_t index) override {
    if (index >= embedded_arts.size()) return {};
    return embedded_arts[index];
  }
  [[nodiscard]] std::span<std::byte> IllustrationWorkspace() override {
    return workspace;
  }
  void BeginIllustrationRead(std::size_t resource) override {
    calls.push_back("read " + std::to_string(resource));
    pending_polls_ = read_polls;
  }
  [[nodiscard]] bool IsIllustrationReadComplete() override {
    if (pending_polls_ == 0) return true;
    --pending_polls_;
    return false;
  }
  void EndIllustrationRead() override { calls.emplace_back("end read"); }
  void FreeIllustration(std::size_t resource) override {
    calls.push_back("free " + std::to_string(resource));
  }
  [[nodiscard]] std::span<const std::byte> IllustrationResource(
      std::size_t resource) override {
    const auto found = resources.find(resource);
    if (found == resources.end()) return {};
    return found->second;
  }
  [[nodiscard]] std::span<const std::uint8_t> Script() override {
    return script;
  }
  [[nodiscard]] std::span<const std::uint8_t> GlyphWidths() override {
    return glyph_widths;
  }
  void UpdateClearProgress() override { calls.emplace_back("update progress"); }
  void ShowClearResults() override { calls.emplace_back("show results"); }

 private:
  int pending_polls_ = 0;
};

std::vector<std::byte> Bytes(std::initializer_list<int> values) {
  std::vector<std::byte> bytes;
  std::ranges::transform(values, std::back_inserter(bytes), [](int value) {
    return static_cast<std::byte>(value);
  });
  return bytes;
}

std::vector<std::uint8_t> Script(std::initializer_list<int> values) {
  std::vector<std::uint8_t> script;
  std::ranges::transform(values, std::back_inserter(script), [](int value) {
    return static_cast<std::uint8_t>(value);
  });
  return script;
}

// Glyph n is 5 + n % 4 pixels wide.
std::vector<std::uint8_t> GlyphWidths(std::size_t count) {
  std::vector<std::uint8_t> widths(count);
  for (std::size_t glyph = 0; glyph < count; ++glyph) {
    widths[glyph] = static_cast<std::uint8_t>(5 + (glyph % 4));
  }
  return widths;
}

std::int16_t Width(std::initializer_list<int> text, std::size_t glyphs = 224) {
  const auto bytes = Script(text);
  return Credits::MeasureTextWidth(bytes, GlyphWidths(glyphs));
}

std::vector<int> SpriteBrightness(
    const std::vector<Ending::SpriteDraw>& sprites) {
  std::vector<int> levels;
  std::ranges::transform(sprites, std::back_inserter(levels),
                         &Ending::SpriteDraw::brightness);
  return levels;
}

std::vector<Credits::ElementKind> Kinds(const Credits::State& state,
                                        std::size_t count) {
  std::vector<Credits::ElementKind> kinds;
  std::ranges::transform(std::span(state.elements).first(count),
                         std::back_inserter(kinds), &Credits::Element::kind);
  return kinds;
}

// Queues resource 7 holding `input` and decodes it into a 16-byte workspace.
bool Decode(Credits::State& state, FakeCreditsHost& host,
            std::vector<std::byte> input) {
  host.resources[kDecodedResource] = std::move(input);
  host.workspace.assign(kWorkspaceBytes, std::byte{});
  Credits::QueueIllustrationDecode(state.decoder, kDecodedResource);
  return Credits::DecodeIllustrationStep(state, host);
}

// "abc", three x, "pq" twice, then "qpq" copied from three back and repeated.
std::vector<std::byte> EncodedImage() {
  return Bytes(
      {2, 'a', 'b', 'c', 0x7D, 2, 'x', 0x7E, 1, 'p', 'q', 0x80, 2, 0x7C, 0x7F});
}

std::string Refusal(const std::string& reason, std::size_t input,
                    std::size_t output) {
  return "Illustration 7 decode refused: " + reason + " (input " +
         std::to_string(input) + " of " + std::to_string(input) + ", output " +
         std::to_string(output) + " of " + std::to_string(kWorkspaceBytes) +
         ")";
}

Ending::SpriteDraw TextSprite(Ending::ScreenPoint position,
                              Ending::TexelPoint texel, std::int16_t width) {
  return {.ordering_slot = 3,
          .position = position,
          .texel = texel,
          .width = width,
          .height = 16,
          .palette = kTextPalette,
          .brightness = 100};
}

// An underline cap: the left one at u 0 and the right one at u 168.
Ending::SpriteDraw UnderlineCap(Ending::ScreenPoint position, std::uint8_t u) {
  return {.ordering_slot = 3,
          .position = position,
          .texel = {u, 208},
          .width = 16,
          .height = 3,
          .palette = kTextPalette,
          .brightness = 100};
}
}  // namespace

TEST_CASE("AddElement starts the first free slot after the main one",
          "[ending][ending-results-credits]") {
  Credits::State state;
  state.elements[1] = {.phase = 2};
  state.elements[2].animation_frame = 9;
  Credits::Element* element =
      Credits::AddElement(state, Credits::ElementKind::kBackground);
  CHECK(element == &state.elements[2]);
  CHECK(element->kind == Credits::ElementKind::kBackground);
  CHECK((element->phase == 1 && element->animation_frame == 0));
  CHECK(state.elements[0].phase == 0);
  Credits::ResetElements(state);
  CHECK(std::ranges::all_of(state.elements, [](const Credits::Element& slot) {
    return slot.phase == 0;
  }));
}

TEST_CASE("AddElement refuses a new element when every slot is in use",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  Credits::State state;
  for (Credits::Element& element : state.elements) element.phase = 1;
  state.elements[0].phase = 0;
  CHECK(Credits::AddElement(state, Credits::ElementKind::kFinCaption) ==
        nullptr);
  CHECK(state.elements[0].kind == Credits::ElementKind::kNone);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Credits element refused: all 31 slots are in use"});
}

TEST_CASE("ReserveMainElement starts the main slot only while it is free",
          "[ending][ending-results-credits]") {
  Credits::State state;
  Credits::Element* main = Credits::ReserveMainElement(
      state, Credits::ElementKind::kFadeInBackground);
  CHECK(main == &state.elements[0]);
  CHECK(main->phase == 1);
  CHECK(Credits::ReserveMainElement(
            state, Credits::ElementKind::kFadeOutBackground) == nullptr);
  CHECK(state.elements[0].kind == Credits::ElementKind::kFadeInBackground);
}

TEST_CASE("DrawElements frees finished slots and draws the active ones",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  Credits::State state;
  state.elements[0] = {.kind = Credits::ElementKind::kBackground, .phase = -1};
  state.elements[1] = {.phase = 1};
  state.elements[2] = {.kind = Credits::ElementKind::kBackground, .phase = 1};
  state.elements[3] = {.kind = Credits::ElementKind::kScrollingCopyright,
                       .phase = 1};
  FakeCreditsHost host;
  CHECK(Credits::DrawElements(state, host) == 3);
  CHECK((state.elements[0].phase == 0 && state.elements[1].phase == 0));
  CHECK(host.sprites.size() == 3);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Credits element refused: phase 1 has no renderer; freeing it"});
}

TEST_CASE("ExecuteScript starts each scripted element until a wait",
          "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  host.script = Script({kOpText,
                        2,
                        'H',
                        'I',
                        kOpIllustration,
                        kOpSpecial,
                        0,
                        kOpSpecial,
                        1,
                        kOpSpecial,
                        2,
                        kOpSpecial,
                        3,
                        kOpSpecial,
                        9,
                        kOpFadeIn,
                        kOpFreezeScroll,
                        kOpHoldFrames,
                        2,
                        0x2C,
                        1,
                        kOpWait,
                        3,
                        kOpEnd});
  Credits::State state;
  state.scroll_speed_fixed8 = kScrollSpeed;
  Credits::ExecuteScript(state, host);
  using Kind = Credits::ElementKind;
  CHECK(Kinds(state, 8) ==
        std::vector<Kind>{Kind::kFadeInBackground, Kind::kScrollingText,
                          Kind::kIllustration, Kind::kScrollingCopyright,
                          Kind::kScrollingTitleLogo, Kind::kFinCaption,
                          Kind::kPhantomPainRipple, Kind::kNone});
  CHECK(state.elements[1].text_offset == 1);
  CHECK(state.scroll_speed_fixed8 == 0);
  // 0x12C frames less the 160-frame bias.
  CHECK(state.illustration.hold_frames[2] == 140);
  CHECK((state.script_cursor == 23 && state.script_delay_frames == 6));
  CHECK_FALSE(state.has_finished);
}

TEST_CASE("ExecuteScript counts down a wait and then reaches the end",
          "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  host.script = Script({kOpWait, 1, kOpEnd});
  Credits::State state;
  Credits::ExecuteScript(state, host);
  CHECK(state.script_delay_frames == 2);
  Credits::ExecuteScript(state, host);
  Credits::ExecuteScript(state, host);
  CHECK((state.script_delay_frames == 0 && !state.has_finished));
  Credits::ExecuteScript(state, host);
  CHECK(state.has_finished);
  CHECK(state.script_cursor == 2);
}

TEST_CASE("ExecuteScript fades out once and stalls on unknown opcodes",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  FakeCreditsHost host;
  host.script = Script({kOpFadeOut, kOpFadeOut, 9, kOpEnd});
  Credits::State state;
  Credits::ExecuteScript(state, host);
  CHECK(state.elements[0].kind == Credits::ElementKind::kFadeOutBackground);
  CHECK(state.elements[0].width == 0);
  CHECK(state.script_cursor == 2);
  CHECK_FALSE(state.has_finished);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Credits fade-out refused: the main element is in use"});
}

TEST_CASE(
    "ExecuteScript ends the staff roll when an operand passes the "
    "script",
    "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  FakeCreditsHost host;
  Credits::State state;
  for (const std::uint8_t opcode : {kOpWait, kOpText, kOpSpecial}) {
    host.script = {opcode};
    state = {};
    Credits::ExecuteScript(state, host);
    CHECK(state.has_finished);
  }
  host.script = Script({kOpHoldFrames, 16, 0, 0});
  state = {};
  Credits::ExecuteScript(state, host);
  CHECK(state.illustration.hold_frames ==
        decltype(state.illustration.hold_frames){});
  const std::string past_one =
      "Credits script read refused: 2 bytes at 0 pass its 1 bytes; ending the "
      "staff roll";
  const std::string past_four =
      "Credits script read refused: 1 bytes at 4 pass its 4 bytes; ending the "
      "staff roll";
  CHECK(warnings.messages ==
        std::vector<std::string>{
            past_one, past_one, past_one,
            "Credits hold time refused: illustration 16 is past the 16 timers",
            past_four});
}

TEST_CASE("MeasureTextWidth overlaps glyphs and follows the table codes",
          "[ending][ending-results-credits]") {
  // 'A' (glyph 33) is 6 wide and 'B' (34) 7; after code 2 'A' is glyph 145.
  CHECK(Width({5, 'A', 'B', 2, 'A', 31}) == 15);
  CHECK(Width({3, 2, 1, 'A'}) == 5);
  CHECK(Width({2, ' ', 3}) == 4);
}

TEST_CASE("MeasureTextWidth clamps the text and skips unknown glyphs",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  CHECK(Width({9, 'A', 'B'}) == 11);
  CHECK(Width({}) == 0);
  CHECK(Width({1, 'z'}, 40) == 0);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Credits text of 9 characters runs past the script; using 2",
            "Credits text refused: it starts past the script",
            "Credits glyph 90 refused: the width table has 40 entries"});
}

TEST_CASE("DrawScrollingText centres its glyphs and underlines the line",
          "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  host.script = Script({kOpText, 3, 'A', 3, 'B'});
  host.glyph_widths = GlyphWidths(224);
  Credits::State state{.scroll_speed_fixed8 = kScrollSpeed,
                       .background_brightness = 100};
  Credits::Element element{.phase = 1, .text_offset = 1};
  Credits::DrawScrollingText(element, state, host);
  CHECK((element.width == 11 && element.phase == 2));
  CHECK(host.sprites ==
        std::vector<Ending::SpriteDraw>{TextSprite({154, 223}, {16, 32}, 6),
                                        TextSprite({159, 223}, {32, 32}, 7),
                                        UnderlineCap({139, 240}, 0),
                                        UnderlineCap({166, 240}, 168)});
  CHECK(host.textured_quads ==
        std::vector<Ending::TexturedQuadDraw>{
            {.ordering_slot = 3,
             .corners = {{{155, 240}, {166, 240}, {155, 243}, {166, 243}}},
             .texels = {{{8, 208}, {16, 208}, {8, 211}, {16, 211}}},
             .palette = kTextPalette,
             .texture_page = kTextPage,
             .brightness = 100}});
  CHECK(host.page_changes ==
        std::vector<Ending::TexturePageChange>{{3, kTextPage}});
}

TEST_CASE(
    "DrawScrollingText fixes the cap heights and frees the line above "
    "the screen",
    "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  host.script = Script({4, 2, 3, 31, 'A'});
  host.glyph_widths = GlyphWidths(224);
  const Credits::State state{.scroll_speed_fixed8 = kScrollSpeed};
  Credits::Element element{
      .phase = 2, .x_fixed8 = 50 << 8, .y_fixed8 = 100 << 8, .text_offset = 0};
  Credits::DrawScrollingText(element, state, host);
  // The second table's underline sits 18 below the line, and the backspace
  // pulls 'A' (glyph 145) back a pixel.
  CHECK(host.sprites.at(0).position == Ending::ScreenPoint{49, 99});
  CHECK(host.sprites.at(0).texel == Ending::TexelPoint{16, 144});
  CHECK(host.sprites.at(1).position.y == 117);
  host.sprites.clear();
  element.y_fixed8 = -(12 << 8);
  Credits::DrawScrollingText(element, state, host);
  CHECK(element.phase == -1);
  CHECK(host.sprites.empty());
}

TEST_CASE("DrawScrollingCopyright scrolls its sprite up from the bottom",
          "[ending][ending-results-credits]") {
  DisplayRecorder host;
  const Credits::State state{.scroll_speed_fixed8 = kScrollSpeed,
                             .background_brightness = 90};
  Credits::Element element{.phase = 1};
  Credits::DrawScrollingCopyright(element, state, host);
  CHECK(host.sprites == std::vector<Ending::SpriteDraw>{{.ordering_slot = 3,
                                                         .position = {100, 223},
                                                         .texel = {0, 0x30},
                                                         .width = 0x78,
                                                         .height = 0x10,
                                                         .palette = 4156,
                                                         .brightness = 90}});
  CHECK(host.page_changes == std::vector<Ending::TexturePageChange>{{3, 47}});
  element.y_fixed8 = -(12 << 8);
  Credits::DrawScrollingCopyright(element, state, host);
  CHECK(element.phase == -1);
  CHECK(host.sprites.size() == 1);
}

TEST_CASE("DrawBackground draws the two halves, shades and borders",
          "[ending][ending-results-credits]") {
  DisplayRecorder host;
  const Credits::State state{.background_brightness = 77};
  Credits::DrawBackground(state, host);
  CHECK(host.sprites.at(0) == Ending::SpriteDraw{.ordering_slot = 0x7FF,
                                                 .position = {0, 10},
                                                 .texel = {0, 10},
                                                 .width = 256,
                                                 .height = 204,
                                                 .palette = 16368,
                                                 .brightness = 77});
  CHECK(host.sprites.at(1).position == Ending::ScreenPoint{256, 10});
  CHECK(host.sprites.at(1).width == 64);
  CHECK(host.gradient_quads.at(1) ==
        Ending::GradientQuadDraw{
            .ordering_slot = 2,
            .corners = {{{0, 194}, {320, 194}, {0, 214}, {320, 214}}},
            .brightness = {0, 0, 255, 255},
            .semi_transparent = true});
  // The top border is queued twice, as in the original.
  CHECK(host.tiles == std::vector<Ending::TileDraw>{
                          {.ordering_slot = 2, .width = 320, .height = 10},
                          {.ordering_slot = 2, .width = 320, .height = 10},
                          {.ordering_slot = 2,
                           .position = {0, 214},
                           .width = 320,
                           .height = 10}});
  CHECK(host.page_changes == std::vector<Ending::TexturePageChange>{
                                 {0x7FF, 44}, {0x7FF, 45}, {2, 192}});
}

TEST_CASE("FadeInBackground eases the brightness up over 128 frames",
          "[ending][ending-results-credits]") {
  Credits::State state{.is_background_fading = true};
  Credits::Element element{.phase = 1};
  for (int frame = 0; frame < 64; ++frame) {
    Credits::FadeInBackground(element, state);
  }
  CHECK(state.background_brightness == 64);
  CHECK(element.phase == 2);
  for (int frame = 64; frame < 128; ++frame) {
    Credits::FadeInBackground(element, state);
  }
  CHECK(state.background_brightness == 128);
  CHECK(element.phase == -1);
  CHECK_FALSE(state.is_background_fading);
}

TEST_CASE("FadeOutBackground eases the brightness down and may end the roll",
          "[ending][ending-results-credits]") {
  Credits::State state;
  Credits::Element element{.phase = 1};
  for (int frame = 0; frame < 64; ++frame) {
    Credits::FadeOutBackground(element, state);
  }
  CHECK(state.is_background_fading);
  CHECK(state.background_brightness == 64);
  for (int frame = 64; frame < 128; ++frame) {
    Credits::FadeOutBackground(element, state);
  }
  CHECK((state.background_brightness == 0 && element.phase == -1));
  CHECK_FALSE(state.has_finished);
  Credits::Element last{.phase = 2, .animation_frame = 127, .width = 1};
  Credits::FadeOutBackground(last, state);
  CHECK(state.has_finished);
}

TEST_CASE("FadeOutForSkip darkens the screen and ends the roll",
          "[ending][ending-results-credits]") {
  DisplayRecorder host;
  Credits::State state;
  Credits::Element element{.phase = 1};
  Credits::FadeOutForSkip(element, state, host);
  const int first =
      (Math::EvaluateEase(Math::EaseCurve::kEaseOut, {1, 128}) * 255) >> 12;
  CHECK(host.tiles.at(0) ==
        Ending::TileDraw{.width = 320,
                         .height = 240,
                         .brightness = static_cast<std::uint8_t>(first),
                         .semi_transparent = true});
  CHECK(host.page_changes == std::vector<Ending::TexturePageChange>{{0, 64}});
  for (int frame = 1; frame < 128; ++frame) {
    Credits::FadeOutForSkip(element, state, host);
  }
  CHECK(host.tiles.back().brightness == 255);
  CHECK(state.has_finished);
}

TEST_CASE("DrawFinCaption fades in, holds and fades out the caption",
          "[ending][ending-results-credits]") {
  DisplayRecorder host;
  Credits::Element element{.phase = 1};
  for (int frame = 0; frame < 360; ++frame) {
    Credits::DrawFinCaption(element, host);
  }
  const auto levels = SpriteBrightness(host.sprites);
  CHECK(std::vector<int>{levels[0], levels[1], levels[59], levels[60]} ==
        std::vector<int>{0, 2, 125, 128});
  CHECK(std::vector<int>{levels[299], levels[300], levels[359]} ==
        std::vector<int>{128, 128, 2});
  CHECK(element.phase == -1);
  CHECK(host.sprites.at(0) == Ending::SpriteDraw{.ordering_slot = 1,
                                                 .position = {248, 184},
                                                 .texel = {0, 160},
                                                 .width = 36,
                                                 .height = 17,
                                                 .palette = 5304});
  CHECK(host.page_changes.at(0) == Ending::TexturePageChange{1, 46});
}

TEST_CASE("Credits elements in an undefined phase draw at brightness 0",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  FakeCreditsHost host;
  Credits::Element fin{.phase = 7};
  Credits::DrawFinCaption(fin, host);
  Credits::Element logo{.phase = 6};
  Credits::DrawScrollingTitleLogo(logo, {.background_brightness = 99}, host);
  Credits::Element ripple{.phase = 6};
  Credits::RippleState grid;
  Credits::DrawPhantomPainRipple(ripple, grid, host);
  CHECK(SpriteBrightness(host.sprites) == std::vector<int>{0, 0, 0});
  CHECK(host.textured_quads.at(0).brightness == 0);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Credits fin caption phase 7 sets no brightness; drawing it at 0",
            "Credits title logo phase 6 sets no brightness; drawing it at 0",
            "Credits phantom pain ripple phase 6 sets no brightness; drawing "
            "it at 0"});
}

TEST_CASE(
    "DrawScrollingTitleLogo fades in with the background and its "
    "shadow",
    "[ending][ending-results-credits]") {
  DisplayRecorder host;
  const Credits::State state{.background_brightness = 120};
  Credits::Element element{.phase = 1};
  Credits::DrawScrollingTitleLogo(element, state, host);
  const Ending::SpriteDraw logo{.ordering_slot = 3,
                                .position = {61, 80},
                                .texel = {0, 96},
                                .width = 208,
                                .height = 48,
                                .palette = 5176,
                                .brightness = 2,
                                .semi_transparent = true};
  Ending::SpriteDraw shadow = logo;
  shadow.palette = 5240;
  CHECK(host.sprites == std::vector<Ending::SpriteDraw>{logo, shadow});
  CHECK(host.page_changes ==
        std::vector<Ending::TexturePageChange>{{3, 0x2E}, {3, 0x4E}});
  for (int frame = 1; frame < 120; ++frame) {
    Credits::DrawScrollingTitleLogo(element, state, host);
  }
  CHECK(host.sprites.at(118).brightness == 120);
  CHECK(element.phase == 4);
}

TEST_CASE("DrawScrollingTitleLogo scrolls out and frees itself",
          "[ending][ending-results-credits]") {
  DisplayRecorder host;
  Credits::State state{.scroll_speed_fixed8 = -256,
                       .background_brightness = 50};
  Credits::Element element{.phase = 4, .x_fixed8 = 61 << 8};
  Credits::DrawScrollingTitleLogo(element, state, host);
  CHECK(host.sprites.at(0).position == Ending::ScreenPoint{61, -1});
  CHECK(host.sprites.at(0).brightness == 50);
  state.scroll_speed_fixed8 = -0x4000;
  Credits::DrawScrollingTitleLogo(element, state, host);
  CHECK(element.phase == -1);
  CHECK(host.sprites.size() == 2);
}

TEST_CASE("FillRippleGrid stores each cell's distance from the grid centre",
          "[ending][ending-results-credits]") {
  Credits::RippleState ripple;
  Credits::FillRippleGrid(ripple);
  const auto root = [](int dx, int dy) {
    return Math::SquareRootInteger((dx * dx) + (dy * dy));
  };
  const auto& grid = ripple.distance_grid;
  // Columns are 18 apart and rows 4, centred on row 2, column 8.
  CHECK(grid[40] == 0);
  CHECK(grid[41] == root(18, 0));
  CHECK(grid[72] == root(0, 8));
  CHECK(grid[0] == root(144, 8));
  CHECK(grid[15] == root(126, 8));
  // The row past the grid and the entry after it.
  CHECK(grid[79] == root(126, 8));
  CHECK(grid[80] == root(144, 12));
}

TEST_CASE("DrawPhantomPainRipple starts a 4x16 grid of projected quads",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  FakeCreditsHost host;
  Credits::Element element{.phase = 1};
  Credits::RippleState ripple;
  Credits::DrawPhantomPainRipple(element, ripple, host);
  CHECK((element.phase == 2 && element.animation_frame == 1));
  CHECK((ripple.phase_angle == 200 && ripple.scale_frame == 1));
  CHECK(host.projection_depths == std::vector<int>{512});
  REQUIRE(host.textured_quads.size() == 64);
  CHECK(host.textured_quads.back() ==
        Ending::TexturedQuadDraw{
            .ordering_slot = 3,
            .corners = {{{63, 4}, {72, 4}, {63, 8}, {72, 8}}},
            .texels = {{{135, 192}, {144, 192}, {135, 196}, {144, 196}}},
            .palette = 5368,
            .texture_page = 46,
            .semi_transparent = true});
  const int scale =
      (Math::EvaluateEase(Math::EaseCurve::kEaseIn, {89, 90}) * 65) >> 10;
  CHECK(host.projected.at(0)[0].z ==
        ((Math::Sine(200 + (ripple.distance_grid[0] * 400)) * scale) >> 12));
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Phantom pain ripple: the last quad row reads 17 distance entries "
            "past the 4x16 grid; using the extended grid"});
}

TEST_CASE("DrawPhantomPainRipple holds flat and fades out",
          "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  Credits::RippleState ripple{.scale_frame = 90};
  Credits::FillRippleGrid(ripple);
  Credits::Element element{.phase = 3, .animation_frame = 119};
  Credits::DrawPhantomPainRipple(element, ripple, host);
  CHECK(host.textured_quads.at(0).brightness == 128);
  CHECK(std::ranges::all_of(host.projected, [](const auto& quad) {
    return std::ranges::all_of(
        quad, [](const Credits::ModelVertex& vertex) { return vertex.z == 0; });
  }));
  CHECK((element.phase == 4 && element.animation_frame == 0));
  host.textured_quads.clear();
  element.animation_frame = 89;
  Credits::DrawPhantomPainRipple(element, ripple, host);
  CHECK(host.textured_quads.at(0).brightness == 1);
  CHECK(element.phase == -1);
}

TEST_CASE("DistanceFromScreenCenter measures from (160, 112)",
          "[ending][ending-results-credits]") {
  CHECK(Credits::DistanceFromScreenCenter(160, 112) == 0);
  CHECK(Credits::DistanceFromScreenCenter(163, 116) == 5);
  CHECK(Credits::DistanceFromScreenCenter(0, 0) ==
        Math::SquareRootInteger((160 * 160) + (112 * 112)));
}

namespace {
using Corners = std::array<Ending::ScreenPoint, Ending::kQuadCorners>;
// Corners at the centre, the origin and about 400 and 300 from the centre.
constexpr Corners kRadialCorners{{
    {160, 112},
    {0, 0},
    {480, 352},
    {460, 112},
}};

std::vector<int> FadedBrightness(std::int16_t step, bool fade_out) {
  std::array<Credits::ShadedVertex, Ending::kQuadCorners> quad{};
  std::ranges::transform(kRadialCorners, quad.begin(),
                         [](Ending::ScreenPoint corner) {
                           return Credits::ShadedVertex{.position = corner};
                         });
  Credits::ApplyRadialVertexFade(quad, step, fade_out);
  std::vector<int> levels;
  std::ranges::transform(quad, std::back_inserter(levels),
                         &Credits::ShadedVertex::brightness);
  return levels;
}

// Each corner's falloff: positive within 200 pixels of the screen centre.
std::array<int, Ending::kQuadCorners> RadialFalloff() {
  std::array<int, Ending::kQuadCorners> falloff{};
  std::ranges::transform(
      kRadialCorners, falloff.begin(), [](Ending::ScreenPoint corner) {
        return ((200 - Credits::DistanceFromScreenCenter(corner.x, corner.y))
                << 7) /
               400;
      });
  return falloff;
}
}  // namespace

TEST_CASE("ApplyRadialVertexFade brightens the centre first when fading in",
          "[ending][ending-results-credits]") {
  const auto falloff = RadialFalloff();
  CHECK(falloff[0] == 64);
  CHECK((falloff[1] == 1 && falloff[2] < -60 && falloff[3] < -30));
  // Step 40 of 80 adds the falloff to 64; the centre's 128 is kept.
  CHECK(FadedBrightness(40, false) == std::vector<int>{128, 64 + falloff[1],
                                                       64 + falloff[2],
                                                       64 + falloff[3]});
  // Past 128 a corner is drawn at full brightness.
  CHECK(FadedBrightness(80, false) ==
        std::vector<int>{128, 128, 128 + falloff[2], 128 + falloff[3]});
}

TEST_CASE("ApplyRadialVertexFade darkens the centre first when fading out",
          "[ending][ending-results-credits]") {
  const auto falloff = RadialFalloff();
  CHECK(FadedBrightness(40, true) ==
        std::vector<int>{0, 64 - falloff[1], 64 - falloff[2], 64 - falloff[3]});
  // Below 0 a corner is drawn black.
  CHECK(FadedBrightness(80, true) ==
        std::vector<int>{0, 0, -falloff[2], -falloff[3]});
}

TEST_CASE("DrawIllustration queues the next decode and fades the image in",
          "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  Credits::State state{.background_brightness = 128};
  Credits::Element element{.phase = 1, .illustration_index = 4};
  Credits::DrawIllustration(element, state, host);
  CHECK(state.decoder.phase == Credits::DecodePhase::kQueued);
  CHECK(state.decoder.resource == 5);
  CHECK((element.phase == 2 && element.animation_frame == 1));
  const Ending::SpriteDraw left{.ordering_slot = 4,
                                .width = 0xA0,
                                .height = 0xE0,
                                .palette = 30976,
                                .semi_transparent = true};
  Ending::SpriteDraw right = left;
  right.position.x = 0xA0;
  right.texel.u = 0x20;
  CHECK(host.sprites == std::vector<Ending::SpriteDraw>{left, right});
  // 8-bit subtractive pages at (768, 256) and (832, 256).
  CHECK(host.page_changes ==
        std::vector<Ending::TexturePageChange>{{4, 220}, {4, 221}});
}

TEST_CASE("DrawIllustration holds the last illustration after its fade-in",
          "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  Credits::State state{.background_brightness = 128};
  Credits::Element element{.phase = 1, .illustration_index = 15};
  for (int frame = 0; frame < 81; ++frame) {
    Credits::DrawIllustration(element, state, host);
  }
  CHECK(state.decoder.phase == Credits::DecodePhase::kIdle);
  CHECK(element.phase == 5);
  CHECK(SpriteBrightness(host.sprites).at(std::size_t{2} * 79) == 126);
  CHECK(state.illustration.brightness == 128);
}

TEST_CASE("DrawIllustration holds, fades out and uploads the decoded image",
          "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  host.workspace = TimBytes({.area = {640, 256, 2, 2}});
  Credits::State state{.background_brightness = 64};
  state.illustration.hold_frames[4] = 2;
  Credits::Element element{.phase = 3, .illustration_index = 4};
  Credits::DrawIllustration(element, state, host);
  Credits::DrawIllustration(element, state, host);
  CHECK((element.phase == 4 && state.illustration.brightness == 128));
  for (int frame = 0; frame < 80; ++frame) {
    Credits::DrawIllustration(element, state, host);
  }
  CHECK(SpriteBrightness(host.sprites).at(4) == 64);
  CHECK((element.phase == 1 && element.illustration_index == 5));
  CHECK(host.loaded_areas == std::vector<Render::ScreenRect>{{640, 256, 2, 2}});
}

TEST_CASE("DrawIllustration uploads nothing before the first disc image",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  FakeCreditsHost host;
  Credits::State state;
  Credits::Element element{
      .phase = 4, .animation_frame = 79, .illustration_index = 3};
  Credits::DrawIllustration(element, state, host);
  CHECK((element.phase == 1 && element.illustration_index == 4));
  CHECK(host.loaded_areas.empty());
  Credits::Element unknown{.phase = 3, .illustration_index = 16};
  Credits::DrawIllustration(unknown, state, host);
  CHECK(unknown.phase == 3);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Illustration 16 hold refused: past the 16 timers; holding"});
}

TEST_CASE("UploadTexture uploads a TIM's CLUT and then its image",
          "[ending][ending-results-credits]") {
  DisplayRecorder host;
  const auto tim =
      TimBytes({.area = {640, 0, 4, 2}, .fill = std::byte{0x22}},
               TimBlockSpec{.area = {0, 480, 16, 1}, .fill = std::byte{0x11}});
  CHECK(Credits::UploadTexture(tim, host));
  CHECK(host.loaded_areas ==
        std::vector<Render::ScreenRect>{{0, 480, 16, 1}, {640, 0, 4, 2}});
  CHECK(host.loaded_pixels == std::vector<std::vector<std::byte>>{
                                  std::vector<std::byte>(32, std::byte{0x11}),
                                  std::vector<std::byte>(16, std::byte{0x22})});
  host.loaded_areas.clear();
  CHECK(Credits::UploadTexture(TimBytes({.area = {1, 2, 3, 4}}), host));
  CHECK(host.loaded_areas == std::vector<Render::ScreenRect>{{1, 2, 3, 4}});
}

TEST_CASE("UploadTexture refuses TIMs whose blocks leave the bytes",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  DisplayRecorder host;
  auto truncated = TimBytes({.area = {0, 0, 4, 2}});
  truncated.resize(30);
  auto headless = TimBytes({.area = {0, 0, 1, 1}});
  headless.resize(12);
  auto negative =
      TimBytes({.area = {0, 0, 1, 1}}, TimBlockSpec{.area = {0, 480, 16, 1}});
  std::fill_n(negative.begin() + 8, 4, std::byte{0xFF});
  CHECK_FALSE(Credits::UploadTexture({}, host));
  CHECK_FALSE(Credits::UploadTexture(truncated, host));
  CHECK_FALSE(Credits::UploadTexture(headless, host));
  CHECK_FALSE(Credits::UploadTexture(negative, host));
  CHECK(host.loaded_areas == std::vector<Render::ScreenRect>{{0, 480, 16, 1}});
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Texture upload refused: 0 bytes hold no TIM header",
            "Texture upload refused: the TIM block at 8 needs 16 pixel bytes "
            "past its 30 bytes",
            "Texture upload refused: the TIM block at 8 lies outside its 12 "
            "bytes",
            "Texture upload refused: the CLUT block length is invalid"});
}

TEST_CASE("DecodeIllustrationStep expands literals, fills and back-references",
          "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  Credits::State state;
  CHECK(Decode(state, host, EncodedImage()));
  const std::string text = "abcxxxpqpqqpqqpq";
  std::vector<std::byte> expected;
  std::ranges::transform(text, std::back_inserter(expected), [](char letter) {
    return static_cast<std::byte>(letter);
  });
  CHECK(host.workspace == expected);
  CHECK(state.decoder.phase == Credits::DecodePhase::kIdle);
  CHECK(state.decoder.input == 14);
  CHECK_FALSE(Credits::DecodeIllustrationStep(state, host));
}

TEST_CASE("DecodeIllustrationStep stops when the frame budget runs out",
          "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  host.vertical_blank_step = 100;
  Credits::State state;
  CHECK_FALSE(Decode(state, host, EncodedImage()));
  CHECK(state.decoder.phase == Credits::DecodePhase::kRunning);
  CHECK(state.decoder.output == 6);
  state.decoder.budget_start_tick = 300;
  CHECK_FALSE(Credits::DecodeIllustrationStep(state, host));
  CHECK(state.decoder.output == 16);
  state.decoder.budget_start_tick = 600;
  CHECK(Credits::DecodeIllustrationStep(state, host));
  CHECK(state.decoder.phase == Credits::DecodePhase::kIdle);
}

TEST_CASE(
    "DecodeIllustrationStep refuses runs that leave the input or "
    "workspace",
    "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  FakeCreditsHost host;
  Credits::State state;
  for (auto input : {Bytes({3, 'a'}), Bytes({0x80, 5}), Bytes({}),
                     Bytes({0x7D, 20, 'x'}), Bytes({0x7D, 1})}) {
    CHECK_FALSE(Decode(state, host, std::move(input)));
    CHECK(state.decoder.phase == Credits::DecodePhase::kIdle);
  }
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refusal("a literal run ends early", 2, 1),
            Refusal("a back-reference leaves the workspace", 2, 0),
            Refusal("its input ends without an end code", 0, 0),
            Refusal("a fill run passes the workspace", 3, 16),
            Refusal("a fill run ends early", 2, 0)});
}

TEST_CASE("DrawSmallText draws printable glyphs seven pixels apart",
          "[ending][ending-results-credits]") {
  DisplayRecorder host;
  const auto text = Script({'A', ' ', 'a', '\\', 200, 0, 'A'});
  Credits::DrawSmallText({10, 20}, 2, text, host);
  // 'a' folds onto 'A' and '\' onto glyph 15; the palette is (960, 34).
  CHECK(host.glyphs ==
        std::vector<Ending::GlyphDraw>{
            {.position = {10, 20}, .texel = {8, 16}, .palette = 2236},
            {.position = {24, 20}, .texel = {8, 16}, .palette = 2236},
            {.position = {31, 20}, .texel = {120, 0}, .palette = 2236}});
  CHECK(host.page_changes == std::vector<Ending::TexturePageChange>{{0, 0xF}});
}

TEST_CASE("InitializeDisplay lays out both buffers and clears VRAM",
          "[ending][ending-results-credits]") {
  DisplayRecorder host;
  host.frame_buffer = 1;
  Credits::InitializeDisplay({.width = 320,
                              .height = 240,
                              .projection_distance = 512,
                              .background = {1, 2, 3}},
                             host);
  CHECK(host.calls == std::vector<std::string>{"display off", "offset 160 120",
                                               "projection 512", "layout",
                                               "show 1", "reset presented"});
  CHECK(host.cleared_areas ==
        std::vector<Render::ScreenRect>{{0, 0, 1024, 512}});
  CHECK((host.layout.width == 320 && host.layout.viewport_height == 224));
  CHECK(host.layout.draw_origins ==
        std::array<Ending::ScreenPoint, 2>{{{0, 0}, {320, 0}}});
  CHECK(host.layout.display_origins ==
        std::array<Ending::ScreenPoint, 2>{{{320, 0}, {0, 0}}});
  CHECK(host.layout.screen == Render::ScreenRect{0, 8, 256, 224});
  CHECK(host.layout.background == Render::ClearColor{1, 2, 3});
}

TEST_CASE("PresentFrame flips the buffer and draws the ordering table",
          "[ending][ending-results-credits]") {
  DisplayRecorder host;
  CHECK(Credits::PresentFrame(0x7FF, host) == 1);
  CHECK(host.frame_buffer == 1);
  CHECK(Credits::PresentFrame(5, host) == 2);
  CHECK(host.frame_buffer == 0);
  CHECK(host.calls == std::vector<std::string>{"show 1", "show 0"});
  CHECK(host.drawn_slots == std::vector<int>{0x7FF, 5});
  CHECK(host.presented_frames == 2);
}

TEST_CASE("AllocateRenderBuffers sets up the display and both frames' buffers",
          "[ending][ending-results-credits]") {
  DisplayRecorder host;
  Credits::AllocateRenderBuffers(host);
  using Kind = Ending::FrameBufferKind;
  CHECK(host.allocations ==
        std::vector<FrameBufferAllocation>{{Kind::kPrimitives, 0, 0x20000},
                                           {Kind::kPrimitives, 1, 0x20000},
                                           {Kind::kOrderingTable, 0, 0x2088},
                                           {Kind::kOrderingTable, 1, 0x2088}});
  CHECK(host.calls.back() == "reset heap");
  CHECK(host.calls.at(1) == "offset 160 120");
  CHECK((host.layout.width == 320 && host.layout.viewport_height == 224));
}

TEST_CASE("BeginFrame reads the controllers and clears the frame's tables",
          "[ending][ending-results-credits]") {
  DisplayRecorder host;
  host.frame_buffer = 1;
  Credits::BeginFrame(host);
  CHECK(host.controller_updates == 1);
  REQUIRE(host.ordering_layouts.size() == 1);
  const Ending::OrderingTableLayout& layout = host.ordering_layouts[0];
  CHECK((layout.buffer == 1 && layout.entries == 0x822));
  CHECK((layout.overlay_offset == 2 && layout.scene_offset == 0x12));
}

TEST_CASE("Staff roll music starts the XA stream and stops it",
          "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  Credits::StartMusic(host);
  Credits::WaitForMusicEnd(host);
  CHECK(host.calls == std::vector<std::string>{"music 32767", "stop music"});
}

TEST_CASE("ConfigureDrawEnvironment clears to black on texture page 0x20",
          "[ending][ending-results-credits]") {
  const Ending::DrawEnvironment environment =
      Credits::ConfigureDrawEnvironment();
  CHECK(environment.clear_background);
  CHECK_FALSE(environment.dither);
  CHECK_FALSE(environment.draw_on_display);
  CHECK(environment.background == Render::ClearColor{0, 0, 0});
  CHECK(environment.texture_page == 0x20);
}

TEST_CASE("RunFrames runs the script until it ends and fades a skip",
          "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  host.script = Script({kOpWait, 2, kOpEnd});
  host.buttons = Credits::kStartButton;
  Credits::State state;
  Credits::RunFrames(state, host);
  CHECK(state.elapsed_frames == 6);
  CHECK(host.presented_frames == 6);
  CHECK(host.controller_updates == 6);
  CHECK(state.was_skip_requested);
  CHECK(state.elements[1].kind == Credits::ElementKind::kFadeOutForSkip);
  // The skip fade starts drawing on the frame after the button.
  CHECK(host.tiles.size() == 5);
}

namespace {
// RunStaffRoll's host and display calls for a script that ends at once.
std::vector<std::string> StaffRollCalls() {
  std::vector<std::string> calls{
      "display off",     "offset 160 120",  "projection 512", "layout",
      "show 0",          "reset presented", "reset heap",     "prepare",
      "offset 160 112",  "projection 512",  "environment 0",  "environment 1",
      "interlace 1 off", "interlace 0 off"};
  for (int resource = 5; resource < 16; ++resource) {
    calls.push_back("read " + std::to_string(resource));
    calls.emplace_back("end read");
  }
  calls.insert(calls.end(), {"display on", "music 32767", "show 1",
                             "stop music", "display off"});
  for (int resource = 5; resource < 16; ++resource) {
    calls.push_back("free " + std::to_string(resource));
  }
  calls.insert(calls.end(), {"stop music", "update progress", "show results"});
  return calls;
}
}  // namespace

TEST_CASE(
    "RunStaffRoll uploads the art, reads the illustrations and runs "
    "the roll",
    "[ending][ending-results-credits]") {
  FakeCreditsHost host;
  host.script = Script({kOpEnd});
  host.read_polls = 1;
  for (std::int16_t index = 0; index < 12; ++index) {
    host.embedded_arts.push_back(TimBytes({.area = {index, 0, 1, 1}}));
  }
  Credits::State state{.has_finished = true, .elapsed_frames = 9};
  Credits::RunStaffRoll(state, host);
  CHECK(host.calls == StaffRollCalls());
  CHECK(host.loaded_areas.size() == 12);
  // Two clock ticks per illustration read, then the roll's one frame.
  CHECK(host.advanced_frames == 23);
  CHECK((state.elapsed_frames == 1 && state.has_finished));
  CHECK(state.scroll_speed_fixed8 == kScrollSpeed);
  CHECK(state.elements[1].kind == Credits::ElementKind::kBackground);
}

TEST_CASE("RunStaffRoll stops when the embedded art cannot be prepared",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  FakeCreditsHost host;
  host.art_prepared = false;
  Credits::State state;
  Credits::RunStaffRoll(state, host);
  CHECK(host.calls.back() == "prepare");
  CHECK(host.allocations.size() == 4);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Staff roll refused: the embedded art could not be prepared"});
}

TEST_CASE("Promoted staff roll functions leave no module address bindings",
          "[ending][ending-results-credits]") {
  // No module image stores these ENDING.PRG addresses, so their entries are
  // retired with the generated code.
  constexpr auto kAddresses = std::to_array<unsigned>(
      {0x80068938, 0x80068EBC, 0x80069088, 0x8006934C, 0x80069388, 0x80069730,
       0x80069AEC, 0x80069BC0, 0x80069F9C, 0x8006A0D8, 0x8006A3BC, 0x8006A438,
       0x8006A4D8, 0x8006A5C0, 0x8006A860, 0x8006A888, 0x8006A9C0, 0x8006AA6C,
       0x8006AAA0, 0x8006AB44, 0x8006ABBC, 0x8006ABF0, 0x8006AF44, 0x8006AF64,
       0x8006B324, 0x8006B450, 0x8006B6A4, 0x8006B884, 0x8006B930, 0x8006B9B4,
       0x8006B9DC});
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (const unsigned address : kAddresses) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "ENDING/ENDING.PRG",
                                            address) == nullptr);
  }
}
