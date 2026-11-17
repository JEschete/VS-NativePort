#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <vector>

#include "Menu/Results/ResultsPalette.hpp"
#include "Menu/Results/TimeAttackTerminal.hpp"

// Test-only endpoints shared by Catch2 and the native differential driver.
namespace VS::tests::time_attack_terminal {
namespace results = Menu::Results;
using StateWords = std::array<std::int64_t, 9>;
inline StateWords Words(const results::TimeAttackState& state) {
  return {state.init_phase, state.timer,       state.phase,
          state.animation,  state.new_slot,    state.time,
          state.shown_map,  state.shown_score, state.selection};
}

enum class Tag : std::int64_t {
  kPulse = 10,
  kDraw = 20,
  kSound = 40,
  kStop = 41,
  kReserve = 50,
  kAllocate = 51,
  kRequest = 52,
  kEnqueue = 53,
  kParse = 54,
  kPixels = 55,
  kPalette = 56,
  kCopyPalette = 57,
  kRelease = 58,
  kQueueMusic = 59,
  kPollMusic = 60,
  kPlayMusic = 61,
  kFree = 62,
  kRetire = 63,
  kTickSpeed = 70,
  kRotation = 71,
  kScale = 72,
  kTint = 73,
  kOffset = 74,
  kBlend = 75,
  kDepth = 76,
  kPrevious = 77,
  kEnabled = 78,
  kInitialize = 90,
  kUpdate = 91,
  kHeal = 92,
  kFinish = 93,
};
inline constexpr std::size_t kCallWords = 16;
using Call = std::array<std::int64_t, kCallWords>;
struct Calls {
  std::vector<Call> rows;
  void Add(Tag tag, std::initializer_list<std::int64_t> fields = {}) {
    Call row{};
    row.front() = static_cast<std::int64_t>(tag);
    std::ranges::copy(fields, row.begin() + 1);
    rows.push_back(row);
  }
  std::size_t Count(Tag tag) const {
    return static_cast<std::size_t>(
        std::ranges::count(rows, static_cast<std::int64_t>(tag),
                           [](const Call& row) { return row.front(); }));
  }
  std::vector<std::int64_t> Tags() const {
    std::vector<std::int64_t> tags;
    for (const auto& row : rows) tags.push_back(row.front());
    return tags;
  }
};

struct Load final : results::TimeAttackLoadHost {
  Calls calls;
  bool reserve_ok = true;
  bool begin_ok = true;
  bool loaded = true;
  bool parse_ok = true;
  bool upload_ok = true;
  bool copy_ok = true;
  bool music_busy = false;
  bool has_run = true;
  bool ready = true;
  bool free_ok = true;
  int file_bytes = 34816;
  std::array<bool, 4> present{true, true, true, true};
  std::array<Render::ScreenRect, 4> rectangles{
      Render::ScreenRect{1, 2, 64, 256}, Render::ScreenRect{3, 4, 256, 1},
      Render::ScreenRect{5, 6, 64, 256}, Render::ScreenRect{7, 8, 256, 1}};
  std::array<std::uint16_t, results::kResultsClutWords> palette{};
  std::array<std::uint16_t, results::kResultsClutWords> saved_palette{};
  std::array<results::TrialScoreRow, results::kTimeAttackCourses> scores{};
  results::TimeAttackRun run{.records = scores};
  std::optional<std::uint8_t> minutes_on_play;
  results::TimeAttackState at_free{};
  std::size_t commits = 0;

  bool ReserveTexturePages(const results::TexturePageLoan& loan) override {
    calls.Add(Tag::kReserve,
              {loan.first_page, loan.page_count, loan.owner_tag});
    return reserve_ok;
  }
  bool BeginTextureRead(results::TimeAttackAsset asset) override {
    calls.Add(Tag::kAllocate, {file_bytes});
    calls.Add(Tag::kRequest,
              {asset == results::TimeAttackAsset::kResults ? 1 : 2});
    calls.Add(Tag::kEnqueue, {1, 1, 1, 1});
    return begin_ok;
  }
  bool TextureReadLoaded() override { return loaded; }
  std::optional<results::ResultsTimBlocks> ParseTim(
      std::size_t index) override {
    calls.Add(Tag::kParse, {static_cast<std::int64_t>(index)});
    if (!parse_ok || index >= results::kTimeAttackStartTims) {
      return std::nullopt;
    }
    return results::ResultsTimBlocks{
        .pixels = present.at(2 * index)
                      ? std::optional(rectangles.at(2 * index))
                      : std::nullopt,
        .palette = present.at(2 * index + 1)
                       ? std::optional(rectangles.at(2 * index + 1))
                       : std::nullopt};
  }
  bool UploadPixels(std::size_t index,
                    const Render::ScreenRect& area) override {
    rectangles.at(2 * index) = area;
    calls.Add(Tag::kPixels, {static_cast<std::int64_t>(index), area.x, area.y,
                             area.width, area.height, 0});
    return upload_ok;
  }
  bool UploadPalette(const Render::ScreenRect& area) override {
    rectangles.at(1) = area;
    palette.at(0) = 0;
    palette.at(1) = 0;
    calls.Add(Tag::kPalette, {0, area.x, area.y, area.width, area.height, 0});
    return upload_ok;
  }
  bool CopyResultPalette() override {
    calls.Add(Tag::kCopyPalette, {1, 1, results::kResultsClutWords * 2});
    saved_palette = palette;
    return copy_ok;
  }
  void ReleaseTextureRequest() override { calls.Add(Tag::kRelease, {1}); }
  void QueueMusic() override { calls.Add(Tag::kQueueMusic); }
  bool MusicLoading() override {
    calls.Add(Tag::kPollMusic);
    return music_busy;
  }
  void PlayMusic() override {
    calls.Add(Tag::kPlayMusic, {2, 127, 0});
    if (minutes_on_play.has_value()) run.minutes = *minutes_on_play;
  }
  std::optional<results::TimeAttackRun> ReadRun() override {
    if (!has_run) return std::nullopt;
    return run;
  }
  void CommitRun(const results::TimeAttackRun&) override { ++commits; }
  bool CompletionReady() override { return ready; }
  bool FreeTextureBlock(const results::TimeAttackState& state) override {
    at_free = state;
    calls.Add(Tag::kFree, {1});
    const auto words = Words(state);
    std::ranges::copy(words, calls.rows.back().begin() + 2);
    return free_ok;
  }
};

struct Screen final : results::TimeAttackScreenHost {
  Calls calls;
  bool effect_ok = true;
  void PulsePalette(std::int32_t weight) override {
    calls.Add(Tag::kPulse, {weight});
  }
  void Draw(results::TimeAttackLine line, results::ResultsPoint origin,
            std::int32_t timer, std::int32_t rank) override {
    calls.Add(Tag::kDraw, {static_cast<std::int64_t>(line), origin.x, origin.y,
                           timer, rank});
  }
  void Sound(results::GameClearSound action, std::int32_t effect) override {
    calls.Add(
        action == results::GameClearSound::kStop ? Tag::kStop : Tag::kSound,
        {126, effect});
  }
  void SetTickSpeed(std::int32_t speed) override {
    calls.Add(Tag::kTickSpeed, {speed});
  }
  void SetEffectRotation(std::int32_t angle) override {
    calls.Add(Tag::kRotation, {angle});
  }
  void SetEffectScale(Render::FramebufferEffectPair scale) override {
    calls.Add(Tag::kScale, {scale.x, scale.y});
  }
  void SetEffectTint(Render::FramebufferEffectTint tint) override {
    calls.Add(Tag::kTint, {tint.red, tint.green, tint.blue});
  }
  void SetEffectOffset(Render::FramebufferEffectPair offset) override {
    calls.Add(Tag::kOffset, {offset.x, offset.y});
  }
  void SetEffectBlend(std::int32_t mode) override {
    calls.Add(Tag::kBlend, {mode});
  }
  void SetEffectDepth(std::uint32_t depth) override {
    calls.Add(Tag::kDepth, {depth});
  }
  void SetPreviousFrame(bool previous) override {
    calls.Add(Tag::kPrevious, {previous ? 1 : 0});
  }
  bool EnableEffect(bool enabled) override {
    calls.Add(Tag::kEnabled, {enabled ? 1 : 0});
    return effect_ok;
  }
  void RetireTexturePages(const results::TexturePageLoan& loan) override {
    calls.Add(Tag::kRetire, {loan.first_page, loan.page_count, loan.owner_tag});
  }
};

struct Presentations final : results::ResultsPresentationHost {
  Calls calls;
  std::int32_t result = 0;
  std::uint8_t* caller = nullptr;
  std::optional<std::uint8_t> callback_write;

  void Record(Tag tag, std::int64_t presentation) {
    calls.Add(tag, {presentation, caller == nullptr ? -1 : *caller});
    if (caller != nullptr && callback_write.has_value()) {
      *caller = *callback_write;
    }
  }
  int Initialize(results::ResultsPresentation presentation) override {
    Record(Tag::kInitialize, static_cast<std::int64_t>(presentation));
    return result;
  }
  int Update(results::ResultsPresentation presentation) override {
    Record(Tag::kUpdate, static_cast<std::int64_t>(presentation));
    return result;
  }
  void ApplyHealingBonus() override { Record(Tag::kHeal, 3); }
  void FinishScreen() override { Record(Tag::kFinish, 3); }
};
}  // namespace VS::tests::time_attack_terminal
