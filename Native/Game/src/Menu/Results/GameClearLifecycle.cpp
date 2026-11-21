#include "Menu/Results/GameClearLifecycle.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Render/UiTexturedQuads.hpp"

namespace VS::Menu::Results {
namespace {
namespace math = Core::Math;
constexpr std::uint32_t kHealingBonus = 100'000;
constexpr std::uint32_t kHealingPenalty = 100;
constexpr std::uint32_t kBossMusicFirst = 31;
constexpr std::uint32_t kBossMusicCount = 9;
constexpr std::uint32_t kLateBossMusicFirst = 45;
constexpr std::uint32_t kLateBossMusicCount = 5;
constexpr std::uint32_t kResultsMusicOffset = 100;
constexpr std::uint32_t kDefaultMusic = 128;
constexpr std::int32_t kMusicSlot = 2;
constexpr std::uint32_t kSelectionMask = 0xFF;
constexpr std::uint32_t kRowMask = 0xF;
constexpr unsigned kEntryShift = 4;
constexpr unsigned kSpeedShift = 3;
constexpr std::uint32_t kRandomEntryMask = 0xF0;
constexpr std::uint32_t kStopSpeed = 8;
constexpr std::uint32_t kTopSpeed = 48;
constexpr std::int32_t kStopped = 2;
constexpr std::int32_t kClosing = 3;
constexpr std::int32_t kReelStart = 272;
constexpr std::int32_t kPromptStart = 288;
constexpr std::int32_t kAutoStop = 2072;
constexpr std::int32_t kResultHold = 450;
constexpr std::int32_t kCloseFrames = 8;
constexpr std::int32_t kSpinSound = 0x74;
constexpr std::int32_t kSpinEcho = 0x75;
constexpr std::int32_t kConfirmSound = 0x76;
constexpr std::int32_t kRevealSound = 0x77;
constexpr std::int32_t kRevealEcho = 0x78;
constexpr std::int32_t kFadeVolume = 0x40;
constexpr std::int32_t kFadePitch = -0x10;
constexpr std::int32_t kFadeTicks = 0x30;
constexpr std::int32_t kScoreDelay = 32;
constexpr std::int32_t kMapDelay = 112;
constexpr std::int32_t kHeadingDelay = 192;
constexpr std::int32_t kRankDelay = 208;
constexpr std::int32_t kLineX = 160;
constexpr std::int32_t kBannerY = 40;
constexpr std::int32_t kScoreY = 80;
constexpr std::int32_t kMapY = 102;
constexpr std::int32_t kHeadingY = 138;
constexpr std::int32_t kRankY = 154;
constexpr ResultsPoint kReelOrigin{.x = 130, .y = 190};
constexpr ResultsPoint kPromptOrigin{.x = 214, .y = 187};
constexpr std::int32_t kWindowTop = 190;
constexpr std::int32_t kWindowBottom = 206;
constexpr std::int32_t kFadePerRow = 16;
constexpr std::int32_t kRowHeight = 16;
constexpr std::int16_t kScreenWidth = 320;
constexpr std::uint16_t kReelClut = 0x7FF2;
constexpr std::uint16_t kFadeClut = 0x7FF3;
constexpr std::uint16_t kSubtractive = 0x40;
constexpr std::size_t kLabel = 11;
constexpr std::size_t kCenterLabel = 14;
constexpr std::size_t kPlus = 24;
constexpr std::size_t kColon = 26;
constexpr std::int32_t kCenterPadding = 14;
constexpr std::int32_t kColonGap = 2;
constexpr std::int32_t kRowDrop = 8;
constexpr std::int32_t kPlusDrop = 4;
constexpr std::int32_t kDigitAdvance = 12;
constexpr std::int32_t kDecimal = 10;
constexpr std::size_t kMaxDigits = 5;
constexpr GameClearColors kReelColors = {
    Render::RegionColor{.r = 128, .g = 96, .b = 64, .step = 0},
    Render::RegionColor{.r = 200, .g = 180, .b = 160, .step = 0},
};
constexpr std::int32_t kTimPixelX = 832;
constexpr std::int32_t kTimPixelStride = 64;
constexpr std::int16_t kTimPixelY = 256;
constexpr std::int16_t kTimHeight = 255;
constexpr Render::ScreenRect kPaletteArea = {
    .x = 768,
    .y = 511,
    .width = 128,
    .height = 1,
};
constexpr std::size_t kAliasWord = 1;
constexpr std::uint32_t kAliasSource = 0x800000;
constexpr std::uint32_t kAliasTarget = 0x400000;
constexpr std::uint32_t kPercent = 100;
constexpr std::uint32_t kCountableRooms = 361;

std::int16_t Half(std::int32_t value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::size_t SelectedEntry(std::uint32_t selection) {
  return (selection & kSelectionMask) >> kEntryShift;
}

bool ValidReel(std::span<const BonusReel> reels, std::int32_t index) {
  return index >= 0 && static_cast<std::size_t>(index) < reels.size();
}

std::size_t SpinningEntry(std::uint32_t selection, std::size_t row) {
  return ((selection >> kEntryShift) + row - 1) & kRowMask;
}

bool Drawable(const BonusReelScene& scene, std::int32_t phase,
              const BonusReelView& view) {
  if (scene.atlas.size() <= kColon ||
      !ValidReel(scene.reels, view.reel_index)) {
    return false;
  }
  const auto& reel = scene.reels[static_cast<std::size_t>(view.reel_index)];
  const auto valid = [&](const BonusReelEntry& entry) {
    return entry.stat >= 0 &&
           static_cast<std::size_t>(entry.stat) < scene.atlas.size() &&
           entry.amount >= 0;
  };
  if (phase >= kStopped) return valid(reel.at(SelectedEntry(view.selection)));
  for (std::size_t row = 0; row < kBonusReelCount; ++row) {
    if (!valid(reel.at(SpinningEntry(view.selection, row)))) return false;
  }
  return true;
}

void DrawEntry(const BonusReelScene& scene, ResultsPoint at,
               const BonusReelEntry& entry, std::optional<std::int32_t> fade,
               BonusReelTarget& target) {
  const auto draw = [&](ResultsPoint origin, std::int32_t texture) {
    if (fade.has_value()) {
      target.Stopped(origin, texture, *fade, kReelClut);
    } else {
      target.Animated(origin, texture);
    }
  };
  const std::int32_t glyph_y = math::WrappingSubtract(at.y, 1);
  draw({.x = at.x, .y = glyph_y}, entry.stat);
  at.x = math::WrappingAdd(
      at.x, scene.atlas[static_cast<std::size_t>(entry.stat)].width);
  draw({.x = at.x, .y = math::WrappingAdd(at.y, kPlusDrop)},
       static_cast<std::int32_t>(kPlus));
  at.x = math::WrappingAdd(at.x, scene.atlas[kPlus].width);
  std::array<std::int32_t, kMaxDigits> digits{};
  std::size_t count = 0;
  std::int32_t amount = entry.amount;
  do {
    digits.at(count++) = amount % kDecimal;
    amount /= kDecimal;
  } while (amount != 0);
  while (count != 0) {
    draw({.x = at.x, .y = glyph_y}, digits.at(--count));
    at.x = math::WrappingAdd(at.x, kDigitAdvance);
  }
}

Render::ScreenRect ReelWindow(const Render::ScreenRect& clip, std::int32_t y,
                              std::uint32_t speed) {
  const std::uint32_t opening = speed >> kSpeedShift;
  return {.x = clip.x >= kScreenWidth ? kScreenWidth : std::int16_t{0},
          .y = Half(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(y) -
                                                opening)),
          .width = kScreenWidth,
          .height = Half(std::bit_cast<std::int32_t>(
              opening * 2 + static_cast<std::uint32_t>(kRowHeight)))};
}

Render::UiTexturedQuad RowQuad(const Render::TextureRegion& texture,
                               ResultsPoint at, std::uint8_t row) {
  return {.top_left = {.x = Half(at.x), .y = Half(at.y)},
          .bottom_right = {.x = Half(math::WrappingAdd(at.x, texture.width)),
                           .y = Half(math::WrappingAdd(at.y, 1))},
          .texture = {.u = texture.u,
                      .v = row,
                      .width = texture.width,
                      .height = 1,
                      .texture_page = texture.texture_page,
                      .clut = kReelClut},
          .tint = Render::QuadColor{.r = kResultsFullBrightness,
                                    .g = kResultsFullBrightness,
                                    .b = kResultsFullBrightness},
          .raw_texture = false,
          .translucent = true};
}

bool DrawRow(const Render::TextureRegion& texture, ResultsPoint at,
             std::uint8_t row, ResultsDrawTarget& target) {
  auto quad = RowQuad(texture, at, row);
  if (at.y >= kWindowTop && at.y < kWindowBottom) return target.DrawQuad(quad);
  const std::int32_t distance =
      at.y < kWindowTop ? math::WrappingSubtract(kWindowTop - 1, at.y)
                        : math::WrappingSubtract(at.y, kWindowBottom);
  const std::int32_t brightness = std::max(
      math::WrappingSubtract(kResultsFullBrightness,
                             math::WrappingMultiply(distance, kFadePerRow)),
      0);
  const auto shade = static_cast<std::uint8_t>(brightness);
  quad.tint = Render::QuadColor{.r = shade, .g = shade, .b = shade};
  quad.texture.clut = kFadeClut;
  quad.texture.texture_page = texture.texture_page | kResultsAdditiveBlend;
  const bool added = target.DrawQuad(quad);
  quad.texture.texture_page = texture.texture_page | kSubtractive;
  const bool subtracted = target.DrawQuad(quad);
  return added && subtracted;
}

bool UploadTextures(GameClearInitHost& host) {
  for (std::size_t index = 0; index < kGameClearTimCount; ++index) {
    const auto blocks = host.ParseTim(index);
    if (!blocks.has_value()) return false;
    if (blocks->pixels.has_value()) {
      auto area = *blocks->pixels;
      area.x =
          Half(kTimPixelX + static_cast<std::int32_t>(index) * kTimPixelStride);
      area.y = kTimPixelY;
      area.height = kTimHeight;
      if (!host.UploadPixels(index, area)) return false;
    }
    if (index == 0 && blocks->palette.has_value() &&
        !host.UploadPalette(index, kPaletteArea)) {
      return false;
    }
  }
  return true;
}

std::int32_t ChooseReel(const GameClearWorld& world) {
  if (world.intelligence >= world.strength && world.agility >= world.strength) {
    return 0;
  }
  if (world.strength >= world.intelligence &&
      world.agility >= world.intelligence) {
    return 1;
  }
  return 2;
}

void SetUpScore(GameClearState& game, GameClearWorld& world) {
  const auto healed = ApplyHealingScoreBonus(
      {.misc_score = world.record.misc_score, .heal_count = world.heal_count});
  world.record.misc_score = healed.misc_score;
  world.heal_count = healed.heal_count;
  game.clear_count = world.clear_count;
  auto& alias = world.room_flags.at(kAliasWord);
  if ((alias & kAliasSource) != 0) alias |= kAliasTarget;
  std::uint32_t rooms = 0;
  for (std::size_t word = 0; word < world.room_flags.size(); ++word) {
    rooms += static_cast<std::uint32_t>(std::popcount(
        world.room_flags.at(word) & world.countable_room_masks.at(word)));
  }
  const auto chests = static_cast<std::uint32_t>(std::ranges::count_if(
      world.chests_opened, [](std::uint8_t flag) { return flag != 0; }));
  world.record.map_completion = std::max(world.record.map_completion, rooms);
  world.record.opened_chest_count =
      std::max(world.record.opened_chest_count, chests);
  game.screen.map_completion = rooms * kPercent / kCountableRooms;
  game.screen.score = Score::ScoreFromRecord(world.record, world.clear_count);
  const auto rank =
      Score::RiskbreakerRankIndex(world.record.titles, game.screen.score);
  if (rank.has_value()) game.rank = static_cast<std::int32_t>(*rank);
}

void DrawScreen(GameClearScreen& screen, bool closing,
                GameClearScreenHost& host) {
  struct Line {
    GameClearLine kind;
    std::int32_t y;
    std::int32_t delay;
  };
  const std::array lines{
      Line{GameClearLine::kCongratulations, kBannerY, 0},
      Line{closing ? GameClearLine::kScore : GameClearLine::kCountingScore,
           kScoreY, kScoreDelay},
      Line{closing ? GameClearLine::kMap : GameClearLine::kCountingMap, kMapY,
           kMapDelay},
      Line{GameClearLine::kRankHeading, kHeadingY, kHeadingDelay},
      Line{GameClearLine::kRank, kRankY, kRankDelay}};
  const auto fade = math::WrappingSubtract(kCloseFrames, screen.state_timer);
  const auto phase = closing ? kClosing : screen.phase;
  for (const auto& line : lines) {
    host.Draw(line.kind, {.x = kLineX, .y = line.y},
              closing ? fade : math::WrappingSubtract(screen.timer, line.delay),
              phase, screen.readouts);
  }
  host.Reel(kReelOrigin,
            closing ? fade : math::WrappingSubtract(screen.timer, kReelStart),
            phase, screen.reel);
}

void BeginStopping(GameClearScreen& screen, bool confirm,
                   GameClearScreenHost& host) {
  if (confirm) host.Sound(GameClearSound::kPlay, kConfirmSound, 0, 0);
  screen.phase = 1;
  screen.state_timer = 0;
  for (const auto effect : {kSpinSound, kSpinEcho}) {
    host.Sound(GameClearSound::kVolume, effect, kFadeVolume, kFadeTicks);
  }
  for (const auto effect : {kSpinSound, kSpinEcho}) {
    host.Sound(GameClearSound::kPitch, effect, kFadePitch, kFadeTicks);
  }
}

void Reveal(GameClearScreen& screen, bool confirm, GameClearScreenHost& host) {
  auto& reel = screen.reel;
  reel.selection =
      (reel.selection + (reel.speed >> kSpeedShift)) & kSelectionMask;
  if (math::WrappingSubtract(screen.timer, kReelStart) <= 0) {
    if (confirm) {
      host.Sound(GameClearSound::kStop, 0, 0, 0);
      screen.timer = kReelStart;
      screen.readouts.score = {
          .shown = std::bit_cast<std::int32_t>(screen.score), .counting = 0};
      screen.readouts.map = {
          .shown = std::bit_cast<std::int32_t>(screen.map_completion),
          .counting = 0};
    }
    return;
  }
  if (reel.speed == kStopSpeed) {
    host.Sound(GameClearSound::kPlay, kSpinSound, 0, 0);
    host.Sound(GameClearSound::kPlay, kSpinEcho, 0, 0);
  }
  if (reel.speed < kTopSpeed) {
    if (screen.timer > kPromptStart) {
      ++reel.speed;
      host.Prompt(kPromptOrigin, static_cast<std::int32_t>(reel.speed),
                  screen.timer);
    }
    return;
  }
  if (confirm || screen.timer > kAutoStop) BeginStopping(screen, confirm, host);
  host.Prompt(kPromptOrigin, kPromptMaxSlide, screen.timer);
}

void StopReel(GameClearScreen& screen, GameClearScreenHost& host) {
  const auto slide =
      math::WrappingSubtract(kPromptMaxSlide, screen.state_timer);
  if (slide > 0) host.Prompt(kPromptOrigin, slide, screen.timer);
  auto& reel = screen.reel;
  const auto phase = reel.selection + (reel.speed >> kSpeedShift);
  reel.selection = phase & kSelectionMask;
  if (reel.speed > kStopSpeed) --reel.speed;
  if ((phase & kRowMask) == kStopSpeed && reel.speed == kStopSpeed) {
    host.Sound(GameClearSound::kStop, kSpinSound, 0, 0);
    host.Sound(GameClearSound::kStop, kSpinEcho, 0, 0);
    host.Sound(GameClearSound::kPlay, kRevealSound, 0, 0);
    host.Sound(GameClearSound::kPlay, kRevealEcho, 0, 0);
    screen.phase = kStopped;
    screen.state_timer = 0;
  }
}
}  // namespace

// decomp: addHealBonus, src/MENU/MENUF.PRG/3B8.c:2873; was _addHealBonus.
HealingScoreRecord ApplyHealingScoreBonus(HealingScoreRecord record) noexcept {
  record.misc_score =
      std::min(record.misc_score + kHealingBonus -
                   std::uint32_t{record.heal_count} * kHealingPenalty,
               Score::kMaxScore);
  record.heal_count = 0;
  return record;
}

// decomp: func_80103748, src/MENU/MENUF.PRG/3B8.c:450.
void QueueResultsMusic(std::uint32_t current_music, ResultsMusicHost& host) {
  const bool boss = current_music - kBossMusicFirst < kBossMusicCount ||
                    current_music - kLateBossMusicFirst < kLateBossMusicCount;
  host.LoadMusicSlot(boss ? current_music + kResultsMusicOffset : kDefaultMusic,
                     kMusicSlot);
}

// decomp: renderStatWheel, src/MENU/MENUF.PRG/3B8.c:2008; was _renderStatWheel.
ResultsDrawResult DrawBonusReel(const BonusReelScene& scene,
                                ResultsPoint origin, std::int32_t timer,
                                std::int32_t screen_state,
                                const BonusReelView& view,
                                BonusReelTarget& target) {
  const auto level = std::clamp(timer, 0, kGameClearRevealSteps);
  if (level == 0) return ResultsDrawResult::kHidden;
  if (!Drawable(scene, screen_state, view)) {
    return ResultsDrawResult::kInvalidTexture;
  }
  GameClearColors colors = kReelColors;
  colors[0].step = static_cast<std::uint8_t>(level);
  const auto& atlas = scene.atlas;
  const auto center =
      (atlas[kLabel].width + atlas[kColon].width + atlas[kCenterLabel].width +
       atlas[kPlus].width + kCenterPadding) /
      2;
  ResultsPoint at{.x = math::WrappingSubtract(origin.x, center), .y = origin.y};
  target.Tint(at, static_cast<std::int32_t>(kLabel), colors);
  at.x = math::WrappingAdd(at.x, atlas[kLabel].width);
  target.Tint(at, static_cast<std::int32_t>(kColon), colors);
  at.x = math::WrappingAdd(at.x, kColonGap + atlas[kColon].width);
  bool full = !target.SetDrawArea(scene.back_clip);
  const auto& reel = scene.reels[static_cast<std::size_t>(view.reel_index)];
  if (screen_state < kStopped) {
    at.y = math::WrappingAdd(
        at.y, kRowDrop + static_cast<std::int32_t>(view.selection & kRowMask));
    for (std::size_t row = 0; row < kBonusReelCount; ++row) {
      DrawEntry(scene, at, reel.at(SpinningEntry(view.selection, row)),
                std::nullopt, target);
      at.y = math::WrappingSubtract(at.y, kRowHeight);
    }
  } else {
    DrawEntry(scene, at, reel.at(SelectedEntry(view.selection)), level, target);
  }
  full |=
      !target.SetDrawArea(ReelWindow(scene.back_clip, origin.y, view.speed));
  return full ? ResultsDrawResult::kFull : ResultsDrawResult::kDrawn;
}

// decomp: renderAnimatedStatWheel, src/MENU/MENUF.PRG/3B8.c:2179; was
// _renderAnimatedStatWheel.
ResultsDrawResult DrawBonusReelTexture(
    std::span<const Render::TextureRegion> atlas, ResultsPoint origin,
    std::int32_t texture_index, ResultsDrawTarget& target) {
  if (texture_index < 0 ||
      static_cast<std::size_t>(texture_index) >= atlas.size()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto& texture = atlas[static_cast<std::size_t>(texture_index)];
  if (texture.height == 0) return ResultsDrawResult::kHidden;
  bool full = false;
  auto row = texture.v;
  for (std::uint32_t index = 0; index < texture.height; ++index) {
    full |= !DrawRow(texture, origin, row, target);
    origin.y = math::WrappingAdd(origin.y, 1);
    ++row;
  }
  return full ? ResultsDrawResult::kFull : ResultsDrawResult::kDrawn;
}

// decomp: initCongratulationsScreen, src/MENU/MENUF.PRG/3B8.c:132; was
// _initCongratulationsScreen.
GameClearStep InitializeGameClear(GameClearState& game,
                                  GameClearInitHost& host) {
  if (game.init_phase < 0 || game.init_phase > 2) {
    return GameClearStep::kRefused;
  }
  if (game.init_phase == 0) {
    if (!host.ReserveTexturePages(kGameClearTexturePages) ||
        !host.BeginTextureRead()) {
      return GameClearStep::kRefused;
    }
    ++game.init_phase;
    return GameClearStep::kPending;
  }
  if (game.init_phase == 1) {
    if (!host.TextureReadLoaded()) return GameClearStep::kPending;
    if (!UploadTextures(host)) return GameClearStep::kRefused;
    host.ReleaseTextureRequest();
    host.QueueMusic();
    ++game.init_phase;
    return GameClearStep::kPending;
  }
  if (host.MusicLoading()) return GameClearStep::kPending;
  host.PlayMusic();
  if (!host.FreeTextureBlock()) return GameClearStep::kRefused;
  auto& screen = game.screen;
  screen.readouts.map.shown = 0;
  screen.readouts.score.shown = 0;
  screen.timer = 0;
  screen.phase = 0;
  screen.reel.speed = 0;
  screen.reel.selection = (host.Random() & kRandomEntryMask) | kStopSpeed;
  auto world = host.ReadWorld();
  if (!world.has_value()) return GameClearStep::kRefused;
  screen.reel.reel_index = ChooseReel(*world);
  game.init_phase = 0;
  SetUpScore(game, *world);
  host.WriteWorld(*world);
  return GameClearStep::kComplete;
}

// decomp: renderCongratulationsScreen, src/MENU/MENUF.PRG/3B8.c:769; was
// _renderCongratulationsScreen.
GameClearStep UpdateGameClear(GameClearScreen& screen,
                              std::span<const BonusReel> reels, bool confirm,
                              GameClearScreenHost& host) {
  if (screen.phase < 0 || screen.phase > kClosing) {
    return GameClearStep::kRefused;
  }
  if (screen.phase == kClosing && screen.state_timer >= kCloseFrames &&
      !ValidReel(reels, screen.reel.reel_index)) {
    return GameClearStep::kRefused;
  }
  if (screen.phase != kClosing) {
    if (screen.timer == kRankDelay) {
      host.Sound(GameClearSound::kPlay, kRevealSound, 0, 0);
      host.Sound(GameClearSound::kPlay, kRevealEcho, 0, 0);
    }
    DrawScreen(screen, false, host);
  }
  switch (screen.phase) {
    case 0:
      Reveal(screen, confirm, host);
      break;
    case 1:
      StopReel(screen, host);
      break;
    case kStopped:
      if (confirm || screen.state_timer > kResultHold) {
        screen.phase = kClosing;
        screen.state_timer = 0;
      }
      break;
    default: {
      DrawScreen(screen, true, host);
      if (screen.state_timer < kCloseFrames) break;
      const auto& entry =
          reels[static_cast<std::size_t>(screen.reel.reel_index)].at(
              SelectedEntry(screen.reel.selection));
      if (entry.stat >= static_cast<std::int16_t>(BonusReelStat::kStrength) &&
          entry.stat <= static_cast<std::int16_t>(BonusReelStat::kMp)) {
        host.Award(static_cast<BonusReelStat>(entry.stat), entry.amount);
      }
      host.RetireTexturePages(kGameClearTexturePages);
      host.FinishScreen();
      return GameClearStep::kComplete;
    }
  }
  screen.state_timer = math::WrappingAdd(screen.state_timer, 1);
  screen.timer = math::WrappingAdd(screen.timer, 1);
  return GameClearStep::kPending;
}
}  // namespace VS::Menu::Results
