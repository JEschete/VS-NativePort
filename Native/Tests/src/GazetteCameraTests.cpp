#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Scratch.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/ActorPreview.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Score/GazetteCamera.hpp"
#include "Menu/Score/GazetteCameraLive.hpp"
#include "Menu/Score/Records.hpp"
#include "Menu/Score/ScoreScreen.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/UiPrimitives.hpp"

// Contracts of the gazette's viewer camera, panels and overview (MENU9.PRG
// 238.c:1402-1408, 1474-1509, 1552-1717, 2146-2174).
namespace {
namespace score = VS::Menu::Score;
namespace math = VS::Core::Math;

using score::PanelColor;
using score::PanelCorners;
using score::PanelPoint;

struct LineCall {
  std::int32_t slot;
  score::PanelLine line;
  friend bool operator==(const LineCall&, const LineCall&) = default;
};
struct FlatCall {
  std::int32_t slot;
  score::PanelFlatQuad quad;
  friend bool operator==(const FlatCall&, const FlatCall&) = default;
};
struct ShadedCall {
  std::int32_t slot;
  score::PanelShadedQuad quad;
  friend bool operator==(const ShadedCall&, const ShadedCall&) = default;
};
struct PageCall {
  std::int32_t slot;
  std::uint16_t page;
  friend bool operator==(const PageCall&, const PageCall&) = default;
};
using PanelCall = std::variant<LineCall, FlatCall, ShadedCall, PageCall>;

class RecordingPanelTarget final : public score::PanelTarget {
 public:
  void DrawLine(std::int32_t slot, const score::PanelLine& line) override {
    calls.emplace_back(LineCall{slot, line});
  }
  void DrawFlatQuad(std::int32_t slot,
                    const score::PanelFlatQuad& quad) override {
    calls.emplace_back(FlatCall{slot, quad});
  }
  void DrawShadedQuad(std::int32_t slot,
                      const score::PanelShadedQuad& quad) override {
    calls.emplace_back(ShadedCall{slot, quad});
  }
  void SetTexturePage(std::int32_t slot, std::uint16_t page) override {
    calls.emplace_back(PageCall{slot, page});
  }

  std::vector<PanelCall> calls;
};

constexpr PanelColor kBlack{};
constexpr PanelColor kGray{128, 128, 128};

PanelCorners Corners(std::int16_t x0, std::int16_t y0, std::int16_t x1,
                     std::int16_t y1, std::int16_t x2, std::int16_t y2,
                     std::int16_t x3, std::int16_t y3) {
  return {PanelPoint{x0, y0}, PanelPoint{x1, y1}, PanelPoint{x2, y2},
          PanelPoint{x3, y3}};
}

TEST_CASE("DrawEnemyBanner slants its gradient lines and shadow",
          "[gazette-camera]") {
  RecordingPanelTarget target;
  CHECK(score::DrawEnemyBanner({.x = 10, .y = 20, .width = 30, .height = 4}, 1,
                               target) == score::PanelDrawResult::kDrawn);
  const PanelColor left{25, 130, 108};
  const PanelColor right{1, 40, 38};
  const std::vector<PanelCall> expected{
      LineCall{7, {{10, 20}, {39, 20}, left, right}},
      LineCall{7, {{10, 21}, {38, 21}, left, right}},
      LineCall{7, {{10, 22}, {37, 22}, left, right}},
      FlatCall{7, {Corners(12, 22, 41, 22, 12, 25, 38, 25), kBlack}},
      PageCall{7, 0x60},
  };
  CHECK(target.calls == expected);
}

TEST_CASE("DrawEnemyBanner draws only the shadow for one row or less",
          "[gazette-camera]") {
  const std::int32_t height = GENERATE(1, 0, -5);
  RecordingPanelTarget target;
  CHECK(score::DrawEnemyBanner({.x = 0, .y = 50, .width = 8, .height = height},
                               0, target) == score::PanelDrawResult::kDrawn);
  REQUIRE(target.calls.size() == 2);
  CHECK(std::holds_alternative<FlatCall>(target.calls[0]));
  CHECK(target.calls[1] == PanelCall{PageCall{7, 0x60}});
}

TEST_CASE("DrawEnemyBanner refuses a colour outside the palette",
          "[gazette-camera]") {
  const std::int32_t gradient = GENERATE(-1, 4, INT32_MIN);
  RecordingPanelTarget target;
  CHECK(score::DrawEnemyBanner({.x = 0, .y = 0, .width = 8, .height = 8},
                               gradient,
                               target) == score::PanelDrawResult::kRefused);
  CHECK(target.calls.empty());
}

TEST_CASE("DrawEnemyBanner bounds its line count", "[gazette-camera]") {
  RecordingPanelTarget target;
  CHECK(score::DrawEnemyBanner({.x = 0, .y = 0, .width = 8, .height = 513}, 3,
                               target) == score::PanelDrawResult::kDrawn);
  CHECK(target.calls.size() == 514);
  target.calls.clear();
  CHECK(score::DrawEnemyBanner({.x = 0, .y = 0, .width = 8, .height = 514}, 3,
                               target) == score::PanelDrawResult::kRefused);
  CHECK(target.calls.empty());
}

TEST_CASE("EnemyBannerLineCount wraps the banner end in a word",
          "[gazette-camera]") {
  // y + h - 1 wraps below y: no lines.
  CHECK(score::EnemyBannerLineCount(
            {.x = 0, .y = INT32_MAX - 1, .width = 4, .height = 5}) == 0);
  // INT32_MIN - 1 wraps to INT32_MAX: every word above y, so refused.
  const score::PanelRect wrapped{
      .x = 0, .y = INT32_MIN, .width = 4, .height = 0};
  CHECK(score::EnemyBannerLineCount(wrapped) == 0xFFFFFFFFLL);
  RecordingPanelTarget target;
  CHECK(score::DrawEnemyBanner(wrapped, 0, target) ==
        score::PanelDrawResult::kRefused);
}

TEST_CASE("DrawGradientPanel draws the gradient quad then its shadow",
          "[gazette-camera]") {
  RecordingPanelTarget target;
  CHECK(score::DrawGradientPanel({.x = -8, .y = 16, .width = 136, .height = 12},
                                 3, target) == score::PanelDrawResult::kDrawn);
  const PanelColor left{64, 56, 32};
  const PanelColor right{16, 16, 8};
  const std::vector<PanelCall> expected{
      ShadedCall{7,
                 {Corners(-8, 16, 127, 16, -8, 27, 127, 27),
                  {left, right, left, right},
                  false}},
      FlatCall{7, {Corners(-6, 18, 129, 18, -6, 29, 129, 29), kBlack}},
      PageCall{7, 0x60},
  };
  CHECK(target.calls == expected);
}

TEST_CASE("DrawGradientPanel refuses a colour outside the palette",
          "[gazette-camera]") {
  const std::int32_t gradient = GENERATE(-1, 4, 0x10000);
  RecordingPanelTarget target;
  CHECK(score::DrawGradientPanel({.x = 0, .y = 0, .width = 8, .height = 8},
                                 gradient,
                                 target) == score::PanelDrawResult::kRefused);
  CHECK(target.calls.empty());
}

TEST_CASE("Panel corners keep the low halfword of each coordinate",
          "[gazette-camera]") {
  RecordingPanelTarget target;
  CHECK(score::DrawGradientPanel(
            {.x = 0x12345, .y = -0x10001, .width = 0x10000, .height = 2}, 0,
            target) == score::PanelDrawResult::kDrawn);
  REQUIRE_FALSE(target.calls.empty());
  const auto& quad = std::get<ShadedCall>(target.calls[0]).quad;
  CHECK(quad.corners == Corners(0x2345, -1, 0x2344, -1, 0x2345, 0, 0x2344, 0));
}

TEST_CASE("DrawFooterBackground fades from its top then fills to the bottom",
          "[gazette-camera]") {
  RecordingPanelTarget target;
  score::DrawFooterBackground(200, target);
  const std::vector<PanelCall> expected{
      ShadedCall{7,
                 {Corners(0, 200, 320, 200, 0, 208, 320, 208),
                  {kBlack, kBlack, kGray, kGray},
                  true}},
      FlatCall{7, {Corners(0, 208, 320, 208, 0, 240, 320, 240), kBlack}},
      PageCall{7, 0x40},
  };
  CHECK(target.calls == expected);
}

TEST_CASE("DrawViewerEdgeFade fades both screen edges in its slot",
          "[gazette-camera]") {
  RecordingPanelTarget target;
  score::DrawViewerEdgeFade(target);
  const std::vector<PanelCall> expected{
      ShadedCall{8,
                 {Corners(0, 0, 64, 0, 0, 240, 64, 240),
                  {kGray, kBlack, kGray, kBlack},
                  true}},
      ShadedCall{8,
                 {Corners(256, 0, 320, 0, 256, 240, 320, 240),
                  {kBlack, kGray, kBlack, kGray},
                  true}},
      PageCall{8, 0x40},
  };
  CHECK(target.calls == expected);
}

TEST_CASE("Panel packers lay out the GPU words", "[gazette-camera]") {
  CHECK(score::PackPanelLine({{1, 2}, {-3, 4}, {5, 6, 7}, {8, 9, 10}}) ==
        std::array<std::uint32_t, 4>{0x50070605, 0x00020001, 0x000A0908,
                                     0x0004FFFD});
  CHECK(score::PackPanelFlatQuad({Corners(1, 2, 3, 4, 5, 6, 7, 8), kBlack}) ==
        std::array<std::uint32_t, 5>{0x28000000, 0x00020001, 0x00040003,
                                     0x00060005, 0x00080007});
  const score::PanelShadedQuad shaded{
      Corners(1, 2, 3, 4, 5, 6, 7, 8),
      {PanelColor{1, 2, 3}, PanelColor{4, 5, 6}, PanelColor{7, 8, 9},
       PanelColor{10, 11, 12}},
      GENERATE(false, true)};
  const std::uint32_t code = shaded.semi_transparent ? 0x3A : 0x38;
  CHECK(score::PackPanelShadedQuad(shaded) ==
        std::array<std::uint32_t, 8>{(code << 24) | 0x030201, 0x00020001,
                                     0x060504, 0x00040003, 0x090807, 0x00060005,
                                     0x0C0B0A, 0x00080007});
  CHECK(score::PackPanelTexturePage(0x260) ==
        std::array<std::uint32_t, 2>{0xE1000260, 0});
  CHECK(score::PackPanelTexturePage(0xFE40) ==
        std::array<std::uint32_t, 2>{0xE1000240, 0});
}

TEST_CASE("RestoreCamera copies every word of both backups",
          "[gazette-camera]") {
  const score::CameraPose backup{{1, -2, 3, 0x44}, {5, 6, -7, 0x88}};
  score::CameraPose camera{{9, 9, 9, 9}, {9, 9, 9, 9}};
  score::RestoreCamera(backup, camera);
  CHECK(camera == backup);
}

TEST_CASE("RestoreCamera restores the same pose on every entry",
          "[gazette-camera]") {
  const score::CameraPose backup{{100, 200, 300, 0}, {10, 20, 30, 0}};
  score::CameraPose camera{};
  score::RestoreCamera(backup, camera);
  camera.position[0] = -1;
  camera.angles[2] = 4095;
  score::RestoreCamera(backup, camera);
  score::RestoreCamera(backup, camera);
  CHECK(camera == backup);
  CHECK(backup == score::CameraPose{{100, 200, 300, 0}, {10, 20, 30, 0}});
}

TEST_CASE("BuildViewerCamera puts the eye behind the origin at zero angles",
          "[gazette-camera]") {
  const score::ViewerCamera camera = score::BuildViewerCamera(
      {.pitch = 0, .yaw = 0, .roll = 7, .distance = 1000, .height_offset = 0});
  CHECK(camera.position == math::Vec3{0, 0, -1000 * 4096});
  CHECK(camera.angles == math::Vec3{0, 0, 7});
  CHECK(camera.view.rotation == math::IdentityRotation());
  CHECK(camera.view.translation == math::Vec3{0, 0, 1000});
}

TEST_CASE("BuildViewerCamera lowers the eye by half the height offset",
          "[gazette-camera]") {
  const auto [offset, eye_y] =
      GENERATE(std::pair{6, -3}, std::pair{-3, 1}, std::pair{3, -1});
  const score::ViewerCamera camera = score::BuildViewerCamera(
      {.pitch = 0,
       .yaw = 0,
       .roll = 0,
       .distance = 0,
       .height_offset = static_cast<std::int16_t>(offset)});
  CHECK(camera.position.y == eye_y * 4096);
  CHECK(camera.view.translation.y == -eye_y);
}

TEST_CASE("BuildViewerCamera truncates each Q12 product toward zero",
          "[gazette-camera]") {
  REQUIRE(math::Sine(512) == 2896);
  REQUIRE(math::Cosine(512) == 2896);
  // sin(45 degrees) * -2 / 4096 is -1.41: -1, not the floor's -2.
  const score::ViewerCamera camera = score::BuildViewerCamera(
      {.pitch = 0, .yaw = 512, .roll = 0, .distance = -2, .height_offset = 0});
  CHECK(camera.position == math::Vec3{-4096, 0, 4096});
  CHECK(camera.angles == math::Vec3{0, 512, 0});
}

TEST_CASE("BuildViewerCamera keeps the low halfword of each eye component",
          "[gazette-camera]") {
  const score::ViewerCamera camera =
      score::BuildViewerCamera({.pitch = 0,
                                .yaw = 0,
                                .roll = 0,
                                .distance = 0x10005,
                                .height_offset = 0});
  // -0x10005 keeps 0xFFFB: -5.
  CHECK(camera.position.z == -5 * 4096);
  CHECK(camera.view.translation.z == 5);
}

TEST_CASE("BuildViewerCamera keeps negative angles and the minimum halfword",
          "[gazette-camera]") {
  const score::ViewerCamera camera =
      score::BuildViewerCamera({.pitch = -1024,
                                .yaw = INT16_MIN,
                                .roll = -1,
                                .distance = 0x8000,
                                .height_offset = 0});
  CHECK(camera.angles == math::Vec3{-1024, INT16_MIN, -1});
  // sin(-90 degrees) * -0x8000 / 4096 = 0x8000, whose halfword is -0x8000.
  CHECK(camera.position.y == -0x8000 * 4096);
  CHECK(camera.view.rotation ==
        math::MultiplyMatrixIntoRight(
            VS::Menu::Framework::BuildPitchMatrix(-1024),
            VS::Menu::Framework::BuildYawMatrix(INT16_MIN)));
}

TEST_CASE("OverviewScoreText prints the score as a signed word",
          "[gazette-camera]") {
  CHECK(score::OverviewScoreText(0) == "#0");
  CHECK(score::OverviewScoreText(123456) == "#123456");
  CHECK(score::OverviewScoreText(0xFFFFFFFF) == "#-1");
  CHECK(score::OverviewScoreText(0x80000000) == "#-2147483648");
}

class RecordingOverviewHost final : public score::OverviewHost {
 public:
  void DrawViewerEdgeFade() override { events.emplace_back("fade"); }
  score::ProjectionOffset GetProjectionOffset() override {
    events.emplace_back("get");
    return saved;
  }
  void SetProjectionOffset(const score::ProjectionOffset& offset) override {
    events.emplace_back("set " + std::to_string(offset.x) + " " +
                        std::to_string(offset.y));
  }
  math::Transform3 BuildViewerCamera() override {
    events.emplace_back("camera");
    return camera;
  }
  void DrawActorPreview(const math::Transform3& view) override {
    events.emplace_back(view == camera ? "preview camera" : "preview other");
  }
  void DrawText(std::string_view text, std::uint32_t position,
                std::uint32_t color) override {
    events.emplace_back(
        "text " + std::string(text) + " " + std::to_string(position & 0xFFFF) +
        " " + std::to_string(position >> 16) + " " + std::to_string(color));
  }
  void DrawGradientPanel(const score::PanelRect& rect,
                         std::int32_t gradient) override {
    events.emplace_back(
        "panel " + std::to_string(rect.x) + " " + std::to_string(rect.y) + " " +
        std::to_string(rect.width) + " " + std::to_string(rect.height) + " " +
        std::to_string(gradient));
  }
  void DrawFooterBackground(std::int32_t top) override {
    events.emplace_back("footer " + std::to_string(top));
  }

  score::ProjectionOffset saved{.x = 160, .y = 112};
  math::Transform3 camera{.rotation = math::IdentityRotation(),
                          .translation = {1, 2, 3}};
  std::vector<std::string> events;
};

TEST_CASE("RenderOverview fades the edges only before the last step",
          "[gazette-camera]") {
  const auto [step, faded] = GENERATE(std::pair{7, true}, std::pair{-1, true},
                                      std::pair{8, false}, std::pair{9, false});
  RecordingOverviewHost host;
  score::RenderOverview(host, step, 0);
  REQUIRE_FALSE(host.events.empty());
  CHECK((host.events.front() == "fade") == faded);
  CHECK(std::ranges::count(host.events, std::string("fade")) ==
        (faded ? 1 : 0));
}

TEST_CASE("RenderOverview restores the projection after the preview",
          "[gazette-camera]") {
  RecordingOverviewHost host;
  score::RenderOverview(host, 8, 0);
  const std::vector<std::string> head(host.events.begin(),
                                      host.events.begin() + 5);
  CHECK(head == std::vector<std::string>{"get", "set 128 240", "camera",
                                         "preview camera", "set 160 112"});
  host.events.clear();
  score::RenderOverview(host, 0, 0);
  CHECK(host.events[2] == "set -128 240");
  CHECK(host.events[5] == "set 160 112");
}

TEST_CASE("RenderOverview slides the score text, panels and footer",
          "[gazette-camera]") {
  RecordingOverviewHost host;
  score::RenderOverview(host, 3, 0xFFFFFF85);
  const std::vector<std::string> tail(host.events.begin() + 6,
                                      host.events.end());
  CHECK(tail == std::vector<std::string>{
                    "text SCORE 65464 16 8421504",
                    "text #-123 48 16 8421504",
                    "panel -80 16 136 12 2",
                    "text RISK   BREAKER   RANK 65464 184 8421504",
                    "panel -80 184 136 12 1",
                    "footer 218",
                });
}

TEST_CASE("RenderOverview wraps its slide arithmetic in a word",
          "[gazette-camera]") {
  RecordingOverviewHost host;
  score::RenderOverview(host, 0x40000000, 0);
  // (8 - step) << 5 and step * 16 wrap; step * 6 wraps to 0x80000000.
  CHECK(host.events[1] == "set -128 240");
  CHECK(host.events[5] == "text SCORE 65416 16 8421504");
  CHECK(host.events[7] == "panel -128 16 136 12 2");
  CHECK(host.events.back() == "footer -2147483412");
}

// The copied MENU9.PRG bodies (generated ScoreScreen.cpp at c8f3e2b4) are
// retired. Each parity case runs the Live entry over the inputs a copied body
// ran and compares the frame's digest with the one the copied body produced,
// recorded while it still ran (#276). Links are located before hashing, so the
// digests do not depend on the order the process issued handles in.

// FNV-1a 64 over each word's bytes, low byte first, with every sequence's
// length mixed in ahead of its contents.
class Digest {
 public:
  void AddWord(std::uint32_t word) {
    for (unsigned shift = 0; shift < kWordBits; shift += kByteBits) {
      hash_ = (hash_ ^ ((word >> shift) & kByteMask)) * kPrime;
    }
  }
  void AddWide(std::int64_t value) {
    const auto bits = static_cast<std::uint64_t>(value);
    AddWord(static_cast<std::uint32_t>(bits));
    AddWord(static_cast<std::uint32_t>(bits >> kWordBits));
  }
  void AddWords(const std::vector<u_long>& words) {
    AddWord(static_cast<std::uint32_t>(words.size()));
    for (const u_long word : words) {
      AddWord(static_cast<std::uint32_t>(word));
    }
  }
  void AddTransform(const math::Transform3& transform) {
    for (const auto& row : transform.rotation.m) {
      for (const std::int16_t value : row) {
        AddWord(static_cast<std::uint16_t>(value));
      }
    }
    AddWord(static_cast<std::uint32_t>(transform.translation.x));
    AddWord(static_cast<std::uint32_t>(transform.translation.y));
    AddWord(static_cast<std::uint32_t>(transform.translation.z));
  }
  [[nodiscard]] std::uint64_t Value() const { return hash_; }

 private:
  static constexpr unsigned kWordBits = 32;
  static constexpr unsigned kByteBits = 8;
  static constexpr std::uint64_t kByteMask = 0xFF;
  static constexpr std::uint64_t kOffsetBasis = 0xCBF29CE484222325ULL;
  static constexpr std::uint64_t kPrime = 0x100000001B3ULL;
  std::uint64_t hash_ = kOffsetBasis;
};

// The frame's primitive buffer and ordering table, the score screen's camera,
// the projection offset and the compact font's glyph tables, saved and
// restored. The glyph tables come from the module image, which earlier tests
// may or may not have loaded, so the fixture pins them: no left trim, every
// glyph 6 pixels wide, and each character in its own cell.
struct LiveFrameFixture {
  static constexpr std::size_t kStorageWords = (0x20000 / sizeof(u_long)) + 2;
  static constexpr std::size_t kOrderingEntries = 0x2088 / sizeof(u_long);
  static constexpr std::size_t kSceneEntry = 16;
  static constexpr u_long kFill = 0xDEADBEEF;
  static constexpr u_char kGlyphWidths = 0x66;

  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_table = VS::Render::legacyOrderingTables[0];
  int saved_framebuffer = VS::Render::frameBuf;
  SVECTOR saved_angles = score::cameraAngles;
  int saved_distance = score::cameraDistance;
  std::int16_t saved_height = score::cameraHeightOffset;
  VECTOR saved_position_backup = score::cameraPositionBackup;
  VECTOR saved_angles_backup = score::cameraAnglesBackup;
  std::uint32_t saved_score = score::score;
  VS::Render::ScreenOffset saved_offset =
      VS::Render::GetLiveProjectionScreenOffset();
  VS::Actors::ActorRecord* saved_preview =
      VS::Actors::actorRecords[VS::Menu::Framework::kPreviewActorSlot];
  decltype(std::to_array(VS::Render::compactGlyphLeftTrim)) saved_trim =
      std::to_array(VS::Render::compactGlyphLeftTrim);
  decltype(std::to_array(VS::Render::compactGlyphWidths)) saved_widths =
      std::to_array(VS::Render::compactGlyphWidths);
  decltype(std::to_array(VS::Render::compactGlyphIndices)) saved_indices =
      std::to_array(VS::Render::compactGlyphIndices);
  std::vector<u_long> storage = std::vector<u_long>(kStorageWords, kFill);
  std::vector<u_long> ordering = std::vector<u_long>(kOrderingEntries);

  LiveFrameFixture() {
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data() + 1;
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Actors::actorRecords[VS::Menu::Framework::kPreviewActorSlot] = nullptr;
    std::ranges::fill(VS::Render::compactGlyphLeftTrim, u_char{0});
    std::ranges::fill(VS::Render::compactGlyphWidths, kGlyphWidths);
    for (std::size_t cell = 0; cell < saved_indices.size(); ++cell) {
      VS::Render::compactGlyphIndices[cell] = static_cast<u_char>(cell);
    }
    Reset();
  }
  ~LiveFrameFixture() {
    std::ranges::copy(saved_trim, VS::Render::compactGlyphLeftTrim);
    std::ranges::copy(saved_widths, VS::Render::compactGlyphWidths);
    std::ranges::copy(saved_indices, VS::Render::compactGlyphIndices);
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_table;
    VS::Render::frameBuf = saved_framebuffer;
    score::cameraAngles = saved_angles;
    score::cameraDistance = saved_distance;
    score::cameraHeightOffset = saved_height;
    score::cameraPositionBackup = saved_position_backup;
    score::cameraAnglesBackup = saved_angles_backup;
    score::score = saved_score;
    VS::Render::SetLiveProjectionScreenOffset(saved_offset);
    VS::Actors::actorRecords[VS::Menu::Framework::kPreviewActorSlot] =
        saved_preview;
  }
  LiveFrameFixture(const LiveFrameFixture&) = delete;
  LiveFrameFixture& operator=(const LiveFrameFixture&) = delete;

  void Reset() {
    std::ranges::fill(storage, kFill);
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
    VS::Render::FrameScratchpad().ordering_table =
        ordering.data() + kSceneEntry;
    VS::Render::FrameScratchpad().camera = VS::Render::Camera{};
  }

  [[nodiscard]] static bool In(const std::vector<u_long>& words,
                               const void* address) {
    const auto at = reinterpret_cast<std::uintptr_t>(address);
    const auto begin = reinterpret_cast<std::uintptr_t>(words.data());
    return at >= begin && at < begin + (words.size() * sizeof(u_long));
  }

  [[nodiscard]] bool InStorage(const void* packet) const {
    return In(storage, packet);
  }

  // A link word with its handle, which depends on the order the process
  // issued handles in, replaced by the buffer and index of the word it
  // reaches; the end marker is kept.
  [[nodiscard]] std::uint32_t Located(u_long word) const {
    constexpr std::uint32_t kLinkMask = 0xFFFFFF;
    constexpr std::uint32_t kStorageLink = 0x800000;
    constexpr std::uint32_t kOrderingLink = 0x400000;
    const auto value = static_cast<std::uint32_t>(word);
    const auto* target = static_cast<const u_long*>(LegacyGpu_Pointer(word));
    std::uint32_t link = value & kLinkMask;
    if (In(storage, target)) {
      link = kStorageLink | static_cast<std::uint32_t>(target - storage.data());
    } else if (In(ordering, target)) {
      link =
          kOrderingLink | static_cast<std::uint32_t>(target - ordering.data());
    }
    return (value & ~kLinkMask) | link;
  }

  struct Frame {
    std::vector<u_long> ordering;
    std::vector<u_long> storage;
    std::ptrdiff_t cursor = 0;
    std::array<std::int64_t, 8> camera{};
  };

  // The frame, with every link located and the colour words' padding bytes,
  // which the copied body left unwritten and the binding zeroes, masked.
  [[nodiscard]] Frame Capture() const {
    Frame frame{.ordering = ordering, .storage = storage};
    for (u_long& entry : frame.ordering) {
      entry = Located(entry);
    }
    for (const u_long& head : ordering) {
      const void* packet = nextPrim(&head);
      for (int guard = 0; InStorage(packet) && guard < 4096; ++guard) {
        const auto* words = static_cast<const u_long*>(packet);
        const auto index = static_cast<std::size_t>(words - storage.data());
        frame.storage[index] = Located(words[0]);
        const auto length = static_cast<std::uint32_t>(words[0]) >> 24;
        for (std::size_t word = 3; word < length; word += 2) {
          if (length == 4 || length == 8) {
            frame.storage[index + word] &= 0x00FFFFFF;
          }
        }
        packet = nextPrim(packet);
      }
    }
    frame.cursor = static_cast<const u_long*>(
                       VS::Render::FrameScratchpad().primitive_cursor) -
                   storage.data();
    const VS::Render::Camera& camera = VS::Render::FrameScratchpad().camera;
    frame.camera = {camera.position.vx,  camera.position.vy, camera.position.vz,
                    camera.position.pad, camera.angles.vx,   camera.angles.vy,
                    camera.angles.vz,    camera.angles.pad};
    return frame;
  }

  // A frame's digest and, for the viewer camera, its matrix's.
  [[nodiscard]] static std::uint64_t DigestOf(const Frame& frame,
                                              const MATRIX* matrix) {
    Digest digest;
    digest.AddWide(frame.cursor);
    digest.AddWords(frame.ordering);
    for (const std::int64_t value : frame.camera) {
      digest.AddWide(value);
    }
    digest.AddWords(frame.storage);
    if (matrix != nullptr) {
      digest.AddTransform(math::TransformOf(*matrix));
    }
    return digest.Value();
  }

  // Runs the binding from a fresh frame and checks the frame, and the viewer
  // camera's matrix, against the copied body's digest over the same inputs.
  template <typename Bound>
  void CheckFrame(std::uint64_t copied, Bound bound,
                  const MATRIX* matrix = nullptr) {
    Reset();
    bound();
    CHECK(DigestOf(Capture(), matrix) == copied);
    CHECK(storage.front() == kFill);
    CHECK(storage.back() == kFill);
  }

  // Whether a packet is linked in front of scene entry `slot`.
  [[nodiscard]] bool Linked(std::size_t slot) const {
    return InStorage(nextPrim(&ordering[kSceneEntry + slot]));
  }
};

TEST_CASE("DrawLiveEnemyBanner matches the copied body",
          "[gazette-camera-live]") {
  LiveFrameFixture fixture;
  const std::uint64_t refused = score::LiveGazetteCameraRefusals();
  using BannerCase = std::tuple<int, int, std::uint64_t>;
  const auto [height, gradient, copied] =
      GENERATE(BannerCase{12, 3, 0x1DE44B15E0ABDCA0ULL},
               BannerCase{1, 0, 0x7EB842C23A1E2C36ULL},
               BannerCase{40, 2, 0xBE8A2DAE8A7D3C32ULL});
  fixture.CheckFrame(
      copied, [&] { score::DrawLiveEnemyBanner(8, 18, 96, height, gradient); });
  CHECK(fixture.Linked(7));
  CHECK(score::LiveGazetteCameraRefusals() == refused);
}

TEST_CASE("DrawLiveGradientPanel matches the copied body",
          "[gazette-camera-live]") {
  LiveFrameFixture fixture;
  const auto [gradient, copied] = GENERATE(
      std::pair{0, 0x987734BA478BEE0CULL}, std::pair{1, 0xBBDC4F67D5ABAA7CULL},
      std::pair{2, 0xEB8A287CE2C92808ULL}, std::pair{3, 0xA3F0E4C967B4DC94ULL});
  fixture.CheckFrame(copied, [&] {
    score::DrawLiveGradientPanel(-40, 184, 136, 12, gradient);
  });
}

TEST_CASE("DrawLiveFooterBackground matches the copied body",
          "[gazette-camera-live]") {
  LiveFrameFixture fixture;
  const auto [top, copied] = GENERATE(std::pair{236, 0xC4F977E444AE866EULL},
                                      std::pair{188, 0x26EBD90B52974B6EULL},
                                      std::pair{-4, 0x8503267967FA271EULL});
  fixture.CheckFrame(copied, [&] { score::DrawLiveFooterBackground(top); });
}

TEST_CASE("DrawLiveViewerEdgeFade matches the copied body",
          "[gazette-camera-live]") {
  LiveFrameFixture fixture;
  fixture.CheckFrame(0xE83CF8C6B5C55853ULL,
                     [] { score::DrawLiveViewerEdgeFade(); });
}

TEST_CASE("Live panels refuse bad colours, tall banners and missing entries",
          "[gazette-camera-live]") {
  LiveFrameFixture fixture;
  const std::uint64_t refused = score::LiveGazetteCameraRefusals();
  score::DrawLiveEnemyBanner(0, 0, 8, 8, 4);
  score::DrawLiveEnemyBanner(0, 0, 8, 600, 0);
  score::DrawLiveGradientPanel(0, 0, 8, 8, -1);
  // An ordering table outside the frame's: every entry is refused.
  VS::Render::FrameScratchpad().ordering_table = fixture.storage.data();
  score::DrawLiveGradientPanel(0, 0, 8, 8, 0);
  score::DrawLiveFooterBackground(0);
  score::DrawLiveViewerEdgeFade();
  CHECK(score::LiveGazetteCameraRefusals() == refused + 6);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.storage.data() + 1);
  CHECK(std::ranges::all_of(fixture.storage, [](u_long word) {
    return word == LiveFrameFixture::kFill;
  }));
}

TEST_CASE("Live panels refuse a call the primitive buffer cannot hold",
          "[gazette-camera-live]") {
  LiveFrameFixture fixture;
  const std::uint64_t refused = score::LiveGazetteCameraRefusals();
  // Leaves room for less than a gradient panel's 18 words.
  VS::Render::FrameScratchpad().primitive_cursor =
      fixture.storage.data() + fixture.storage.size() - 12;
  score::DrawLiveGradientPanel(0, 0, 8, 8, 0);
  CHECK(score::LiveGazetteCameraRefusals() == refused + 1);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.storage.data() + fixture.storage.size() - 12);
}

void SetCameraSettings(std::int16_t pitch, std::int16_t yaw, int distance,
                       std::int16_t height) {
  score::cameraAngles = SVECTOR{pitch, yaw, 33, 0};
  score::cameraDistance = distance;
  score::cameraHeightOffset = height;
}

TEST_CASE("BuildLiveViewerCamera matches the copied body",
          "[gazette-camera-live]") {
  LiveFrameFixture fixture;
  using CameraCase =
      std::tuple<std::int16_t, std::int16_t, int, std::int16_t, std::uint64_t>;
  const auto [pitch, yaw, distance, height, copied] =
      GENERATE(CameraCase{-200, 1300, 0x900, 0x180, 0xAC38D94474FA65C2ULL},
               CameraCase{512, -3000, -0x12345, -7, 0x9E9D52409AA6EDD7ULL},
               CameraCase{INT16_MIN, INT16_MAX, 0x7FFFFFFF, INT16_MIN,
                          0xF2EAD2265D03B1A3ULL});
  SetCameraSettings(pitch, yaw, distance, height);
  MATRIX bound{};
  fixture.CheckFrame(
      copied, [&] { score::BuildLiveViewerCamera(&bound); }, &bound);
}

TEST_CASE("BuildLiveViewerCamera refuses a null matrix before any write",
          "[gazette-camera-live]") {
  LiveFrameFixture fixture;
  SetCameraSettings(100, 200, 0x800, 0);
  const std::uint64_t refused = score::LiveGazetteCameraRefusals();
  score::BuildLiveViewerCamera(nullptr);
  CHECK(score::LiveGazetteCameraRefusals() == refused + 1);
  CHECK(fixture.Capture().camera == std::array<std::int64_t, 8>{});
}

TEST_CASE("RestoreLiveCamera matches the copied body on every entry",
          "[gazette-camera-live]") {
  LiveFrameFixture fixture;
  score::cameraPositionBackup = VECTOR{-0x100000, 0x2000, 0x7000, 0x55};
  score::cameraAnglesBackup = VECTOR{-12, 3000, 4, 0x66};
  const auto restore_twice = [](auto restore) {
    return [restore] {
      restore();
      VS::Render::FrameScratchpad().camera.position.vx = 1;
      VS::Render::FrameScratchpad().camera.angles.pad = 2;
      restore();
    };
  };
  fixture.CheckFrame(0x86FD88A372514171ULL,
                     restore_twice([] { score::RestoreLiveCamera(); }));
  CHECK(fixture.Capture().camera ==
        std::array<std::int64_t, 8>{-0x100000, 0x2000, 0x7000, 0x55, -12, 3000,
                                    4, 0x66});
  CHECK(score::cameraPositionBackup.vx == -0x100000);
}

TEST_CASE("RenderLiveOverview matches the copied body and restores the offset",
          "[gazette-camera-live]") {
  LiveFrameFixture fixture;
  const auto [step, copied] = GENERATE(std::pair{0, 0xA0C64B9465EF0A0DULL},
                                       std::pair{5, 0xE998DC57FBE1D6B8ULL},
                                       std::pair{8, 0x3202E8A764875E52ULL},
                                       std::pair{12, 0xCC61E3F789251562ULL});
  SetCameraSettings(-300, 900, 0xA00, 0x40);
  score::score = 0xFFFFFF00;
  const VS::Render::ScreenOffset offset{.x = 160, .y = 112};
  fixture.CheckFrame(copied, [&offset, step] {
    VS::Render::SetLiveProjectionScreenOffset(offset);
    score::RenderLiveOverview(step);
  });
  const VS::Render::ScreenOffset restored =
      VS::Render::GetLiveProjectionScreenOffset();
  CHECK(restored.x == offset.x);
  CHECK(restored.y == offset.y);
  CHECK(fixture.Linked(7));
  CHECK(fixture.Linked(8) == (step < 8));
}
}  // namespace
