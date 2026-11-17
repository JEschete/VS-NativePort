#include "Menu/Results/TimeAttackTerminal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu::Results {
namespace {
namespace math = Core::Math;
constexpr std::int32_t kLoadedPhase = 2;
constexpr std::int32_t kClosingPhase = 2;
constexpr std::int32_t kResultDone = 3;
constexpr std::int32_t kStartDone = 4;
constexpr std::int16_t kImageX = 832;
constexpr std::int16_t kImageY = 256;
constexpr std::int16_t kImageHeight = 255;
constexpr std::int16_t kImageStride = 64;
constexpr Render::ScreenRect kPaletteArea{768, 511, 160, 1};
constexpr unsigned kMinuteShift = 16;
constexpr unsigned kSecondShift = 8;
constexpr unsigned kRoundShift = 24;
constexpr unsigned kDifficultyShift = 31;
constexpr std::uint32_t kRoundMask = 127;
constexpr std::uint32_t kDefaultTime = 0x3B3B63;
constexpr std::uint32_t kPulseStep = 64;
constexpr std::uint32_t kPulseMask = 0xFFF;
constexpr std::int32_t kPulseDivisor = 256;
constexpr std::int32_t kCountSound = 0x72;
constexpr std::int32_t kRevealSound = 0x73;
constexpr std::int32_t kRecordSound = 0x76;
constexpr std::int32_t kConfirmSound = 5;
constexpr std::int32_t kCountSoundAt = 48;
constexpr std::int32_t kRatingAt = 96;
constexpr std::int32_t kTimeAt = 32;
constexpr std::int32_t kRecordAt = 128;
constexpr auto kRecordDelays = std::to_array<std::int32_t>({160, 168, 176});
constexpr std::int32_t kRevealEnd = 192;
constexpr std::int32_t kHoldEnd = 450;
constexpr std::int32_t kClosingTimer = 7;
constexpr ResultsPoint kResultHeading{160, 52};
constexpr ResultsPoint kTimeOrigin{16, 164};
constexpr ResultsPoint kRatingOrigin{16, 144};
constexpr ResultsPoint kRecordOrigin{70, 184};
constexpr ResultsPoint kStoredOrigin{304, 144};
constexpr ResultsPoint kCountdownOrigin{160, 120};
constexpr std::array kCountdownOffsets{121, 93, 67, 43, 25, 12, 3, 0};
constexpr std::int32_t kIntroductionEnd = 90;
constexpr std::int32_t kIntroductionFade = 8;
constexpr std::int32_t kLastIntroductionSound = 46;
constexpr std::int32_t kGoAt = 45;
constexpr std::int32_t kBlendOffAt = 46;
constexpr std::int32_t kNeutralAt = 55;
constexpr std::int32_t kScaleBackAt = 60;
constexpr std::int32_t kCountdownEnd = 62;
constexpr std::int32_t kGoFadeBase = 104;
constexpr std::int32_t kIntroductionSound = 0x7A;
constexpr std::int32_t kCountdownSound = 0x7D;
constexpr std::int32_t kGoSound = 0x80;
constexpr std::int32_t kSoundLayers = 3;
constexpr std::int32_t kCountdownTickSpeed = 4;
constexpr std::int32_t kNormalTickSpeed = 2;
constexpr Render::FramebufferEffectPair kCountdownScale{0x1029, 0x1029};
constexpr Render::FramebufferEffectPair kGoScale{0x107A, 0x107A};
constexpr Render::FramebufferEffectTint kCountdownTint{64, 128, 220};
constexpr Render::FramebufferEffectTint kGoTint{220, 80, 64};

bool UploadTimeAttackTextures(TimeAttackAsset asset, TimeAttackLoadHost& host) {
  const std::size_t count =
      asset == TimeAttackAsset::kStart ? kTimeAttackStartTims : 1;
  for (std::size_t index = 0; index < count; ++index) {
    const auto blocks = host.ParseTim(index);
    if (!blocks.has_value()) return false;
    if (blocks->pixels.has_value()) {
      auto area = *blocks->pixels;
      area.x = static_cast<std::int16_t>(
          kImageX + static_cast<std::int32_t>(index) * kImageStride);
      area.y = kImageY;
      area.height = kImageHeight;
      if (!host.UploadPixels(index, area)) return false;
    }
    if (index == 0 && blocks->palette.has_value()) {
      if (!host.UploadPalette(kPaletteArea)) return false;
      if (asset == TimeAttackAsset::kResults && !host.CopyResultPalette()) {
        return false;
      }
    }
  }
  return true;
}

GameClearStep LoadTextures(TimeAttackState& state, TimeAttackAsset asset,
                           TimeAttackLoadHost& host) {
  if (state.init_phase == 0) {
    const auto& pages = asset == TimeAttackAsset::kStart
                            ? kTimeAttackStartPages
                            : kTimeAttackResultPages;
    if (!host.ReserveTexturePages(pages) || !host.BeginTextureRead(asset)) {
      return GameClearStep::kRefused;
    }
    state.init_phase = math::WrappingAdd(state.init_phase, 1);
  } else if (host.TextureReadLoaded()) {
    if (!UploadTimeAttackTextures(asset, host)) return GameClearStep::kRefused;
    host.ReleaseTextureRequest();
    if (asset == TimeAttackAsset::kResults) host.QueueMusic();
    state.init_phase = math::WrappingAdd(state.init_phase, 1);
  }
  return GameClearStep::kPending;
}

std::int32_t PackedTime(const TimeAttackRun& run) {
  const std::uint32_t time = (std::uint32_t{run.minutes} << kMinuteShift) |
                             (std::uint32_t{run.seconds} << kSecondShift) |
                             run.hundredths;
  return static_cast<std::int32_t>(time == 0 ? kDefaultTime : time);
}

std::int32_t InsertRecord(const TimeAttackRun& run, std::int32_t time) {
  auto& row = run.records[run.course];
  std::size_t rank = 0;
  while (rank < row.size() && (row.at(rank) & kTrialScoreTimeMask) <
                                  static_cast<std::uint32_t>(time)) {
    ++rank;
  }
  if (rank != row.size()) {
    for (std::size_t next = row.size() - 1; next > rank; --next) {
      row.at(next) = row.at(next - 1);
    }
    row.at(rank) = static_cast<std::uint32_t>(time) |
                   ((std::uint32_t{run.round} & kRoundMask) << kRoundShift) |
                   ((std::uint32_t{run.difficulty} & 1U) << kDifficultyShift);
  }
  return static_cast<std::int32_t>(rank);
}

void ResetPresentation(TimeAttackState& state) {
  state.timer = 0;
  state.phase = 0;
  state.animation = 0;
  state.init_phase = 0;
}

void DrawResults(const TimeAttackState& state, std::int32_t timer,
                 TimeAttackScreenHost& host) {
  host.Draw(TimeAttackLine::kResultsHeading, kResultHeading, timer);
  host.Draw(TimeAttackLine::kTime, kTimeOrigin, timer);
  host.Draw(TimeAttackLine::kRating, kRatingOrigin, timer);
  if (state.new_slot == 0) {
    host.Draw(TimeAttackLine::kNewRecord, kRecordOrigin, timer);
  }
  host.Draw(TimeAttackLine::kLeaderboard, kStoredOrigin, timer);
}

void RevealResults(TimeAttackState& state, bool confirm,
                   TimeAttackScreenHost& host) {
  if (state.timer == kCountSoundAt) {
    host.Sound(GameClearSound::kPlay, kCountSound);
  }
  if (state.timer == kRatingAt) {
    host.Sound(GameClearSound::kStop, kCountSound);
    host.Sound(GameClearSound::kPlay, kRevealSound);
  }
  host.Draw(TimeAttackLine::kResultsHeading, kResultHeading, state.timer);
  host.Draw(TimeAttackLine::kCountingTime, kTimeOrigin,
            math::WrappingSubtract(state.timer, kTimeAt));
  host.Draw(TimeAttackLine::kRatingWipe, kRatingOrigin,
            math::WrappingSubtract(state.timer, kRatingAt));
  if (state.new_slot == 0) {
    host.Draw(TimeAttackLine::kNewRecord, kRecordOrigin,
              math::WrappingSubtract(state.timer, kRecordAt));
    if (state.timer == kRecordAt) {
      host.Sound(GameClearSound::kPlay, kRecordSound);
    }
  }
  for (const auto delay : kRecordDelays) {
    if (state.timer == delay) host.Sound(GameClearSound::kPlay, kRevealSound);
  }
  for (std::size_t rank = 0; rank < kRecordDelays.size(); ++rank) {
    host.Draw(TimeAttackLine::kStoredTime, kStoredOrigin,
              math::WrappingSubtract(state.timer, kRecordDelays.at(rank)),
              static_cast<std::int32_t>(rank));
  }
  state.timer = math::WrappingAdd(state.timer, 1);
  if (confirm || state.timer >= kRevealEnd) {
    host.Sound(GameClearSound::kStop, 0);
    state.timer = 0;
    state.phase = math::WrappingAdd(state.phase, 1);
  }
}

void LayeredSound(std::int32_t first, TimeAttackScreenHost& host) {
  for (std::int32_t layer = 0; layer < kSoundLayers; ++layer) {
    host.Sound(GameClearSound::kPlay, first + layer);
  }
}

void DrawIntroduction(const TimeAttackState& state, bool stagger,
                      TimeAttackScreenHost& host) {
  constexpr auto kStartOrigins = std::to_array<ResultsPoint>(
      {{160, 48}, {160, 92}, {160, 120}, {160, 156}});
  constexpr auto kStartLines = std::to_array<TimeAttackLine>(
      {TimeAttackLine::kStartHeading, TimeAttackLine::kCourse,
       TimeAttackLine::kBoss, TimeAttackLine::kBestTime});
  for (std::size_t line = 0; line < kStartLines.size(); ++line) {
    const auto delay =
        stagger ? static_cast<std::int32_t>(line) * kTimeAttackCountdownFrames
                : 0;
    host.Draw(kStartLines.at(line), kStartOrigins.at(line),
              math::WrappingSubtract(state.timer, delay));
  }
}

void EnterCountdown(TimeAttackState& state) {
  state.phase = kClosingPhase;
  state.timer = 0;
}

bool StartCountdownEffect(TimeAttackScreenHost& host) {
  host.SetTickSpeed(kCountdownTickSpeed);
  host.SetEffectRotation(0);
  host.SetEffectScale(kCountdownScale);
  host.SetEffectTint(kCountdownTint);
  host.SetEffectOffset({});
  host.SetEffectBlend(0);
  host.SetEffectDepth(0);
  host.SetPreviousFrame(false);
  return host.EnableEffect(true);
}

void GoEffect(std::int32_t timer, TimeAttackScreenHost& host) {
  if (timer == kGoAt) {
    host.SetEffectBlend(1);
    host.SetEffectScale(kGoScale);
    host.SetEffectTint(kGoTint);
  } else if (timer == kBlendOffAt) {
    host.SetEffectBlend(0);
  } else if (timer == kNeutralAt) {
    host.SetEffectTint({});
  } else if (timer == kScaleBackAt) {
    host.SetEffectScale(kCountdownScale);
  }
}

GameClearStep Countdown(TimeAttackState& state, TimeAttackScreenHost& host) {
  const auto interval = state.timer / kTimeAttackCountdownFrames;
  const auto remainder = state.timer % kTimeAttackCountdownFrames;
  if (state.timer < kGoAt && remainder < 0) return GameClearStep::kRefused;
  if (remainder == 0) {
    if (interval >= 0 && interval < kSoundLayers) {
      LayeredSound(kCountdownSound, host);
    } else if (interval == kSoundLayers) {
      LayeredSound(kGoSound, host);
    }
  }
  auto at = kCountdownOrigin;
  auto timer = state.timer;
  if (state.timer >= kGoAt) {
    GoEffect(state.timer, host);
    timer = std::max(math::WrappingSubtract(kGoFadeBase, state.timer), kGoAt);
  } else {
    const auto index = std::min(static_cast<std::size_t>(remainder),
                                kCountdownOffsets.size() - 1);
    at.x = math::WrappingAdd(at.x, kCountdownOffsets.at(index));
  }
  host.Draw(TimeAttackLine::kCountdown, at, timer);
  state.timer = math::WrappingAdd(state.timer, 1);
  if (state.timer >= kCountdownEnd) {
    state.phase = math::WrappingAdd(state.phase, 1);
  }
  return GameClearStep::kPending;
}

bool ValidSelector(std::uint8_t selector) {
  return selector <= static_cast<std::uint8_t>(ResultsPresentation::kCubeQuit);
}
}  // namespace

// decomp: _initTimeAttackEnd, MENUF.PRG/3B8.c:256; instructions
// 0x8010310C..0x80103530, including the return delay slot.
GameClearStep InitializeTimeAttackResults(TimeAttackState& state,
                                          TimeAttackLoadHost& host) {
  if (state.init_phase < 0 || state.init_phase > kLoadedPhase) {
    return GameClearStep::kRefused;
  }
  if (state.init_phase < kLoadedPhase) {
    return LoadTextures(state, TimeAttackAsset::kResults, host);
  }
  if (host.MusicLoading()) return GameClearStep::kPending;
  if (!host.CompletionReady()) return GameClearStep::kRefused;
  host.PlayMusic();
  const auto run = host.ReadRun();
  if (!run.has_value() || run->course >= run->records.size() ||
      run->course >= kTimeAttackCourses) {
    return GameClearStep::kRefused;
  }
  state.time = PackedTime(*run);
  const auto rank = InsertRecord(*run, state.time);
  host.CommitRun(*run);
  state.new_slot = rank;
  ResetPresentation(state);
  return host.FreeTextureBlock(state) ? GameClearStep::kComplete
                                      : GameClearStep::kRefused;
}

// decomp: _initTimeAttackStart, MENUF.PRG/3B8.c:372; instructions
// 0x80103530..0x80103748.
GameClearStep InitializeTimeAttackStart(TimeAttackState& state,
                                        TimeAttackLoadHost& host) {
  if (state.init_phase < 0 || state.init_phase > kLoadedPhase) {
    return GameClearStep::kRefused;
  }
  if (state.init_phase < kLoadedPhase) {
    return LoadTextures(state, TimeAttackAsset::kStart, host);
  }
  if (!host.CompletionReady() || !host.FreeTextureBlock(state)) {
    return GameClearStep::kRefused;
  }
  state.shown_map = 0;
  state.shown_score = 0;
  ResetPresentation(state);
  state.selection = 0;
  return GameClearStep::kComplete;
}

// decomp: _renderTimeAttackEnd, MENUF.PRG/3B8.c:905; instructions
// 0x80103D88..0x8010412C.
GameClearStep UpdateTimeAttackResults(TimeAttackState& state, bool confirm,
                                      TimeAttackScreenHost& host) {
  if (state.phase < 0 || state.phase > kResultDone || state.new_slot < 0 ||
      state.new_slot > static_cast<std::int32_t>(kTimeAttackRanks)) {
    return GameClearStep::kRefused;
  }
  const auto sine = math::Sine(std::bit_cast<std::int32_t>(state.animation));
  host.PulsePalette((sine < 0 ? -sine : sine) / kPulseDivisor);
  state.animation = (state.animation + kPulseStep) & kPulseMask;
  if (state.phase == 0) {
    RevealResults(state, confirm, host);
  } else if (state.phase == 1) {
    DrawResults(state, kTimeAttackRevealSteps, host);
    state.timer = math::WrappingAdd(state.timer, 1);
    if (confirm || state.timer >= kHoldEnd) {
      if (confirm) host.Sound(GameClearSound::kPlay, kConfirmSound);
      state.phase = math::WrappingAdd(state.phase, 1);
      state.timer = kClosingTimer;
    }
  } else if (state.phase == kClosingPhase) {
    DrawResults(state, state.timer, host);
    if (state.timer > 0) {
      state.timer = math::WrappingSubtract(state.timer, 1);
    } else {
      state.phase = math::WrappingAdd(state.phase, 1);
    }
  } else {
    host.RetireTexturePages(kTimeAttackResultPages);
    return GameClearStep::kComplete;
  }
  return GameClearStep::kPending;
}

// decomp: _renderTimeAttackStart, MENUF.PRG/3B8.c:1014; instructions
// 0x8010412C..0x8010459C. The table at 0x801096D0 is data, not code.
GameClearStep UpdateTimeAttackStart(TimeAttackState& state, bool confirm,
                                    TimeAttackScreenHost& host) {
  if (state.phase < 0 || state.phase > kStartDone) {
    return GameClearStep::kRefused;
  }
  switch (state.phase) {
    case 0:
      if (state.timer % kTimeAttackCountdownFrames == 0 &&
          state.timer < kLastIntroductionSound) {
        LayeredSound(kIntroductionSound, host);
      }
      DrawIntroduction(state, true, host);
      state.timer = math::WrappingAdd(state.timer, 1);
      if (confirm) {
        EnterCountdown(state);
      } else if (state.timer >= kIntroductionEnd) {
        state.phase = 1;
        state.timer = kIntroductionFade;
      }
      break;
    case 1:
      state.timer = math::WrappingSubtract(state.timer, 1);
      DrawIntroduction(state, false, host);
      if (confirm || state.timer == 0) EnterCountdown(state);
      break;
    case kClosingPhase:
      if (!StartCountdownEffect(host)) return GameClearStep::kRefused;
      state.phase = math::WrappingAdd(state.phase, 1);
      break;
    case kResultDone:
      return Countdown(state, host);
    case kStartDone:
      host.SetTickSpeed(kNormalTickSpeed);
      host.SetEffectScale({});
      static_cast<void>(host.EnableEffect(false));
      host.RetireTexturePages(kTimeAttackStartPages);
      return GameClearStep::kComplete;
    default:
      break;
  }
  return GameClearStep::kPending;
}

// decomp: _initMenu, MENUF.PRG/3B8.c:59 (0x80102C54..0x80102D10).
std::optional<int> InitializeResultsScreen(std::uint8_t selector,
                                           ResultsPresentationHost& host) {
  if (!ValidSelector(selector)) return std::nullopt;
  const auto presentation = static_cast<ResultsPresentation>(selector);
  if (presentation == ResultsPresentation::kHealingScoreOnly) {
    host.ApplyHealingBonus();
    host.FinishScreen();
    return 1;
  }
  return host.Initialize(presentation);
}

// decomp: _execMenu, MENUF.PRG/3B8.c:453 (0x801037A0..0x8010384C).
std::optional<int> UpdateResultsPresentation(std::uint8_t selector,
                                             ResultsPresentationHost& host) {
  if (!ValidSelector(selector)) return std::nullopt;
  const auto presentation = static_cast<ResultsPresentation>(selector);
  if (presentation == ResultsPresentation::kHealingScoreOnly) return 1;
  return host.Update(presentation);
}

// decomp: vs_menuF_exec, MENUF.PRG/3B8.c:23 (0x80102BB8..0x80102C54).
std::optional<int> UpdateResultsScreen(std::span<std::uint8_t> caller_state,
                                       std::uint8_t selector,
                                       ResultsPresentationHost& host) {
  if (caller_state.empty()) return std::nullopt;
  auto& phase = caller_state.front();
  if (phase == 0) phase = 1;
  if (phase == 1) {
    const auto previous = phase;
    const auto result = InitializeResultsScreen(selector, host);
    if (!result.has_value()) return std::nullopt;
    phase = static_cast<std::uint8_t>(previous + (*result != 0 ? 1 : 0));
  } else if (phase == 2) {
    const auto result = UpdateResultsPresentation(selector, host);
    if (!result.has_value()) return std::nullopt;
    if (*result != 0) {
      phase = 0;
      return 1;
    }
  }
  return 0;
}
}  // namespace VS::Menu::Results
