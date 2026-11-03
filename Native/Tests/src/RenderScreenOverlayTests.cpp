#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.h"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.h"
#include "Render/ScreenOverlay.hpp"
#include "Render/ScreenOverlayLive.hpp"
extern "C" {
#include "Core/Gte.h"
#include "Field/RoomLoading.h"
#include "Field/Transitions.h"
#include "Render/Display.h"
#include "Render/FramebufferEffects.h"
#include "Render/ScreenTransition.h"
}

namespace {
namespace Render = VS::Render;
namespace Math = VS::Core::Math;
using Render::FramebufferEffect;
using Render::FramebufferEffectDraw;
using Render::FramebufferEffectFrame;
using Render::FramebufferEffectLayout;
using Render::FramebufferEffectPhase;
using Render::FramebufferEffectSettings;
using Render::FramebufferEffectTint;
using Render::ScreenTransition;
using Render::ScreenTransitionFade;
using Render::ScreenTransitionStyle;
using Render::ScreenTransitionWipe;
using Render::ScreenVertex;

// Records the loaded transform and maps (x, y) to (2x + 1 + bias, 3y - 1, 7).
class FakeTransformer final : public Render::ScreenTransformer {
 public:
  explicit FakeTransformer(std::int32_t bias = 0) : bias_(bias) {}

  void LoadTransform(const Math::Transform3& transform) override {
    loaded.push_back(transform);
  }
  Math::Vec3 Transform(std::int16_t x, std::int16_t y) override {
    ++transforms;
    return {(2 * x) + 1 + bias_, (3 * y) - 1, 7};
  }

  std::vector<Math::Transform3> loaded;
  int transforms = 0;

 private:
  std::int32_t bias_;
};

class FakePackets final : public Render::FramebufferEffectPackets {
 public:
  [[nodiscard]] bool Ready() const override { return ready; }
  bool Acquire(const FramebufferEffectTint& tint) override {
    acquired.push_back(tint);
    ready = accept;
    return accept;
  }
  void Release() override {
    ++releases;
    ready = false;
  }
  void Forget() override {
    ++forgets;
    ready = false;
  }
  void Submit(const FramebufferEffectFrame& frame) override {
    submitted.push_back(frame);
  }

  bool ready = false;
  bool accept = true;
  std::vector<FramebufferEffectTint> acquired;
  std::vector<FramebufferEffectFrame> submitted;
  int releases = 0;
  int forgets = 0;
};

class FakeTarget final : public Render::ScreenTransitionTarget {
 public:
  void DrawFade(const ScreenTransitionFade& fade) override {
    fades.push_back(fade);
  }
  void DrawWipe(const ScreenTransitionWipe& wipe) override {
    wipes.push_back(wipe);
  }

  std::vector<ScreenTransitionFade> fades;
  std::vector<ScreenTransitionWipe> wipes;
};

FramebufferEffectSettings ChangedSettings() {
  FramebufferEffectSettings settings;
  settings.roll = 0x123;
  settings.scale = {0x2000, 0x1800};
  settings.offset = {12, -34};
  settings.tint = {1, 2, 3};
  settings.blend_mode = 3;
  settings.parameter = 9;
  settings.depth = 40;
  settings.uses_previous_frame = 1;
  return settings;
}

// Saves the Live globals the overlays read and write, and leaves the effect
// with no packet block.
class LiveOverlayGuard {
 public:
  LiveOverlayGuard() { Render::InitializeLiveFramebufferEffect(); }
  LiveOverlayGuard(const LiveOverlayGuard&) = delete;
  LiveOverlayGuard& operator=(const LiveOverlayGuard&) = delete;
  LiveOverlayGuard(LiveOverlayGuard&&) = delete;
  LiveOverlayGuard& operator=(LiveOverlayGuard&&) = delete;
  ~LiveOverlayGuard() {
    Render::InitializeLiveFramebufferEffect();
    Field_screenTransitionAlpha = alpha_;
    Field_screenTransitionStep = step_;
    Field_screenTransitionEffect = effect_;
    Field_screenTransitionWipeAngle = angle_;
    Field_screenTransitionSpeed = speed_;
    Render_frameBuf = frame_buffer_;
    Core_Gte_Reset();
  }

 private:
  int alpha_ = Field_screenTransitionAlpha;
  u_int step_ = Field_screenTransitionStep;
  int effect_ = Field_screenTransitionEffect;
  int angle_ = Field_screenTransitionWipeAngle;
  int speed_ = Field_screenTransitionSpeed;
  int frame_buffer_ = Render_frameBuf;
};

// The packets linked at `entry`, in the order the GPU runs them.
std::vector<const void*> Chain(u_long* entry, const u_long* next_entry) {
  std::vector<const void*> packets;
  const void* packet = nextPrim(entry);
  while (packet != next_entry && packets.size() < 64) {
    packets.push_back(packet);
    packet = nextPrim(packet);
  }
  return packets;
}

DR_MODE ExpectedDrawMode(std::uint16_t page) {
  DR_MODE mode{};
  RECT clip{};
  SetDrawMode(&mode, 0, 1, page, &clip);
  return mode;
}

void CheckDrawMode(const void* packet, std::uint16_t page) {
  const auto& mode = *static_cast<const DR_MODE*>(packet);
  const DR_MODE expected = ExpectedDrawMode(page);
  CHECK(mode.code[0] == expected.code[0]);
  CHECK(mode.code[1] == expected.code[1]);
}

template <typename Quad>
std::array<ScreenVertex, 4> Corners(const Quad& quad) {
  return {ScreenVertex{quad.x0, quad.y0}, ScreenVertex{quad.x1, quad.y1},
          ScreenVertex{quad.x2, quad.y2}, ScreenVertex{quad.x3, quad.y3}};
}

template <typename Quad>
void CheckQuad(const Quad& quad, std::array<ScreenVertex, 4> corners) {
  CHECK(Corners(quad) == corners);
}

// The sprite fields the flat effect sets, compared as one value.
struct SpriteFields {
  int code = 0;
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  std::array<int, 3> rgb{};

  friend bool operator==(const SpriteFields&, const SpriteFields&) = default;
};

SpriteFields FieldsOf(const SPRT& sprite) {
  return {
      getcode(&sprite), sprite.x0,
      sprite.y0,        sprite.w,
      sprite.h,         std::array<int, 3>{sprite.r0, sprite.g0, sprite.b0}};
}

std::array<int, 3> RgbOf(const POLY_F4& quad) {
  return {quad.r0, quad.g0, quad.b0};
}

// Draws once and expects nothing on screen; returns the phase it leaves.
FramebufferEffectPhase DrawHidden(FramebufferEffect& effect,
                                  FakePackets& packets,
                                  FakeTransformer& transformer) {
  CHECK(Render::DrawFramebufferEffect(effect, packets, transformer, 0) ==
        FramebufferEffectDraw::kHidden);
  return effect.phase;
}
}  // namespace

TEST_CASE("Framebuffer effect reset restores the neutral settings",
          "[render-overlay][render]") {
  FramebufferEffect effect{FramebufferEffectPhase::kDrawing, ChangedSettings()};
  Render::ResetFramebufferEffect(effect);
  CHECK(effect == FramebufferEffect{});
  CHECK(effect.phase == FramebufferEffectPhase::kOff);
  CHECK(effect.settings.scale == Render::FramebufferEffectPair{4096, 4096});
  CHECK(effect.settings.tint == FramebufferEffectTint{0x80, 0x80, 0x80});
  CHECK(effect.settings.depth == 0);
  CHECK(effect.settings.roll == 0);
}

TEST_CASE("Framebuffer effect start-up forgets the block without freeing it",
          "[render-overlay][render]") {
  FakePackets packets;
  packets.ready = true;
  FramebufferEffect effect{FramebufferEffectPhase::kReleasing,
                           ChangedSettings()};
  Render::InitializeFramebufferEffect(effect, packets);
  CHECK(effect == FramebufferEffect{});
  CHECK(packets.forgets == 1);
  CHECK(packets.releases == 0);
  CHECK_FALSE(packets.ready);
}

TEST_CASE("Framebuffer effect slots span 64 columns of each frame buffer",
          "[render-overlay][render]") {
  const auto templates = Render::BuildFramebufferEffectTemplates();
  REQUIRE(templates.size() == 10);
  for (std::size_t slot = 0; slot < templates.size(); ++slot) {
    CAPTURE(slot);
    const auto left = static_cast<std::int32_t>(slot) * 64;
    CHECK(templates[slot].left == left);
    CHECK(templates[slot].right_texel == (slot % 5 == 4 ? 63 : 64));
    CHECK(templates[slot].texture_page == (0x100 | (left >> 6)));
  }
  CHECK(templates[5] == Render::FramebufferEffectSlotTemplate{320, 64, 0x105});
  CHECK(templates[9] == Render::FramebufferEffectSlotTemplate{576, 63, 0x109});
}

TEST_CASE("Framebuffer effect texture pages pack the blend and VRAM column",
          "[render-overlay][render]") {
  CHECK(Render::FramebufferEffectTexturePage(0, 0) == 0x100);
  CHECK(Render::FramebufferEffectTexturePage(1, 64) == 0x121);
  CHECK(Render::FramebufferEffectTexturePage(3, 640) == 0x16A);
  CHECK(Render::FramebufferEffectTexturePage(5, 0) == 0x120);
  CHECK(Render::FramebufferEffectTexturePage(0, 1024) == 0x100);
  CHECK(Render::FramebufferEffectTexturePage(0, -64) == 0x10F);
}

TEST_CASE("Framebuffer effect is transformed when rolled or scaled",
          "[render-overlay][render]") {
  FramebufferEffectSettings settings;
  CHECK_FALSE(Render::FramebufferEffectIsTransformed(settings));
  settings.offset = {100, -100};
  CHECK_FALSE(Render::FramebufferEffectIsTransformed(settings));
  settings.roll = 0x1000;
  CHECK_FALSE(Render::FramebufferEffectIsTransformed(settings));
  settings.roll = 1;
  CHECK(Render::FramebufferEffectIsTransformed(settings));
  settings.roll = 0;
  settings.scale = {0x2000, 0x2000};
  CHECK(Render::FramebufferEffectIsTransformed(settings));
  settings.scale = {4096, 0x2000};
  CHECK(Render::FramebufferEffectIsTransformed(settings));
}

TEST_CASE("Framebuffer effect transform rolls scales and centres the frame",
          "[render-overlay][render]") {
  FramebufferEffectSettings settings;
  settings.offset = {10, -20};
  Math::Transform3 transform = Render::FramebufferEffectTransform(settings);
  CHECK(transform.translation == Math::Vec3{170, 92, 0});
  CHECK(transform.rotation.m[0][0] == 4096);
  CHECK(transform.rotation.m[1][1] == 4096);
  CHECK(transform.rotation.m[2][2] == 0);

  settings.roll = 0x200;
  settings.scale = {0x2000, 0x800};
  transform = Render::FramebufferEffectTransform(settings);
  CHECK(transform.rotation ==
        Math::ScaleMatrixColumns(Math::BuildRotationXyz({0, 0, 0x200}),
                                 {0x2000, 0x800, 0}));
}

TEST_CASE("Flat framebuffer effect frames place sprites at the offset",
          "[render-overlay][render]") {
  FramebufferEffectSettings settings;
  settings.offset = {5, -7};
  settings.blend_mode = 1;
  settings.depth = 12;
  settings.tint = {10, 20, 30};
  FakeTransformer transformer;
  const FramebufferEffectFrame frame =
      Render::PlanFramebufferEffectFrame(settings, 3, transformer);
  CHECK(frame.layout == FramebufferEffectLayout::kFlat);
  CHECK(frame.tint == settings.tint);
  CHECK(frame.depth == 12);
  CHECK(transformer.transforms == 0);
  for (std::size_t strip = 0; strip < 5; ++strip) {
    CAPTURE(strip);
    const auto left = static_cast<std::int32_t>(strip) * 64;
    CHECK(frame.strips[strip].slot == 5 + strip);
    CHECK(frame.strips[strip].texture_page ==
          Render::FramebufferEffectTexturePage(1, left + 320));
    CHECK(frame.strips[strip].origin ==
          ScreenVertex{static_cast<std::int16_t>(5 + left), -7});
  }
}

TEST_CASE("Framebuffer effect shows the previous frame only when unrolled",
          "[render-overlay][render]") {
  FramebufferEffectSettings settings;
  settings.uses_previous_frame = 1;
  settings.roll = GENERATE(std::int16_t{0}, std::int16_t{0x1000});
  FakeTransformer transformer;
  FramebufferEffectFrame frame =
      Render::PlanFramebufferEffectFrame(settings, 0, transformer);
  CHECK(frame.layout == FramebufferEffectLayout::kFlat);
  CHECK(frame.strips[2].slot == 2);
  CHECK(frame.strips[2].texture_page ==
        Render::FramebufferEffectTexturePage(0, 128 + 320));
  frame = Render::PlanFramebufferEffectFrame(settings, 1, transformer);
  CHECK(frame.strips[2].slot == 7);
  CHECK(frame.strips[2].texture_page ==
        Render::FramebufferEffectTexturePage(0, 128));

  settings.roll = 1;
  frame = Render::PlanFramebufferEffectFrame(settings, 0, transformer);
  CHECK(frame.layout == FramebufferEffectLayout::kTransformed);
  CHECK(frame.strips[2].texture_page ==
        Render::FramebufferEffectTexturePage(0, 128));
}

TEST_CASE("Transformed framebuffer effect strips are quads through the GTE",
          "[render-overlay][render]") {
  FramebufferEffectSettings settings;
  settings.scale = {0x2000, 0x2000};
  settings.offset = {500, 500};
  FakeTransformer transformer;
  const FramebufferEffectFrame frame =
      Render::PlanFramebufferEffectFrame(settings, 0, transformer);
  CHECK(frame.layout == FramebufferEffectLayout::kTransformed);
  CHECK(transformer.transforms == 20);
  for (std::size_t strip = 0; strip < 5; ++strip) {
    CAPTURE(strip);
    const std::int32_t near_x = (static_cast<std::int32_t>(strip) * 64) - 160;
    const auto left_x = static_cast<std::int16_t>((2 * near_x) + 1);
    const auto right_x = static_cast<std::int16_t>((2 * (near_x + 64)) + 1);
    CHECK(frame.strips[strip].corners ==
          std::array<ScreenVertex, 4>{
              ScreenVertex{left_x, -337}, ScreenVertex{right_x, -337},
              ScreenVertex{left_x, 332}, ScreenVertex{right_x, 332}});
    CHECK(frame.strips[strip].origin == ScreenVertex{});
  }

  FakeTransformer wrapping(0x10000);
  const FramebufferEffectFrame wrapped =
      Render::PlanFramebufferEffectFrame(settings, 0, wrapping);
  CHECK(wrapped.strips[0].corners[0] == ScreenVertex{-319, -337});
}

TEST_CASE("Framebuffer effect draw starts then waits for its packets",
          "[render-overlay][render]") {
  FakePackets packets;
  FakeTransformer transformer;
  FramebufferEffect effect{FramebufferEffectPhase::kStarting,
                           ChangedSettings()};
  CHECK(DrawHidden(effect, packets, transformer) ==
        FramebufferEffectPhase::kDrawing);
  CHECK(transformer.loaded.empty());

  CHECK(Render::DrawFramebufferEffect(effect, packets, transformer, 1) ==
        FramebufferEffectDraw::kNoPackets);
  CHECK(effect.phase == FramebufferEffectPhase::kDrawing);
  CHECK(packets.submitted.empty());
}

TEST_CASE("Framebuffer effect draw submits the planned frame",
          "[render-overlay][render]") {
  FakePackets packets;
  packets.ready = true;
  FakeTransformer transformer;
  FramebufferEffect effect{FramebufferEffectPhase::kDrawing, ChangedSettings()};
  effect.settings.roll = 0;
  effect.settings.uses_previous_frame = 0;
  CHECK(Render::DrawFramebufferEffect(effect, packets, transformer, 1) ==
        FramebufferEffectDraw::kDrawn);
  REQUIRE(packets.submitted.size() == 1);
  FakeTransformer replay;
  CHECK(packets.submitted[0] ==
        Render::PlanFramebufferEffectFrame(effect.settings, 1, replay));
  REQUIRE(transformer.loaded.size() == 1);
  CHECK(transformer.loaded[0] ==
        Render::FramebufferEffectTransform(effect.settings));
}

TEST_CASE("Framebuffer effect draw stops then releases its packets once",
          "[render-overlay][render]") {
  FakePackets packets;
  packets.ready = true;
  FakeTransformer transformer;
  FramebufferEffect effect{FramebufferEffectPhase::kStopping,
                           ChangedSettings()};
  CHECK(DrawHidden(effect, packets, transformer) ==
        FramebufferEffectPhase::kReleasing);
  CHECK(packets.releases == 0);
  CHECK(DrawHidden(effect, packets, transformer) ==
        FramebufferEffectPhase::kOff);
  CHECK(packets.releases == 1);
  CHECK(DrawHidden(effect, packets, transformer) ==
        FramebufferEffectPhase::kOff);
  CHECK(packets.releases == 1);
  CHECK(packets.submitted.empty());
}

TEST_CASE("Enabling the framebuffer effect acquires its block once",
          "[render-overlay][render]") {
  FakePackets packets;
  FramebufferEffect effect;
  effect.settings.tint = {7, 8, 9};
  CHECK(Render::SetFramebufferEffectEnabled(effect, packets, true));
  CHECK(effect.phase == FramebufferEffectPhase::kStarting);
  REQUIRE(packets.acquired.size() == 1);
  CHECK(packets.acquired[0] == FramebufferEffectTint{7, 8, 9});
  CHECK(Render::SetFramebufferEffectEnabled(effect, packets, true));
  CHECK(packets.acquired.size() == 1);
}

TEST_CASE("Enabling the framebuffer effect without a block keeps the phase",
          "[render-overlay][render]") {
  FakePackets packets;
  packets.accept = false;
  FramebufferEffect effect;
  std::size_t attempts = 0;
  for (const FramebufferEffectPhase phase :
       {FramebufferEffectPhase::kOff, FramebufferEffectPhase::kStopping,
        FramebufferEffectPhase::kReleasing}) {
    CAPTURE(phase);
    effect.phase = phase;
    CHECK_FALSE(Render::SetFramebufferEffectEnabled(effect, packets, true));
    CHECK(effect.phase == phase);
    CHECK(packets.acquired.size() == ++attempts);
  }
}

TEST_CASE("Disabling the framebuffer effect starts the release",
          "[render-overlay][render]") {
  FakePackets packets;
  packets.ready = true;
  FramebufferEffect effect{FramebufferEffectPhase::kDrawing, {}};
  CHECK(Render::SetFramebufferEffectEnabled(effect, packets, false));
  CHECK(effect.phase == FramebufferEffectPhase::kStopping);
  CHECK(packets.releases == 0);
  CHECK(packets.acquired.empty());
}

TEST_CASE("Framebuffer effect roll scale tint and offset keep their widths",
          "[render-overlay][render]") {
  FramebufferEffectSettings settings;
  Render::SetFramebufferEffectRoll(settings, 0x12345);
  CHECK(Render::ReadFramebufferEffectRoll(settings) == 0x2345);
  Render::SetFramebufferEffectRoll(settings, 0x18000);
  CHECK(Render::ReadFramebufferEffectRoll(settings) == -0x8000);

  Render::SetFramebufferEffectScale(settings, {0x2000, 0x800});
  CHECK(settings.scale == Render::FramebufferEffectPair{0x2000, 0x800});
  CHECK(Render::ReadFramebufferEffectScale(settings) ==
        Render::FramebufferEffectPair{0x2000, 0x2000});

  Render::SetFramebufferEffectTint(settings, {4, 5, 6});
  CHECK(Render::ReadFramebufferEffectTint(settings) ==
        FramebufferEffectTint{4, 5, 6});
  Render::SetFramebufferEffectOffset(settings, {-3, 9});
  CHECK(Render::ReadFramebufferEffectOffset(settings) ==
        Render::FramebufferEffectPair{-3, 9});
}

TEST_CASE("Framebuffer effect mode parameter and previous frame round-trip",
          "[render-overlay][render]") {
  FramebufferEffectSettings settings;
  Render::SetFramebufferEffectBlendMode(settings, 6);
  CHECK(Render::ReadFramebufferEffectBlendMode(settings) == 6);
  Render::SetFramebufferEffectParameter(settings, -4);
  CHECK(Render::ReadFramebufferEffectParameter(settings) == -4);
  Render::SetFramebufferEffectPreviousFrame(settings, 1);
  CHECK(Render::ReadFramebufferEffectPreviousFrame(settings) == 1);
}

TEST_CASE("Framebuffer effect depth stays inside the ordering table",
          "[render-overlay][render]") {
  FramebufferEffectSettings settings;
  CHECK(Render::SetFramebufferEffectDepth(settings, 0x7FF));
  CHECK(Render::ReadFramebufferEffectDepth(settings) == 0x7FF);
  CHECK_FALSE(Render::SetFramebufferEffectDepth(settings, 0x800));
  CHECK_FALSE(Render::SetFramebufferEffectDepth(settings, 0xFFFFFFFFU));
  CHECK(Render::ReadFramebufferEffectDepth(settings) == 0x7FF);
}

TEST_CASE("Framebuffer effect state codes are the original state words",
          "[render-overlay][render]") {
  const auto phase = GENERATE(
      FramebufferEffectPhase::kOff, FramebufferEffectPhase::kStarting,
      FramebufferEffectPhase::kDrawing, FramebufferEffectPhase::kStopping,
      FramebufferEffectPhase::kReleasing);
  const FramebufferEffect effect{phase, ChangedSettings()};
  CHECK(Render::FramebufferEffectStateCode(effect) ==
        static_cast<std::int32_t>(phase));
  CHECK(Render::FramebufferEffectStateCode(
            {FramebufferEffectPhase::kReleasing, {}}) == 4);
}

TEST_CASE("Screen transition styles fades and wipe edges follow the table",
          "[render-overlay][render]") {
  using Style = ScreenTransitionStyle;
  const std::array<Style, 8> expected{
      Style::kFadeToBlack, Style::kFadeToWhite, Style::kWipe, Style::kNone,
      Style::kFadeToBlack, Style::kFadeToBlack, Style::kNone, Style::kNone};
  for (std::size_t effect = 0; effect < expected.size(); ++effect) {
    CAPTURE(effect);
    CHECK(Render::ScreenTransitionStyleOf(static_cast<std::int32_t>(effect)) ==
          expected[effect]);
  }
  CHECK(Render::ScreenTransitionStyleOf(-1) == Style::kNone);

  CHECK(Render::PlanScreenTransitionFade(Style::kFadeToBlack, 255) ==
        ScreenTransitionFade{255, true, 0x40});
  CHECK(Render::PlanScreenTransitionFade(Style::kFadeToWhite, 128) ==
        ScreenTransitionFade{128, true, 0x20});
  CHECK(Render::PlanScreenTransitionFade(Style::kFadeToWhite, 255) ==
        ScreenTransitionFade{255, false, 0x20});

  using Edges = std::array<std::int16_t, 3>;
  CHECK(Render::ScreenTransitionWipeEdges(0) == Edges{-360, -360, -200});
  CHECK(Render::ScreenTransitionWipeEdges(128) == Edges{-219, -79, 81});
  CHECK(Render::ScreenTransitionWipeEdges(255) == Edges{-80, 200, 360});
}

TEST_CASE("Screen transition wipe turns half a turn while uncovering",
          "[render-overlay][render]") {
  ScreenTransition transition{255, 1, 2, 0x100, 5};
  Math::Transform3 transform =
      Render::ScreenTransitionWipeTransform(transition);
  CHECK(transform.rotation == Math::BuildRotationXyz({0, 0, 0x100}));
  CHECK(transform.translation == Math::Vec3{160, 120, 0});
  transition.step = GENERATE(4U, 7U);
  transform = Render::ScreenTransitionWipeTransform(transition);
  CHECK(transform.rotation == Math::BuildRotationXyz({0, 0, 0x900}));
  transition.wipe_angle = 0xFFFF;
  transform = Render::ScreenTransitionWipeTransform(transition);
  CHECK(transform.rotation == Math::BuildRotationXyz({0, 0, 0x7FF}));

  FakeTransformer transformer;
  transition.alpha = 128;
  const ScreenTransitionWipe wipe =
      Render::PlanScreenTransitionWipe(transition, transformer);
  CHECK(wipe.texture_page == 0x40);
  const std::array<std::int16_t, 4> columns{-360, -219, -79, 81};
  for (std::size_t column = 0; column < columns.size(); ++column) {
    CAPTURE(column);
    const auto x = static_cast<std::int16_t>((2 * columns[column]) + 1);
    CHECK(wipe.columns[column].top == ScreenVertex{x, -601});
    CHECK(wipe.columns[column].bottom == ScreenVertex{x, 599});
  }
}

TEST_CASE("Screen transition alpha steps to its ends",
          "[render-overlay][render]") {
  const auto advanced = [](ScreenTransition transition) {
    Render::AdvanceScreenTransition(transition);
    return transition;
  };
  const std::uint32_t cover = GENERATE(1U, 2U);
  CHECK(advanced({250, cover, 0, 0, 10}) ==
        ScreenTransition{255, cover, 0, 0, 10});
  CHECK(advanced({244, cover, 0, 0, 10}) ==
        ScreenTransition{254, cover, 0, 0, 10});
  CHECK(advanced({250, 5, 0, 0, 10}) == ScreenTransition{255, 6, 0, 0, 10});
  CHECK(advanced({200, 5, 0, 0, 10}) == ScreenTransition{210, 5, 0, 0, 10});
  const std::uint32_t uncover = GENERATE(4U, 7U);
  CHECK(advanced({10, uncover, 0, 0, 10}) == ScreenTransition{0, 0, 0, 0, 10});
  CHECK(advanced({11, uncover, 0, 0, 10}) ==
        ScreenTransition{1, uncover, 0, 0, 10});
  const std::uint32_t still = GENERATE(0U, 3U, 6U, 8U);
  CHECK(advanced({100, still, 0, 0, 10}) ==
        ScreenTransition{100, still, 0, 0, 10});
}

TEST_CASE("Screen transition fades while covered and always steps",
          "[render-overlay][render]") {
  FakeTarget target;
  FakeTransformer transformer;
  ScreenTransition transition{0, 1, 0, 0, 10};
  Render::DrawScreenTransition(transition, target, transformer);
  CHECK(target.fades.empty());
  CHECK(transition.alpha == 10);

  Render::DrawScreenTransition(transition, target, transformer);
  REQUIRE(target.fades.size() == 1);
  CHECK(target.fades[0] == ScreenTransitionFade{10, true, 0x40});
  CHECK(transition.alpha == 20);

  transition = {250, 1, 1, 0, 10};
  Render::DrawScreenTransition(transition, target, transformer);
  REQUIRE(target.fades.size() == 2);
  CHECK(target.fades[1] == ScreenTransitionFade{250, true, 0x20});
  CHECK(transition.alpha == 255);
}

TEST_CASE("Screen transition wipes through the GTE and skips no-draw styles",
          "[render-overlay][render]") {
  FakeTarget target;
  FakeTransformer transformer;
  ScreenTransition transition{200, 4, 2, 0x40, 10};
  Render::DrawScreenTransition(transition, target, transformer);
  REQUIRE(target.wipes.size() == 1);
  REQUIRE(transformer.loaded.size() == 1);
  CHECK(transformer.loaded[0] ==
        Render::ScreenTransitionWipeTransform({200, 4, 2, 0x40, 10}));
  CHECK(transition.alpha == 190);

  transition = {200, 4, 3, 0, 10};
  Render::DrawScreenTransition(transition, target, transformer);
  CHECK(target.fades.empty());
  CHECK(target.wipes.size() == 1);
  CHECK(transition.alpha == 190);
}

TEST_CASE("Live framebuffer effect stays off without heap for its packets",
          "[render-overlay][render]") {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 8>
          arena{};
  const VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  const LiveOverlayGuard guard;
  const std::uint64_t refused = Render::RefusedLiveScreenOverlayCount();
  Render::SetLiveFramebufferEffectEnabled(1);
  CHECK(Render::LiveFramebufferEffect().phase == FramebufferEffectPhase::kOff);
  CHECK_FALSE(Render::HasLiveFramebufferEffectPackets());
  CHECK_FALSE(Render::IsLiveFramebufferEffectShowing());
  CHECK(Render::RefusedLiveScreenOverlayCount() == refused + 1);

  std::array<u_long, 4> ordering{};
  ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  Render::DrawLiveFramebufferEffect(ordering.data());
  CHECK(Render::LiveFramebufferEffect().phase == FramebufferEffectPhase::kOff);
  CHECK(Render::RefusedLiveScreenOverlayCount() == refused + 1);
  CHECK(nextPrim(&ordering[3]) == &ordering[2]);
}

namespace {
// The flat strips the Live effect links for frame buffer 1, each run as its
// draw mode then its sprite, last strip first.
void CheckFlatStrips(const std::vector<const void*>& chain) {
  REQUIRE(chain.size() == 10);
  for (std::size_t strip = 0; strip < 5; ++strip) {
    CAPTURE(strip);
    const std::size_t drawn = 4 - strip;
    const auto left = static_cast<std::int32_t>(drawn) * 64;
    CheckDrawMode(chain[strip * 2],
                  Render::FramebufferEffectTexturePage(1, left + 320));
    const auto& sprite = *static_cast<const SPRT*>(chain[(strip * 2) + 1]);
    CHECK(FieldsOf(sprite) ==
          SpriteFields{0x66, 8 + left, -4, 64, 223, {10, 20, 30}});
  }
}

// Disabling hides the effect at once and frees the block two draws later.
void CheckDisableFreesBlock(u_long* ordering) {
  Render::SetLiveFramebufferEffectEnabled(0);
  CHECK_FALSE(Render::IsLiveFramebufferEffectShowing());
  Render::DrawLiveFramebufferEffect(ordering);
  CHECK(Render::HasLiveFramebufferEffectPackets());
  Render::DrawLiveFramebufferEffect(ordering);
  CHECK_FALSE(Render::HasLiveFramebufferEffectPackets());
  CHECK(Render::LiveFramebufferEffect().phase == FramebufferEffectPhase::kOff);
}
}  // namespace

TEST_CASE("Live framebuffer effect links flat strips and frees its block",
          "[render-overlay][render]") {
  // Room for one packet block: a second allocation fails if one leaks.
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 64>
          arena{};
  const VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  const LiveOverlayGuard guard;
  Render_frameBuf = 1;
  const CVECTOR tint{10, 20, 30, 0};
  Render_FramebufferEffectTranslation offset{8, -4, 0, 0};
  Render::SetLiveFramebufferEffectTint(&tint);
  Render::SetLiveFramebufferEffectOffset(&offset);
  Render::SetLiveFramebufferEffectBlendMode(1);
  Render::SetLiveFramebufferEffectDepth(3);
  std::array<u_long, 6> ordering{};
  for (int cycle = 0; cycle < 3; ++cycle) {
    CAPTURE(cycle);
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render::SetLiveFramebufferEffectEnabled(1);
    REQUIRE(Render::HasLiveFramebufferEffectPackets());
    CHECK(Render::IsLiveFramebufferEffectShowing());
    Render::DrawLiveFramebufferEffect(ordering.data());
    CHECK(nextPrim(&ordering[3]) == &ordering[2]);
    Render::DrawLiveFramebufferEffect(ordering.data());
    CheckFlatStrips(Chain(&ordering[3], &ordering[2]));
    CheckDisableFreesBlock(ordering.data());
  }
}

TEST_CASE("Live framebuffer effect places scaled strips through the GTE",
          "[render-overlay][render]") {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 64>
          arena{};
  const VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  const LiveOverlayGuard guard;
  Render_frameBuf = 0;
  Render_FramebufferEffectTranslation scale{0x2000, 0x2000, 0, 0};
  Render::SetLiveFramebufferEffectScale(&scale);
  Render::SetLiveFramebufferEffectDepth(1);
  Render::SetLiveFramebufferEffectEnabled(1);
  std::array<u_long, 4> ordering{};
  ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  Render::DrawLiveFramebufferEffect(ordering.data());
  Render::DrawLiveFramebufferEffect(ordering.data());
  const auto chain = Chain(&ordering[1], &ordering[0]);
  REQUIRE(chain.size() == 5);
  for (std::size_t strip = 0; strip < 5; ++strip) {
    CAPTURE(strip);
    const std::int32_t drawn = 4 - static_cast<std::int32_t>(strip);
    const auto left_x =
        static_cast<std::int16_t>((2 * (drawn * 64 - 160)) + 160);
    const auto right_x = static_cast<std::int16_t>(left_x + 128);
    const auto& quad = *static_cast<const POLY_FT4*>(chain[strip]);
    CHECK(getcode(&quad) == 0x2E);
    CHECK(quad.tpage == Render::FramebufferEffectTexturePage(0, drawn * 64));
    CHECK(quad.r0 == 0x80);
    CheckQuad(quad, {ScreenVertex{left_x, -112}, ScreenVertex{right_x, -112},
                     ScreenVertex{left_x, 334}, ScreenVertex{right_x, 334}});
  }
}

TEST_CASE("Live framebuffer effect setters and readers refuse null records",
          "[render-overlay][render]") {
  const LiveOverlayGuard guard;
  const std::uint64_t refused = Render::RefusedLiveScreenOverlayCount();
  const FramebufferEffect before = Render::LiveFramebufferEffect();
  Render::SetLiveFramebufferEffectScale(nullptr);
  Render::SetLiveFramebufferEffectTint(nullptr);
  Render::SetLiveFramebufferEffectOffset(nullptr);
  Render::ReadLiveFramebufferEffectScale(nullptr);
  Render::ReadLiveFramebufferEffectTint(nullptr);
  Render::ReadLiveFramebufferEffectOffset(nullptr);
  CHECK(Render::RefusedLiveScreenOverlayCount() == refused + 6);
  CHECK(Render::LiveFramebufferEffect() == before);
}

TEST_CASE("Live framebuffer effect refuses a null ordering table",
          "[render-overlay][render]") {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 64>
          arena{};
  const VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  const LiveOverlayGuard guard;
  const std::uint64_t refused = Render::RefusedLiveScreenOverlayCount();
  Render::SetLiveFramebufferEffectEnabled(1);
  Render::DrawLiveFramebufferEffect(nullptr);
  Render::DrawLiveFramebufferEffect(nullptr);
  CHECK(Render::LiveFramebufferEffect().phase ==
        FramebufferEffectPhase::kDrawing);
  CHECK(Render::RefusedLiveScreenOverlayCount() == refused + 1);
}

TEST_CASE("Live screen transition steps without an ordering table",
          "[render-overlay][render]") {
  const LiveOverlayGuard guard;
  const std::uint64_t refused = Render::RefusedLiveScreenOverlayCount();
  Field_screenTransitionAlpha = 0;
  Field_screenTransitionStep = 1;
  Field_screenTransitionEffect = 0;
  Field_screenTransitionSpeed = 30;
  Render::DrawLiveScreenTransition(nullptr);
  CHECK(Field_screenTransitionAlpha == 30);
  CHECK(Render::RefusedLiveScreenOverlayCount() == refused);
  Render::DrawLiveScreenTransition(nullptr);
  CHECK(Field_screenTransitionAlpha == 60);
  CHECK(Render::RefusedLiveScreenOverlayCount() == refused + 1);
}

TEST_CASE("Live framebuffer effect scale reader writes only its fields",
          "[render-overlay][render]") {
  const LiveOverlayGuard guard;
  Render_FramebufferEffectTranslation scale{0x2000, 0x800, 5, 6};
  Render::SetLiveFramebufferEffectScale(&scale);
  Render_FramebufferEffectTranslation read{1, 2, 3, 4};
  Render::ReadLiveFramebufferEffectScale(&read);
  CHECK(read.x == 0x2000);
  CHECK(read.y == 0x2000);
  CHECK(read.reserved8 == 0);
  CHECK(read.reservedC == 4);
}

TEST_CASE("Live framebuffer effect tint reader keeps the code byte",
          "[render-overlay][render]") {
  const LiveOverlayGuard guard;
  const CVECTOR tint{4, 5, 6, 7};
  Render::SetLiveFramebufferEffectTint(&tint);
  CVECTOR tint_read{1, 2, 3, 99};
  Render::ReadLiveFramebufferEffectTint(&tint_read);
  CHECK(tint_read.r == 4);
  CHECK(tint_read.g == 5);
  CHECK(tint_read.b == 6);
  CHECK(tint_read.cd == 99);
}

TEST_CASE("Live framebuffer effect offset reader writes only x and y",
          "[render-overlay][render]") {
  const LiveOverlayGuard guard;
  Render_FramebufferEffectTranslation offset{-9, 11, 12, 13};
  Render::SetLiveFramebufferEffectOffset(&offset);
  Render_FramebufferEffectTranslation offset_read{1, 2, 3, 4};
  Render::ReadLiveFramebufferEffectOffset(&offset_read);
  CHECK(offset_read.x == -9);
  CHECK(offset_read.y == 11);
  CHECK(offset_read.reserved8 == 3);
  CHECK(offset_read.reservedC == 4);
}

TEST_CASE("Live framebuffer effect setters update the settings",
          "[render-overlay][render]") {
  const LiveOverlayGuard guard;
  Render::SetLiveFramebufferEffectRotation(0x12345);
  Render::SetLiveFramebufferPreviousFrame(1);
  Render::SetLiveFramebufferEffectDepth(0x7FF);
  Render::SetLiveFramebufferEffectDepth(0x800);
  const FramebufferEffectSettings& settings =
      Render::LiveFramebufferEffect().settings;
  CHECK(settings.roll == 0x2345);
  CHECK(settings.uses_previous_frame == 1);
  CHECK(settings.depth == 0x7FF);
  Render::ResetLiveFramebufferEffect();
  CHECK(Render::LiveFramebufferEffect() == FramebufferEffect{});
}

TEST_CASE("Live screen transition fades the frame being built",
          "[render-overlay][render]") {
  const LiveOverlayGuard guard;
  Render_frameBuf = 1;
  Field_screenTransitionAlpha = 100;
  Field_screenTransitionStep = 1;
  Field_screenTransitionEffect = 0;
  Field_screenTransitionSpeed = 10;
  std::array<u_long, 4> ordering{};
  ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  Render::DrawLiveScreenTransition(ordering.data());
  CHECK(Field_screenTransitionAlpha == 110);
  CHECK(Field_screenTransitionStep == 1);
  const auto chain = Chain(&ordering[1], &ordering[0]);
  REQUIRE(chain.size() == 2);
  CHECK(chain[0] == &Render_legacyTransitionDrawModes[1]);
  CheckDrawMode(chain[0], 0x40);
  REQUIRE(chain[1] == &Render_legacyTransitionFadePackets[1]);
  const POLY_F4& quad = Render_legacyTransitionFadePackets[1];
  CHECK((getcode(&quad) & 2) == 2);
  CHECK(RgbOf(quad) == std::array<int, 3>{100, 100, 100});
  CheckQuad(quad, {ScreenVertex{0, 0}, ScreenVertex{320, 0},
                   ScreenVertex{0, 240}, ScreenVertex{320, 240}});
}

TEST_CASE("Live screen transition wipes across the centred band",
          "[render-overlay][render]") {
  const LiveOverlayGuard guard;
  Render_frameBuf = 0;
  Field_screenTransitionAlpha = 255;
  Field_screenTransitionStep = 1;
  Field_screenTransitionEffect = 2;
  Field_screenTransitionWipeAngle = 0;
  Field_screenTransitionSpeed = 5;
  std::array<u_long, 4> ordering{};
  ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  Render::DrawLiveScreenTransition(ordering.data());
  CHECK(Field_screenTransitionAlpha == 255);
  CHECK(Field_screenTransitionStep == 1);
  const auto chain = Chain(&ordering[1], &ordering[0]);
  REQUIRE(chain.size() == 4);
  CHECK(chain[0] == &Render_legacyTransitionDrawModes[0]);
  CheckDrawMode(chain[0], 0x40);
  CHECK(chain[1] == &Render_legacyTransitionEdgePackets[0]);
  CHECK(chain[2] == &Render_legacyTransitionWipePackets[0].edgeQuad);
  CHECK(chain[3] == &Render_legacyTransitionWipePackets[0].coveredQuad);
  const auto column = [](std::int16_t x) {
    return std::array<ScreenVertex, 2>{ScreenVertex{x, -80},
                                       ScreenVertex{x, 320}};
  };
  const auto quad_between = [&](std::int16_t left, std::int16_t right) {
    const auto left_column = column(left);
    const auto right_column = column(right);
    return std::array<ScreenVertex, 4>{left_column[0], right_column[0],
                                       left_column[1], right_column[1]};
  };
  CheckQuad(Render_legacyTransitionWipePackets[0].coveredQuad,
            quad_between(-200, 80));
  CheckQuad(Render_legacyTransitionWipePackets[0].edgeQuad,
            quad_between(80, 360));
  CheckQuad(Render_legacyTransitionEdgePackets[0], quad_between(360, 520));
}

TEST_CASE("Screen overlay functions no longer resolve by module address",
          "[render-overlay][render]") {
  const unsigned address =
      GENERATE(0x8006D0A4U, 0x8007D360U, 0x8007D3F8U, 0x8007D41CU, 0x8007D734U,
               0x8007DD50U, 0x8007DDACU, 0x8007DDB8U, 0x8007DDD4U, 0x8007DDF8U,
               0x8007DE2CU, 0x8007DE38U, 0x8007DE44U, 0x8007DE5CU, 0x8007DE68U,
               0x8007DE78U, 0x8007DE88U, 0x8007DEA8U, 0x8007DECCU, 0x8007DF00U,
               0x8007DF10U, 0x8007DF20U, 0x8007DF30U);
  CAPTURE(address);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          address) == nullptr);
}

TEST_CASE("Screen overlay state addresses have no module symbol entry",
          "[render-overlay][render]") {
  // Neither an exact entry nor a neighbour's interior extent may claim a
  // retired address (#280).
  const unsigned address =
      GENERATE(0x800F1A20U, 0x800F1A28U, 0x800F1A30U, 0x800F1A40U, 0x800F1A44U,
               0x800F1A68U, 0x800F1A78U, 0x800F1A9CU, 0x800F1ABCU, 0x800F1B98U,
               0x800F1B9CU);
  CAPTURE(address);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(std::ranges::none_of(symbols, [address](const auto& symbol) {
    return std::string_view(symbol.module) == "BATTLE/BATTLE.PRG" &&
           symbol.address == address;
  }));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          address) == nullptr);
}
