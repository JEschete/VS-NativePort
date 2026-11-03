#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/AppearanceTransition.hpp"
#include "Legacy/Psx.h"
#include "Render/PaletteAnimation.hpp"
#include "Render/PaletteAnimationLive.hpp"

namespace {
namespace Render = VS::Render;
using Render::ColorOffset;
using Render::ColorTransform;
using Render::PaletteAnimation;
using Render::RgbBytes;
using Render::TintCode;
using VS::Actors::PaletteTransition;

// Modes the originals had no case for.
// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
constexpr auto kUnnamedTransform = static_cast<ColorTransform>(200);
// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
constexpr auto kUnnamedTransition = static_cast<PaletteTransition>(7);

constexpr std::uint16_t Pack(int red, int green, int blue, int mask = 0) {
  return static_cast<std::uint16_t>(red | (green << 5) | (blue << 10) |
                                    (mask << 15));
}

class FakePaletteHost final : public Render::PaletteHost {
 public:
  struct ClutUpload {
    std::uint32_t row = 0;
    std::uint32_t first_color = 0;
    std::vector<std::uint16_t> colors;
  };

  void UploadClutColors(std::uint32_t row, std::uint32_t first_color,
                        std::span<const std::uint16_t> colors) override {
    uploads.push_back({row, first_color, {colors.begin(), colors.end()}});
  }
  void UploadAnimatedClutRows(
      std::span<const Render::ClutRowColors, Render::kAnimatedClutRowCount>
          rows) override {
    ++row_uploads;
    std::copy(rows.begin(), rows.end(), uploaded_rows.begin());
  }
  void SetRoomBackgroundColors(
      const Render::BackgroundColors& colors) override {
    backgrounds.push_back(colors);
  }
  void SetRoomTint(const TintCode& tint, std::int32_t blend) override {
    room_tints.emplace_back(tint, blend);
  }
  void SetActorTint(std::uint32_t actor, const TintCode& tint) override {
    actor_tints.emplace_back(actor, tint);
  }

  std::vector<ClutUpload> uploads;
  int row_uploads = 0;
  std::array<Render::ClutRowColors, Render::kAnimatedClutRowCount>
      uploaded_rows{};
  std::vector<Render::BackgroundColors> backgrounds;
  std::vector<std::pair<TintCode, std::int32_t>> room_tints;
  std::vector<std::pair<std::uint32_t, TintCode>> actor_tints;
};

// The first two colours of an animated row and its timer.
struct RowSnapshot {
  std::uint16_t first = 0;
  std::uint16_t second = 0;
  bool active = false;
  ColorTransform mode = ColorTransform::kOffsetCurrent;

  friend bool operator==(const RowSnapshot&, const RowSnapshot&) = default;
};

// The last tint an actor was given and its tint slot's timer.
struct TintSnapshot {
  std::uint32_t actor = 0;
  TintCode code{};
  ColorTransform mode = ColorTransform::kOffsetCurrent;
  bool active = false;

  friend bool operator==(const TintSnapshot&, const TintSnapshot&) = default;
};

// An initialised animator (enabled, tints neutral) and a recording host.
struct Animator {
  std::unique_ptr<PaletteAnimation> state =
      std::make_unique<PaletteAnimation>();
  FakePaletteHost host;

  Animator() { Render::InitializeClutAnimation(*state); }

  PaletteAnimation& Get() { return *state; }

  bool Load(std::uint32_t row, std::uint32_t first_color,
            std::vector<std::uint16_t> colors) {
    return Render::LoadClutColors(*state, host, row, first_color, colors);
  }
  bool LoadEveryRow(std::uint16_t color) {
    bool loaded = true;
    for (std::uint32_t row = 0; row < Render::kAnimatedClutRowCount; ++row) {
      loaded &= Load(row, 0, {color});
    }
    return loaded;
  }
  void UpdateRows(int frames) {
    for (int frame = 0; frame < frames; ++frame) {
      Render::UpdateClutTransitions(*state);
    }
  }
  void UpdateTints(int frames) {
    for (int frame = 0; frame < frames; ++frame) {
      Render::UpdateModelTintTransitions(*state, host);
    }
  }
  [[nodiscard]] RowSnapshot Row(std::size_t row) const {
    return {.first = state->colors[row][0],
            .second = state->colors[row][1],
            .active = state->rows[row].timer.active,
            .mode = state->rows[row].timer.mode};
  }
  // The row after each of `legs` runs of `frames` updates.
  std::vector<RowSnapshot> RowLegs(std::size_t row, int frames, int legs) {
    std::vector<RowSnapshot> snapshots;
    for (int leg = 0; leg < legs; ++leg) {
      UpdateRows(frames);
      snapshots.push_back(Row(row));
    }
    return snapshots;
  }
  [[nodiscard]] TintSnapshot ActorTint(std::size_t tint) const {
    const auto& [actor, code] = host.actor_tints.back();
    return {.actor = actor,
            .code = code,
            .mode = state->tints[tint].timer.mode,
            .active = state->tints[tint].timer.active};
  }
  std::vector<TintSnapshot> TintLegs(std::size_t tint, int frames, int legs) {
    std::vector<TintSnapshot> snapshots;
    for (int leg = 0; leg < legs; ++leg) {
      UpdateTints(frames);
      snapshots.push_back(ActorTint(tint));
    }
    return snapshots;
  }
  // The first background colour and the gradient's mode after each leg.
  std::vector<std::pair<std::int32_t, ColorTransform>> BackgroundLegs(
      int frames, int legs) {
    std::vector<std::pair<std::int32_t, ColorTransform>> snapshots;
    for (int leg = 0; leg < legs; ++leg) {
      for (int frame = 0; frame < frames; ++frame) {
        Render::UpdateBackgroundGradientTransition(*state, host);
      }
      snapshots.emplace_back(host.backgrounds.back()[0],
                             state->background.timer.mode);
    }
    return snapshots;
  }
};

std::vector<std::uint16_t> RowOf(std::uint16_t first) {
  std::vector<std::uint16_t> row(Render::kClutColorCount, 0);
  row[0] = first;
  return row;
}

// 1, 2, 3, ... so a colour's value names its source index + 1.
std::vector<std::uint16_t> CountingSource(std::size_t count) {
  std::vector<std::uint16_t> source(count);
  std::iota(source.begin(), source.end(), std::uint16_t{1});
  return source;
}

std::array<std::uint16_t, Render::kAnimatedClutRowCount> FirstColors(
    const PaletteAnimation& animation) {
  std::array<std::uint16_t, Render::kAnimatedClutRowCount> colors{};
  std::ranges::transform(
      animation.colors, colors.begin(),
      [](const Render::ClutRowColors& row) { return row[0]; });
  return colors;
}

std::array<std::uint16_t, Render::kClutColorCount> BaseSource() {
  std::array<std::uint16_t, Render::kClutColorCount> source{};
  source[1] = Pack(10, 20, 30, 1);
  source[2] = Pack(1, 1, 1);
  return source;
}

// A row's first three base colours, its second colour and that colour's
// channels.
struct BaseSnapshot {
  std::array<std::uint16_t, 3> base{};
  std::uint16_t color = 0;
  RgbBytes rgb{};

  friend bool operator==(const BaseSnapshot&, const BaseSnapshot&) = default;
};

BaseSnapshot SnapshotBase(const PaletteAnimation& animation, std::size_t row) {
  const Render::ClutRowAnimation& clut = animation.rows[row];
  return {.base = {clut.base[0], clut.base[1], clut.base[2]},
          .color = animation.colors[row][1],
          .rgb = clut.colors[1].rgb};
}

Render::BackgroundColors RoomBackground() {
  Render::BackgroundColors colors{};
  colors.fill(0x302010);
  return colors;
}

struct ModeTarget {
  ColorTransform mode;
  int red;
  int green;
  int blue;
};

struct TransitionCase {
  std::uint16_t color = 0;
  PaletteTransition mode = PaletteTransition::kRemoveRedGreen;
  int amount = 0;
  std::uint16_t expected = 0;
  bool changed = false;
};

// Saves the VRAM the game's animated and direct CLUT rows use and leaves the
// game's animator initialised.
class LiveClutGuard {
 public:
  LiveClutGuard() {
    StoreImage(&region_, reinterpret_cast<u_long*>(saved_.data()));
    Render::InitializeLiveClutAnimation();
  }
  LiveClutGuard(const LiveClutGuard&) = delete;
  LiveClutGuard& operator=(const LiveClutGuard&) = delete;
  LiveClutGuard(LiveClutGuard&&) = delete;
  LiveClutGuard& operator=(LiveClutGuard&&) = delete;
  ~LiveClutGuard() {
    Render::InitializeLiveClutAnimation();
    LoadImage(&region_, reinterpret_cast<u_long*>(saved_.data()));
  }

 private:
  RECT region_{768, 224, 256, 32};
  std::vector<u_short> saved_ = std::vector<u_short>(std::size_t{256} * 32);
};

std::vector<std::uint16_t> ReadVram(RECT area) {
  // Padded so an odd width still has a whole word to land in.
  std::vector<std::uint16_t> pixels(static_cast<std::size_t>(area.w) + 1);
  StoreImage(&area, reinterpret_cast<u_long*>(pixels.data()));
  pixels.resize(static_cast<std::size_t>(area.w));
  return pixels;
}

template <typename Request>
std::uint64_t RefusalsOf(const Request& request) {
  const std::uint64_t before = Render::RefusedLivePaletteRequestCount();
  request();
  return Render::RefusedLivePaletteRequestCount() - before;
}
}  // namespace

TEST_CASE("Palette helpers keep the original channel arithmetic",
          "[render-palette]") {
  CHECK(Render::WeightedLuminance(Pack(1, 2, 3)) == 11);
  CHECK(Render::WeightedLuminance(0x7FFF) == 186);
  CHECK(Render::WeightedLuminance(0x8000) == 0);
  CHECK(Render::ToFixed16(0x40) == 0x400000);
  CHECK(Render::ToFixed16(-1) == -0x10000);
  CHECK(Render::ToFixed16(0x8000) == static_cast<std::int32_t>(0x80000000U));
}

// _transformClutSlot (main.c:10612-10706): current (10, 20, 30), base
// (4, 8, 12), offset (1, -2, 3).
TEST_CASE("CLUT row transforms reach each mode's original target",
          "[render-palette]") {
  const ModeTarget target =
      GENERATE(ModeTarget{ColorTransform::kOffsetCurrent, 11, 18, 31},
               ModeTarget{ColorTransform::kHalveCurrent, 6, 8, 18},
               ModeTarget{ColorTransform::kGreyCurrent, 19, 16, 21},
               ModeTarget{ColorTransform::kDarkGreyCurrent, 10, 7, 12},
               ModeTarget{ColorTransform::kOffsetBase, 5, 6, 15},
               ModeTarget{ColorTransform::kHalveBase, 3, 2, 9},
               ModeTarget{ColorTransform::kGreyBase, 8, 5, 10},
               ModeTarget{ColorTransform::kDarkGreyBase, 4, 1, 6},
               ModeTarget{ColorTransform::kRestoreBase, 4, 8, 12},
               ModeTarget{ColorTransform::kPulse, 5, 6, 15},
               ModeTarget{ColorTransform::kPulseReturn, 11, 18, 31},
               ModeTarget{ColorTransform::kInvert, 22, 9, 4},
               ModeTarget{ColorTransform::kThreeQuarterBase, 4, 4, 12},
               ModeTarget{ColorTransform::kFadeThenRestore, 11, 18, 31},
               ModeTarget{ColorTransform::kRestoreBaseFinal, 4, 8, 12},
               ModeTarget{kUnnamedTransform, 11, 18, 31});
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  REQUIRE(animator.Load(0, 0, {0, Pack(10, 20, 30, 1)}));
  animation.rows[0].base[1] = Pack(4, 8, 12);
  animation.upload_pending = false;

  REQUIRE(
      Render::ApplyClutRowTransform(animation, target.mode, 0, 0, {1, -2, 3}));

  const Render::ClutColorFade& entry = animation.rows[0].colors[1];
  CHECK(entry.rgb == RgbBytes{static_cast<std::uint8_t>(target.red),
                              static_cast<std::uint8_t>(target.green),
                              static_cast<std::uint8_t>(target.blue)});
  CHECK(animation.colors[0][1] ==
        Pack(target.red, target.green, target.blue, 1));
  CHECK(animation.colors[0][0] == 0);
  CHECK(animation.rows[0].colors[0].rgb == RgbBytes{});
  CHECK(animation.upload_pending);
  CHECK_FALSE(animation.rows[0].timer.active);
}

TEST_CASE("CLUT transforms clamp halfword channels and keep black blue",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  REQUIRE(animator.Load(1, 0, {0, Pack(10, 20, 30)}));
  const std::array<ColorOffset, 3> offsets{
      {{-40, -40, -40}, {40, 3, 0}, {0x7FFF, 0, 0}}};
  std::vector<std::uint16_t> results;
  for (const ColorOffset& offset : offsets) {
    Render::ApplyClutRowTransform(animation, ColorTransform::kOffsetCurrent, 0,
                                  1, offset);
    results.push_back(animation.colors[1][1]);
  }
  // 31 + 0x7FFF wraps negative in the original's halfword, so it clamps to 0.
  CHECK(results == std::vector<std::uint16_t>{Pack(0, 0, 1), Pack(31, 3, 1),
                                              Pack(0, 3, 1)});
}

TEST_CASE("kCancelPulse clears the mode at the first coloured entry",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  REQUIRE(animator.Load(1, 0, {0, Pack(10, 20, 30)}));
  Render::ClutRowAnimation& row = animation.rows[1];
  row.timer.active = true;
  row.timer.mode = ColorTransform::kPulse;
  row.colors[0].step_rows = {5, 5, 5};
  row.colors[1].step_rows = {7, 7, 7};

  REQUIRE(Render::ApplyClutRowTransform(animation, ColorTransform::kCancelPulse,
                                        4, 1, {9, 9, 9}));

  CHECK(animator.Row(1) ==
        RowSnapshot{0, Pack(10, 20, 30), true, ColorTransform::kOffsetCurrent});
  // The black entry before it is held, as the original's loop did.
  CHECK(std::pair(row.colors[0].step_rows, row.colors[1].step_rows) ==
        std::pair{RgbBytes{31, 31, 31}, RgbBytes{7, 7, 7}});
}

// func_800483FC: the speed picks 16, 8, 4 or 2 columns and waits speed / 8
// frames between them.
TEST_CASE("Timed CLUT transitions step through the original delta tables",
          "[render-palette]") {
  const auto [duration, frames] =
      GENERATE(std::pair{1, 2}, std::pair{2, 4}, std::pair{3, 4},
               std::pair{4, 8}, std::pair{8, 16}, std::pair{16, 32});
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  REQUIRE(animator.Load(3, 0, {Pack(1, 1, 1), 0}));
  REQUIRE(Render::BuildClutTransition(animation,
                                      static_cast<std::int16_t>(duration), 3,
                                      RowOf(Pack(31, 0, 17))));
  // Row 31 + d moves a channel by d; the black colour holds.
  CHECK(std::pair(animation.rows[3].colors[0].step_rows,
                  animation.rows[3].colors[1].step_rows) ==
        std::pair{RgbBytes{61, 30, 47}, RgbBytes{31, 31, 31}});

  const RowSnapshot before_end = animator.RowLegs(3, frames - 1, 1).front();
  CHECK(before_end.active);
  CHECK(before_end.first != Pack(31, 0, 17));
  CHECK(animator.RowLegs(3, 1, 1).front() ==
        RowSnapshot{Pack(31, 0, 17), 0, false, ColorTransform::kOffsetCurrent});
  CHECK(animation.refused_steps == 0);
}

TEST_CASE("CommitClutRows uploads the animated rows once per change",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  REQUIRE(animator.Load(3, 0, {Pack(31, 0, 17)}));
  REQUIRE(animation.upload_pending);
  Render::CommitClutRows(animation, animator.host);
  CHECK(animator.host.uploaded_rows[3][0] == Pack(31, 0, 17));
  CHECK_FALSE(animation.upload_pending);
  Render::CommitClutRows(animation, animator.host);
  CHECK(animator.host.row_uploads == 1);

  // A running transition marks the rows every frame.
  REQUIRE(Render::BuildClutTransition(animation, 8, 3, RowOf(Pack(1, 1, 1))));
  animation.upload_pending = false;
  animator.UpdateRows(1);
  Render::CommitClutRows(animation, animator.host);
  CHECK(animator.host.row_uploads == 2);
}

TEST_CASE("CLUT pulses alternate between the full and half offsets",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  REQUIRE(animator.Load(5, 0, {Pack(10, 10, 10)}));
  Render::ApplyClutRowTransform(animation, ColorTransform::kPulse, 2, 5,
                                {4, 4, 4});
  CHECK(animation.rows[5].pulse == std::array<std::int8_t, 3>{4, 4, 4});

  std::vector<RowSnapshot> legs = animator.RowLegs(5, 4, 3);
  // Cancelling ends the pulse after the leg already running.
  Render::ApplyClutRowTransform(animation, ColorTransform::kCancelPulse, 0, 5,
                                {});
  legs.push_back(animator.Row(5));
  legs.push_back(animator.RowLegs(5, 4, 1).front());
  CHECK(legs ==
        std::vector<RowSnapshot>{
            {Pack(14, 14, 14), 0, true, ColorTransform::kPulseReturn},
            {Pack(12, 12, 12), 0, true, ColorTransform::kPulse},
            {Pack(14, 14, 14), 0, true, ColorTransform::kPulseReturn},
            {Pack(14, 14, 14), 0, true, ColorTransform::kOffsetCurrent},
            {Pack(12, 12, 12), 0, false, ColorTransform::kOffsetCurrent}});
}

TEST_CASE("CLUT fade-then-restore finishes on the row base",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  REQUIRE(animator.Load(6, 0, {Pack(10, 10, 10)}));
  Render::ApplyClutRowTransform(animation, ColorTransform::kFadeThenRestore, 2,
                                6, {-6, -6, -6});
  CHECK(animator.RowLegs(6, 4, 2) ==
        std::vector<RowSnapshot>{
            {Pack(4, 4, 4), 0, true, ColorTransform::kRestoreBaseFinal},
            {Pack(10, 10, 10), 0, false, ColorTransform::kRestoreBaseFinal}});
}

TEST_CASE("A disabled animator ignores the gated CLUT requests",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  REQUIRE(animator.Load(2, 0, {Pack(5, 5, 5)}));
  // The original kept the low 16 bits of the flag.
  Render::SetClutAnimationEnabled(animation, 0x10000);
  REQUIRE_FALSE(animation.enabled);
  const auto before = std::make_unique<PaletteAnimation>(animation);

  const std::array accepted{
      Render::TransformClutRow(animation, ColorTransform::kInvert, 0, 2, {}),
      Render::StartClutTransition(animation, 0, 2, RowOf(1), 0),
      Render::StartClutTransitionWithBase(animation, 0, 2, RowOf(1), 0),
      Render::SaveClutBase(animation, 2)};
  CHECK(accepted == std::array{true, true, true, true});
  CHECK(animation == *before);
  Render::SetClutAnimationEnabled(animation, 0x10001);
  CHECK(animation.enabled);
}

TEST_CASE("CLUT transitions refuse rows and sources the original overran",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  const std::vector<std::uint16_t> short_source(255, 1);
  const std::vector<std::uint16_t> offset_source(272, 1);
  constexpr std::size_t kPastRows = Render::kAnimatedClutRowCount;
  const std::array accepted{
      Render::TransformClutRow(animation, ColorTransform::kInvert, 0, kPastRows,
                               {}),
      Render::BuildClutTransition(animation, 0, 2, short_source),
      Render::StartClutTransition(animation, 0, 2, RowOf(1), 1),
      Render::StartClutTransition(animation, 0, 2, offset_source, 1),
      Render::StopClutTransition(animation, kPastRows),
      Render::SaveClutBase(animation, kPastRows)};
  CHECK(accepted == std::array{false, false, false, true, false, false});
}

TEST_CASE("LoadClutColors refuses spans past the row or VRAM",
          "[render-palette]") {
  Animator animator;
  const std::vector<std::uint16_t> seven(7, 1);
  const std::array accepted{
      animator.Load(2, 250, seven), animator.Load(2, 249, seven),
      animator.Load(2, 257, {}),    animator.Load(2, 256, {}),
      animator.Load(288, 0, {1}),   animator.Load(287, 0, {1})};
  CHECK(accepted == std::array{false, true, false, true, false, true});
  CHECK(animator.host.uploads.size() == 1);
}

TEST_CASE("Delta-table rows and frames outside their table hold",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  REQUIRE(animator.Load(2, 0, {Pack(5, 5, 5)}));
  REQUIRE(Render::BuildClutTransition(animation, 8, 2, RowOf(Pack(6, 6, 6))));
  animation.rows[2].colors[0].step_rows[1] = 200;
  animator.UpdateRows(16);
  CHECK(animation.refused_steps == 16);
  CHECK(animation.rows[2].colors[0].rgb == RgbBytes{6, 5, 6});

  REQUIRE(Render::BuildClutTransition(animation, 8, 2, RowOf(Pack(6, 6, 6))));
  animation.rows[2].timer.frame = 16;
  animator.UpdateRows(1);
  CHECK(animation.refused_steps == 17);
  CHECK_FALSE(animation.rows[2].timer.active);
}

TEST_CASE("LoadClutColors fills an animated row and stops its transition",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  const Render::ClutRowAnimation& row = animation.rows[4];
  REQUIRE(Render::BuildClutTransition(animation, 8, 4, RowOf(Pack(9, 9, 9))));
  REQUIRE(row.timer.active);

  REQUIRE(animator.Load(4, 16, {Pack(1, 2, 3, 1), Pack(31, 0, 0)}));
  CHECK_FALSE(row.timer.active);
  CHECK(std::to_array({animation.colors[4][16], row.base[16], row.base[17],
                       row.base[18]}) ==
        std::array{Pack(1, 2, 3, 1), Pack(1, 2, 3, 1), Pack(31, 0, 0),
                   std::uint16_t{0}});
  CHECK(std::pair(row.colors[16].rgb, row.colors[16].mask) ==
        std::pair{RgbBytes{1, 2, 3}, std::uint8_t{1}});
  CHECK(animation.upload_pending);
  CHECK(animator.host.uploads.empty());
}

TEST_CASE("Locked rows ignore LoadClutColors and other rows go to VRAM",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  animation.rows[7].locked = true;
  const auto before = std::make_unique<PaletteAnimation>(animation);
  CHECK(animator.Load(7, 0, {Pack(3, 3, 3)}));
  CHECK(animation == *before);

  REQUIRE(animator.Load(14, 16, {Pack(1, 2, 3), Pack(4, 5, 6)}));
  REQUIRE(animator.host.uploads.size() == 1);
  const FakePaletteHost::ClutUpload& upload = animator.host.uploads[0];
  CHECK(std::pair(upload.row, upload.first_color) == std::pair{14U, 16U});
  CHECK(upload.colors ==
        std::vector<std::uint16_t>{Pack(1, 2, 3), Pack(4, 5, 6)});
}

TEST_CASE("CLUT base transforms keep zero colours zero and black blue",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  REQUIRE(Render::TransformClutBase(animation, ColorTransform::kHalveBase,
                                    BaseSource(), 1, {-1, -1, -1}, true));
  CHECK(SnapshotBase(animation, 1) ==
        BaseSnapshot{{0, Pack(4, 9, 14, 1), Pack(0, 0, 1)},
                     Pack(4, 9, 14, 1),
                     {4, 9, 14}});
  CHECK(animation.upload_pending);
}

TEST_CASE("CLUT base offsets leave a running transition's channels",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  const auto source = BaseSource();
  REQUIRE(Render::TransformClutBase(animation, ColorTransform::kHalveBase,
                                    source, 1, {-1, -1, -1}, true));
  animation.rows[1].timer.active = true;
  REQUIRE(Render::OffsetClutBase(animation, source, 1, {1, 1, 1}, true));
  const BaseSnapshot running = SnapshotBase(animation, 1);
  animation.upload_pending = false;
  REQUIRE(Render::OffsetClutBase(animation, source, 1, {2, 2, 2}, false));

  CHECK(running == BaseSnapshot{{0, Pack(11, 21, 31, 1), Pack(2, 2, 2)},
                                Pack(11, 21, 31, 1),
                                {4, 9, 14}});
  CHECK(SnapshotBase(animation, 1) ==
        BaseSnapshot{{0, Pack(12, 22, 31, 1), Pack(3, 3, 3)},
                     Pack(11, 21, 31, 1),
                     {4, 9, 14}});
  CHECK(animation.upload_pending);
  CHECK_FALSE(Render::OffsetClutBase(animation, source,
                                     Render::kAnimatedClutRowCount, {}, true));
}

TEST_CASE("The background gradient offsets and clamps the room colours",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  Render::CaptureRoomBackgroundColors(animation, RoomBackground());
  CHECK(animation.background.base[0] == RgbBytes{0x10, 0x20, 0x30});
  CHECK(animation.background.colors[4][2].value == 0x300000);

  // Unlike the tints, the gradient reads kRestoreBaseFinal as an offset.
  const std::array<std::pair<ColorTransform, ColorOffset>, 4> requests{{
      {ColorTransform::kOffsetCurrent, {1, 2, 3}},
      {ColorTransform::kRestoreBaseFinal, {1, 1, 1}},
      {ColorTransform::kOffsetCurrent, {300, -300, 0}},
      {ColorTransform::kRestoreBase, {}},
  }};
  std::vector<std::int32_t> reported;
  for (const auto& [mode, offset] : requests) {
    Render::TransformBackgroundGradient(animation, animator.host, mode, 0,
                                        offset);
    reported.push_back(animator.host.backgrounds.back()[0]);
  }
  CHECK(reported ==
        std::vector<std::int32_t>{0x332211, 0x342312, 0x3400FF, 0x302010});
}

TEST_CASE("The background gradient pulse alternates its offsets",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  Render::CaptureRoomBackgroundColors(animation, RoomBackground());
  Render::TransformBackgroundGradient(animation, animator.host,
                                      ColorTransform::kPulse, 2, {8, 8, 8});
  CHECK(animator.BackgroundLegs(4, 3) ==
        std::vector<std::pair<std::int32_t, ColorTransform>>{
            {0x382818, ColorTransform::kPulseReturn},
            {0x342414, ColorTransform::kPulse},
            {0x382818, ColorTransform::kPulseReturn}});
}

TEST_CASE("A disabled animator ignores the gated gradient and tint requests",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  FakePaletteHost& host = animator.host;
  Render::SetClutAnimationEnabled(animation, 0);
  const auto before = std::make_unique<PaletteAnimation>(animation);

  Render::RequestBackgroundGradientTransform(
      animation, host, ColorTransform::kOffsetCurrent, 0, {1, 1, 1});
  const std::array<RgbBytes, Render::kGradientColorCount> colors{};
  Render::SetBackgroundGradientColors(animation, host, 0, colors);
  Render::SetBackgroundGradientBase(animation, host, 0, colors);
  const std::array accepted{
      Render::SetModelTint(animation, host, 0, 1, {}),
      Render::SetModelTintBase(animation, host, 0, 1, {}),
      Render::RequestModelTintTransform(
          animation, host, ColorTransform::kOffsetCurrent, 0, 1, {})};
  CHECK(accepted == std::array{true, true, true});
  CHECK(animation == *before);
  CHECK(host.backgrounds.empty());
  CHECK(host.actor_tints.empty());
}

TEST_CASE("Model tints reach the actors as codes", "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  FakePaletteHost& host = animator.host;
  CHECK(animation.tints[5].base == RgbBytes{0x40, 0x40, 0x40});

  REQUIRE(Render::SetModelTint(animation, host, 0, 2, {0x50, 0x40, 0x30}));
  CHECK(
      animator.ActorTint(2) ==
      TintSnapshot{1, {0x10, 0, 0xF0}, ColorTransform::kOffsetCurrent, false});
  const std::array accepted{
      Render::SetModelTint(animation, host, 0, Render::kModelTintCount, {}),
      Render::RequestModelTintTransform(animation, host,
                                        ColorTransform::kOffsetCurrent, 0,
                                        Render::kModelTintCount, {})};
  CHECK(accepted == std::array{false, false});
}

TEST_CASE("The grey tint modes move the room tint's blend",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  FakePaletteHost& host = animator.host;
  // The room shader takes the blend / 4.
  REQUIRE(Render::TransformModelTint(animation, host, ColorTransform::kGreyBase,
                                     0, 0, {2, 0, 0}));
  CHECK(host.room_tints.back() == std::pair{TintCode{2, 0, 0}, 0x1000});

  REQUIRE(Render::TransformModelTint(animation, host,
                                     ColorTransform::kRestoreBase, 2, 0, {}));
  CHECK(animation.tints[0].blend_step == -0x1000);
  std::vector<std::int32_t> blends;
  for (int frame = 0; frame < 4; ++frame) {
    animator.UpdateTints(1);
    blends.push_back(host.room_tints.back().second);
  }
  CHECK(blends == std::vector<std::int32_t>{0xC00, 0x800, 0x400, 0});
  CHECK(host.room_tints.back().first == TintCode{0, 0, 0});
}

TEST_CASE("Model tints restore their base on kRestoreBaseFinal",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  FakePaletteHost& host = animator.host;
  REQUIRE(Render::SetModelTint(animation, host, 0, 3, {0x60, 0x60, 0x60}));
  REQUIRE(Render::TransformModelTint(
      animation, host, ColorTransform::kRestoreBaseFinal, 0, 3, {5, 5, 5}));
  CHECK(animator.ActorTint(3) ==
        TintSnapshot{2, {0, 0, 0}, ColorTransform::kOffsetCurrent, false});

  REQUIRE(Render::TransformModelTint(
      animation, host, ColorTransform::kFadeThenRestore, 1, 1, {-8, 0, 0}));
  CHECK(animator.TintLegs(1, 2, 2) ==
        std::vector<TintSnapshot>{
            {0, {0xF8, 0, 0}, ColorTransform::kRestoreBaseFinal, true},
            {0, {0, 0, 0}, ColorTransform::kRestoreBaseFinal, false}});
}

TEST_CASE("Model tint pulses alternate between the full and half offsets",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  FakePaletteHost& host = animator.host;
  REQUIRE(Render::RequestModelTintTransform(
      animation, host, ColorTransform::kPulse, 1, 4, {8, 0, -8}));
  std::vector<TintSnapshot> legs = animator.TintLegs(4, 2, 2);
  REQUIRE(Render::RequestModelTintTransform(
      animation, host, ColorTransform::kCancelPulse, 0, 4, {}));
  legs.push_back(animator.TintLegs(4, 2, 1).front());
  CHECK(legs == std::vector<TintSnapshot>{
                    {3, {8, 0, 0xF8}, ColorTransform::kPulseReturn, true},
                    {3, {4, 0, 0xFC}, ColorTransform::kPulse, true},
                    {3, {8, 0, 0xF8}, ColorTransform::kOffsetCurrent, false}});
}

TEST_CASE("The BATTLE actor wrapper drives the actor's row and tint",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  FakePaletteHost& host = animator.host;
  REQUIRE(animator.LoadEveryRow(Pack(10, 10, 10)));
  const auto before = std::make_unique<PaletteAnimation>(animation);
  Render::TransformActorColors(animation, host, ColorTransform::kOffsetCurrent,
                               0, Render::kColorTransformActorLimit, {1, 1, 1});
  CHECK(animation == *before);
  CHECK(host.actor_tints.empty());

  Render::TransformActorColors(animation, host, ColorTransform::kOffsetCurrent,
                               0, 2, {1, 1, 1});
  std::array<std::uint16_t, Render::kAnimatedClutRowCount> expected{};
  expected.fill(Pack(10, 10, 10));
  expected[6] = Pack(11, 11, 11);
  CHECK(FirstColors(animation) == expected);
  CHECK(host.actor_tints.back().first == 2);
}

TEST_CASE("The BATTLE room and scene wrappers drive their rows",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  FakePaletteHost& host = animator.host;
  REQUIRE(animator.LoadEveryRow(Pack(10, 10, 10)));
  Render::TransformRoomPalettes(animation, ColorTransform::kOffsetCurrent, 0,
                                {1, 1, 1});
  Render::TransformSceneColors(animation, host, ColorTransform::kOffsetCurrent,
                               0, {1, 1, 1});

  std::array<std::uint16_t, Render::kAnimatedClutRowCount> expected{};
  expected.fill(Pack(10, 10, 10));
  std::fill_n(expected.begin(), Render::kRoomClutRowCount, Pack(12, 12, 12));
  expected[Render::kSceneEffectClutRow] = Pack(11, 11, 11);
  CHECK(FirstColors(animation) == expected);
  CHECK(host.room_tints.size() == 1);
  CHECK(host.actor_tints.empty());
}

TEST_CASE("The unused room transition reads every row at clut_offset",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  const std::vector<std::uint16_t> source = CountingSource(784);
  CHECK_FALSE(Render::StartRoomPaletteTransition(
      animation, 0, std::span(source).first(783), 1));
  // The original dropped clut_offset for rows 1 and 2.
  REQUIRE(Render::StartRoomPaletteTransition(animation, 0, source, 1));
  CHECK(std::to_array({animation.colors[0][0], animation.colors[1][0],
                       animation.colors[2][255]}) ==
        std::array{source[16], source[272], source[783]});
}

TEST_CASE("Shared and based CLUT transitions read their sources",
          "[render-palette]") {
  Animator animator;
  PaletteAnimation& animation = animator.Get();
  const std::vector<std::uint16_t> source = CountingSource(784);
  CHECK_FALSE(
      Render::StartSharedPaletteTransition(animation, 0, 10, source, 0));
  REQUIRE(Render::StartSharedPaletteTransition(animation, 0, 9, source, 2));
  CHECK(animation.colors[13][0] == source[32]);
  // The base comes from the source's start, not from clut_offset.
  REQUIRE(Render::StartClutTransitionWithBase(animation, 0, 12, source, 2));
  CHECK(std::pair(animation.colors[12][0], animation.rows[12].base[0]) ==
        std::pair{source[32], source[0]});
}

// func_800AC168 (40564.c:2033); the reference colour is (12, 8, 6) with STP.
TEST_CASE("Actor palette transitions keep the original word arithmetic",
          "[render-palette][actors]") {
  const TransitionCase test = GENERATE(
      TransitionCase{Pack(10, 3, 7), PaletteTransition::kRemoveRedGreen, 2,
                     Pack(6, 0, 7, 1), true},
      TransitionCase{Pack(2, 2, 7), PaletteTransition::kRemoveRedGreen, 2,
                     Pack(0, 0, 7, 1), false},
      TransitionCase{Pack(10, 3, 7), PaletteTransition::kDarken, 1,
                     Pack(6, 0, 3, 1), true},
      TransitionCase{Pack(3, 3, 3), PaletteTransition::kDarken, 1,
                     Pack(0, 0, 0, 1), false},
      TransitionCase{Pack(10, 3, 7), PaletteTransition::kWhite, 1, 0xFFFF,
                     false},
      TransitionCase{Pack(10, 28, 7), PaletteTransition::kBrightenGreenBlue, 3,
                     Pack(13, 31, 13, 1), false},
      TransitionCase{Pack(20, 10, 5), PaletteTransition::kRestore, 2,
                     Pack(16, 8, 6, 1), true},
      TransitionCase{Pack(13, 8, 6), PaletteTransition::kRestore, 2,
                     Pack(12, 8, 6, 1), false},
      TransitionCase{Pack(10, 3, 7), PaletteTransition::kSlowDarken, 2,
                     Pack(8, 1, 5, 1), true},
      TransitionCase{Pack(10, 3, 7), PaletteTransition::kBlack, 2, 0x8000,
                     false},
      TransitionCase{Pack(10, 3, 7), kUnnamedTransition, 2, Pack(10, 3, 7),
                     false},
      // A negative amount raises the channels past 31; the original ORed the
      // words, so they carry into their neighbours: 50 | 43 << 5 | 47 << 10.
      TransitionCase{Pack(10, 3, 7), PaletteTransition::kSlowDarken, -40,
                     0xBD72, true});
  const std::array<std::uint16_t, 1> reference{Pack(12, 8, 6, 1)};
  std::array<std::uint16_t, 1> colors{test.color};
  const bool changed = VS::Actors::ApplyPaletteTransition(
      colors, reference, test.mode, test.amount);
  CHECK(std::pair(colors[0], changed) ==
        std::pair{test.expected, test.changed});
}

TEST_CASE("Actor palette transitions cover the shorter span",
          "[render-palette][actors]") {
  const std::array<std::uint16_t, 2> reference{};
  std::array<std::uint16_t, 3> colors{Pack(9, 9, 9), Pack(9, 9, 9),
                                      Pack(9, 9, 9)};
  CHECK(VS::Actors::ApplyPaletteTransition(colors, reference,
                                           PaletteTransition::kSlowDarken, 1));
  CHECK(colors ==
        std::array{Pack(8, 8, 8, 1), Pack(8, 8, 8, 1), Pack(9, 9, 9)});
}

TEST_CASE("The live palette adapters refuse what the original overran",
          "[render-palette][live]") {
  const LiveClutGuard guard;
  const std::array<u_short, 7> colors{1, 2, 3, 4, 5, 6, 7};
  const std::array refused{
      RefusalsOf([] { Render::LoadLiveClutColors(nullptr, 20, 0, 4); }),
      RefusalsOf([] { Render::LoadLiveClutColors(nullptr, 20, 0, 0); }),
      RefusalsOf(
          [&] { Render::LoadLiveClutColors(colors.data(), 20, 250, 7); }),
      RefusalsOf([&] { Render::LoadLiveClutColors(colors.data(), 0, 256, 1); }),
      RefusalsOf([] { Render::TransformLiveActorColors(0, 0, -1, 0, 0, 0); }),
      RefusalsOf([] { Render::TransformLiveActorColors(0, 0, 9, 0, 0, 0); }),
      RefusalsOf([] { Render::TransformLiveSceneColors(256, 0, 0, 0, 0); }),
      RefusalsOf([] { Render::TransformLiveSceneColors(-1, 0, 0, 0, 0); }),
      RefusalsOf(
          [] { Render::TransformLiveBackgroundColors(0, 0x8000, 0, 0, 0); })};
  CHECK(refused == std::array<std::uint64_t, 9>{1, 0, 1, 1, 1, 0, 1, 1, 1});
}

TEST_CASE("A live direct CLUT row reaches its VRAM line",
          "[render-palette][live]") {
  const LiveClutGuard guard;
  const std::array<u_short, 3> colors{Pack(1, 2, 3), Pack(4, 5, 6),
                                      Pack(7, 8, 9)};
  Render::LoadLiveClutColors(colors.data(), 20, 8, 3);
  CHECK(ReadVram({768 + 8, 224 + 20, 3, 1}) ==
        std::vector<std::uint16_t>{colors.begin(), colors.end()});
}

TEST_CASE("Live room palettes keep the red offset's low 16 bits",
          "[render-palette][live]") {
  const LiveClutGuard guard;
  REQUIRE(Render::LoadLiveClutRow(
      0, 0, std::array<std::uint16_t, 1>{Pack(10, 10, 10)}));
  Render::TransformLiveRoomPalettes(0, 0, 0x10001, 0, 0);
  CHECK(Render::LivePaletteAnimation().colors[0][0] == Pack(11, 10, 10));
  Render::CommitLiveClutRows();
  CHECK_FALSE(Render::LivePaletteAnimation().upload_pending);
  CHECK(ReadVram({768, 224, 1, 1}) ==
        std::vector<std::uint16_t>{Pack(11, 10, 10)});
}
