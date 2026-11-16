#pragma once
// Hand-maintained (tools/port/owned.csv).
// MENUF.PRG cube-puzzle start, results and quit screens (F03046-F03051). The
// initializers read IQ.DIS or ESC.DIS through a CubePuzzleLoadHost, upload its
// images and palette and look up the room's par time and rank cap; the updates
// time the banners, sounds and prompt, pulse the palette (F03024, in
// ResultsPalette.hpp) and finish on Circle through a CubePuzzleScreenHost. The
// cores reach no global state.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Menu/Results/CubePuzzleText.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsPalette.hpp"
#include "Render/DisplayControl.hpp"

namespace VS::Menu::Results {
// The asset each initializer reads (_disFiles[3] and [4]).
enum class CubePuzzleAsset : std::uint8_t { kIq, kEsc };

// IQ.DIS and ESC.DIS hold their TIMs this many bytes apart; IQ.DIS has two,
// ESC.DIS one. Only the first TIM's palette is used.
inline constexpr std::size_t kCubePuzzleTimStride = 0x8220;
inline constexpr std::size_t kCubePuzzleStartImages = 2;
inline constexpr std::size_t kCubePuzzleQuitImages = 1;
// Each image goes to (832 + 64 * image, 256), 255 rows high, at its own width.
inline constexpr std::int16_t kCubePuzzleImageX = 0x340;
inline constexpr std::int16_t kCubePuzzleImageStep = 0x40;
inline constexpr std::int16_t kCubePuzzleImageY = 0x100;
inline constexpr std::int16_t kCubePuzzleImageRows = 0xFF;
// The first TIM's palette goes to the results palette area as 160 colors, its
// first two colors cleared.
inline constexpr std::size_t kCubePuzzleClearedColors = 2;

// T00181: IQ.DIS's puzzle records, 64 of { zone, map, par seconds, rank cap }
// as little-endian 16-bit values from byte 0x10440.
struct CubePuzzleRecord {
  std::int16_t zone_id = 0;
  std::int16_t map_id = 0;
  std::int16_t par_seconds = 0;
  std::int16_t rank_cap = 0;
};
inline constexpr std::size_t kCubePuzzleRecordOffset = 0x10440;
inline constexpr std::size_t kCubePuzzleRecordBytes = 8;
inline constexpr std::size_t kCubePuzzleRecordCount = 64;

// The room the puzzle was played in (the minimap scene's first room).
struct CubePuzzleRoom {
  std::int16_t zone_id = 0;
  std::int16_t map_id = 0;
};

// _puzzleParTime and _puzzleRankCap; a room without a record keeps these.
inline constexpr std::int16_t kCubePuzzleDefaultParSeconds = 600;
inline constexpr std::int16_t kCubePuzzleDefaultRankCap = 1;
struct CubePuzzleRating {
  std::int16_t par_seconds = kCubePuzzleDefaultParSeconds;
  std::int16_t rank_cap = kCubePuzzleDefaultRankCap;
};

// The puzzle clock and puzzleSetMaxTimer of vs_main_stateFlags. A set flag
// makes the results screen show 59'59"99.
struct CubePuzzleTimer {
  CubePuzzleClock clock;
  std::uint8_t show_maximum = 0;
};
inline constexpr std::uint8_t kCubePuzzleMaximumMinutes = 59;
inline constexpr std::uint8_t kCubePuzzleMaximumSeconds = 59;
inline constexpr std::uint8_t kCubePuzzleMaximumHundredths = 99;

// The Results module state these screens use: vs_battle_submenuState (the
// load step), _screenTimer, _screenState, _elementAnimationState and the
// counters the initializers clear.
struct CubePuzzleScreen {
  std::int32_t load_step = 0;
  std::int32_t timer = 0;
  std::int32_t screen_state = 0;
  std::uint32_t animation = 0;
  std::int32_t on_screen_map_completion = 0;
  std::int32_t on_screen_score = 0;
  std::uint32_t buff_reel_selection = 0;
};

// Texture pages 29 onwards, lent to owner 5: two for the start and results
// screens, one for the quit screen.
inline constexpr std::uint32_t kCubePuzzleFirstPage = 0x1D;
inline constexpr std::uint32_t kCubePuzzleStartPages = 2;
inline constexpr std::uint32_t kCubePuzzleQuitPages = 1;
inline constexpr std::int32_t kCubePuzzlePageOwner = 5;

// What an initializer could not use; the original read or wrote outside the
// data instead.
enum class CubePuzzleRefusal : std::uint8_t {
  // A TIM that does not parse, or whose pixels hold fewer than 255 rows.
  kImage,
  // A first TIM without a palette of at least 160 colors where it has one.
  kPalette,
  // An asset too short for the 64 puzzle records.
  kRecords,
  // No minimap room to look the records up with.
  kRoom,
};

class CubePuzzleLoadHost {
 public:
  CubePuzzleLoadHost() = default;
  CubePuzzleLoadHost(const CubePuzzleLoadHost&) = delete;
  CubePuzzleLoadHost& operator=(const CubePuzzleLoadHost&) = delete;
  CubePuzzleLoadHost(CubePuzzleLoadHost&&) = delete;
  CubePuzzleLoadHost& operator=(CubePuzzleLoadHost&&) = delete;
  virtual ~CubePuzzleLoadHost() = default;
  // func_8007DFF0.
  virtual void ReserveTexturePages(std::uint32_t first_page,
                                   std::uint32_t page_count,
                                   std::int32_t owner) = 0;
  // vs_main_allocHeapR, vs_main_allocateCdQueueSlot and vs_main_cdEnqueue of
  // the whole file.
  virtual void BeginAssetRead(CubePuzzleAsset asset) = 0;
  // The request reached the loaded state.
  [[nodiscard]] virtual bool AssetReadLoaded() = 0;
  // The loaded file's bytes; the initializer clears palette colors in them.
  [[nodiscard]] virtual std::span<std::byte> LoadedAsset() = 0;
  // LoadImage.
  virtual void UploadImage(const Render::ScreenRect& area,
                           std::span<const std::byte> pixels) = 0;
  [[nodiscard]] virtual std::optional<CubePuzzleRoom> CurrentRoom() = 0;
  // vs_main_freeCdQueueSlot.
  virtual void ReleaseAssetRead() = 0;
  // vs_main_freeHeapR.
  virtual void FreeAssetBuffer() = 0;
  virtual void Refuse(CubePuzzleRefusal refusal) = 0;
};

enum class CubePuzzleLoad : std::uint8_t { kLoading, kFinished };

// The record matching `room`, or nothing (the first of duplicates wins).
[[nodiscard]] std::optional<CubePuzzleRecord> FindCubePuzzleRecord(
    std::span<const std::byte> asset, CubePuzzleRoom room);

// Load step 0 reserves two pages and starts reading IQ.DIS; step 1 waits for
// it, uploads both images and the palette (also copied into `clut`), sets the
// rating from the room's record and releases the request; any other step
// frees the file, clears the screen state and finishes. The original's call
// to _nop0 (0x80108E38, an immediate return) is left out.
CubePuzzleLoad InitializeCubePuzzleStart(
    CubePuzzleScreen& screen, CubePuzzleRating& rating,
    std::span<std::uint16_t, kResultsClutWords> clut, CubePuzzleLoadHost& host);

// Shows 59'59"99 when the maximum flag is set (clearing it), at every step,
// then runs InitializeCubePuzzleStart.
CubePuzzleLoad InitializeCubePuzzleResults(
    CubePuzzleScreen& screen, CubePuzzleRating& rating, CubePuzzleTimer& timer,
    std::span<std::uint16_t, kResultsClutWords> clut, CubePuzzleLoadHost& host);

// As InitializeCubePuzzleStart with one page, ESC.DIS's one image and no
// records.
CubePuzzleLoad InitializeCubePuzzleQuit(
    CubePuzzleScreen& screen, std::span<std::uint16_t, kResultsClutWords> clut,
    CubePuzzleLoadHost& host);

// rsin(animation) / 512 + 8, the division rounding toward zero.
[[nodiscard]] std::int32_t CubePuzzlePulseWeight(std::uint32_t animation);

// The results rank: 15 minus the cap at or under par; above par, one more
// step per started two seconds, kept within 0-15. At or under par the rank is
// not bounded, so a cap outside 0-15 gives a rank outside the points table.
[[nodiscard]] std::int32_t CubePuzzleRank(CubePuzzleClock clock,
                                          CubePuzzleRating rating);

// _puzzleModeRankPoints.
inline constexpr std::array<std::uint32_t, 16> kCubePuzzleRankPoints{
    100,  200,  400,  600,  800,  1000, 2000,  3000,
    4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000,
};

// The messages the screens reveal (B50-09's draws and the par time).
enum class CubePuzzleMessage : std::uint8_t {
  kEvolveOrDie,
  kAverageTime,
  kParTime,
  kCountingTime,
  kFoodChain,
  kGivingUp,
  kChicken,
};

class CubePuzzleScreenHost {
 public:
  CubePuzzleScreenHost() = default;
  CubePuzzleScreenHost(const CubePuzzleScreenHost&) = delete;
  CubePuzzleScreenHost& operator=(const CubePuzzleScreenHost&) = delete;
  CubePuzzleScreenHost(CubePuzzleScreenHost&&) = delete;
  CubePuzzleScreenHost& operator=(CubePuzzleScreenHost&&) = delete;
  virtual ~CubePuzzleScreenHost() = default;
  // vs_main_playSfxDefault.
  virtual void PlaySfx(std::int32_t group, std::int32_t effect) = 0;
  // func_80045D64.
  virtual void StopSfx(std::int32_t group, std::int32_t effect) = 0;
  // func_8007E0A8.
  virtual void RetireTexturePages(std::uint32_t first_page,
                                  std::uint32_t page_count,
                                  std::int32_t owner) = 0;
  // _pulseCubePuzzleEnd.
  virtual void PulsePalette(std::int32_t blend_weight) = 0;
  // The giving-up and chicken messages ignore the x coordinate.
  virtual void DrawMessage(CubePuzzleMessage message, ResultsPoint origin,
                           std::int32_t timer) = 0;
  // _renderCubePuzzleRank.
  virtual void DrawRank(ResultsPoint origin, std::int32_t timer,
                        std::int32_t rank) = 0;
  // _renderPressButtonPrompt.
  virtual void DrawPrompt(ResultsPoint origin, std::int32_t slide_timer,
                          std::int32_t bounce_timer) = 0;
};

enum class CubePuzzleScreenStep : std::uint8_t {
  kShowing,
  kFinished,
  // Finished without awarding points: the rank lies outside the points table.
  kFinishedRankOutsideTable,
};

// Every screen counts its timer up to 0x7FFF and finishes on Circle once the
// timer reaches its end; an earlier Circle skips to the end.
inline constexpr std::int32_t kCubePuzzleTimerLimit = 0x7FFF;
inline constexpr std::int32_t kCubePuzzleStartEnd = 78;
inline constexpr std::int32_t kCubePuzzleResultsEnd = 138;
inline constexpr std::int32_t kCubePuzzleQuitEnd = 63;
inline constexpr std::int32_t kCubePuzzleSfxGroup = 0x7E;

// "Evolve or die", the average-time heading, the par time and the prompt.
CubePuzzleScreenStep UpdateCubePuzzleStart(CubePuzzleScreen& screen,
                                           bool confirm_pressed,
                                           CubePuzzleScreenHost& host);

// Pulses the palette, then the counting time, "food chain", the rank and the
// prompt; finishing adds the rank's points to `misc_score`. The original
// indexed _puzzleModeRankPoints with the rank unchecked; a rank outside the
// table finishes without points (#409). The results and quit screens' calls to
// _nop1 (0x80108E40, an immediate return) are left out.
CubePuzzleScreenStep UpdateCubePuzzleResults(CubePuzzleScreen& screen,
                                             CubePuzzleRating rating,
                                             CubePuzzleClock clock,
                                             bool confirm_pressed,
                                             std::uint32_t& misc_score,
                                             CubePuzzleScreenHost& host);

// Pulses the palette, then "giving up", "chicken" and the prompt.
CubePuzzleScreenStep UpdateCubePuzzleQuit(CubePuzzleScreen& screen,
                                          bool confirm_pressed,
                                          CubePuzzleScreenHost& host);
}  // namespace VS::Menu::Results
