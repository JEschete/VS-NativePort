// Hand-maintained (tools/port/owned.csv).
#include "Ending/ClearResultsCore.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <string>

#include "Core/Log.hpp"
#include "Ending/Credits/StaffRollCore.hpp"
#include "Render/TimImage.hpp"

namespace VS::Ending {
namespace {
constexpr int kMinutesPerHour = 60;
constexpr std::uint32_t kTenHoursMinutes = 600;
constexpr std::uint32_t kNineHoursMinutes = 540;
// A clear in nine to ten hours scores a quarter more.
constexpr std::uint32_t kNineHoursBonusDivisor = 4;
constexpr std::uint32_t kFiveHoursMinutes = 300;
constexpr std::uint8_t kClearCountLimit = 99;
constexpr std::uint8_t kFirstClearDifficulty = 1;
constexpr std::uint8_t kClearEntryZone = 1;
constexpr std::uint8_t kClearEntryRoom = 0;

// Title bits.
constexpr std::uint32_t kTitleUnderTenHours = 1U << 1U;
constexpr unsigned kTitleFirstBoss = 4;
constexpr std::uint32_t kTitleGoldKey = 1U << 12U;
constexpr std::uint32_t kTitleChestKey = 1U << 13U;
// Actions 184-223 all unlocked.
constexpr std::uint32_t kTitleUpperActions = 1U << 14U;
// Actions 22-53, except 33, 36, 38 and 39, all unlocked.
constexpr std::uint32_t kTitleLowerActions = 1U << 15U;
constexpr std::uint32_t kTitleNeverSaved = 1U << 16U;
constexpr std::uint32_t kTitleNoMagic = 1U << 17U;
constexpr std::uint32_t kTitleNoBattleAbilities = 1U << 18U;
constexpr std::uint32_t kTitleNoBreakArts = 1U << 19U;
constexpr std::uint32_t kRankedTitleMask = 0xFFFF;
constexpr std::size_t kUpperActionsFirst = 184;
constexpr std::size_t kUpperActionsEnd = 224;
constexpr std::size_t kLowerActionsFirst = 22;
constexpr std::size_t kLowerActionsEnd = 54;
constexpr std::array<std::size_t, 4> kLowerActionsExcluded{33, 36, 38, 39};

constexpr std::array<std::uint32_t, kEnemyClassCount> kEnemyKillPoints{
    20, 20, 40, 80, 100, 60,
};
constexpr std::array<std::uint32_t, kWeaponCategoryCount> kWeaponAttackPoints{
    20, 40, 20, 100, 60, 100, 60, 100, 80, 60,
};
constexpr std::uint32_t kBossTrialPoints = 20000;
constexpr std::uint32_t kClearPoints = 100000;

// Chain points per link: below each limit, the multiplier beside it.
struct ChainBonus {
  std::uint16_t below = 0;
  std::uint32_t points = 0;
};
constexpr std::array<ChainBonus, 7> kChainBonuses{{
    {6, 10},
    {11, 20},
    {16, 40},
    {21, 80},
    {26, 160},
    {31, 320},
    {51, 640},
}};
constexpr std::uint32_t kLongChainPoints = 1280;

constexpr std::uint32_t kPercent = 100;
constexpr std::uint32_t kCountableRooms = 361;

// Load phases.
constexpr int kLoadAtlas = 0;
constexpr int kUploadAtlas = 1;
constexpr int kLoadRankScreen = 2;
constexpr int kUploadRankScreen = 3;
constexpr int kLoadDone = 4;

constexpr std::size_t kAtlasImages = 3;
constexpr std::size_t kAtlasImageStride = 0x8220;
constexpr std::int16_t kAtlasLeft = 832;
constexpr std::int16_t kAtlasPageWidth = 64;
constexpr std::int16_t kAtlasTop = 0x100;
constexpr std::int16_t kAtlasHeight = 0xFF;
constexpr Render::ScreenRect kAtlasPaletteArea{0x300, 0x1FF, 0x80, 1};
constexpr std::size_t kAtlasPaletteBytes = 0x100;
// The palette's first two colors are cleared before it is uploaded.
constexpr std::size_t kAtlasPaletteClearedBytes = 4;
constexpr std::int16_t kRankScreenLeft = 0;
constexpr std::int16_t kRankScreenTop = 0x100;
constexpr std::size_t kHalfwordBytes = 2;

// Screen frames and positions.
constexpr int kRankRevealFrame = 0xD0;
constexpr int kScoreFrame = 0x20;
constexpr int kMapCompletionFrame = 0x70;
constexpr int kRankHeadingFrame = 0xC0;
constexpr int kHeldFrame = 0x1000;
constexpr int kLeaveFrames = 8;
constexpr int kCenterX = 0xA0;
constexpr ScreenPoint kCongratulationsOrigin{kCenterX, 0x28};
constexpr ScreenPoint kScoreOrigin{kCenterX, 0x50};
constexpr ScreenPoint kMapCompletionOrigin{kCenterX, 0x66};
constexpr ScreenPoint kRankHeadingOrigin{kCenterX, 0x8A};
constexpr ScreenPoint kRankOrigin{kCenterX, 0x9A};

constexpr int kSoundCounting = 0x72;
constexpr int kSoundCounted = 0x73;
constexpr int kSoundRankReveal = 0x77;
constexpr int kSoundRankRevealEcho = 0x78;

constexpr int kFadeMaximum = 64;
constexpr int kCountStartFade = 32;
constexpr int kCountShift = 5;
constexpr std::uint32_t kCountSteps = 32;
constexpr int kWipeSpeedShift = 3;

constexpr int kRankBackgroundPanels = 5;
constexpr int kRankBackgroundSlot = 0x7FE;
constexpr std::int16_t kRankBackgroundTop = 8;
constexpr std::int16_t kRankBackgroundBottom = 232;
constexpr std::uint8_t kRankBackgroundTexelBottom = 224;
constexpr std::uint16_t kRankBackgroundFirstPage = 16;
// getTPage(3, 0, 0, 0): the reserved colour depth the original names.
constexpr std::uint16_t kRankBackgroundPageBits = 0x180;

// Atlas regions: digits 0-9 come first.
constexpr int kRegionDigitSeparator = 0xA;
constexpr int kRegionRankHeading = 12;
constexpr int kRegionScoreLabel = 0x12;
constexpr int kRegionPercentSign = 0x13;
constexpr int kRegionScoreUnit = 0x14;
constexpr int kRegionMapLabel = 0x15;
constexpr int kRegionCongratulations = 22;
constexpr int kRegionCongratulationsEnd = 23;
constexpr int kRegionColon = 0x1A;

constexpr std::size_t kScoreDigits = 9;
constexpr std::size_t kMapDigits = 3;
// The score's thousands separators follow these digits.
constexpr std::size_t kFirstSeparatorDigit = 2;
constexpr std::size_t kSecondSeparatorDigit = 5;
constexpr int kDigitAdvance = 12;
constexpr int kDigitDrop = 3;
constexpr int kColonDrop = 7;
constexpr int kColonGap = 2;
constexpr int kUnitDrop = 8;
constexpr int kSeparatorDrop = 0xE;
constexpr int kSeparatorGap = 3;
constexpr int kScoreDigitsWidth = 0x74;
constexpr int kMapDigitsWidth = 0x26;

struct Pen {
  int x = 0;
  int y = 0;
};

enum class RegionStyle : std::uint8_t { kTintFade, kFlashFade, kWipe };

// Draws atlas regions through the host and measures them; a region id outside
// the atlas draws nothing and measures 0.
class RegionPainter {
 public:
  RegionPainter(const ResultsTables& tables, ResultsHost& host)
      : atlas_(tables.atlas), host_(host) {}

  [[nodiscard]] int Width(int region_id) const {
    const Render::TextureRegion* region = Find(region_id);
    return region != nullptr ? region->width : 0;
  }

  void Draw(RegionStyle style, int region_id, Pen pen,
            const RegionColorPair& colors, int wipe_edge = 0) const {
    const Render::TextureRegion* region = Find(region_id);
    if (region == nullptr) return;
    switch (style) {
      case RegionStyle::kTintFade:
        Render::DrawTextureTintFade(*region, pen.x, pen.y, colors, host_);
        break;
      case RegionStyle::kFlashFade:
        Render::DrawTextureFlashFade(*region, pen.x, pen.y, colors, host_);
        break;
      case RegionStyle::kWipe:
        Render::DrawTextureWipe(*region, pen.x, pen.y, colors, wipe_edge,
                                host_);
        break;
    }
  }

 private:
  [[nodiscard]] const Render::TextureRegion* Find(int region_id) const {
    if (region_id < 0 || static_cast<std::size_t>(region_id) >= atlas_.size()) {
      VS_LOG_WARN("ending",
                  "Results region {} refused: the atlas has {} regions",
                  region_id, atlas_.size());
      return nullptr;
    }
    return &atlas_[static_cast<std::size_t>(region_id)];
  }

  std::span<const Render::TextureRegion> atlas_;
  ResultsHost& host_;
};

int ClampFade(int frame) { return std::clamp(frame, 0, kFadeMaximum); }

RegionColorPair Faded(RegionColorPair colors, int fade) {
  colors[0].step = static_cast<std::uint8_t>(fade);
  return colors;
}

int CountVisitedRooms(const ClearProgress& progress,
                      std::span<const std::uint32_t> countable_room_bits) {
  if (countable_room_bits.size() < kRoomFlagWords) {
    VS_LOG_WARN("ending",
                "Countable rooms table has {} of {} words; counting those",
                countable_room_bits.size(), kRoomFlagWords);
  }
  const std::size_t words =
      std::min(countable_room_bits.size(), progress.visited_rooms.size());
  int count = 0;
  for (std::size_t word = 0; word < words; ++word) {
    count +=
        std::popcount(progress.visited_rooms[word] & countable_room_bits[word]);
  }
  return count;
}

int CountOpenedChests(const PlaythroughFlags& flags) {
  return static_cast<int>(std::ranges::count_if(
      flags.chests_opened, [](std::uint8_t opened) { return opened != 0; }));
}

// Raises the best room and chest counts to this playthrough's.
void RaiseExplorationRecords(ClearProgress& progress,
                             std::span<const std::uint32_t> countable_rooms) {
  const auto rooms =
      static_cast<std::uint32_t>(CountVisitedRooms(progress, countable_rooms));
  const auto chests =
      static_cast<std::uint32_t>(CountOpenedChests(progress.flags));
  ScoreRecord& score = progress.score;
  score.map_completion = std::max(score.map_completion, rooms);
  score.opened_chest_count = std::max(score.opened_chest_count, chests);
}

bool AllActionsUnlocked(const ClearProgress& progress, std::size_t first,
                        std::size_t end,
                        std::span<const std::size_t> excluded) {
  for (std::size_t action = first; action < end; ++action) {
    if (std::ranges::find(excluded, action) != excluded.end()) continue;
    if (!progress.unlocked_actions[action]) return false;
  }
  return true;
}

void ResetPlaythrough(ClearProgress& progress, ProgressHost& host) {
  host.ResetPlaythroughRecords();
  progress.visited_rooms = {};
  PlaythroughFlags& flags = progress.flags;
  flags = {.clear_count = flags.clear_count,
           .difficulty = flags.difficulty,
           .normal_mode_cleared = flags.normal_mode_cleared};
}

// Uploads an image block with its rectangle moved to `area`'s origin and, when
// `area` has a height, that height; false when its pixels lie outside `data`.
bool UploadMoved(std::span<const std::byte> data, const Render::TimBlock& block,
                 Render::ScreenRect area, ResultsHost& host) {
  if (area.height == 0) area.height = block.area.height;
  area.width = block.area.width;
  const std::size_t bytes = static_cast<std::size_t>(area.width) *
                            static_cast<std::size_t>(area.height) *
                            kHalfwordBytes;
  if (area.width < 0 || area.height < 0 || block.pixel_offset > data.size() ||
      data.size() - block.pixel_offset < bytes) {
    VS_LOG_WARN("ending",
                "Results image refused: {} bytes at {} lie outside the {}-byte "
                "file",
                bytes, block.pixel_offset, data.size());
    return false;
  }
  host.LoadImage(area, data.subspan(block.pixel_offset, bytes));
  return true;
}

void UploadAtlasPalette(std::span<const std::byte> data,
                        const Render::TimBlock& palette, ResultsHost& host) {
  if (palette.pixel_offset > data.size() ||
      data.size() - palette.pixel_offset < kAtlasPaletteBytes) {
    VS_LOG_WARN("ending",
                "Results atlas palette refused: it lies outside the "
                "file");
    return;
  }
  std::array<std::byte, kAtlasPaletteBytes> colors{};
  std::memcpy(colors.data(), data.subspan(palette.pixel_offset).data(),
              colors.size());
  std::fill_n(colors.begin(), kAtlasPaletteClearedBytes, std::byte{0});
  host.LoadImage(kAtlasPaletteArea, colors);
}

void UploadAtlas(std::span<const std::byte> data, ResultsHost& host) {
  for (std::size_t index = 0; index < kAtlasImages; ++index) {
    const std::size_t offset = index * kAtlasImageStride;
    const auto tim = offset <= data.size()
                         ? Render::ParseTim(data.subspan(offset))
                         : std::nullopt;
    if (!tim) {
      VS_LOG_WARN("ending", "Results atlas image {} refused: no TIM at {}",
                  index, offset);
      continue;
    }
    Render::TimBlock image = tim->image;
    image.pixel_offset += offset;
    const auto left = static_cast<std::int16_t>(
        kAtlasLeft + (static_cast<int>(index) * kAtlasPageWidth));
    UploadMoved(data, image, {left, kAtlasTop, 0, kAtlasHeight}, host);
    if (index == 0 && tim->clut) {
      Render::TimBlock palette = *tim->clut;
      palette.pixel_offset += offset;
      UploadAtlasPalette(data, palette, host);
    }
  }
}

void UploadRankScreen(std::span<const std::byte> data, ResultsHost& host) {
  const auto tim = Render::ParseTim(data);
  if (!tim) {
    VS_LOG_WARN("ending", "Rank screen refused: its {} bytes hold no TIM",
                data.size());
    return;
  }
  UploadMoved(data, tim->image, {kRankScreenLeft, kRankScreenTop, 0, 0}, host);
}

void ScoreFile(ResultsState& state, ClearProgress& progress,
               const ResultsTables& tables, ResultsHost& host) {
  host.FreeResultsData();
  state.displayed_map_completion = 0;
  state.displayed_score = 0;
  state.animation_frame = 0;
  state.is_leaving = false;
  state.clear_count = progress.flags.clear_count;
  RaiseExplorationRecords(progress, tables.countable_room_bits);
  state.map_completion = static_cast<int>(
      (progress.score.map_completion * kPercent) / kCountableRooms);
  CalculateClearScore(state, progress.score);
  DetermineRiskbreakerRank(state, progress.score, tables);
  const std::size_t file = static_cast<std::size_t>(state.rank) + 1;
  if (file >= kResultsFileCount) {
    VS_LOG_WARN("ending", "Rank screen {} refused: there are {} rank screens",
                state.rank, kRankCount);
    state.load_phase = kLoadDone;
    return;
  }
  host.BeginResultsRead(file);
  ++state.load_phase;
}

// The digits "%0Nd" prints, cut to N characters.
std::string Digits(std::int64_t value, std::size_t count) {
  std::string text = std::format("{:0{}}", value, count);
  text.resize(count, '0');
  return text;
}

struct NumberLayout {
  int label = 0;
  int unit = 0;
  std::size_t digits = 0;
  // The digits' width and separators' allowance in the centring.
  int digits_width = 0;
  bool separators = false;
};

constexpr NumberLayout kScoreLayout{
    .label = kRegionScoreLabel,
    .unit = kRegionScoreUnit,
    .digits = kScoreDigits,
    .digits_width = kScoreDigitsWidth,
    .separators = true,
};
constexpr NumberLayout kMapLayout{
    .label = kRegionMapLabel,
    .unit = kRegionPercentSign,
    .digits = kMapDigits,
    .digits_width = kMapDigitsWidth,
};

// The label, colon, digits (with separators after the third and sixth) and
// unit, centred on origin.x.
void DrawNumber(const NumberLayout& layout, ScreenPoint origin,
                const std::string& digits, RegionStyle style,
                const RegionColorPair& colors, int wipe_edge,
                const RegionPainter& painter) {
  int width = painter.Width(layout.label) + painter.Width(kRegionColon) +
              painter.Width(layout.unit) + layout.digits_width;
  if (layout.separators) width += 2 * painter.Width(kRegionDigitSeparator);
  Pen pen{origin.x - (width >> 1), origin.y};
  painter.Draw(style, layout.label, pen, colors, wipe_edge);
  pen.x += painter.Width(layout.label);
  painter.Draw(style, kRegionColon, {pen.x, pen.y + kColonDrop}, colors,
               wipe_edge);
  pen.x += kColonGap + painter.Width(kRegionColon);
  for (std::size_t index = 0; index < layout.digits; ++index) {
    painter.Draw(style, digits[index] - '0', {pen.x, pen.y + kDigitDrop},
                 colors, wipe_edge);
    pen.x += kDigitAdvance;
    if (layout.separators &&
        (index == kFirstSeparatorDigit || index == kSecondSeparatorDigit)) {
      painter.Draw(style, kRegionDigitSeparator,
                   {pen.x, pen.y + kSeparatorDrop}, colors, wipe_edge);
      pen.x += kSeparatorGap + painter.Width(kRegionDigitSeparator);
    }
  }
  painter.Draw(style, layout.unit, {pen.x, pen.y + kUnitDrop}, colors,
               wipe_edge);
}

// Starts the counting sound when a count begins and swaps it for the counted
// sound when the count reaches its target.
void PlayCountSounds(bool counting_started, bool count_reached,
                     bool& is_incrementing, ResultsHost& host) {
  if (counting_started) {
    is_incrementing = true;
    host.PlaySound(kSoundCounting);
  }
  if (count_reached && is_incrementing) {
    is_incrementing = false;
    host.StopSound(kSoundCounting);
    host.PlaySound(kSoundCounted);
  }
}
}  // namespace

// decomp: ENDING.PRG _do_updateClearGameStats (src/ENDING/ENDING.PRG/D4.c:1502)
void CommitClearProgress(ClearProgress& progress,
                         std::span<const std::uint32_t> countable_room_bits,
                         ProgressHost& host) {
  ScoreRecord& score = progress.score;
  const int minutes =
      (progress.play_time.hours * kMinutesPerHour) + progress.play_time.minutes;
  if (score.completion_time_minutes == 0 ||
      static_cast<std::uint32_t>(minutes) < score.completion_time_minutes) {
    score.clear_count = progress.flags.clear_count;
    score.completion_time_minutes = static_cast<std::uint32_t>(minutes);
  }
  AwardClearTitles(progress, countable_room_bits);
  ResetPlaythrough(progress, host);
  PlaythroughFlags& flags = progress.flags;
  if (flags.difficulty != 0) {
    flags.normal_mode_cleared = 1;
  } else {
    flags.difficulty = kFirstClearDifficulty;
  }
  if (flags.clear_count < kClearCountLimit) ++flags.clear_count;
  progress.entry_zone = kClearEntryZone;
  progress.entry_room = kClearEntryRoom;
}

// decomp: ENDING.PRG _updateClearGameStats (src/ENDING/ENDING.PRG/D4.c:1549)
void UpdateClearProgress(ClearProgress& progress,
                         std::span<const std::uint32_t> countable_room_bits,
                         ProgressHost& host) {
  CommitClearProgress(progress, countable_room_bits, host);
}

// decomp: ENDING.PRG _updateTitles (src/ENDING/ENDING.PRG/D4.c:1570)
void AwardClearTitles(ClearProgress& progress,
                      std::span<const std::uint32_t> countable_room_bits) {
  ScoreRecord& score = progress.score;
  const PlaythroughFlags& flags = progress.flags;
  if (score.completion_time_minutes < kTenHoursMinutes) {
    score.titles |= kTitleUnderTenHours;
  }
  if (AllActionsUnlocked(progress, kUpperActionsFirst, kUpperActionsEnd, {})) {
    score.titles |= kTitleUpperActions;
  }
  if (AllActionsUnlocked(progress, kLowerActionsFirst, kLowerActionsEnd,
                         kLowerActionsExcluded)) {
    score.titles |= kTitleLowerActions;
  }
  if (progress.save_count == 0) score.titles |= kTitleNeverSaved;
  if (!flags.magic_used) score.titles |= kTitleNoMagic;
  if (!flags.battle_abilities_used) score.titles |= kTitleNoBattleAbilities;
  if (!flags.break_arts_used) score.titles |= kTitleNoBreakArts;
  RaiseExplorationRecords(progress, countable_room_bits);
  for (std::size_t boss = 0; boss < flags.bosses_defeated.size(); ++boss) {
    if (flags.bosses_defeated[boss]) {
      score.titles |= 1U << (kTitleFirstBoss + boss);
    }
  }
  if (flags.gold_key_obtained) score.titles |= kTitleGoldKey;
  if (flags.chest_key_obtained) score.titles |= kTitleChestKey;
}

// decomp: ENDING.PRG func_8006BD78 (src/ENDING/ENDING.PRG/D4.c:1677)
void ShowClearResults(ResultsState& state, ClearProgress& progress,
                      const ResultsTables& tables, ResultsHost& host) {
  state.load_phase = kLoadAtlas;
  host.SetDisplayEnabled(true);
  bool is_complete = false;
  do {
    Credits::BeginFrame(host);
    is_complete = LoadClearResultsStep(state, progress, tables, host);
    Credits::PresentFrame(Credits::kFrameOrderingSlot, host);
  } while (!is_complete);
  do {
    Credits::BeginFrame(host);
    is_complete = DrawClearResults(state, tables, host);
    host.WaitForVerticalBlank();
    Credits::PresentFrame(Credits::kFrameOrderingSlot, host);
  } while (!is_complete);
}

// decomp: ENDING.PRG func_8006BE04 (src/ENDING/ENDING.PRG/D4.c:1698)
bool LoadClearResultsStep(ResultsState& state, ClearProgress& progress,
                          const ResultsTables& tables, ResultsHost& host) {
  switch (state.load_phase) {
    case kLoadAtlas:
      host.BeginResultsRead(kResultsAtlasFile);
      ++state.load_phase;
      return false;
    case kUploadAtlas:
      if (!host.IsResultsReadComplete()) return false;
      UploadAtlas(host.ResultsData(), host);
      host.EndResultsRead();
      ++state.load_phase;
      return false;
    case kLoadRankScreen:
      ScoreFile(state, progress, tables, host);
      return false;
    case kUploadRankScreen:
      if (!host.IsResultsReadComplete()) return false;
      UploadRankScreen(host.ResultsData(), host);
      host.EndResultsRead();
      ++state.load_phase;
      return false;
    default:
      state.load_phase = kLoadAtlas;
      host.FreeResultsData();
      return true;
  }
}

// decomp: ENDING.PRG _renderCongratulationsScreen
// (src/ENDING/ENDING.PRG/D4.c:1844)
bool DrawClearResults(ResultsState& state, const ResultsTables& tables,
                      ResultsHost& host) {
  if (!state.is_leaving) {
    const int frame = state.animation_frame;
    if (frame == kRankRevealFrame) {
      host.PlaySound(kSoundRankReveal);
      host.PlaySound(kSoundRankRevealEcho);
    }
    DrawRankBackground(frame, host);
    DrawCongratulations(kCongratulationsOrigin, frame, tables, host);
    DrawCountingScore(kScoreOrigin, frame - kScoreFrame, state, tables, host);
    DrawCountingMapCompletion(kMapCompletionOrigin, frame - kMapCompletionFrame,
                              state, tables, host);
    DrawRiskbreakerRankHeading(kRankHeadingOrigin, frame - kRankHeadingFrame,
                               tables, host);
    DrawRiskbreakerRank(kRankOrigin, frame - kRankRevealFrame, state, tables,
                        host);
    if (state.animation_frame > kHeldFrame) state.animation_frame = kHeldFrame;
    if (host.ButtonsPressed() != 0) {
      state.is_leaving = true;
      state.animation_frame = 0;
    }
  } else {
    const int fade = kLeaveFrames - state.animation_frame;
    DrawRankBackground(fade, host);
    DrawCongratulations(kCongratulationsOrigin, fade, tables, host);
    DrawScore(kScoreOrigin, fade, state, tables, host);
    DrawMapCompletion(kMapCompletionOrigin, fade, state, tables, host);
    DrawRiskbreakerRankHeading(kRankHeadingOrigin, fade, tables, host);
    DrawRiskbreakerRank(kRankOrigin, fade, state, tables, host);
    if (state.animation_frame >= kLeaveFrames) return true;
  }
  ++state.animation_frame;
  return false;
}

// decomp: ENDING.PRG func_8006C3CC (src/ENDING/ENDING.PRG/D4.c:1887)
void DrawRankBackground(int fade_frame, DisplayHost& host) {
  const int fade = ClampFade(fade_frame);
  if (fade <= 0) return;
  for (int panel = 0; panel < kRankBackgroundPanels; ++panel) {
    const auto left = static_cast<std::int16_t>(panel * kAtlasPageWidth);
    const auto right = static_cast<std::int16_t>(left + kAtlasPageWidth);
    constexpr auto kTexelRight = static_cast<std::uint8_t>(kAtlasPageWidth);
    host.DrawTexturedQuad(
        {.ordering_slot = kRankBackgroundSlot,
         .corners = {{{left, kRankBackgroundTop},
                      {right, kRankBackgroundTop},
                      {left, kRankBackgroundBottom},
                      {right, kRankBackgroundBottom}}},
         .texels = {{{0, 0},
                     {kTexelRight, 0},
                     {0, kRankBackgroundTexelBottom},
                     {kTexelRight, kRankBackgroundTexelBottom}}},
         .texture_page = static_cast<std::uint16_t>(
             (kRankBackgroundFirstPage + panel) | kRankBackgroundPageBits),
         .brightness = static_cast<std::uint8_t>(fade * 2)});
  }
}

// decomp: ENDING.PRG _renderCongratulations (src/ENDING/ENDING.PRG/D4.c:1925)
void DrawCongratulations(ScreenPoint origin, int fade_frame,
                         const ResultsTables& tables, ResultsHost& host) {
  const int fade = ClampFade(fade_frame);
  if (fade <= 0) return;
  auto colors = tables.tints.congratulations;
  colors[0].step = static_cast<std::uint8_t>(fade);
  colors[1].step = static_cast<std::uint8_t>(fade);
  const RegionPainter painter(tables, host);
  const int first_width = painter.Width(kRegionCongratulations);
  const int x = origin.x -
                ((first_width + painter.Width(kRegionCongratulationsEnd)) >> 1);
  painter.Draw(RegionStyle::kFlashFade, kRegionCongratulations, {x, origin.y},
               {colors[0], colors[1]});
  painter.Draw(RegionStyle::kFlashFade, kRegionCongratulationsEnd,
               {x + first_width, origin.y}, {colors[1], colors[2]});
}

// decomp: ENDING.PRG _renderScore (src/ENDING/ENDING.PRG/D4.c:1946)
void DrawScore(ScreenPoint origin, int fade_frame, const ResultsState& state,
               const ResultsTables& tables, ResultsHost& host) {
  const int fade = ClampFade(fade_frame);
  if (fade <= 0) return;
  const RegionPainter painter(tables, host);
  DrawNumber(kScoreLayout, origin,
             Digits(static_cast<std::int32_t>(state.final_score), kScoreDigits),
             RegionStyle::kTintFade, Faded(tables.tints.score, fade), 0,
             painter);
}

// decomp: ENDING.PRG _renderIncrementalScore (src/ENDING/ENDING.PRG/D4.c:1996)
void DrawCountingScore(ScreenPoint origin, int animation_frame,
                       ResultsState& state, const ResultsTables& tables,
                       ResultsHost& host) {
  const int fade = ClampFade(animation_frame);
  if (fade <= 0) return;
  const int wipe_edge = origin.x + (fade << kWipeSpeedShift);
  if (state.final_score != 0 && fade >= kCountStartFade) {
    PlayCountSounds(
        state.displayed_score == 0,
        static_cast<std::uint32_t>(state.displayed_score) == state.final_score,
        state.is_score_incrementing, host);
  }
  if (fade >= kCountStartFade) {
    state.displayed_score = static_cast<std::int32_t>(
        (state.final_score >> kCountShift) *
        static_cast<std::uint32_t>(fade - kCountStartFade));
  }
  if (fade == kFadeMaximum) {
    state.displayed_score = static_cast<std::int32_t>(state.final_score);
  }
  const RegionPainter painter(tables, host);
  DrawNumber(kScoreLayout, origin, Digits(state.displayed_score, kScoreDigits),
             RegionStyle::kWipe, tables.tints.counting_score, wipe_edge,
             painter);
}

// decomp: ENDING.PRG _renderMapCompletion (src/ENDING/ENDING.PRG/D4.c:2070)
void DrawMapCompletion(ScreenPoint origin, int fade_frame,
                       const ResultsState& state, const ResultsTables& tables,
                       ResultsHost& host) {
  const int fade = ClampFade(fade_frame);
  if (fade <= 0) return;
  const RegionPainter painter(tables, host);
  DrawNumber(kMapLayout, origin, Digits(state.map_completion, kMapDigits),
             RegionStyle::kTintFade, Faded(tables.tints.map_completion, fade),
             0, painter);
}

// decomp: ENDING.PRG _renderIncrementalMapCompletion
// (src/ENDING/ENDING.PRG/D4.c:2113)
void DrawCountingMapCompletion(ScreenPoint origin, int fade_frame,
                               ResultsState& state, const ResultsTables& tables,
                               ResultsHost& host) {
  const int fade = ClampFade(fade_frame);
  if (fade <= 0) return;
  const int wipe_edge = origin.x + (fade << kWipeSpeedShift);
  const auto target = static_cast<std::uint32_t>(state.map_completion);
  if (state.map_completion != 0 && fade >= kCountStartFade) {
    PlayCountSounds(state.displayed_map_completion == 0,
                    state.displayed_map_completion == target,
                    state.is_map_completion_incrementing, host);
  }
  if (fade >= kCountStartFade) {
    if (target >= kCountSteps) {
      state.displayed_map_completion =
          static_cast<std::uint32_t>(state.map_completion *
                                     (fade - kCountStartFade)) >>
          kCountShift;
    } else if (state.displayed_map_completion < target) {
      ++state.displayed_map_completion;
    }
  }
  const RegionPainter painter(tables, host);
  DrawNumber(kMapLayout, origin,
             Digits(static_cast<std::int32_t>(state.displayed_map_completion),
                    kMapDigits),
             RegionStyle::kWipe, tables.tints.counting_map_completion,
             wipe_edge, painter);
}

// decomp: ENDING.PRG _renderRiskbreakerRankHeader
// (src/ENDING/ENDING.PRG/D4.c:2181)
void DrawRiskbreakerRankHeading(ScreenPoint origin, int timer,
                                const ResultsTables& tables,
                                ResultsHost& host) {
  const int fade = ClampFade(timer);
  if (fade <= 0) return;
  const RegionPainter painter(tables, host);
  painter.Draw(RegionStyle::kTintFade, kRegionRankHeading,
               {origin.x - (tables.rank_heading_width >> 1), origin.y},
               Faded(tables.tints.rank_heading, fade));
}

// decomp: ENDING.PRG _renderRiskbreakerRank (src/ENDING/ENDING.PRG/D4.c:2197)
void DrawRiskbreakerRank(ScreenPoint origin, int timer,
                         const ResultsState& state, const ResultsTables& tables,
                         ResultsHost& host) {
  const int fade = ClampFade(timer);
  if (fade <= 0) return;
  if (state.rank >= tables.rank_labels.size()) {
    VS_LOG_WARN("ending", "Rank {} label refused: there are {} labels",
                state.rank, tables.rank_labels.size());
    return;
  }
  const RankLabel& label = tables.rank_labels[state.rank];
  std::size_t count = label[0];
  if (count > kRankLabelRegions) {
    VS_LOG_WARN("ending", "Rank {} label names {} regions; drawing {}",
                state.rank, count, kRankLabelRegions);
    count = kRankLabelRegions;
  }
  const auto regions = std::span(label).subspan(1, count);
  const RegionPainter painter(tables, host);
  int inset = 0;
  for (const std::uint8_t region : regions) inset += painter.Width(region);
  const auto colors = Faded(tables.tints.rank_label, fade);
  Pen pen{origin.x - (inset / 2), origin.y};
  for (const std::uint8_t region : regions) {
    painter.Draw(RegionStyle::kFlashFade, region, pen, colors);
    pen.x += painter.Width(region);
  }
}

// decomp: ENDING.PRG _determineRank (src/ENDING/ENDING.PRG/D4.c:2255)
void DetermineRiskbreakerRank(ResultsState& state, const ScoreRecord& score,
                              const ResultsTables& tables) {
  const int titles = std::popcount(score.titles & kRankedTitleMask);
  const std::size_t ranks =
      std::min({kRankCount, tables.rank_title_thresholds.size(),
                tables.rank_score_thresholds.size()});
  if (ranks < kRankCount) {
    VS_LOG_WARN("ending", "Rank tables hold {} of {} ranks; checking those",
                ranks, kRankCount);
  }
  for (std::size_t rank = 0; rank < ranks; ++rank) {
    if (titles >= tables.rank_title_thresholds[rank] &&
        state.final_score >= tables.rank_score_thresholds[rank]) {
      state.rank = static_cast<std::uint32_t>(rank);
      return;
    }
  }
}

// decomp: ENDING.PRG _updateScore (src/ENDING/ENDING.PRG/D4.c:2277)
void CalculateClearScore(ResultsState& state, const ScoreRecord& score) {
  std::uint32_t total = 0;
  for (std::size_t index = 0; index < kEnemyClassCount; ++index) {
    total += score.enemy_kills[index] * kEnemyKillPoints[index];
  }
  for (std::size_t index = 0; index < kWeaponCategoryCount; ++index) {
    total += score.weapon_attacks[index] * kWeaponAttackPoints[index];
  }
  for (const std::uint32_t time : score.boss_trial_times) {
    if (time != kUnsetTrialTime) total += kBossTrialPoints;
  }
  const auto bonus = std::ranges::find_if(
      kChainBonuses,
      [&](const ChainBonus& entry) { return score.max_chain < entry.below; });
  total += score.max_chain *
           (bonus != kChainBonuses.end() ? bonus->points : kLongChainPoints);
  total += static_cast<std::uint32_t>(state.clear_count) * kClearPoints;
  if (state.clear_count != 0 &&
      score.completion_time_minutes < kTenHoursMinutes) {
    if (score.completion_time_minutes >= kNineHoursMinutes) {
      total += total / kNineHoursBonusDivisor;
    } else if (score.completion_time_minutes >= kFiveHoursMinutes) {
      total += total / 2;
    } else {
      total *= 2;
    }
  }
  total += score.streak_score;
  total += score.misc_score;
  state.final_score = std::min(total, kMaximumScore);
}
}  // namespace VS::Ending
