#include "Menu/Results/GameClearLifecycleLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

#include "Audio/MusicLive.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SfxLive.hpp"
#include "Audio/SoundState.hpp"
#include "Battle/StatusEffects.h"
#include "Content/CdDriveLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "DiscQueue.h"
#include "Field/MapProgress.h"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Results/GameClearLifecycle.hpp"
#include "Menu/Results/GameClearScoreLive.hpp"
#include "Menu/Results/Results.h"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Menu/Results/ResultsStatsLive.hpp"
#include "Menu/Results/TimeAttack.hpp"
#include "Menu/Score/ScoreRankLive.hpp"
#include "Render/Display.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/TextureRegionDrawLive.hpp"
#include "Render/TimImage.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiTexturedQuads.hpp"
#include "State/Progress.hpp"
#include "State/StateFlags.hpp"

namespace VS::Menu::Results {
namespace {
constexpr int kOrderingSlot = -7;
constexpr std::int32_t kSoundGroup = 0x7E;
constexpr std::int32_t kMusicSlot = 2;
constexpr std::int32_t kMusicVolume = 0x7F;
constexpr std::int32_t kClosing = 3;
constexpr std::int32_t kCloseFrames = 8;
constexpr std::uint32_t kSelectionMask = 0xFF;
constexpr unsigned kEntryShift = 4;
constexpr std::size_t kHalfwordBytes = 2;
constexpr std::size_t kPaletteClearBytes = 4;
constexpr std::size_t kAreaCommandWords = 2;
static_assert(sizeof(u_long) == sizeof(std::uint32_t));
static_assert(std::size(VS::State::roomVisitProgress.room_flags) ==
              Score::kRoomFlagWords);
static_assert(std::size(Field_countableRoomMask) == Score::kRoomFlagWords);
static_assert(std::size(VS::State::stateFlags.chests_opened) ==
              Score::kChestFlagCount);

struct LoadOwner {
  std::byte* buffer = nullptr;
  std::size_t bytes = 0;
  Content_DiscQueueSlot* request = nullptr;
  std::uint64_t generation = 0;
  bool pages = false;
};
LoadOwner owner;
// These are the original file-backed private slots, not the native owner.
bool request_bound = false;
bool buffer_bound = false;
std::int32_t state_timer = 0;
std::uint64_t refusals = 0;

void Refuse(const char* reason) {
  if (std::has_single_bit(++refusals)) {
    VS::Core::Warnf("menu.results", "Game-clear refused: %s", reason);
  }
}

bool RequestMatches() {
  return owner.request != nullptr && owner.generation != 0 &&
         Content::LiveDiscRequestGeneration(owner.request) == owner.generation;
}

std::optional<std::span<std::byte>> OwnedBuffer() {
  if (owner.buffer == nullptr || owner.bytes == 0) {
    Refuse("missing RANK.DIS storage");
    return std::nullopt;
  }
  const auto capacity = Core::Memory::LivePayloadCapacity(owner.buffer);
  if (!capacity.has_value() || *capacity < owner.bytes) {
    Refuse("RANK.DIS storage no longer belongs to the live heap");
    return std::nullopt;
  }
  return std::span(owner.buffer, owner.bytes);
}

void ReleaseRequest() {
  if (owner.request != nullptr) {
    if (RequestMatches()) {
      Content::ReleaseLiveDiscRequest(owner.request);
    } else {
      Refuse("RANK.DIS request identity changed; not releasing another owner");
    }
  }
  owner.request = nullptr;
  owner.generation = 0;
  request_bound = false;
}

bool FreeBuffer() {
  const auto bytes = OwnedBuffer();
  if (!bytes.has_value()) return false;
  if (!Content::CancelLiveDiscWrites(*bytes)) {
    Refuse("RANK.DIS writes could not be cancelled; retaining storage");
    return false;
  }
  Core::Memory::FreeLiveFromHighEnd(owner.buffer);
  owner.buffer = nullptr;
  owner.bytes = 0;
  buffer_bound = false;
  return true;
}

bool RetirePageLoan(const TexturePageLoan& loan) {
  if (Render::RetireLiveTexturePageRange(loan.first_page, loan.page_count,
                                         loan.owner_tag) == 0) {
    Refuse("texture page retirement failed; retaining the outstanding loan");
    return false;
  }
  owner.pages = false;
  return true;
}

GameClearState LoadState() {
  return {.init_phase = Battle_unusedSubmenuState,
          .screen = {.timer = screenTimer,
                     .phase = screenState,
                     .state_timer = state_timer,
                     .reel = {.selection = Menu_Results_buffReelSelection,
                              .speed = Menu_Results_elementAnimationState,
                              .reel_index = Menu_Results_buffReelIndex},
                     .score = Menu_Results_score,
                     .map_completion = Menu_Results_mapCompletion,
                     .readouts =
                         {.score = {.shown = Menu_Results_onScreenScore,
                                    .counting = Menu_Results_incrementingScore},
                          .map = {.shown = Menu_Results_onScreenMapCompletion,
                                  .counting =
                                      Menu_Results_incrementingMapCompletion}}},
          .clear_count = Menu_Results_clearCount,
          .rank = Menu_Results_rank};
}

void StoreReadouts(const GameClearReadouts& readouts) {
  Menu_Results_onScreenScore = readouts.score.shown;
  Menu_Results_incrementingScore = readouts.score.counting;
  Menu_Results_onScreenMapCompletion = readouts.map.shown;
  Menu_Results_incrementingMapCompletion = readouts.map.counting;
}

void StoreState(const GameClearState& game) {
  Battle_unusedSubmenuState = game.init_phase;
  const auto& screen = game.screen;
  screenTimer = screen.timer;
  screenState = screen.phase;
  state_timer = screen.state_timer;
  Menu_Results_buffReelSelection = screen.reel.selection;
  Menu_Results_elementAnimationState = screen.reel.speed;
  Menu_Results_buffReelIndex = screen.reel.reel_index;
  Menu_Results_score = screen.score;
  Menu_Results_mapCompletion = screen.map_completion;
  StoreReadouts(screen.readouts);
  Menu_Results_clearCount = game.clear_count;
  Menu_Results_rank = game.rank;
}

std::array<BonusReel, kBonusReelCount> LiveReels() {
  std::array<BonusReel, kBonusReelCount> reels{};
  const std::span<const Menu_Results_BuffReel[kBonusReelEntries]> source(
      Menu_Results_buffReels, kBonusReelCount);
  for (std::size_t index = 0; index < reels.size(); ++index) {
    std::ranges::transform(source[index], reels.at(index).begin(),
                           [](const Menu_Results_BuffReel& entry) {
                             return BonusReelEntry{.stat = entry.stat,
                                                   .amount = entry.amount};
                           });
  }
  return reels;
}

void Report(ResultsDrawResult result) {
  if (result == ResultsDrawResult::kInvalidTexture) {
    Refuse("reel or glyph outside its bounded table");
  } else if (result == ResultsDrawResult::kFull) {
    Refuse("reel primitive buffer or ordering entry unavailable");
  }
}

class LiveRows final : public ResultsDrawTarget {
 public:
  bool DrawQuad(const Render::UiTexturedQuad& quad) override {
    auto* const entry = Render::LiveFrameOrderingEntry(
        Render::LiveOrderingBase::kScene, kOrderingSlot);
    if (entry == nullptr) return false;
    Render::LivePacketQueue queue(entry);
    auto* const storage =
        Render::ReserveLivePrimitiveWords(Render::kUiTexturedQuadWords);
    if (storage == nullptr) return false;
    Render::UiTexturedQuadPacket previous{};
    std::ranges::copy(std::span(storage, previous.size()), previous.begin());
    return queue.Emit(
        Render::BuildUiTexturedQuadPacket(quad, previous, queue.ChainLink()));
  }
  void DrawText(std::string_view, std::int32_t, std::uint32_t) override {
    Refuse("unexpected text in the texture-row renderer");
  }
};

class LiveReel final : public BonusReelTarget {
 public:
  void Tint(ResultsPoint origin, std::int32_t texture,
            const GameClearColors& colors) override {
    std::array<CVECTOR, 2> vectors{};
    std::ranges::transform(
        colors, vectors.begin(), [](const Render::RegionColor& color) {
          return CVECTOR{
              .r = color.r, .g = color.g, .b = color.b, .cd = color.step};
        });
    Render::DrawLiveTextureTintFade(Menu_Results_disMap, origin.x, origin.y,
                                    texture, vectors);
  }
  void Animated(ResultsPoint origin, std::int32_t texture) override {
    LiveDrawBonusReelTexture(origin.x, origin.y, texture);
  }
  void Stopped(ResultsPoint origin, std::int32_t texture, std::int32_t timer,
               std::uint16_t clut) override {
    LiveDrawFadingTextureClut(origin.x, origin.y, texture, timer, clut);
  }
  bool SetDrawArea(const Render::ScreenRect& area) override {
    auto* const entry = Render::LiveFrameOrderingEntry(
        Render::LiveOrderingBase::kScene, kOrderingSlot);
    if (entry == nullptr) return false;
    RECT rect{.x = area.x, .y = area.y, .w = area.width, .h = area.height};
    DR_AREA command{};
    ::SetDrawArea(&command, &rect);
    Render::LivePacketQueue queue(entry);
    const std::array<std::uint32_t, kAreaCommandWords + 1> words{
        Render::PackTagWord(kAreaCommandWords, queue.ChainLink()),
        command.code[0], command.code[1]};
    return queue.Emit(words);
  }
};

void DrawLiveReel(ResultsPoint origin, std::int32_t timer, std::int32_t phase,
                  const BonusReelView& view) {
  const auto atlas = LiveResultsAtlas();
  const auto reels = LiveReels();
  const auto frame = (static_cast<std::uint32_t>(VS::Render::frameBuf) + 1) & 1;
  const auto& clip = VS::Render::legacyDrawEnvironments[frame].clip;
  const BonusReelScene scene{
      .atlas = atlas,
      .reels = reels,
      .back_clip = {
          .x = clip.x, .y = clip.y, .width = clip.w, .height = clip.h}};
  LiveReel target;
  Report(DrawBonusReel(scene, origin, timer, phase, view, target));
}

class LiveInit final : public GameClearInitHost {
 public:
  bool ReserveTexturePages(const TexturePageLoan& loan) override {
    if (owner.pages || owner.buffer != nullptr || owner.request != nullptr) {
      Refuse("new load before the previous game-clear owner was cleaned up");
      return false;
    }
    owner.pages = Render::ReserveLiveTexturePageRange(
                      loan.first_page, loan.page_count, loan.owner_tag) != 0;
    return owner.pages;
  }
  bool BeginTextureRead() override {
    const auto& file = kDisFiles[Menu_Results_DisFile_Rank];
    if (file.size <= 0) {
      Refuse("invalid RANK.DIS size");
      return false;
    }
    owner.bytes = static_cast<std::size_t>(file.size);
    owner.buffer = static_cast<std::byte*>(
        Core::Memory::AllocateLiveFromHighEnd(owner.bytes));
    buffer_bound = owner.buffer != nullptr;
    if (!buffer_bound) {
      owner.bytes = 0;
      Refuse("RANK.DIS allocation refused");
      return false;
    }
    owner.request = Content::AllocateLiveDiscRequest(&file);
    if (owner.request != nullptr) {
      Content::EnqueueLiveDiscRequest(owner.request, owner.buffer);
    } else {
      owner.request =
          Content::CompleteLiveRefusedDiscRequest(&file, owner.buffer);
    }
    owner.generation = Content::LiveDiscRequestGeneration(owner.request);
    request_bound = RequestMatches();
    if (!request_bound) {
      Refuse("RANK.DIS request allocation refused");
      return false;
    }
    return true;
  }
  bool TextureReadLoaded() override {
    if (!request_bound || !buffer_bound || !RequestMatches()) {
      Refuse("missing or reused RANK.DIS controller request");
      return false;
    }
    if (owner.request->state == Content_DiscRequestState_Error) {
      Refuse("RANK.DIS queue error is not Loaded");
    }
    return owner.request->state == Content_DiscRequestState_Loaded;
  }
  std::optional<ResultsTimBlocks> ParseTim(std::size_t index) override {
    const auto bytes = TimBytes(index);
    if (!bytes.has_value()) return std::nullopt;
    tims_.at(index) = Render::ParseTim(*bytes);
    const auto& tim = tims_.at(index);
    if (!tim.has_value()) {
      Refuse("malformed RANK.DIS TIM");
      return std::nullopt;
    }
    ResultsTimBlocks blocks{.pixels = tim->image.area, .palette = std::nullopt};
    if (tim->clut.has_value()) blocks.palette = tim->clut->area;
    return blocks;
  }
  bool UploadPixels(std::size_t index,
                    const Render::ScreenRect& area) override {
    if (index >= tims_.size()) {
      Refuse("image upload outside the TIM table");
      return false;
    }
    const auto& tim = tims_.at(index);
    if (!tim.has_value()) {
      Refuse("image upload without a parsed TIM");
      return false;
    }
    return Upload(index, tim->image, area, false);
  }
  bool UploadPalette(std::size_t index,
                     const Render::ScreenRect& area) override {
    if (index >= tims_.size()) {
      Refuse("palette upload outside the TIM table");
      return false;
    }
    const auto& tim = tims_.at(index);
    if (!tim.has_value() || !tim->clut.has_value()) {
      Refuse("palette upload without a parsed CLUT");
      return false;
    }
    return Upload(index, *tim->clut, area, true);
  }
  void ReleaseTextureRequest() override { ReleaseRequest(); }
  void QueueMusic() override { LiveQueueResultsMusic(); }
  bool MusicLoading() override { return Audio::LivePollMusicLoads() != 0; }
  void PlayMusic() override {
    Audio::LivePlayMusicSlot(kMusicSlot, kMusicVolume, 0);
  }
  bool FreeTextureBlock() override {
    if (!buffer_bound) {
      Refuse("free without an active RANK.DIS controller buffer");
      return false;
    }
    if (!ReadLiveAshleyResultStats().has_value()) {
      Refuse("completion without character stats; retaining RANK.DIS storage");
      return false;
    }
    return FreeBuffer();
  }
  std::uint32_t Random() override {
    return static_cast<std::uint32_t>(VS::Core::NextBiosRand());
  }
  std::optional<GameClearWorld> ReadWorld() override {
    const auto stats = ReadLiveAshleyResultStats();
    if (!stats.has_value()) return std::nullopt;
    GameClearWorld world;
    world.record = Score::ReadLiveScoreRecord();
    world.heal_count = VS::State::scoreData.heal_count;
    for (std::size_t index = 0; index < world.room_flags.size(); ++index) {
      world.room_flags.at(index) = static_cast<std::uint32_t>(
          VS::State::roomVisitProgress.room_flags[index]);
      world.countable_room_masks.at(index) =
          static_cast<std::uint32_t>(Field_countableRoomMask[index]);
    }
    for (std::size_t index = 0; index < world.chests_opened.size(); ++index) {
      world.chests_opened.at(index) =
          static_cast<std::uint8_t>(VS::State::stateFlags.chests_opened[index]);
    }
    world.clear_count =
        static_cast<std::uint8_t>(VS::State::stateFlags.clear_count);
    world.strength = stats->total_strength;
    world.intelligence = stats->total_intelligence;
    world.agility = stats->total_agility;
    return world;
  }
  void WriteWorld(const GameClearWorld& world) override {
    VS::State::scoreData.misc_score = world.record.misc_score;
    VS::State::scoreData.heal_count = world.heal_count;
    VS::State::scoreData.map_completion = world.record.map_completion;
    VS::State::scoreData.opened_chest_count = world.record.opened_chest_count;
    for (std::size_t index = 0; index < world.room_flags.size(); ++index) {
      VS::State::roomVisitProgress.room_flags[index] =
          std::bit_cast<std::int32_t>(world.room_flags.at(index));
    }
  }

 private:
  std::array<std::optional<Render::TimImage>, kGameClearTimCount> tims_{};

  static std::optional<std::span<std::byte>> TimBytes(std::size_t index) {
    const auto bytes = OwnedBuffer();
    if (!buffer_bound || !bytes.has_value() || index >= kGameClearTimCount ||
        index * kGameClearTimStride >= bytes->size()) {
      Refuse("TIM offset outside the active RANK.DIS buffer");
      return std::nullopt;
    }
    const auto tail = bytes->subspan(index * kGameClearTimStride);
    return tail.first(std::min(tail.size(), kGameClearTimStride));
  }
  static bool Upload(std::size_t index, const Render::TimBlock& block,
                     const Render::ScreenRect& area, bool palette) {
    const auto bytes = TimBytes(index);
    if (!bytes.has_value()) return false;
    const std::array<std::int16_t, 4> rect{area.x, area.y, area.width,
                                           area.height};
    const auto count = static_cast<std::size_t>(
        std::max(0, static_cast<int>(area.width) * area.height));
    if (area.width <= 0 || area.height <= 0 ||
        block.rect_offset > bytes->size() ||
        sizeof(rect) > bytes->size() - block.rect_offset ||
        block.pixel_offset > bytes->size() ||
        count * kHalfwordBytes > bytes->size() - block.pixel_offset ||
        count * kHalfwordBytes > block.pixel_bytes ||
        (palette && block.pixel_bytes < kPaletteClearBytes)) {
      Refuse("TIM upload exceeds its parsed pixel or rectangle storage");
      return false;
    }
    std::memcpy(bytes->data() + block.rect_offset, rect.data(), sizeof(rect));
    auto pixels = bytes->subspan(block.pixel_offset, count * kHalfwordBytes);
    if (palette) {
      std::ranges::fill(pixels.first(kPaletteClearBytes), std::byte{0});
    }
    Render::UploadLiveImageRectangle(
        Render::PackScreenWord(area.x, area.y), pixels.data(),
        Render::PackScreenWord(area.width, area.height));
    return true;
  }
};

class LiveScreen final : public GameClearScreenHost {
 public:
  void Draw(GameClearLine line, ResultsPoint origin, std::int32_t timer,
            std::int32_t phase, GameClearReadouts& readouts) override {
    StoreReadouts(readouts);
    switch (line) {
      case GameClearLine::kCongratulations:
        LiveDrawCongratulations(origin.x, origin.y, timer);
        break;
      case GameClearLine::kCountingScore:
        LiveDrawCountingScore(origin.x, origin.y, timer, phase);
        break;
      case GameClearLine::kCountingMap:
        LiveDrawCountingMapCompletion(origin.x, origin.y, timer, phase);
        break;
      case GameClearLine::kRankHeading:
        LiveDrawRiskbreakerRankHeading(origin.x, origin.y, timer);
        break;
      case GameClearLine::kRank:
        LiveDrawRiskbreakerRank(origin.x, origin.y, timer);
        break;
      case GameClearLine::kScore:
        LiveDrawScore(origin.x, origin.y, timer, phase);
        break;
      case GameClearLine::kMap:
        LiveDrawMapCompletion(origin.x, origin.y, timer, phase);
        break;
    }
    readouts = LoadState().screen.readouts;
  }
  void Reel(ResultsPoint origin, std::int32_t timer, std::int32_t phase,
            const BonusReelView& view) override {
    DrawLiveReel(origin, timer, phase, view);
  }
  void Prompt(ResultsPoint origin, std::int32_t slide,
              std::int32_t bounce) override {
    LiveDrawPressButtonPrompt(origin.x, origin.y, slide, bounce);
  }
  void Sound(GameClearSound action, std::int32_t effect, std::int32_t value,
             std::int32_t ticks) override {
    switch (action) {
      case GameClearSound::kPlay:
        Audio::LivePlaySfxCentered(kSoundGroup, effect);
        break;
      case GameClearSound::kStop:
        Audio::LiveStopSfx(kSoundGroup, effect);
        break;
      case GameClearSound::kVolume:
        Audio::LiveFadeSfxVolume(kSoundGroup, effect, value, ticks);
        break;
      case GameClearSound::kPitch:
        Audio::LiveFadeSfxPitch(kSoundGroup, effect, value, ticks);
        break;
    }
  }
  void Award(BonusReelStat stat, std::int32_t amount) override {
    switch (stat) {
      case BonusReelStat::kStrength:
        ApplyLiveStrengthBonus(amount);
        break;
      case BonusReelStat::kAgility:
        ApplyLiveAgilityBonus(amount);
        break;
      case BonusReelStat::kIntelligence:
        ApplyLiveIntelligenceBonus(amount);
        break;
      case BonusReelStat::kHp:
        ApplyLiveHpBonus(amount);
        break;
      case BonusReelStat::kMp:
        ApplyLiveMpBonus(amount);
        break;
    }
  }
  void RetireTexturePages(const TexturePageLoan& loan) override {
    static_cast<void>(RetirePageLoan(loan));
  }
  void FinishScreen() override { Framework::NoOperation(); }
};

bool CanAward(const GameClearScreen& screen, std::span<const BonusReel> reels) {
  if (screen.phase != kClosing || screen.state_timer < kCloseFrames) {
    return true;
  }
  const auto reel = screen.reel.reel_index;
  if (reel < 0 || static_cast<std::size_t>(reel) >= reels.size()) return false;
  const auto& entry = reels[static_cast<std::size_t>(reel)].at(
      (screen.reel.selection & kSelectionMask) >> kEntryShift);
  if (entry.stat < static_cast<std::int16_t>(BonusReelStat::kStrength) ||
      entry.stat > static_cast<std::int16_t>(BonusReelStat::kMp)) {
    return true;
  }
  return ReadLiveAshleyResultStats().has_value();
}
}  // namespace

// decomp: _initCongratulationsScreen, src/MENU/MENUF.PRG/3B8.c:118.
int LiveInitializeGameClear() {
  auto game = LoadState();
  LiveInit host;
  const auto step = InitializeGameClear(game, host);
  StoreState(game);
  if (step == GameClearStep::kRefused) Refuse("invalid initializer context");
  return step == GameClearStep::kComplete ? 1 : 0;
}

// decomp: func_80103748, src/MENU/MENUF.PRG/3B8.c:436.
void LiveQueueResultsMusic() {
  class Host final : public ResultsMusicHost {
   public:
    void LoadMusicSlot(std::uint32_t music, std::int32_t slot) override {
      Audio::LiveLoadMusicSlot(std::bit_cast<std::int32_t>(music), slot);
    }
  };
  Host host;
  QueueResultsMusic(VS::Audio::soundState.current_music_id, host);
}

// decomp: _renderCongratulationsScreen, src/MENU/MENUF.PRG/3B8.c:755.
int LiveUpdateGameClear() {
  auto game = LoadState();
  const auto reels = LiveReels();
  if (!CanAward(game.screen, reels)) {
    Refuse("award without a valid reel and character stats");
    return 0;
  }
  const bool confirm = (VS::Input::buttonsPressed & PADRright) != 0;
  LiveScreen host;
  const auto step = UpdateGameClear(game.screen, reels, confirm, host);
  StoreState(game);
  if (step == GameClearStep::kRefused) Refuse("invalid presentation context");
  return step == GameClearStep::kComplete ? 1 : 0;
}

// decomp: _renderStatWheel, src/MENU/MENUF.PRG/3B8.c:1996.
void LiveDrawBonusReel(int x, int y, int timer, int screen_state) {
  DrawLiveReel({.x = x, .y = y}, timer, screen_state, LoadState().screen.reel);
}

// decomp: _renderAnimatedStatWheel, src/MENU/MENUF.PRG/3B8.c:2168.
void LiveDrawBonusReelTexture(int x, int y, int texture_index) {
  const auto atlas = LiveResultsAtlas();
  LiveRows target;
  Report(DrawBonusReelTexture(atlas, {.x = x, .y = y}, texture_index, target));
}

// decomp: _addHealBonus, src/MENU/MENUF.PRG/3B8.c:2865.
void LiveApplyHealingScoreBonus() {
  const auto healed =
      ApplyHealingScoreBonus({.misc_score = VS::State::scoreData.misc_score,
                              .heal_count = VS::State::scoreData.heal_count});
  VS::State::scoreData.misc_score = healed.misc_score;
  VS::State::scoreData.heal_count = healed.heal_count;
}

void ResetLiveGameClearModuleData() {
  GameClearState initial;
  initial.init_phase = Battle_unusedSubmenuState;
  StoreState(initial);
  request_bound = false;
  buffer_bound = false;
}

bool CleanupLiveGameClearOwnedState() {
  if (owner.buffer != nullptr && !FreeBuffer()) return false;
  ReleaseRequest();
  return !owner.pages || RetirePageLoan(kGameClearTexturePages);
}

LiveGameClearStatus InspectLiveGameClear() {
  return {.state_timer = state_timer,
          .request_bound = request_bound,
          .buffer_bound = buffer_bound,
          .owns_request = owner.request != nullptr,
          .owns_buffer = owner.buffer != nullptr,
          .owns_pages = owner.pages};
}
}  // namespace VS::Menu::Results
