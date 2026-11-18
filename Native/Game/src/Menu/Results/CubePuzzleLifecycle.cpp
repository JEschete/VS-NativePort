// Hand-maintained (tools/port/owned.csv).
#include "Menu/Results/CubePuzzleLifecycle.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

#include "Core/Math/ScalarMath.hpp"
#include "Menu/Results/CubePuzzleText.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsPalette.hpp"
#include "Render/DisplayControl.hpp"
#include "Render/TimImage.hpp"

namespace VS::Menu::Results {
namespace {
namespace math = Core::Math;

constexpr std::size_t kPixelBytes = 2;
constexpr unsigned kBitsPerByte = 8;
constexpr std::size_t kPaletteBytes = kResultsClutWords * kPixelBytes;
constexpr std::size_t kRecordZone = 0;
constexpr std::size_t kRecordMap = 2;
constexpr std::size_t kRecordPar = 4;
constexpr std::size_t kRecordCap = 6;

// The rsin result divided by 512 (rounding toward zero), then centred on 8.
constexpr std::int32_t kPulseDivisor = 512;
constexpr std::int32_t kPulseCentre = 8;
constexpr std::uint32_t kAnimationStep = 64;
constexpr std::uint32_t kAnimationMask = 0xFFF;

// The puzzle clock in hundredths of a second.
constexpr std::int32_t kHundredthsPerMinute = 6000;
constexpr std::int32_t kHundredthsPerSecond = 100;
constexpr std::int32_t kHundredthsPerRankStep = 200;
constexpr std::int32_t kTopRank = 15;

// Sound effects in group 0x7E; zero stops the whole group.
constexpr std::int32_t kAllEffects = 0;
constexpr std::int32_t kRevealSfx = 0x7D;
constexpr std::int32_t kCountingSfx = 0x72;
constexpr std::int32_t kCountedSfx = 0x73;
constexpr std::int32_t kQuitSfx = 0x7A;

// Where each screen's reveals start.
constexpr ResultsPoint kPromptOrigin{.x = 214, .y = 187};
constexpr ResultsPoint kEvolveOrDieOrigin{.x = 160, .y = 64};
constexpr ResultsPoint kAverageTimeOrigin{.x = 128, .y = 116};
constexpr ResultsPoint kParTimeOrigin{.x = 192, .y = 144};
constexpr ResultsPoint kCountingTimeOrigin{.x = 88, .y = 64};
constexpr ResultsPoint kFoodChainOrigin{.x = 160, .y = 100};
constexpr ResultsPoint kRankOrigin{.x = 160, .y = 124};
constexpr ResultsPoint kGivingUpOrigin{.x = 0, .y = 64};
constexpr ResultsPoint kChickenOrigin{.x = 0, .y = 100};
constexpr std::int32_t kStartAverageTimeDelay = 15;
constexpr std::int32_t kStartParTimeDelay = 30;
constexpr std::int32_t kStartSecondSfx = 30;
constexpr std::int32_t kResultsCountingSfx = 16;
constexpr std::int32_t kResultsCountedSfx = 64;
constexpr std::int32_t kResultsFirstRevealSfx = 67;
constexpr std::int32_t kResultsFoodChainDelay = 75;
constexpr std::int32_t kResultsRankDelay = 90;
constexpr std::int32_t kQuitChickenDelay = 15;

[[nodiscard]] std::int16_t ReadInt16(std::span<const std::byte> bytes,
                                     std::size_t offset) {
  return static_cast<std::int16_t>(
      std::to_integer<std::uint16_t>(bytes[offset]) |
      (std::to_integer<std::uint16_t>(bytes[offset + 1]) << kBitsPerByte));
}

// Uploads image `index` and, for the first, its palette. The original wrote
// the upload rectangles into the file's TIM headers first; nothing reads them
// before the buffer is freed, so the native upload passes them directly.
void UploadImage(std::span<std::byte> asset, std::size_t index,
                 std::span<std::uint16_t, kResultsClutWords> clut,
                 CubePuzzleLoadHost& host) {
  const std::size_t offset = index * kCubePuzzleTimStride;
  if (offset >= asset.size()) {
    host.Refuse(CubePuzzleRefusal::kImage);
    return;
  }
  const std::span<std::byte> bytes = asset.subspan(offset);
  const std::optional<Render::TimImage> tim = Render::ParseTim(bytes);
  if (!tim.has_value()) {
    host.Refuse(CubePuzzleRefusal::kImage);
    return;
  }
  const auto width = static_cast<std::size_t>(tim->image.area.width);
  const std::size_t image_bytes =
      width * kPixelBytes * static_cast<std::size_t>(kCubePuzzleImageRows);
  if (image_bytes > tim->image.pixel_bytes) {
    host.Refuse(CubePuzzleRefusal::kImage);
  } else {
    const Render::ScreenRect area{
        .x = static_cast<std::int16_t>(
            kCubePuzzleImageX +
            (static_cast<std::int32_t>(index) * kCubePuzzleImageStep)),
        .y = kCubePuzzleImageY,
        .width = tim->image.area.width,
        .height = kCubePuzzleImageRows};
    host.UploadImage(area, bytes.subspan(tim->image.pixel_offset, image_bytes));
  }
  if (index != 0 || !tim->clut.has_value()) {
    return;
  }
  if (tim->clut->pixel_bytes < kPaletteBytes) {
    host.Refuse(CubePuzzleRefusal::kPalette);
    return;
  }
  const std::span<std::byte> palette =
      bytes.subspan(tim->clut->pixel_offset, kPaletteBytes);
  std::ranges::fill(palette.first(kCubePuzzleClearedColors * kPixelBytes),
                    std::byte{0});
  host.UploadImage(kResultsClutArea, palette);
  for (std::size_t color = 0; color < kResultsClutWords; ++color) {
    clut[color] =
        static_cast<std::uint16_t>(ReadInt16(palette, color * kPixelBytes));
  }
}

// The shared steps of _initCubePuzzleStart and _initCubePuzzleQuit.
CubePuzzleLoad InitializeScreen(
    CubePuzzleScreen& screen, CubePuzzleAsset asset, std::uint32_t pages,
    std::size_t images, CubePuzzleRating* rating,
    std::span<std::uint16_t, kResultsClutWords> clut,
    CubePuzzleLoadHost& host) {
  if (screen.load_step == 0) {
    host.ReserveTexturePages(kCubePuzzleFirstPage, pages, kCubePuzzlePageOwner);
    host.BeginAssetRead(asset);
    screen.load_step = 1;
    return CubePuzzleLoad::kLoading;
  }
  if (screen.load_step == 1) {
    if (!host.AssetReadLoaded()) {
      return CubePuzzleLoad::kLoading;
    }
    const std::span<std::byte> bytes = host.LoadedAsset();
    for (std::size_t index = 0; index < images; ++index) {
      UploadImage(bytes, index, clut, host);
    }
    if (rating != nullptr) {
      *rating = {};
      if (bytes.size() < kCubePuzzleRecordOffset + (kCubePuzzleRecordCount *
                                                    kCubePuzzleRecordBytes)) {
        host.Refuse(CubePuzzleRefusal::kRecords);
      } else if (const std::optional<CubePuzzleRoom> room = host.CurrentRoom();
                 !room.has_value()) {
        host.Refuse(CubePuzzleRefusal::kRoom);
      } else if (const std::optional<CubePuzzleRecord> record =
                     FindCubePuzzleRecord(bytes, *room);
                 record.has_value()) {
        *rating = {.par_seconds = record->par_seconds,
                   .rank_cap = record->rank_cap};
      }
    }
    host.ReleaseAssetRead();
    screen.load_step = 2;
    return CubePuzzleLoad::kLoading;
  }
  host.FreeAssetBuffer();
  screen = {};
  return CubePuzzleLoad::kFinished;
}

// The shared countdown end: Circle at or past `end` finishes, earlier skips
// there. Both stop the group's sounds.
[[nodiscard]] bool ConfirmFinishes(CubePuzzleScreen& screen,
                                   bool confirm_pressed, std::int32_t end,
                                   CubePuzzleScreenHost& host) {
  if (screen.timer < kCubePuzzleTimerLimit) {
    ++screen.timer;
  }
  if (!confirm_pressed) {
    return false;
  }
  host.StopSfx(kCubePuzzleSfxGroup, kAllEffects);
  if (screen.timer >= end) {
    return true;
  }
  screen.timer = end;
  return false;
}

void Pulse(CubePuzzleScreen& screen, CubePuzzleScreenHost& host) {
  host.PulsePalette(CubePuzzlePulseWeight(screen.animation));
  screen.animation = (screen.animation + kAnimationStep) & kAnimationMask;
}

}  // namespace

std::optional<CubePuzzleRecord> FindCubePuzzleRecord(
    std::span<const std::byte> asset, CubePuzzleRoom room) {
  if (asset.size() < kCubePuzzleRecordOffset +
                         (kCubePuzzleRecordCount * kCubePuzzleRecordBytes)) {
    return std::nullopt;
  }
  for (std::size_t index = 0; index < kCubePuzzleRecordCount; ++index) {
    const std::span<const std::byte> record = asset.subspan(
        kCubePuzzleRecordOffset + (index * kCubePuzzleRecordBytes),
        kCubePuzzleRecordBytes);
    if (ReadInt16(record, kRecordZone) == room.zone_id &&
        ReadInt16(record, kRecordMap) == room.map_id) {
      return CubePuzzleRecord{.zone_id = room.zone_id,
                              .map_id = room.map_id,
                              .par_seconds = ReadInt16(record, kRecordPar),
                              .rank_cap = ReadInt16(record, kRecordCap)};
    }
  }
  return std::nullopt;
}

// decomp: MENUF.PRG _initCubePuzzleStart (src/MENU/MENUF.PRG/3B8.c:2260),
// 0x80107A8C
CubePuzzleLoad InitializeCubePuzzleStart(
    CubePuzzleScreen& screen, CubePuzzleRating& rating,
    std::span<std::uint16_t, kResultsClutWords> clut,
    CubePuzzleLoadHost& host) {
  return InitializeScreen(screen, CubePuzzleAsset::kIq, kCubePuzzleStartPages,
                          kCubePuzzleStartImages, &rating, clut, host);
}

// decomp: MENUF.PRG _initCubePuzzleEnd (src/MENU/MENUF.PRG/3B8.c:2351),
// 0x80107D4C
CubePuzzleLoad InitializeCubePuzzleResults(
    CubePuzzleScreen& screen, CubePuzzleRating& rating, CubePuzzleTimer& timer,
    std::span<std::uint16_t, kResultsClutWords> clut,
    CubePuzzleLoadHost& host) {
  if (timer.show_maximum != 0) {
    timer.clock = {.minutes = kCubePuzzleMaximumMinutes,
                   .seconds = kCubePuzzleMaximumSeconds,
                   .hundredths = kCubePuzzleMaximumHundredths};
    timer.show_maximum = 0;
  }
  return InitializeCubePuzzleStart(screen, rating, clut, host);
}

// decomp: MENUF.PRG _initCubePuzzleQuit (src/MENU/MENUF.PRG/3B8.c:2363),
// 0x80107D98
CubePuzzleLoad InitializeCubePuzzleQuit(
    CubePuzzleScreen& screen, std::span<std::uint16_t, kResultsClutWords> clut,
    CubePuzzleLoadHost& host) {
  return InitializeScreen(screen, CubePuzzleAsset::kEsc, kCubePuzzleQuitPages,
                          kCubePuzzleQuitImages, nullptr, clut, host);
}

std::int32_t CubePuzzlePulseWeight(std::uint32_t animation) {
  return (math::Sine(static_cast<std::int32_t>(animation)) / kPulseDivisor) +
         kPulseCentre;
}

std::int32_t CubePuzzleRank(CubePuzzleClock clock, CubePuzzleRating rating) {
  const std::int32_t over_par = (clock.minutes * kHundredthsPerMinute) +
                                (clock.seconds * kHundredthsPerSecond) +
                                clock.hundredths -
                                (rating.par_seconds * kHundredthsPerSecond);
  if (over_par <= 0) {
    return kTopRank - rating.rank_cap;
  }
  const std::int32_t steps = ((over_par - 1) / kHundredthsPerRankStep) + 1;
  return std::clamp(kTopRank - (rating.rank_cap + steps), 0, kTopRank);
}

// decomp: MENUF.PRG _renderCubePuzzleStart (src/MENU/MENUF.PRG/3B8.c:2432),
// 0x80107FC0
CubePuzzleScreenStep UpdateCubePuzzleStart(CubePuzzleScreen& screen,
                                           bool confirm_pressed,
                                           CubePuzzleScreenHost& host) {
  if (screen.screen_state != 0) {
    return CubePuzzleScreenStep::kShowing;
  }
  const std::int32_t timer = screen.timer;
  if (timer == 0 || timer == kStartSecondSfx) {
    host.PlaySfx(kCubePuzzleSfxGroup, kRevealSfx);
  }
  host.DrawMessage(CubePuzzleMessage::kEvolveOrDie, kEvolveOrDieOrigin, timer);
  host.DrawMessage(CubePuzzleMessage::kAverageTime, kAverageTimeOrigin,
                   math::WrappingSubtract(timer, kStartAverageTimeDelay));
  const std::int32_t reveal = math::WrappingSubtract(timer, kStartParTimeDelay);
  host.DrawMessage(CubePuzzleMessage::kParTime, kParTimeOrigin, reveal);
  host.DrawPrompt(kPromptOrigin, reveal, timer);
  if (!ConfirmFinishes(screen, confirm_pressed, kCubePuzzleStartEnd, host)) {
    return CubePuzzleScreenStep::kShowing;
  }
  host.RetireTexturePages(kCubePuzzleFirstPage, kCubePuzzleStartPages,
                          kCubePuzzlePageOwner);
  return CubePuzzleScreenStep::kFinished;
}

// decomp: MENUF.PRG _renderCubePuzzleEnd (src/MENU/MENUF.PRG/3B8.c:2477),
// 0x801080E4
CubePuzzleScreenStep UpdateCubePuzzleResults(CubePuzzleScreen& screen,
                                             CubePuzzleRating rating,
                                             CubePuzzleClock clock,
                                             bool confirm_pressed,
                                             std::uint32_t& misc_score,
                                             CubePuzzleScreenHost& host) {
  Pulse(screen, host);
  if (screen.screen_state != 0) {
    return CubePuzzleScreenStep::kShowing;
  }
  const std::int32_t rank = CubePuzzleRank(clock, rating);
  const std::int32_t timer = screen.timer;
  if (timer == kResultsCountingSfx) {
    host.PlaySfx(kCubePuzzleSfxGroup, kCountingSfx);
  }
  if (timer == kResultsCountedSfx) {
    host.StopSfx(kCubePuzzleSfxGroup, kCountingSfx);
    host.PlaySfx(kCubePuzzleSfxGroup, kCountedSfx);
  }
  if (timer == kResultsFirstRevealSfx || timer == kResultsFoodChainDelay ||
      timer == kResultsRankDelay) {
    host.PlaySfx(kCubePuzzleSfxGroup, kRevealSfx);
  }
  host.DrawMessage(CubePuzzleMessage::kCountingTime, kCountingTimeOrigin,
                   timer);
  host.DrawMessage(CubePuzzleMessage::kFoodChain, kFoodChainOrigin,
                   math::WrappingSubtract(timer, kResultsFoodChainDelay));
  const std::int32_t reveal = math::WrappingSubtract(timer, kResultsRankDelay);
  host.DrawRank(kRankOrigin, reveal, rank);
  host.DrawPrompt(kPromptOrigin, reveal, timer);
  if (!ConfirmFinishes(screen, confirm_pressed, kCubePuzzleResultsEnd, host)) {
    return CubePuzzleScreenStep::kShowing;
  }
  host.RetireTexturePages(kCubePuzzleFirstPage, kCubePuzzleStartPages,
                          kCubePuzzlePageOwner);
  // Issue #409: the original adds _puzzleModeRankPoints[rank] unchecked; an
  // at-par rank from a cap outside 0-15 would read past the table.
  if (rank < 0 || std::cmp_greater_equal(rank, kCubePuzzleRankPoints.size())) {
    return CubePuzzleScreenStep::kFinishedRankOutsideTable;
  }
  misc_score += kCubePuzzleRankPoints.at(static_cast<std::size_t>(rank));
  return CubePuzzleScreenStep::kFinished;
}

// decomp: MENUF.PRG _renderCubePuzzleQuit (src/MENU/MENUF.PRG/3B8.c:2566),
// 0x801083AC
CubePuzzleScreenStep UpdateCubePuzzleQuit(CubePuzzleScreen& screen,
                                          bool confirm_pressed,
                                          CubePuzzleScreenHost& host) {
  Pulse(screen, host);
  if (screen.screen_state != 0) {
    return CubePuzzleScreenStep::kShowing;
  }
  const std::int32_t timer = screen.timer;
  if (timer == 0 || timer == kQuitChickenDelay) {
    host.PlaySfx(kCubePuzzleSfxGroup, kQuitSfx);
  }
  host.DrawMessage(CubePuzzleMessage::kGivingUp, kGivingUpOrigin, timer);
  const std::int32_t reveal = math::WrappingSubtract(timer, kQuitChickenDelay);
  host.DrawMessage(CubePuzzleMessage::kChicken, kChickenOrigin, reveal);
  host.DrawPrompt(kPromptOrigin, reveal, timer);
  if (!ConfirmFinishes(screen, confirm_pressed, kCubePuzzleQuitEnd, host)) {
    return CubePuzzleScreenStep::kShowing;
  }
  host.RetireTexturePages(kCubePuzzleFirstPage, kCubePuzzleQuitPages,
                          kCubePuzzlePageOwner);
  return CubePuzzleScreenStep::kFinished;
}
}  // namespace VS::Menu::Results
