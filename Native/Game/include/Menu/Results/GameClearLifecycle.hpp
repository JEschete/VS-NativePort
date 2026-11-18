#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Menu/Results/GameClearScore.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Score/ScoreRank.hpp"
#include "Render/DisplayControl.hpp"

namespace VS::Menu::Results {
struct HealingScoreRecord {
  std::uint32_t misc_score = 0;
  std::uint16_t heal_count = 0;
};

// decomp: _addHealBonus, MENUF.PRG/3B8.c:2865 (0x80108E48).
[[nodiscard]] HealingScoreRecord ApplyHealingScoreBonus(
    HealingScoreRecord record) noexcept;

class ResultsMusicHost {
 public:
  ResultsMusicHost() = default;
  ResultsMusicHost(const ResultsMusicHost&) = delete;
  ResultsMusicHost& operator=(const ResultsMusicHost&) = delete;
  ResultsMusicHost(ResultsMusicHost&&) = delete;
  ResultsMusicHost& operator=(ResultsMusicHost&&) = delete;
  virtual ~ResultsMusicHost() = default;
  virtual void LoadMusicSlot(std::uint32_t music, std::int32_t slot) = 0;
};

// decomp: func_80103748, MENUF.PRG/3B8.c:436.
void QueueResultsMusic(std::uint32_t current_music, ResultsMusicHost& host);

inline constexpr std::size_t kBonusReelCount = 3;
inline constexpr std::size_t kBonusReelEntries = 16;
inline constexpr std::size_t kGameClearTimCount = 3;
inline constexpr std::size_t kGameClearTimStride = 0x8220;

enum class BonusReelStat : std::int16_t {
  kStrength = 13,
  kAgility,
  kIntelligence,
  kHp,
  kMp,
};

struct BonusReelEntry {
  std::int16_t stat = 0;
  std::int16_t amount = 0;
};
using BonusReel = std::array<BonusReelEntry, kBonusReelEntries>;

struct BonusReelView {
  std::uint32_t selection = 0;
  std::uint32_t speed = 0;
  std::int32_t reel_index = 0;
};

struct BonusReelScene {
  std::span<const Render::TextureRegion> atlas;
  std::span<const BonusReel> reels;
  Render::ScreenRect back_clip{};
};

class BonusReelTarget {
 public:
  BonusReelTarget() = default;
  BonusReelTarget(const BonusReelTarget&) = delete;
  BonusReelTarget& operator=(const BonusReelTarget&) = delete;
  BonusReelTarget(BonusReelTarget&&) = delete;
  BonusReelTarget& operator=(BonusReelTarget&&) = delete;
  virtual ~BonusReelTarget() = default;
  virtual void Tint(ResultsPoint origin, std::int32_t texture,
                    const GameClearColors& colors) = 0;
  virtual void Animated(ResultsPoint origin, std::int32_t texture) = 0;
  virtual void Stopped(ResultsPoint origin, std::int32_t texture,
                       std::int32_t timer, std::uint16_t clut) = 0;
  virtual bool SetDrawArea(const Render::ScreenRect& area) = 0;
};

// decomp: _renderStatWheel, MENUF.PRG/3B8.c:1996 (0x801060A8).
ResultsDrawResult DrawBonusReel(const BonusReelScene& scene,
                                ResultsPoint origin, std::int32_t timer,
                                std::int32_t screen_state,
                                const BonusReelView& view,
                                BonusReelTarget& target);
// decomp: _renderAnimatedStatWheel, MENUF.PRG/3B8.c:2168 (0x80107698).
ResultsDrawResult DrawBonusReelTexture(
    std::span<const Render::TextureRegion> atlas, ResultsPoint origin,
    std::int32_t texture_index, ResultsDrawTarget& target);

struct TexturePageLoan {
  std::uint32_t first_page = 0;
  std::uint32_t page_count = 0;
  std::int32_t owner_tag = 0;
};
inline constexpr TexturePageLoan kGameClearTexturePages = {
    .first_page = 0x1D,
    .page_count = 3,
    .owner_tag = 5,
};

struct GameClearReadouts {
  CountingReadout score{};
  CountingReadout map{};
};

struct GameClearScreen {
  std::int32_t timer = 0;
  std::int32_t phase = 0;
  std::int32_t state_timer = 0;
  BonusReelView reel{};
  std::uint32_t score = 0;
  std::uint32_t map_completion = 0;
  GameClearReadouts readouts{};
};

struct GameClearState {
  // BATTLE owns this phase; a MENUF reload must not clear it.
  std::int32_t init_phase = 0;
  GameClearScreen screen{};
  std::int32_t clear_count = 0;
  std::int32_t rank = 0;
};

struct GameClearWorld {
  Score::ScoreRecord record{};
  std::uint16_t heal_count = 0;
  std::array<std::uint32_t, Score::kRoomFlagWords> room_flags{};
  std::array<std::uint32_t, Score::kRoomFlagWords> countable_room_masks{};
  std::array<std::uint8_t, Score::kChestFlagCount> chests_opened{};
  std::uint8_t clear_count = 0;
  std::int32_t strength = 0;
  std::int32_t intelligence = 0;
  std::int32_t agility = 0;
};

struct ResultsTimBlocks {
  std::optional<Render::ScreenRect> pixels;
  std::optional<Render::ScreenRect> palette;
};

enum class GameClearStep : std::uint8_t { kPending, kComplete, kRefused };

class GameClearInitHost {
 public:
  GameClearInitHost() = default;
  GameClearInitHost(const GameClearInitHost&) = delete;
  GameClearInitHost& operator=(const GameClearInitHost&) = delete;
  GameClearInitHost(GameClearInitHost&&) = delete;
  GameClearInitHost& operator=(GameClearInitHost&&) = delete;
  virtual ~GameClearInitHost() = default;
  virtual bool ReserveTexturePages(const TexturePageLoan& loan) = 0;
  // Allocates RANK.DIS storage, allocates a request, then enqueues it.
  virtual bool BeginTextureRead() = 0;
  // Only original queue state Loaded advances; error is not completion. The
  // original queue never set its error state: a failed read was retried until
  // it loaded (main.c _processCdQueue, diskUpdate), so this step waited. The
  // native queue's terminal Error (#314) cannot load, and the step keeps
  // waiting without a retry; cold or VM reset cleanup releases its owners
  // (#410).
  virtual bool TextureReadLoaded() = 0;
  virtual std::optional<ResultsTimBlocks> ParseTim(std::size_t index) = 0;
  virtual bool UploadPixels(std::size_t index,
                            const Render::ScreenRect& area) = 0;
  // Clears the first 32-bit palette word before uploading.
  virtual bool UploadPalette(std::size_t index,
                             const Render::ScreenRect& area) = 0;
  virtual void ReleaseTextureRequest() = 0;
  virtual void QueueMusic() = 0;
  virtual bool MusicLoading() = 0;
  virtual void PlayMusic() = 0;
  virtual bool FreeTextureBlock() = 0;
  virtual std::uint32_t Random() = 0;
  virtual std::optional<GameClearWorld> ReadWorld() = 0;
  virtual void WriteWorld(const GameClearWorld& world) = 0;
};

// decomp: _initCongratulationsScreen, MENUF.PRG/3B8.c:118 (0x80102D10).
// Fresh presentation does not reset the private state timer or counting
// flags: the original only resets those on transition or overlay reload.
GameClearStep InitializeGameClear(GameClearState& game,
                                  GameClearInitHost& host);

enum class GameClearLine : std::uint8_t {
  kCongratulations,
  kCountingScore,
  kCountingMap,
  kRankHeading,
  kRank,
  kScore,
  kMap,
};
enum class GameClearSound : std::uint8_t { kPlay, kStop, kVolume, kPitch };

class GameClearScreenHost {
 public:
  GameClearScreenHost() = default;
  GameClearScreenHost(const GameClearScreenHost&) = delete;
  GameClearScreenHost& operator=(const GameClearScreenHost&) = delete;
  GameClearScreenHost(GameClearScreenHost&&) = delete;
  GameClearScreenHost& operator=(GameClearScreenHost&&) = delete;
  virtual ~GameClearScreenHost() = default;
  virtual void Draw(GameClearLine line, ResultsPoint origin, std::int32_t timer,
                    std::int32_t phase, GameClearReadouts& readouts) = 0;
  virtual void Reel(ResultsPoint origin, std::int32_t timer, std::int32_t phase,
                    const BonusReelView& view) = 0;
  virtual void Prompt(ResultsPoint origin, std::int32_t slide,
                      std::int32_t bounce) = 0;
  // Group is the original results SFX group 0x7E.
  virtual void Sound(GameClearSound action, std::int32_t effect,
                     std::int32_t value, std::int32_t ticks) = 0;
  virtual void Award(BonusReelStat stat, std::int32_t amount) = 0;
  virtual void RetireTexturePages(const TexturePageLoan& loan) = 0;
  virtual void FinishScreen() = 0;
};

// decomp: _renderCongratulationsScreen, MENUF.PRG/3B8.c:755 (0x8010384C).
// Completion leaves both timers unchanged. The production caller owns
// exiting: vs_menuF_exec resets its phase on completion, and the event menu
// that runs it returns and closes, so it is not called again. Deliberately no
// new once-only reward latch (#410).
GameClearStep UpdateGameClear(GameClearScreen& screen,
                              std::span<const BonusReel> reels, bool confirm,
                              GameClearScreenHost& host);
}  // namespace VS::Menu::Results
