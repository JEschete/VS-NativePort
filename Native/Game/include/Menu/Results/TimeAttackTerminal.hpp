#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Menu/Results/GameClearLifecycle.hpp"
#include "Menu/Results/TimeAttackRating.hpp"
#include "Render/ScreenOverlay.hpp"

namespace VS::Menu::Results {
enum class TimeAttackAsset : std::uint8_t { kResults, kStart };
inline constexpr std::size_t kTimeAttackStartTims = 2;
inline constexpr TexturePageLoan kTimeAttackResultPages{29, 1, 5};
inline constexpr TexturePageLoan kTimeAttackStartPages{29, 2, 5};

struct TimeAttackState {
  // BATTLE owns init_phase; MENUF reload does not reset it.
  std::int32_t init_phase = 0;
  std::int32_t timer = 0;
  std::int32_t phase = 0;
  std::uint32_t animation = 0;
  std::int32_t new_slot = 0;
  std::int32_t time = 0;
  std::int32_t shown_map = 0;
  std::int32_t shown_score = 0;
  std::uint32_t selection = 0;
  friend bool operator==(const TimeAttackState&,
                         const TimeAttackState&) = default;
};

struct TimeAttackRun {
  std::uint8_t course = 0;
  std::uint8_t minutes = 0;
  std::uint8_t seconds = 0;
  std::uint8_t hundredths = 0;
  std::uint8_t round = 0;
  std::uint8_t difficulty = 0;
  std::span<TrialScoreRow> records;
};

class TimeAttackLoadHost {
 public:
  TimeAttackLoadHost() = default;
  TimeAttackLoadHost(const TimeAttackLoadHost&) = delete;
  TimeAttackLoadHost& operator=(const TimeAttackLoadHost&) = delete;
  TimeAttackLoadHost(TimeAttackLoadHost&&) = delete;
  TimeAttackLoadHost& operator=(TimeAttackLoadHost&&) = delete;
  virtual ~TimeAttackLoadHost() = default;

  virtual bool ReserveTexturePages(const TexturePageLoan& loan) = 0;
  virtual bool BeginTextureRead(TimeAttackAsset asset) = 0;
  virtual bool TextureReadLoaded() = 0;
  virtual std::optional<ResultsTimBlocks> ParseTim(std::size_t index) = 0;
  virtual bool UploadPixels(std::size_t index,
                            const Render::ScreenRect& area) = 0;
  // Clears the first word, not just the first color, before uploading.
  virtual bool UploadPalette(const Render::ScreenRect& area) = 0;
  virtual bool CopyResultPalette() = 0;
  virtual void ReleaseTextureRequest() = 0;
  virtual void QueueMusic() = 0;
  virtual bool MusicLoading() = 0;
  virtual void PlayMusic() = 0;
  virtual std::optional<TimeAttackRun> ReadRun() = 0;
  virtual void CommitRun(const TimeAttackRun& run) = 0;
  // Preflight owner validity without freeing, cancelling or resetting it.
  virtual bool CompletionReady() = 0;
  // State is observable at the free callback: results reset before freeing,
  // while the start screen frees before resetting.
  virtual bool FreeTextureBlock(const TimeAttackState& state) = 0;
};

// decomp: initTimeAttackEnd, src/MENU/MENUF.PRG/3B8.c:270; was
// _initTimeAttackEnd (0x8010310C).
GameClearStep InitializeTimeAttackResults(TimeAttackState& state,
                                          TimeAttackLoadHost& host);
// decomp: initTimeAttackStart, src/MENU/MENUF.PRG/3B8.c:386; was
// _initTimeAttackStart (0x80103530).
GameClearStep InitializeTimeAttackStart(TimeAttackState& state,
                                        TimeAttackLoadHost& host);

enum class TimeAttackLine : std::uint8_t {
  kResultsHeading,
  kCountingTime,
  kRatingWipe,
  kNewRecord,
  kStoredTime,
  kTime,
  kRating,
  kLeaderboard,
  kStartHeading,
  kCourse,
  kBoss,
  kBestTime,
  kCountdown,
};

class TimeAttackScreenHost {
 public:
  TimeAttackScreenHost() = default;
  TimeAttackScreenHost(const TimeAttackScreenHost&) = delete;
  TimeAttackScreenHost& operator=(const TimeAttackScreenHost&) = delete;
  TimeAttackScreenHost(TimeAttackScreenHost&&) = delete;
  TimeAttackScreenHost& operator=(TimeAttackScreenHost&&) = delete;
  virtual ~TimeAttackScreenHost() = default;

  virtual void PulsePalette(std::int32_t weight) = 0;
  void Draw(TimeAttackLine line, ResultsPoint origin, std::int32_t timer) {
    Draw(line, origin, timer, 0);
  }
  virtual void Draw(TimeAttackLine line, ResultsPoint origin,
                    std::int32_t timer, std::int32_t rank) = 0;
  virtual void Sound(GameClearSound action, std::int32_t effect) = 0;
  virtual void SetTickSpeed(std::int32_t speed) = 0;
  virtual void SetEffectRotation(std::int32_t angle) = 0;
  virtual void SetEffectScale(Render::FramebufferEffectPair scale) = 0;
  virtual void SetEffectTint(Render::FramebufferEffectTint tint) = 0;
  virtual void SetEffectOffset(Render::FramebufferEffectPair offset) = 0;
  virtual void SetEffectBlend(std::int32_t mode) = 0;
  virtual void SetEffectDepth(std::uint32_t depth) = 0;
  virtual void SetPreviousFrame(bool previous) = 0;
  virtual bool EnableEffect(bool enabled) = 0;
  virtual void RetireTexturePages(const TexturePageLoan& loan) = 0;
};

// decomp: renderTimeAttackEnd, src/MENU/MENUF.PRG/3B8.c:918; was
// _renderTimeAttackEnd (0x80103D88).
GameClearStep UpdateTimeAttackResults(TimeAttackState& state, bool confirm,
                                      TimeAttackScreenHost& host);
// decomp: renderTimeAttackStart, src/MENU/MENUF.PRG/3B8.c:1027; was
// _renderTimeAttackStart (0x8010412C).
GameClearStep UpdateTimeAttackStart(TimeAttackState& state, bool confirm,
                                    TimeAttackScreenHost& host);

enum class ResultsPresentation : std::uint8_t {
  kGameClear,
  kTimeAttackStart,
  kTimeAttackResults,
  kHealingScoreOnly,
  kCubeStart,
  kCubeResults,
  kCubeQuit,
};

class ResultsPresentationHost {
 public:
  ResultsPresentationHost() = default;
  ResultsPresentationHost(const ResultsPresentationHost&) = delete;
  ResultsPresentationHost& operator=(const ResultsPresentationHost&) = delete;
  ResultsPresentationHost(ResultsPresentationHost&&) = delete;
  ResultsPresentationHost& operator=(ResultsPresentationHost&&) = delete;
  virtual ~ResultsPresentationHost() = default;
  virtual int Initialize(ResultsPresentation presentation) = 0;
  virtual int Update(ResultsPresentation presentation) = 0;
  virtual void ApplyHealingBonus() = 0;
  virtual void FinishScreen() = 0;
};

// decomp: initMenu, src/MENU/MENUF.PRG/3B8.c:73; was _initMenu (0x80102C54).
// Unknown selectors refuse instead of returning the original stale register.
std::optional<int> InitializeResultsScreen(std::uint8_t selector,
                                           ResultsPresentationHost& host);
// decomp: execMenu, src/MENU/MENUF.PRG/3B8.c:467; was _execMenu (0x801037A0).
std::optional<int> UpdateResultsPresentation(std::uint8_t selector,
                                             ResultsPresentationHost& host);
// decomp: vs_menuF_exec, src/MENU/MENUF.PRG/3B8.c:32 (0x80102BB8).
// Only the first byte belongs to this entry. Initialization increments the
// pre-callback byte; presentation completion clears it after the callback.
std::optional<int> UpdateResultsScreen(std::span<std::uint8_t> caller_state,
                                       std::uint8_t selector,
                                       ResultsPresentationHost& host);
}  // namespace VS::Menu::Results
