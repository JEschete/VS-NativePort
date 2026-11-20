#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/ScreenDistortion/ScreenOverlays.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Render.hpp"
#include "Render/UiPackets.hpp"

namespace {
namespace sd = VS::Effects::ScreenDistortion;

// Records packets; each linked packet gets handle `base` + its index.
class RecordingQueue final : public VS::Render::PacketQueue {
 public:
  RecordingQueue(std::uint32_t head, std::uint32_t base)
      : head_(head), base_(base) {}

  [[nodiscard]] std::uint32_t ChainLink() const override { return head_; }
  bool Emit(std::span<const std::uint32_t> packet) override {
    packets_.emplace_back(packet.begin(), packet.end());
    head_ = base_ + static_cast<std::uint32_t>(packets_.size() - 1);
    return true;
  }

  [[nodiscard]] const std::vector<std::vector<std::uint32_t>>& Packets() const {
    return packets_;
  }
  [[nodiscard]] std::uint32_t Handle(std::size_t index) const {
    return base_ + static_cast<std::uint32_t>(index);
  }

 private:
  std::uint32_t head_;
  std::uint32_t base_;
  std::vector<std::vector<std::uint32_t>> packets_;
};

class RecordingPages final : public sd::FeedbackTexturePages {
 public:
  void Reserve(const sd::TexturePageLease& lease) override {
    Record(lease);
    ++reserved;
  }
  void Retire(const sd::TexturePageLease& lease) override {
    Record(lease);
    ++retired;
  }

  int reserved = 0;
  int retired = 0;
  bool feedback_lease = true;

 private:
  void Record(const sd::TexturePageLease& lease) {
    feedback_lease = feedback_lease && lease.first_page == 0x1B &&
                     lease.page_count == 5 && lease.owner_tag == 6;
  }
};

constexpr std::uint32_t kHead = 0xABCDEF;
constexpr std::uint32_t kSceneBase = 0x10000;
constexpr std::uint32_t kFeedbackBase = 0x80000;

std::uint32_t Tag(std::uint32_t link, std::uint32_t words) {
  return (link & 0xFFFFFF) | ((words - 1) << 24);
}

struct OverlayController {
  VS::Effects::LiveScreenEffectOverlay overlay{};
  VS::Effects::LiveScreenEffectOverlay* saved =
      VS::Effects::screenEffectOverlay;

  OverlayController() { VS::Effects::screenEffectOverlay = &overlay; }
  ~OverlayController() { VS::Effects::screenEffectOverlay = saved; }
  OverlayController(const OverlayController&) = delete;
  OverlayController& operator=(const OverlayController&) = delete;
  OverlayController(OverlayController&&) = delete;
  OverlayController& operator=(OverlayController&&) = delete;
};
}  // namespace

TEST_CASE("Feedback layer lends pages on the first nonzero word",
          "[screen-distortion]") {
  sd::ScreenOverlayControls controls{};
  RecordingPages pages;

  sd::SetFeedbackLayer(controls, 0x10005, pages);
  CHECK(controls.feedback_layer == 5);
  CHECK(controls.feedback_pages_reserved == 1);
  CHECK(pages.reserved == 1);

  sd::SetFeedbackLayer(controls, 7, pages);
  CHECK(controls.feedback_layer == 7);
  CHECK(pages.reserved == 1);

  // A word whose low halfword is zero stores layer 0 but keeps the pages.
  sd::SetFeedbackLayer(controls, 0x10000, pages);
  CHECK(controls.feedback_layer == 0);
  CHECK(controls.feedback_pages_reserved == 1);
  CHECK(pages.retired == 0);
}

TEST_CASE("Feedback layer zero retires any nonzero reservation byte",
          "[screen-distortion]") {
  sd::ScreenOverlayControls controls{.feedback_pages_reserved = 2,
                                     .feedback_layer = 9};
  RecordingPages pages;

  sd::SetFeedbackLayer(controls, 4, pages);
  CHECK(controls.feedback_pages_reserved == 2);
  CHECK(pages.reserved == 0);

  sd::SetFeedbackLayer(controls, 0, pages);
  CHECK(controls.feedback_pages_reserved == 0);
  CHECK(controls.feedback_layer == 0);
  CHECK(pages.retired == 1);
  sd::SetFeedbackLayer(controls, 0, pages);
  CHECK(pages.retired == 1);
  CHECK(pages.reserved == 0);
  CHECK(pages.feedback_lease);
}

TEST_CASE("Radial warp and vignette setters store only their words",
          "[screen-distortion]") {
  const sd::RadialWarpSettings warp = sd::RadialWarpFromWords(0x12345, 0x18000);
  CHECK(warp.scale == 0x2345);
  CHECK(warp.growth == -0x8000);

  sd::ScreenOverlayControls controls{.feedback_pages_reserved = 1,
                                     .feedback_layer = 0x33,
                                     .vignette_enabled = 0};
  sd::SetRadialWarp(controls, warp);
  CHECK(controls.radial_warp.scale == 0x2345);
  CHECK(controls.radial_warp.growth == -0x8000);

  sd::SetVignetteEnabled(controls, 0x12340000);
  CHECK(controls.vignette_enabled == 0x12340000);
  CHECK(controls.feedback_layer == 0x33);
  CHECK(controls.feedback_pages_reserved == 1);
  CHECK(controls.radial_warp.scale == 0x2345);
}

TEST_CASE("Feedback ordering slot is the layer less three",
          "[screen-distortion]") {
  CHECK(sd::FeedbackOrderingSlot(0) == -3);
  CHECK(sd::FeedbackOrderingSlot(3) == 0);
  CHECK(sd::FeedbackOrderingSlot(0xFFFF) == 0xFFFC);
}

TEST_CASE("Ellipse points widen x and pack y in the high halfword",
          "[screen-distortion]") {
  CHECK(sd::EllipsePoint(0, 0x80) == 0x00700150);
  CHECK(sd::EllipsePoint(16, 0x80) == 0x00F000A0);
  // Negative x keeps only its low halfword.
  CHECK(sd::EllipsePoint(32, 0x80) == 0x0070FFF0);
  CHECK(sd::EllipsePoint(48, 0x60) == 0x001000A0);
  CHECK(sd::EllipsePoint(64, 0) == 0x007000A0);
}

TEST_CASE("Vignette corner fans four triangles toward the ellipse",
          "[screen-distortion]") {
  RecordingQueue queue(kHead, kSceneBase);
  sd::AppendVignetteCorner(0x00E00140, 6, queue);

  const auto& packets = queue.Packets();
  REQUIRE(packets.size() == 4);
  CHECK(packets[0] ==
        std::vector<std::uint32_t>{Tag(kHead, 7), 0x32FFFFFF, 0x00E00140,
                                   0xA0A0A0, sd::EllipsePoint(7, 0x80),
                                   0xA0A0A0, sd::EllipsePoint(6, 0x80)});
  CHECK(packets[1][0] == Tag(queue.Handle(0), 7));
  CHECK(packets[3][4] == sd::EllipsePoint(10, 0x80));
  CHECK(packets[3][6] == sd::EllipsePoint(9, 0x80));
  CHECK(queue.ChainLink() == queue.Handle(3));
}

TEST_CASE("Vignette corner tests its bound after a wrapping end",
          "[screen-distortion]") {
  RecordingQueue queue(kHead, kSceneBase);
  sd::AppendVignetteCorner(0, INT32_MAX - 1, queue);
  REQUIRE(queue.Packets().size() == 1);
  CHECK(queue.Packets()[0][4] == sd::EllipsePoint(INT32_MAX, 0x80));
}

TEST_CASE("Vignette links corners then rim quads in front of the overlay",
          "[screen-distortion]") {
  RecordingQueue queue(kHead, kSceneBase);
  sd::DrawVignette(queue);

  const auto& packets = queue.Packets();
  REQUIRE(packets.size() == 80);
  CHECK(packets[0][0] == Tag(kHead, 7));
  CHECK(packets[4][2] == 0x00E00000);
  CHECK(packets[4][6] == sd::EllipsePoint(0x16, 0x80));
  CHECK(packets[12][2] == 0x140);
  CHECK(packets[15][4] == sd::EllipsePoint(0x3A, 0x80));
  CHECK(packets[16] ==
        std::vector<std::uint32_t>{
            Tag(queue.Handle(15), 10), 0xE1000220, 0x3AA0A0A0,
            sd::EllipsePoint(1, 0x80), 0xA0A0A0, sd::EllipsePoint(0, 0x80), 0,
            sd::EllipsePoint(1, 0x60), 0, sd::EllipsePoint(0, 0x60)});
  CHECK(packets[79][3] == sd::EllipsePoint(64, 0x80));
  CHECK(packets[79][9] == sd::EllipsePoint(63, 0x60));
}

TEST_CASE("Feedback tiles queue edge strips then three tile passes",
          "[screen-distortion]") {
  RecordingQueue scene(kHead, kSceneBase);
  RecordingQueue feedback(0x123456, kFeedbackBase);
  sd::DrawFeedbackTiles(0, scene, feedback);

  const auto& sprites = scene.Packets();
  REQUIRE(sprites.size() == 316);
  CHECK(sprites[0] == std::vector<std::uint32_t>{Tag(kHead, 6), 0xE100037F,
                                                 0x66606060, 0x00DE0102, 0xDC00,
                                                 0x00040040});
  CHECK(sprites[1] ==
        std::vector<std::uint32_t>{Tag(scene.Handle(0), 6), 0xE100037F,
                                   0x66606060, 0x00C2013E, 0xC03C, 0x001C0004});
  CHECK(sprites[105] ==
        std::vector<std::uint32_t>{Tag(scene.Handle(104), 6), 0xE100037F,
                                   0x66606060, 0x00C20122, 0xC020, 0x001C001C});
  CHECK(sprites[106][1] == 0xE100031F);
  CHECK(sprites[106][3] == 0x00BE011E);
  CHECK(sprites[107][2] == 0x64606060);
  CHECK(sprites[313][3] == 0xFFFEFFFE);
  CHECK(sprites[314] == std::vector<std::uint32_t>{Tag(scene.Handle(313), 6),
                                                   0xE100031B, 0x64606060, 0, 0,
                                                   0x00200020});
}

TEST_CASE("Feedback capture and restore packets follow the frame buffer",
          "[screen-distortion]") {
  RecordingQueue scene(kHead, kSceneBase);
  RecordingQueue feedback(0x89123456, kFeedbackBase);
  sd::DrawFeedbackTiles(0, scene, feedback);

  REQUIRE(feedback.Packets().size() == 1);
  CHECK(feedback.Packets()[0] ==
        std::vector<std::uint32_t>{0x07123456, 0xE30402C0, 0xE4077FFF,
                                   0xE50802C0, 0x60000000, 0, 0x00E00140,
                                   0xE6000001});
  CHECK(scene.Packets().back() ==
        std::vector<std::uint32_t>{Tag(scene.Handle(314), 5), 0xE3000140,
                                   0xE4037E7F, 0xE5000140, 0xE6000000});

  RecordingQueue second(kHead, kSceneBase);
  sd::DrawFeedbackTiles(1, second, second);
  REQUIRE(second.Packets().size() == 317);
  CHECK(second.Packets()[315][0] == Tag(second.Handle(314), 8));
  CHECK(second.Packets().back() ==
        std::vector<std::uint32_t>{Tag(second.Handle(315), 5), 0xE3000000,
                                   0xE4037D3F, 0xE5000000, 0xE6000000});
}

TEST_CASE("Live screen overlay setters write the controller",
          "[screen-distortion]") {
  OverlayController controller;
  controller.overlay.feedback_layer = 0x44;

  sd::SetLiveRadialWarpWords(0x11000, -2);
  CHECK(controller.overlay.radial_warp_scale == 0x1000);
  CHECK(controller.overlay.radial_warp_growth == -2);
  sd::SetLiveRadialWarp({.scale = 0x8000, .growth = 3});
  CHECK(controller.overlay.radial_warp_scale == -0x8000);
  CHECK(controller.overlay.radial_warp_growth == 3);

  sd::SetLiveVignetteEnabled(5);
  CHECK(controller.overlay.vignette_enabled == 5);
  // Layer zero with no pages lent touches no texture page.
  sd::SetLiveFeedbackLayer(0);
  CHECK(controller.overlay.feedback_layer == 0);
  CHECK(controller.overlay.feedback_pages_reserved == 0);
}

TEST_CASE("Live screen overlays refuse a missing controller or cursor",
          "[screen-distortion]") {
  OverlayController controller;
  VS::Effects::screenEffectOverlay = nullptr;
  sd::SetLiveVignetteEnabled(1);
  sd::SetLiveRadialWarpWords(1, 1);
  CHECK(controller.overlay.vignette_enabled == 0);
  CHECK(controller.overlay.radial_warp_scale == 0);

  std::vector<int> words(8, 0x5A5A5A5A);
  std::uint32_t head = 0x77;
  int* const result =
      sd::AppendLiveVignetteCorner(0, 0, words.data() + 1, &head);
  CHECK(result == words.data() + 1);
  CHECK(head == 0x77);
  CHECK(words[1] == 0x5A5A5A5A);
}
