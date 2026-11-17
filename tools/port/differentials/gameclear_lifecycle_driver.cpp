// Native endpoint recordings for the six independent MENUF game-clear cases.
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Menu/Results/GameClearLifecycle.hpp"
#include "Render/UiTexturedQuads.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;
using Output = std::vector<std::int64_t>;
constexpr std::size_t kCallWords = 12;
constexpr std::size_t kAtlasEntries = 127;
constexpr std::size_t kCapacity = 512;
constexpr std::uint32_t kPrimitiveBase = 0x80180000;
constexpr std::size_t kTimBlocks = r::kGameClearTimCount * 2;
constexpr std::size_t kColorWords = 8;
constexpr std::size_t kColorBytes = 4;
constexpr std::size_t kColorStep = 3;
constexpr std::int32_t kAreaPacketBytes = 12;
constexpr std::int32_t kLoaded = 4;
constexpr std::int32_t kSoundGroup = 126;
constexpr std::int32_t kMusicVolume = 127;
constexpr std::uint32_t kConfirmBit = 0x20;
constexpr std::uint32_t kPacketTagMask = 0xFF000000;
constexpr std::uint32_t kPacketAddressMask = 0xFFFFFF;
constexpr std::uint32_t kStaleWordStep = 0x01020304;
constexpr std::array<std::int64_t, kColorWords> kOriginalReelColors = {
    128, 96, 64, 0, 200, 180, 160, 0,
};

enum CallTag : std::int64_t {
  kDrawLine = 10,
  kDrawReel = 17,
  kPrompt = 18,
  kSound = 30,
  kAward = 40,
  kRetire = 41,
  kFinish = 42,
  kReserve = 50,
  kAllocate = 51,
  kRequest = 52,
  kEnqueue = 53,
  kParseTim = 54,
  kUploadPixels = 55,
  kUploadPalette = 56,
  kReleaseRequest = 57,
  kQueueMusic = 58,
  kPollMusic = 59,
  kPlayMusic = 60,
  kFree = 61,
  kRandom = 62,
  kReadStrength = 63,
  kReadIntelligence = 64,
  kReadAgility = 65,
};
enum CaseMode : std::int32_t {
  kInitializerCase = 0,
  kMusicCase,
  kScreenCase,
  kReelCase,
  kTextureRowsCase,
  kHealingCase,
};
enum ReelCallTag : std::int64_t {
  kTintCall = 0,
  kAnimatedCall,
  kStoppedCall,
  kAreaCall,
  kInsertCall,
};

template <typename T = std::int32_t>
T Read() {
  std::int64_t value = 0;
  std::cin >> value;
  return static_cast<T>(value);
}

struct Calls {
  std::vector<std::array<std::int64_t, kCallWords>> rows;
  void Add(std::initializer_list<std::int64_t> fields) {
    std::array<std::int64_t, kCallWords> row{};
    std::ranges::copy(fields, row.begin());
    rows.push_back(row);
  }
  void Append(Output& output) const {
    output.push_back(static_cast<std::int64_t>(rows.size()));
    for (const auto& row : rows) {
      output.insert(output.end(), row.begin(), row.end());
    }
  }
};

r::GameClearState ReadState() {
  r::GameClearState game;
  game.init_phase = Read();
  auto& s = game.screen;
  s.timer = Read();
  s.phase = Read();
  s.state_timer = Read();
  s.reel.selection = Read<std::uint32_t>();
  s.reel.speed = Read<std::uint32_t>();
  s.reel.reel_index = Read();
  s.score = Read<std::uint32_t>();
  s.map_completion = Read<std::uint32_t>();
  s.readouts.score.shown = Read();
  s.readouts.score.counting = Read();
  s.readouts.map.shown = Read();
  s.readouts.map.counting = Read();
  game.clear_count = Read();
  game.rank = Read();
  return game;
}

void AppendState(Output& output, const r::GameClearState& game) {
  const auto& s = game.screen;
  const std::initializer_list<std::int64_t> values{game.init_phase,
                                                   s.timer,
                                                   s.phase,
                                                   s.state_timer,
                                                   s.reel.selection,
                                                   s.reel.speed,
                                                   s.reel.reel_index,
                                                   s.score,
                                                   s.map_completion,
                                                   s.readouts.score.shown,
                                                   s.readouts.score.counting,
                                                   s.readouts.map.shown,
                                                   s.readouts.map.counting,
                                                   game.clear_count,
                                                   game.rank};
  output.insert(output.end(), values.begin(), values.end());
}

auto ReadReels() {
  std::array<r::BonusReel, r::kBonusReelCount> reels{};
  for (auto& reel : reels) {
    for (auto& entry : reel) {
      entry.stat = Read<std::int16_t>();
      entry.amount = Read<std::int16_t>();
    }
  }
  return reels;
}

auto ReadAtlas() {
  std::array<render::TextureRegion, kAtlasEntries> atlas{};
  for (auto& entry : atlas) {
    entry.u = Read<std::uint8_t>();
    entry.v = Read<std::uint8_t>();
    entry.width = Read<std::uint8_t>();
    entry.height = Read<std::uint8_t>();
    entry.texture_page = Read<std::uint16_t>();
    entry.clut = Read<std::uint16_t>();
  }
  return atlas;
}

render::ScreenRect ReadRect() {
  return {Read<std::int16_t>(), Read<std::int16_t>(), Read<std::int16_t>(),
          Read<std::int16_t>()};
}

struct Music final : r::ResultsMusicHost {
  Output output;
  void LoadMusicSlot(std::uint32_t music, std::int32_t slot) override {
    output = {music, slot};
  }
};

struct Screen final : r::GameClearScreenHost {
  Calls calls;
  std::uint32_t delta = 0;
  void Draw(r::GameClearLine line, r::ResultsPoint origin, std::int32_t timer,
            std::int32_t phase, r::GameClearReadouts& readouts) override {
    const bool phase_argument = line == r::GameClearLine::kCountingScore ||
                                line == r::GameClearLine::kCountingMap ||
                                line == r::GameClearLine::kScore ||
                                line == r::GameClearLine::kMap;
    calls.Add({kDrawLine + static_cast<int>(line), origin.x, origin.y, timer,
               phase_argument ? phase : 0});
    if (line == r::GameClearLine::kCountingScore ||
        line == r::GameClearLine::kCountingMap) {
      auto& counter = line == r::GameClearLine::kCountingScore ? readouts.score
                                                               : readouts.map;
      counter.shown = std::bit_cast<std::int32_t>(
          static_cast<std::uint32_t>(counter.shown) + delta);
      counter.counting ^= 1;
    }
  }
  void Reel(r::ResultsPoint origin, std::int32_t timer, std::int32_t phase,
            const r::BonusReelView& view) override {
    calls.Add({kDrawReel, origin.x, origin.y, timer, phase, view.selection,
               view.speed, view.reel_index});
  }
  void Prompt(r::ResultsPoint origin, std::int32_t slide,
              std::int32_t bounce) override {
    calls.Add({kPrompt, origin.x, origin.y, slide, bounce});
  }
  void Sound(r::GameClearSound action, std::int32_t effect, std::int32_t value,
             std::int32_t ticks) override {
    calls.Add(
        {kSound + static_cast<int>(action), kSoundGroup, effect, value, ticks});
  }
  void Award(r::BonusReelStat stat, std::int32_t amount) override {
    calls.Add({kAward, static_cast<int>(stat), amount});
  }
  void RetireTexturePages(const r::TexturePageLoan& loan) override {
    calls.Add({kRetire, loan.first_page, loan.page_count, loan.owner_tag});
  }
  void FinishScreen() override { calls.Add({kFinish}); }
};

struct Initializer final : r::GameClearInitHost {
  r::GameClearWorld world;
  Calls calls;
  std::uint32_t random = 0;
  bool music_busy = false;
  std::int32_t queue_state = 0;
  std::uint32_t file_bytes = 0;
  std::array<bool, kTimBlocks> present{};
  std::array<render::ScreenRect, kTimBlocks> rects{};
  std::uint32_t palette_word = 0;

  void ReadInput() {
    auto& record = world.record;
    record.titles = Read<std::uint32_t>();
    for (auto& value : record.enemy_kills) value = Read<std::uint16_t>();
    for (auto& value : record.weapon_attacks) value = Read<std::uint16_t>();
    for (auto& value : record.boss_trial_words) value = Read<std::uint32_t>();
    record.max_chain = Read<std::uint16_t>();
    record.completion_time_minutes = Read<std::uint32_t>();
    record.map_completion = Read<std::uint32_t>();
    record.opened_chest_count = Read<std::uint32_t>();
    record.streak_score = Read<std::uint32_t>();
    record.misc_score = Read<std::uint32_t>();
    world.heal_count = Read<std::uint16_t>();
    for (auto& value : world.room_flags) value = Read<std::uint32_t>();
    for (auto& value : world.countable_room_masks) {
      value = Read<std::uint32_t>();
    }
    for (auto& value : world.chests_opened) value = Read<std::uint8_t>();
    world.clear_count = Read<std::uint8_t>();
    world.strength = Read();
    world.intelligence = Read();
    world.agility = Read();
    random = Read<std::uint32_t>();
    music_busy = Read() != 0;
    queue_state = Read();
    file_bytes = Read<std::uint32_t>();
    for (auto& value : present) value = Read() != 0;
    for (auto& rect : rects) rect = ReadRect();
    palette_word = Read<std::uint32_t>();
  }
  bool ReserveTexturePages(const r::TexturePageLoan& loan) override {
    calls.Add({kReserve, loan.first_page, loan.page_count, loan.owner_tag});
    return true;
  }
  bool BeginTextureRead() override {
    calls.Add({kAllocate, file_bytes});
    calls.Add({kRequest, 1});
    calls.Add({kEnqueue, 1, 1, 1, 1});
    return true;
  }
  bool TextureReadLoaded() override { return queue_state == kLoaded; }
  std::optional<r::ResultsTimBlocks> ParseTim(std::size_t index) override {
    calls.Add({kParseTim, static_cast<std::int64_t>(index)});
    r::ResultsTimBlocks result;
    if (present.at(index * 2)) result.pixels = rects.at(index * 2);
    if (present.at(index * 2 + 1)) result.palette = rects.at(index * 2 + 1);
    return result;
  }
  bool UploadPixels(std::size_t index,
                    const render::ScreenRect& area) override {
    rects.at(index * 2) = area;
    calls.Add({kUploadPixels, static_cast<std::int64_t>(index), area.x, area.y,
               area.width, area.height});
    return true;
  }
  bool UploadPalette(std::size_t index,
                     const render::ScreenRect& area) override {
    rects.at(index * 2 + 1) = area;
    palette_word = 0;
    calls.Add({kUploadPalette, static_cast<std::int64_t>(index), area.x, area.y,
               area.width, area.height, palette_word});
    return true;
  }
  void ReleaseTextureRequest() override { calls.Add({kReleaseRequest, 1}); }
  void QueueMusic() override { calls.Add({kQueueMusic}); }
  bool MusicLoading() override {
    calls.Add({kPollMusic});
    return music_busy;
  }
  void PlayMusic() override { calls.Add({kPlayMusic, 2, kMusicVolume, 0}); }
  bool FreeTextureBlock() override {
    calls.Add({kFree, 1});
    return true;
  }
  std::uint32_t Random() override {
    calls.Add({kRandom});
    return random;
  }
  std::optional<r::GameClearWorld> ReadWorld() override {
    calls.Add({kReadStrength});
    calls.Add({kReadIntelligence});
    calls.Add({kReadAgility});
    return world;
  }
  void WriteWorld(const r::GameClearWorld& value) override { world = value; }
  void Append(Output& output) const {
    output.insert(output.end(), {world.record.misc_score, world.heal_count,
                                 world.record.map_completion,
                                 world.record.opened_chest_count});
    for (const auto word : world.room_flags) output.push_back(word);
    for (const auto& rect : rects) {
      output.insert(output.end(), {rect.x, rect.y, rect.width, rect.height});
    }
    output.push_back(palette_word);
    calls.Append(output);
  }
};

struct Reel final : r::BonusReelTarget {
  Calls calls;
  std::int32_t cursor = 0;
  std::array<std::int64_t, kColorWords> colors = kOriginalReelColors;
  void Tint(r::ResultsPoint origin, std::int32_t texture,
            const r::GameClearColors& tint) override {
    colors = {tint[0].r, tint[0].g, tint[0].b, tint[0].step,
              tint[1].r, tint[1].g, tint[1].b, tint[1].step};
    calls.Add({kTintCall, origin.x, origin.y, texture, colors[0], colors[1],
               colors[2], colors[kColorStep], colors[kColorBytes],
               colors[kColorBytes + 1], colors[kColorBytes + 2],
               colors[kColorBytes + kColorStep]});
  }
  void Animated(r::ResultsPoint origin, std::int32_t texture) override {
    calls.Add({kAnimatedCall, origin.x, origin.y, texture});
  }
  void Stopped(r::ResultsPoint origin, std::int32_t texture, std::int32_t timer,
               std::uint16_t clut) override {
    calls.Add({kStoppedCall, origin.x, origin.y, texture, timer, clut});
  }
  bool SetDrawArea(const render::ScreenRect& area) override {
    calls.Add({kAreaCall, area.x, area.y, area.width, area.height});
    calls.Add({kInsertCall, 0, cursor});
    cursor += kAreaPacketBytes;
    return true;
  }
};

struct Rows final : r::ResultsDrawTarget {
  std::array<std::uint32_t, kCapacity * render::kUiTexturedQuadWords> words{};
  std::size_t cursor = 0;
  std::uint32_t ordering = 0;
  bool DrawQuad(const render::UiTexturedQuad& quad) override {
    render::UiTexturedQuadPacket previous{};
    std::copy_n(words.begin() + static_cast<std::ptrdiff_t>(cursor),
                previous.size(), previous.begin());
    const auto packet =
        render::BuildUiTexturedQuadPacket(quad, previous, ordering);
    std::ranges::copy(packet,
                      words.begin() + static_cast<std::ptrdiff_t>(cursor));
    ordering = (ordering & kPacketTagMask) |
               ((kPrimitiveBase +
                 static_cast<std::uint32_t>(cursor * sizeof(std::uint32_t))) &
                kPacketAddressMask);
    cursor += packet.size();
    return true;
  }
  void DrawText(std::string_view, std::int32_t, std::uint32_t) override {}
};

Output Run(std::int32_t operation) {
  if (operation == kHealingCase) {
    const auto result =
        r::ApplyHealingScoreBonus({.misc_score = Read<std::uint32_t>(),
                                   .heal_count = Read<std::uint16_t>()});
    return {result.misc_score, result.heal_count};
  }
  if (operation == kMusicCase) {
    Music host;
    r::QueueResultsMusic(Read<std::uint32_t>(), host);
    return host.output;
  }
  if (operation == kInitializerCase) {
    auto state = ReadState();
    Initializer host;
    host.ReadInput();
    Output output{static_cast<int>(r::InitializeGameClear(state, host))};
    AppendState(output, state);
    host.Append(output);
    return output;
  }
  if (operation == kScreenCase) {
    auto state = ReadState();
    const auto buttons = Read<std::uint32_t>();
    Screen host;
    host.delta = Read<std::uint32_t>();
    const auto reels = ReadReels();
    Output output{static_cast<int>(r::UpdateGameClear(
        state.screen, reels, (buttons & kConfirmBit) != 0, host))};
    AppendState(output, state);
    host.calls.Append(output);
    return output;
  }
  if (operation == kReelCase) {
    const r::ResultsPoint origin{Read(), Read()};
    const auto timer = Read();
    const auto phase = Read();
    const r::BonusReelView view{Read<std::uint32_t>(), Read<std::uint32_t>(),
                                Read()};
    const auto clip = ReadRect();
    const auto reels = ReadReels();
    const auto atlas = ReadAtlas();
    Reel target;
    r::DrawBonusReel({atlas, reels, clip}, origin, timer, phase, view, target);
    Output output{target.cursor};
    output.insert(output.end(), target.colors.begin(), target.colors.end());
    target.calls.Append(output);
    return output;
  }
  const r::ResultsPoint origin{Read(), Read()};
  const auto index = Read();
  const auto seed = Read<std::uint32_t>();
  Rows target;
  target.ordering = Read<std::uint32_t>();
  for (std::size_t word = 0; word < target.words.size(); ++word) {
    target.words[word] =
        seed + static_cast<std::uint32_t>(word) * kStaleWordStep;
  }
  const auto atlas = ReadAtlas();
  r::DrawBonusReelTexture(atlas, origin, index, target);
  Output output{static_cast<std::int64_t>(target.cursor), target.ordering};
  for (std::size_t word = 0; word < target.cursor; ++word) {
    output.push_back(target.words[word]);
  }
  return output;
}
}  // namespace

// A malformed proof fixture must fail the process, never become a valid vector.
// NOLINTNEXTLINE(bugprone-exception-escape)
int main() {
  std::int32_t operation = 0;
  while (std::cin >> operation) {
    const auto output = Run(operation);
    for (const auto word : output) std::cout << word << ' ';
    std::cout << '\n';
  }
}
