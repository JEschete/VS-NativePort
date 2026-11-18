#include "Menu/Results/TimeAttackTerminalLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

#include "Audio/MusicLive.hpp"
#include "Audio/Seq/Channel.h"
#include "Audio/SfxLive.hpp"
#include "Battle/StatusEffects.h"
#include "Content/CdDriveLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "DiscQueue.h"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/CubePuzzleLifecycleLive.hpp"
#include "Menu/Results/GameClearLifecycleLive.hpp"
#include "Menu/Results/Results.h"
#include "Menu/Results/ResultsPalette.hpp"
#include "Menu/Results/TimeAttack.h"
#include "Menu/Results/TimeAttackHeadingLive.hpp"
#include "Menu/Results/TimeAttackRatingLive.hpp"
#include "Render/FramebufferEffects.h"
#include "Render/ScreenOverlayLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/TimImage.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/Progress.h"
#include "State/SceneTimerLive.hpp"
#include "State/StateFlags.h"

namespace VS::Menu::Results {
namespace {
constexpr std::int32_t kSoundGroup = 0x7E;
constexpr std::int32_t kMusicSlot = 2;
constexpr std::int32_t kMusicVolume = 127;
constexpr std::uint32_t kConfirmMask = 0x20;
constexpr std::size_t kHalfwordBytes = 2;
constexpr std::size_t kPaletteClearBytes = 4;
constexpr unsigned kRoundShift = 24;
constexpr unsigned kDifficultyShift = 31;
constexpr std::uint32_t kRoundMask = 127;
constexpr std::int32_t kCountdownTickSpeed = 4;
constexpr std::int32_t kNormalTickSpeed = 2;
constexpr std::int32_t kResultsClosingPhase = 2;
static_assert(std::size(Menu_Results_clutBuf) == kResultsClutWords);
static_assert(std::size(Audio_Seq_unusedScoredata.bossTimeTrialScores) ==
              kTimeAttackCourses);

struct LoadOwner {
  std::byte* buffer = nullptr;
  std::size_t bytes = 0;
  Content_DiscQueueSlot* request = nullptr;
  std::uint64_t generation = 0;
  bool pages = false;
  // Original file-backed pointer slots are views, not native owners.
  bool request_bound = false;
  bool buffer_bound = false;
};
std::array<LoadOwner, 2> owners{};
bool countdown_controls = false;
std::uint64_t refusals = 0;

LoadOwner& Owner(TimeAttackAsset asset) {
  return owners.at(static_cast<std::size_t>(asset));
}

void Refuse(const char* reason) {
  if (std::has_single_bit(++refusals)) {
    VS::Core::Warnf("menu.results", "Time-attack terminal refused: %s", reason);
  }
}

bool RequestMatches(const LoadOwner& owner) {
  return owner.request != nullptr && owner.generation != 0 &&
         Content::LiveDiscRequestGeneration(owner.request) == owner.generation;
}

std::optional<std::span<std::byte>> OwnedBuffer(const LoadOwner& owner) {
  if (owner.buffer == nullptr || owner.bytes == 0) {
    Refuse("missing texture storage");
    return std::nullopt;
  }
  const auto capacity = Core::Memory::LivePayloadCapacity(owner.buffer);
  if (!capacity.has_value() || *capacity < owner.bytes) {
    Refuse("texture storage no longer belongs to the live heap");
    return std::nullopt;
  }
  return std::span(owner.buffer, owner.bytes);
}

void ReleaseRequest(LoadOwner& owner) {
  if (owner.request != nullptr) {
    if (RequestMatches(owner)) {
      Content::ReleaseLiveDiscRequest(owner.request);
    } else {
      Refuse("request identity changed; not releasing another owner");
    }
  }
  owner.request = nullptr;
  owner.generation = 0;
  owner.request_bound = false;
}

bool FreeBuffer(LoadOwner& owner) {
  const auto bytes = OwnedBuffer(owner);
  if (!bytes.has_value()) return false;
  if (!Content::CancelLiveDiscWrites(*bytes)) {
    Refuse("texture writes could not be cancelled; retaining storage");
    return false;
  }
  Core::Memory::FreeLiveFromHighEnd(owner.buffer);
  owner.buffer = nullptr;
  owner.bytes = 0;
  owner.buffer_bound = false;
  return true;
}

bool RetirePages(LoadOwner& owner, const TexturePageLoan& loan) {
  if (Render::RetireLiveTexturePageRange(loan.first_page, loan.page_count,
                                         loan.owner_tag) == 0) {
    Refuse("page retirement failed; retaining any outstanding loan");
    return false;
  }
  owner.pages = false;
  return true;
}

TimeAttackState LoadState() {
  return {.init_phase = Battle_unusedSubmenuState,
          .timer = Menu_Results_screenTimer,
          .phase = Menu_Results_screenState,
          .animation = Menu_Results_elementAnimationState,
          .new_slot = Menu_Results_newTimeSlot,
          .time = Menu_Results_timeTrialTime,
          .shown_map = Menu_Results_onScreenMapCompletion,
          .shown_score = Menu_Results_onScreenScore,
          .selection = Menu_Results_buffReelSelection};
}

void StoreState(const TimeAttackState& state) {
  Battle_unusedSubmenuState = state.init_phase;
  Menu_Results_screenTimer = state.timer;
  Menu_Results_screenState = state.phase;
  Menu_Results_elementAnimationState = state.animation;
  Menu_Results_newTimeSlot = state.new_slot;
  Menu_Results_timeTrialTime = state.time;
  Menu_Results_onScreenMapCompletion = state.shown_map;
  Menu_Results_onScreenScore = state.shown_score;
  Menu_Results_buffReelSelection = state.selection;
}

bool ValidCourse() {
  if (Audio_Seq_unusedStateFlags.timeAttackBoss < kTimeAttackCourses) {
    return true;
  }
  Refuse("course outside the resident score table");
  return false;
}

class LiveLoad final : public TimeAttackLoadHost {
 public:
  explicit LiveLoad(TimeAttackAsset asset)
      : asset_(asset), owner_(Owner(asset)) {}

  bool ReserveTexturePages(const TexturePageLoan& loan) override {
    if (owner_.pages || owner_.buffer != nullptr || owner_.request != nullptr) {
      Refuse("new load before the previous owner was cleaned up");
      return false;
    }
    owner_.pages = Render::ReserveLiveTexturePageRange(
                       loan.first_page, loan.page_count, loan.owner_tag) != 0;
    return owner_.pages;
  }
  bool BeginTextureRead(TimeAttackAsset asset) override {
    const auto index = asset == TimeAttackAsset::kResults
                           ? Menu_Results_DisFile_Time
                           : Menu_Results_DisFile_Attack;
    const auto& file = Menu_Results_kDisFiles[index];
    if (file.size <= 0) {
      Refuse("invalid texture file size");
      return false;
    }
    owner_.bytes = static_cast<std::size_t>(file.size);
    owner_.buffer = static_cast<std::byte*>(
        Core::Memory::AllocateLiveFromHighEnd(owner_.bytes));
    owner_.buffer_bound = owner_.buffer != nullptr;
    if (!owner_.buffer_bound) {
      owner_.bytes = 0;
      Refuse("texture allocation refused");
      return false;
    }
    owner_.request = Content::AllocateLiveDiscRequest(&file);
    if (owner_.request != nullptr) {
      Content::EnqueueLiveDiscRequest(owner_.request, owner_.buffer);
    } else {
      owner_.request =
          Content::CompleteLiveRefusedDiscRequest(&file, owner_.buffer);
    }
    owner_.generation = Content::LiveDiscRequestGeneration(owner_.request);
    owner_.request_bound = RequestMatches(owner_);
    if (!owner_.request_bound) {
      Refuse("texture request allocation refused");
      return false;
    }
    return true;
  }
  bool TextureReadLoaded() override {
    if (!owner_.request_bound || !owner_.buffer_bound ||
        !RequestMatches(owner_)) {
      Refuse("missing or reused controller request");
      return false;
    }
    if (owner_.request->state == Content_DiscRequestState_Error) {
      Refuse("queue Error is not Loaded");
    }
    return owner_.request->state == Content_DiscRequestState_Loaded;
  }
  std::optional<ResultsTimBlocks> ParseTim(std::size_t index) override {
    const auto bytes = TimBytes(index);
    if (!bytes.has_value()) return std::nullopt;
    tims_.at(index) = Render::ParseTim(*bytes);
    const auto& tim = tims_.at(index);
    if (!tim.has_value()) {
      Refuse("malformed time-attack TIM");
      return std::nullopt;
    }
    ResultsTimBlocks blocks{.pixels = tim->image.area, .palette = std::nullopt};
    if (tim->clut.has_value()) blocks.palette = tim->clut->area;
    return blocks;
  }
  bool UploadPixels(std::size_t index,
                    const Render::ScreenRect& area) override {
    if (index >= tims_.size()) return false;
    const auto& tim = tims_.at(index);
    if (!tim.has_value()) return false;
    return Upload(index, tim->image, area, false);
  }
  bool UploadPalette(const Render::ScreenRect& area) override {
    const auto& tim = tims_.front();
    if (!tim.has_value() || !tim->clut.has_value()) return false;
    return Upload(0, *tim->clut, area, true);
  }
  bool CopyResultPalette() override {
    const auto& tim = tims_.front();
    const auto bytes = TimBytes(0);
    constexpr auto kBytes = kResultsClutWords * kHalfwordBytes;
    if (!bytes.has_value() || !tim.has_value() || !tim->clut.has_value() ||
        tim->clut->pixel_bytes < kBytes ||
        tim->clut->pixel_offset > bytes->size() ||
        kBytes > bytes->size() - tim->clut->pixel_offset) {
      Refuse("result palette exceeds its TIM");
      return false;
    }
    std::memcpy(Menu_Results_clutBuf, bytes->data() + tim->clut->pixel_offset,
                kBytes);
    return true;
  }
  void ReleaseTextureRequest() override { ReleaseRequest(owner_); }
  void QueueMusic() override { LiveQueueResultsMusic(); }
  bool MusicLoading() override { return Audio::LivePollMusicLoads() != 0; }
  void PlayMusic() override {
    Audio::LivePlayMusicSlot(kMusicSlot, kMusicVolume, 0);
  }
  std::optional<TimeAttackRun> ReadRun() override {
    if (!ValidCourse()) return std::nullopt;
    for (std::size_t course = 0; course < records_.size(); ++course) {
      for (std::size_t rank = 0; rank < kTimeAttackRanks; ++rank) {
        const auto& record =
            Audio_Seq_unusedScoredata.bossTimeTrialScores[course][rank];
        records_.at(course).at(rank) = record.time |
                                       (record.round << kRoundShift) |
                                       (record.difficulty << kDifficultyShift);
      }
    }
    const auto& flags = Audio_Seq_unusedStateFlags;
    return TimeAttackRun{
        .course = flags.timeAttackBoss,
        .minutes = flags.timeAttackMins,
        .seconds = flags.timeAttackSecs,
        .hundredths = flags.timeAttackHundredths,
        .round = static_cast<std::uint8_t>(flags.clearCount),
        .difficulty = static_cast<std::uint8_t>(flags.difficulty),
        .records = records_};
  }
  void CommitRun(const TimeAttackRun& run) override {
    for (std::size_t rank = 0; rank < kTimeAttackRanks; ++rank) {
      auto& record =
          Audio_Seq_unusedScoredata.bossTimeTrialScores[run.course][rank];
      const auto word = run.records[run.course].at(rank);
      record.time = word & kTrialScoreTimeMask;
      record.round = (word >> kRoundShift) & kRoundMask;
      record.difficulty = word >> kDifficultyShift;
    }
  }
  bool CompletionReady() override {
    if (!owner_.buffer_bound || !OwnedBuffer(owner_).has_value() ||
        owner_.request != nullptr) {
      Refuse("completion without a loaded buffer; retaining ownership");
      return false;
    }
    return asset_ != TimeAttackAsset::kResults || ValidCourse();
  }
  bool FreeTextureBlock(const TimeAttackState& state) override {
    StoreState(state);
    return FreeBuffer(owner_);
  }

 private:
  TimeAttackAsset asset_;
  LoadOwner& owner_;
  std::array<std::optional<Render::TimImage>, kTimeAttackStartTims> tims_{};
  std::array<TrialScoreRow, kTimeAttackCourses> records_{};

  std::optional<std::span<std::byte>> TimBytes(std::size_t index) const {
    const auto bytes = OwnedBuffer(owner_);
    const auto count = asset_ == TimeAttackAsset::kStart ? kTimeAttackStartTims
                                                         : std::size_t{1};
    if (!owner_.buffer_bound || !bytes.has_value() || index >= count ||
        index * kGameClearTimStride >= bytes->size()) {
      Refuse("TIM offset outside its active texture buffer");
      return std::nullopt;
    }
    const auto tail = bytes->subspan(index * kGameClearTimStride);
    return tail.first(std::min(tail.size(), kGameClearTimStride));
  }
  bool Upload(std::size_t index, const Render::TimBlock& block,
              const Render::ScreenRect& area, bool palette) const {
    const auto bytes = TimBytes(index);
    if (!bytes.has_value()) return false;
    const std::array<std::int16_t, 4> rect{area.x, area.y, area.width,
                                           area.height};
    const auto count = static_cast<std::size_t>(
        std::max(0, static_cast<int>(area.width) * area.height));
    const auto size = count * kHalfwordBytes;
    if (area.width <= 0 || area.height <= 0 ||
        block.rect_offset > bytes->size() ||
        sizeof(rect) > bytes->size() - block.rect_offset ||
        block.pixel_offset > bytes->size() ||
        size > bytes->size() - block.pixel_offset || size > block.pixel_bytes ||
        (palette && size < kPaletteClearBytes)) {
      Refuse("TIM upload exceeds parsed rectangle or pixel storage");
      return false;
    }
    std::memcpy(bytes->data() + block.rect_offset, rect.data(), sizeof(rect));
    const auto pixels = bytes->subspan(block.pixel_offset, size);
    if (palette) {
      std::ranges::fill(pixels.first(kPaletteClearBytes), std::byte{0});
    }
    Render::UploadLiveImageRectangle(
        Render::PackScreenWord(area.x, area.y), pixels.data(),
        Render::PackScreenWord(area.width, area.height));
    return true;
  }
};

void DrawLine(TimeAttackLine line, ResultsPoint at, std::int32_t timer,
              std::int32_t rank) {
  switch (line) {
    case TimeAttackLine::kResultsHeading:
      LiveDrawTimeAttackResultsHeading(at.x, at.y, timer);
      break;
    case TimeAttackLine::kCountingTime:
      LiveDrawCountingTimeAttackTime(at.x, at.y, timer);
      break;
    case TimeAttackLine::kRatingWipe:
      LiveDrawTimeAttackRatingWipe(at.x, at.y, timer);
      break;
    case TimeAttackLine::kNewRecord:
      LiveDrawNewRecord(at.x, at.y, timer);
      break;
    case TimeAttackLine::kStoredTime:
      LiveDrawLeaderboardEntry(at.x, at.y, rank, timer);
      break;
    case TimeAttackLine::kTime:
      LiveDrawTimeAttackTime(at.x, at.y, timer);
      break;
    case TimeAttackLine::kRating:
      LiveDrawTimeAttackRating(at.x, at.y, timer);
      break;
    case TimeAttackLine::kLeaderboard:
      LiveDrawLeaderboard(at.x, at.y, timer);
      break;
    case TimeAttackLine::kStartHeading:
      LiveDrawTimeAttackHeading(at.x, at.y, timer);
      break;
    case TimeAttackLine::kCourse:
      LiveDrawCourseNumber(at.x, at.y, timer);
      break;
    case TimeAttackLine::kBoss:
      LiveDrawBossName(at.x, at.y, timer);
      break;
    case TimeAttackLine::kBestTime:
      LiveDrawBestTime(at.x, at.y, timer);
      break;
    case TimeAttackLine::kCountdown:
      LiveDrawStartCountdown(at.x, at.y, timer);
      break;
  }
}

class LiveScreen final : public TimeAttackScreenHost {
 public:
  LiveScreen(TimeAttackState& state, TimeAttackAsset asset)
      : state_(state), owner_(Owner(asset)) {}
  void PulsePalette(std::int32_t weight) override {
    StoreState(state_);
    LivePulseTimeAttackClut(weight);
  }
  void Draw(TimeAttackLine line, ResultsPoint origin, std::int32_t timer,
            std::int32_t rank) override {
    StoreState(state_);
    DrawLine(line, origin, timer, rank);
  }
  void Sound(GameClearSound action, std::int32_t effect) override {
    StoreState(state_);
    if (action == GameClearSound::kStop) {
      Audio::LiveStopSfx(kSoundGroup, effect);
    } else {
      Audio::LivePlaySfxCentered(kSoundGroup, effect);
    }
  }
  void SetTickSpeed(std::int32_t speed) override {
    StoreState(state_);
    if (speed == kCountdownTickSpeed) countdown_controls = true;
    State::SetLiveTickSpeed(speed);
  }
  void SetEffectRotation(std::int32_t angle) override {
    Render::SetLiveFramebufferEffectRotation(angle);
  }
  void SetEffectScale(Render::FramebufferEffectPair scale) override {
    const Render_FramebufferEffectTranslation value{scale.x, scale.y, 0, 0};
    Render::SetLiveFramebufferEffectScale(&value);
  }
  void SetEffectTint(Render::FramebufferEffectTint tint) override {
    const CVECTOR value{tint.red, tint.green, tint.blue, 0};
    Render::SetLiveFramebufferEffectTint(&value);
  }
  void SetEffectOffset(Render::FramebufferEffectPair offset) override {
    const Render_FramebufferEffectTranslation value{offset.x, offset.y, 0, 0};
    Render::SetLiveFramebufferEffectOffset(&value);
  }
  void SetEffectBlend(std::int32_t mode) override {
    Render::SetLiveFramebufferEffectBlendMode(mode);
  }
  void SetEffectDepth(std::uint32_t depth) override {
    Render::SetLiveFramebufferEffectDepth(depth);
  }
  void SetPreviousFrame(bool previous) override {
    Render::SetLiveFramebufferPreviousFrame(previous ? 1 : 0);
  }
  bool EnableEffect(bool enabled) override {
    Render::SetLiveFramebufferEffectEnabled(enabled ? 1 : 0);
    if (!enabled) {
      countdown_controls = false;
      return true;
    }
    if (Render::IsLiveFramebufferEffectShowing() &&
        Render::HasLiveFramebufferEffectPackets()) {
      return true;
    }
    Refuse("countdown framebuffer allocation refused");
    return false;
  }
  void RetireTexturePages(const TexturePageLoan& loan) override {
    StoreState(state_);
    // Completion remains the caller's responsibility, even on a repeated
    // direct call. Failed retirement retains the native loan for cleanup.
    static_cast<void>(RetirePages(owner_, loan));
  }

 private:
  TimeAttackState& state_;
  LoadOwner& owner_;
};

class LivePresentations final : public ResultsPresentationHost {
 public:
  int Initialize(ResultsPresentation presentation) override {
    switch (presentation) {
      case ResultsPresentation::kGameClear:
        return LiveInitializeGameClear();
      case ResultsPresentation::kTimeAttackStart:
        return LiveInitializeTimeAttackStart();
      case ResultsPresentation::kTimeAttackResults:
        return LiveInitializeTimeAttackResults();
      case ResultsPresentation::kCubeStart:
        return LiveInitializeCubePuzzleStart();
      case ResultsPresentation::kCubeResults:
        return LiveInitializeCubePuzzleResults();
      case ResultsPresentation::kCubeQuit:
        return LiveInitializeCubePuzzleQuit();
      case ResultsPresentation::kHealingScoreOnly:
        break;
    }
    return 0;
  }
  int Update(ResultsPresentation presentation) override {
    switch (presentation) {
      case ResultsPresentation::kGameClear:
        return LiveUpdateGameClear();
      case ResultsPresentation::kTimeAttackStart:
        return LiveUpdateTimeAttackStart();
      case ResultsPresentation::kTimeAttackResults:
        return LiveUpdateTimeAttackResults();
      case ResultsPresentation::kCubeStart:
        return LiveUpdateCubePuzzleStart();
      case ResultsPresentation::kCubeResults:
        return LiveUpdateCubePuzzleResults();
      case ResultsPresentation::kCubeQuit:
        return LiveUpdateCubePuzzleQuit();
      case ResultsPresentation::kHealingScoreOnly:
        break;
    }
    return 0;
  }
  void ApplyHealingBonus() override { LiveApplyHealingScoreBonus(); }
  void FinishScreen() override { Framework::NoOperation(); }
};

int Return(GameClearStep step) {
  if (step == GameClearStep::kRefused) Refuse("invalid state or missing owner");
  return step == GameClearStep::kComplete ? 1 : 0;
}

int Return(std::optional<int> result) {
  if (!result.has_value()) {
    Refuse("missing caller state or unknown presentation selector");
    return 0;
  }
  return *result;
}

bool CleanupOwner(TimeAttackAsset asset, const TexturePageLoan& loan) {
  auto& owner = Owner(asset);
  if (owner.buffer != nullptr && !FreeBuffer(owner)) return false;
  ReleaseRequest(owner);
  return !owner.pages || RetirePages(owner, loan);
}
}  // namespace

// decomp: _initTimeAttackEnd, MENUF.PRG/3B8.c:256 (0x8010310C).
int LiveInitializeTimeAttackResults() {
  auto state = LoadState();
  LiveLoad host(TimeAttackAsset::kResults);
  const auto step = InitializeTimeAttackResults(state, host);
  StoreState(state);
  return Return(step);
}

// decomp: _initTimeAttackStart, MENUF.PRG/3B8.c:372 (0x80103530).
int LiveInitializeTimeAttackStart() {
  auto state = LoadState();
  LiveLoad host(TimeAttackAsset::kStart);
  const auto step = InitializeTimeAttackStart(state, host);
  StoreState(state);
  return Return(step);
}

// decomp: _renderTimeAttackEnd, MENUF.PRG/3B8.c:905 (0x80103D88).
int LiveUpdateTimeAttackResults() {
  auto state = LoadState();
  if (state.phase <= kResultsClosingPhase && !ValidCourse()) return 0;
  LiveScreen host(state, TimeAttackAsset::kResults);
  const auto step = UpdateTimeAttackResults(
      state, (VS::Input::buttonsPressed.allButtons & kConfirmMask) != 0, host);
  StoreState(state);
  return Return(step);
}

// decomp: _renderTimeAttackStart, MENUF.PRG/3B8.c:1014 (0x8010412C).
int LiveUpdateTimeAttackStart() {
  auto state = LoadState();
  if (state.phase < kResultsClosingPhase && !ValidCourse()) return 0;
  LiveScreen host(state, TimeAttackAsset::kStart);
  const auto step = UpdateTimeAttackStart(
      state, (VS::Input::buttonsPressed.allButtons & kConfirmMask) != 0, host);
  StoreState(state);
  return Return(step);
}

// decomp: _initMenu, MENUF.PRG/3B8.c:59 (0x80102C54).
int LiveInitializeResultsScreen() {
  LivePresentations host;
  return Return(InitializeResultsScreen(
      static_cast<std::uint8_t>(Audio_Seq_unusedStateFlags.menuFSubmenu),
      host));
}

// decomp: _execMenu, MENUF.PRG/3B8.c:453 (0x801037A0).
int LiveUpdateResultsPresentation() {
  LivePresentations host;
  return Return(UpdateResultsPresentation(
      static_cast<std::uint8_t>(Audio_Seq_unusedStateFlags.menuFSubmenu),
      host));
}

// decomp: vs_menuF_exec, MENUF.PRG/3B8.c:23 (0x80102BB8).
int LiveUpdateResultsScreen(unsigned char* caller_state) {
  LivePresentations host;
  const auto bytes = caller_state == nullptr
                         ? std::span<std::uint8_t>{}
                         : std::span<std::uint8_t>(caller_state, 1);
  return Return(UpdateResultsScreen(
      bytes, static_cast<std::uint8_t>(Audio_Seq_unusedStateFlags.menuFSubmenu),
      host));
}

void ResetLiveTimeAttackModuleData() {
  for (auto& owner : owners) {
    owner.request_bound = false;
    owner.buffer_bound = false;
  }
  Menu_Results_newTimeSlot = 0;
  Menu_Results_timeTrialTime = 0;
  std::ranges::fill(Menu_Results_clutBuf, 0);
}

bool CleanupLiveTimeAttackOwnedState() {
  if (countdown_controls) {
    State::SetLiveTickSpeed(kNormalTickSpeed);
    const Render_FramebufferEffectTranslation stopped{};
    Render::SetLiveFramebufferEffectScale(&stopped);
    Render::SetLiveFramebufferEffectEnabled(0);
    countdown_controls = false;
  }
  const bool results =
      CleanupOwner(TimeAttackAsset::kResults, kTimeAttackResultPages);
  const bool start =
      CleanupOwner(TimeAttackAsset::kStart, kTimeAttackStartPages);
  return results && start;
}

LiveTimeAttackLoadStatus InspectLiveTimeAttackLoad(TimeAttackAsset asset) {
  if (asset != TimeAttackAsset::kResults && asset != TimeAttackAsset::kStart) {
    Refuse("unknown time-attack asset");
    return {};
  }
  const auto& owner = Owner(asset);
  return {.request_bound = owner.request_bound,
          .buffer_bound = owner.buffer_bound,
          .owns_request = owner.request != nullptr,
          .owns_buffer = owner.buffer != nullptr,
          .owns_pages = owner.pages};
}

bool LiveTimeAttackOwnsCountdownControls() { return countdown_controls; }
}  // namespace VS::Menu::Results
