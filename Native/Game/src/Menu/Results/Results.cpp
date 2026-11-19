// The MENUF.PRG results screens' tables and shared statics
// (src/MENU/MENUF.PRG/3B8.c).
#include "Menu/Results/Results.hpp"

#include <concepts>
#include <cstdint>
#include <utility>

#include "Ending/DisplayHost.hpp"
#include "Menu/Results/GameClearLifecycle.hpp"
#include "Menu/Results/GameClearScore.hpp"
#include "Render/Images.hpp"

namespace VS::Menu::Results {
namespace {
using Ending::BlendMode;
using Ending::PaletteBits;
using Ending::TextureDepth;
using Ending::TexturePageBits;

// Every results image is a 4-bit texture in the page row at y 256, blended by
// averaging, with its palette in the bottom VRAM row.
constexpr Render::TextureRegion Glyph(std::uint8_t u, std::uint8_t v,
                                      std::uint8_t width, std::uint8_t height,
                                      int page_x, int palette_x) noexcept {
  constexpr int kPageY = 256;
  constexpr int kPaletteY = 511;
  return {
      u,
      v,
      width,
      height,
      TexturePageBits(TextureDepth::k4Bit, BlendMode::kAverage, page_x, kPageY),
      PaletteBits(palette_x, kPaletteY)};
}

// A rank row stores its word count, then up to three atlas entries; unused
// words stay kCommonZero.
template <std::same_as<DisIndex>... Words>
constexpr RiskbreakerRankRow RankRow(Words... words) noexcept {
  static_assert(sizeof...(words) <= kRiskbreakerRankWords);
  return {static_cast<std::uint8_t>(sizeof...(words)),
          std::to_underlying(words)...};
}

constexpr auto kStrength = std::to_underlying(BonusReelStat::kStrength);
constexpr auto kAgility = std::to_underlying(BonusReelStat::kAgility);
constexpr auto kIntelligence = std::to_underlying(BonusReelStat::kIntelligence);
constexpr auto kHp = std::to_underlying(BonusReelStat::kHp);
constexpr auto kMp = std::to_underlying(BonusReelStat::kMp);
}  // namespace

// decomp: _buffReelIndex (_buffReelIndex@src/MENU/MENUF.PRG/3B8.c)
std::int32_t buffReelIndex;

// decomp: _buffReelSelection (_buffReelSelection@src/MENU/MENUF.PRG/3B8.c)
std::uint32_t buffReelSelection;

// decomp: _buffReels (_buffReels@src/MENU/MENUF.PRG/3B8.c)
const BonusReels kBuffReels = {
    BonusReel{{{kHp, 5},
               {kHp, 3},
               {kStrength, 2},
               {kAgility, 1},
               {kHp, 4},
               {kIntelligence, 1},
               {kMp, 2},
               {kHp, 3},
               {kStrength, 2},
               {kMp, 1},
               {kHp, 4},
               {kHp, 4},
               {kStrength, 3},
               {kHp, 3},
               {kMp, 2},
               {kStrength, 1}}},
    BonusReel{{{kMp, 1},
               {kHp, 4},
               {kIntelligence, 1},
               {kHp, 3},
               {kHp, 5},
               {kHp, 4},
               {kIntelligence, 2},
               {kStrength, 1},
               {kHp, 4},
               {kMp, 2},
               {kAgility, 1},
               {kIntelligence, 3},
               {kHp, 4},
               {kIntelligence, 2},
               {kHp, 3},
               {kHp, 3}}},
    BonusReel{{{kHp, 4},
               {kIntelligence, 1},
               {kAgility, 2},
               {kHp, 4},
               {kMp, 2},
               {kAgility, 1},
               {kMp, 1},
               {kHp, 3},
               {kAgility, 3},
               {kHp, 4},
               {kHp, 3},
               {kHp, 5},
               {kAgility, 2},
               {kMp, 2},
               {kHp, 3},
               {kStrength, 1}}},
};

// decomp: _clearCount (_clearCount@src/MENU/MENUF.PRG/3B8.c)
std::int32_t clearCount;

// decomp: _disMap (_disMap@src/MENU/MENUF.PRG/3B8.c)
const ResultsAtlas kDisMap = {
    Glyph(0, 0, 14, 20, 832, 768),      Glyph(14, 0, 14, 20, 832, 768),
    Glyph(28, 0, 14, 20, 832, 768),     Glyph(42, 0, 14, 20, 832, 768),
    Glyph(56, 0, 14, 20, 832, 768),     Glyph(70, 0, 14, 20, 832, 768),
    Glyph(84, 0, 14, 20, 832, 768),     Glyph(98, 0, 14, 20, 832, 768),
    Glyph(112, 0, 14, 20, 832, 768),    Glyph(126, 0, 14, 20, 832, 768),
    Glyph(240, 16, 4, 8, 832, 768),     Glyph(148, 4, 60, 16, 832, 768),
    Glyph(6, 24, 154, 16, 832, 768),    Glyph(4, 44, 38, 18, 832, 768),
    Glyph(50, 44, 44, 18, 832, 768),    Glyph(98, 44, 38, 18, 832, 768),
    Glyph(144, 44, 30, 18, 832, 768),   Glyph(182, 44, 34, 18, 832, 768),
    Glyph(0, 64, 66, 22, 832, 768),     Glyph(104, 70, 12, 12, 832, 768),
    Glyph(118, 70, 28, 14, 832, 768),   Glyph(0, 88, 170, 22, 832, 768),
    Glyph(8, 118, 136, 24, 832, 768),   Glyph(8, 150, 134, 24, 832, 768),
    Glyph(200, 82, 10, 10, 832, 768),   Glyph(216, 82, 8, 6, 832, 768),
    Glyph(232, 82, 6, 12, 832, 768),    Glyph(30, 12, 66, 24, 896, 768),
    Glyph(98, 12, 70, 24, 896, 768),    Glyph(170, 12, 76, 24, 896, 768),
    Glyph(70, 36, 72, 24, 896, 768),    Glyph(144, 36, 102, 24, 896, 768),
    Glyph(78, 60, 70, 24, 896, 768),    Glyph(152, 60, 94, 24, 896, 768),
    Glyph(38, 84, 80, 24, 896, 768),    Glyph(118, 84, 122, 24, 896, 768),
    Glyph(46, 108, 118, 24, 896, 768),  Glyph(166, 108, 74, 24, 896, 768),
    Glyph(78, 132, 94, 24, 896, 768),   Glyph(172, 132, 74, 24, 896, 768),
    Glyph(82, 156, 86, 24, 896, 768),   Glyph(170, 156, 76, 24, 896, 768),
    Glyph(60, 180, 96, 24, 896, 768),   Glyph(158, 180, 88, 24, 896, 768),
    Glyph(120, 204, 54, 24, 896, 768),  Glyph(178, 204, 68, 24, 896, 768),
    Glyph(56, 228, 114, 24, 896, 768),  Glyph(172, 228, 72, 24, 896, 768),
    Glyph(110, 12, 26, 24, 960, 768),   Glyph(138, 12, 54, 24, 960, 768),
    Glyph(194, 12, 52, 24, 960, 768),   Glyph(64, 36, 88, 24, 960, 768),
    Glyph(182, 36, 64, 24, 960, 768),   Glyph(90, 60, 90, 24, 960, 768),
    Glyph(184, 60, 62, 24, 960, 768),   Glyph(82, 84, 66, 24, 960, 768),
    Glyph(148, 84, 98, 24, 960, 768),   Glyph(42, 108, 102, 24, 960, 768),
    Glyph(144, 108, 102, 24, 960, 768), Glyph(50, 132, 116, 24, 960, 768),
    Glyph(168, 132, 78, 24, 960, 768),  Glyph(74, 156, 92, 24, 960, 768),
    Glyph(166, 156, 80, 24, 960, 768),  Glyph(58, 180, 68, 24, 960, 768),
    Glyph(130, 180, 52, 24, 960, 768),  Glyph(184, 180, 62, 24, 960, 768),
    Glyph(106, 204, 78, 24, 960, 768),  Glyph(188, 204, 58, 24, 960, 768),
    Glyph(106, 228, 140, 24, 960, 768), Glyph(146, 2, 88, 14, 832, 896),
    Glyph(0, 24, 144, 24, 832, 864),    Glyph(0, 48, 144, 24, 832, 864),
    Glyph(176, 78, 56, 18, 832, 800),   Glyph(144, 16, 8, 8, 832, 800),
    Glyph(152, 16, 8, 8, 832, 800),     Glyph(192, 18, 32, 20, 832, 800),
    Glyph(180, 38, 44, 20, 832, 800),   Glyph(184, 58, 40, 20, 832, 800),
    Glyph(2, 72, 116, 20, 832, 832),    Glyph(0, 96, 70, 20, 832, 832),
    Glyph(72, 96, 68, 20, 832, 832),    Glyph(122, 72, 46, 20, 832, 832),
    Glyph(144, 96, 100, 20, 832, 832),  Glyph(238, 4, 12, 16, 832, 768),
    Glyph(166, 20, 76, 18, 832, 832),   Glyph(4, 32, 140, 24, 832, 896),
    Glyph(4, 56, 114, 24, 832, 896),    Glyph(4, 80, 204, 24, 832, 896),
    Glyph(4, 104, 198, 24, 832, 896),   Glyph(4, 128, 232, 24, 832, 896),
    Glyph(4, 152, 204, 24, 832, 896),   Glyph(4, 176, 114, 24, 832, 896),
    Glyph(122, 56, 100, 24, 832, 896),  Glyph(4, 200, 176, 24, 832, 896),
    Glyph(4, 227, 212, 28, 832, 864),   Glyph(146, 180, 100, 18, 832, 800),
    Glyph(158, 14, 86, 106, 896, 768),  Glyph(50, 14, 92, 106, 896, 768),
    Glyph(16, 14, 30, 106, 896, 768),   Glyph(30, 132, 204, 106, 896, 768),
    Glyph(4, 227, 106, 28, 832, 768),   Glyph(110, 227, 106, 28, 832, 768),
    Glyph(4, 120, 111, 30, 832, 768),   Glyph(115, 120, 111, 30, 832, 768),
    Glyph(4, 150, 160, 24, 832, 768),   Glyph(8, 176, 170, 24, 832, 768),
    Glyph(8, 200, 116, 24, 832, 768),   Glyph(8, 8, 170, 20, 896, 896),
    Glyph(8, 28, 134, 20, 896, 896),    Glyph(102, 48, 118, 20, 896, 896),
    Glyph(8, 48, 94, 20, 896, 896),     Glyph(142, 28, 88, 20, 896, 896),
    Glyph(8, 68, 142, 20, 896, 896),    Glyph(8, 88, 144, 20, 896, 896),
    Glyph(8, 108, 128, 20, 896, 896),   Glyph(8, 128, 98, 20, 896, 896),
    Glyph(136, 108, 104, 20, 896, 896), Glyph(8, 148, 156, 20, 896, 896),
    Glyph(8, 168, 174, 20, 896, 896),   Glyph(106, 128, 88, 20, 896, 896),
    Glyph(8, 188, 154, 20, 896, 896),   Glyph(8, 208, 156, 20, 896, 896),
    Glyph(8, 228, 194, 20, 896, 896),   Glyph(0, 0, 160, 24, 832, 864),
    Glyph(0, 24, 160, 24, 832, 864),    Glyph(0, 48, 160, 24, 832, 832),
    Glyph(0, 72, 160, 24, 832, 832),
};

// decomp: _elementAnimationState
// (_elementAnimationState@src/MENU/MENUF.PRG/3B8.c)
std::uint32_t elementAnimationState;

// decomp: _incrementingMapCompletion
// (_incrementingMapCompletion@src/MENU/MENUF.PRG/3B8.c)
std::int32_t incrementingMapCompletion;

// decomp: _incrementingScore (_incrementingScore@src/MENU/MENUF.PRG/3B8.c)
std::int32_t incrementingScore;

// decomp: _mapCompletion (_mapCompletion@src/MENU/MENUF.PRG/3B8.c)
std::uint32_t mapCompletion;

// decomp: _onScreenMapCompletion
// (_onScreenMapCompletion@src/MENU/MENUF.PRG/3B8.c)
std::int32_t onScreenMapCompletion;

// decomp: _onScreenScore (_onScreenScore@src/MENU/MENUF.PRG/3B8.c)
std::int32_t onScreenScore;

// decomp: _rank (_rank@src/MENU/MENUF.PRG/3B8.c)
std::int32_t rank;

// decomp: _riskbreakerRanks (_riskbreakerRanks@src/MENU/MENUF.PRG/3B8.c)
const RankTitles kRiskbreakerRanks = {
    RankRow(DisIndex::kRank1Grand, DisIndex::kRank1Master,
            DisIndex::kRank1Breaker),
    RankRow(DisIndex::kRank1Grand, DisIndex::kRank1Paladin),
    RankRow(DisIndex::kRank1Radiant, DisIndex::kRank1Knight),
    RankRow(DisIndex::kRank1Raging, DisIndex::kRank1Berserker),
    RankRow(DisIndex::kRank2Dragon, DisIndex::kRank2Slayer),
    RankRow(DisIndex::kRank1Couragous, DisIndex::kRank1Adventurer),
    RankRow(DisIndex::kRank1Master, DisIndex::kRank1Gladiator),
    RankRow(DisIndex::kRank1Blade, DisIndex::kRank1Master),
    RankRow(DisIndex::kRank2Mystic, DisIndex::kRank2Wanderer),
    RankRow(DisIndex::kRank1Paladin),
    RankRow(DisIndex::kRank2Spectrebane),
    RankRow(DisIndex::kRank2Destroyer),
    RankRow(DisIndex::kRank1Berserker),
    RankRow(DisIndex::kRank1DareDevil),
    RankRow(DisIndex::kRank1Gladiator),
    RankRow(DisIndex::kRank2Normal, DisIndex::kRank2Agent),
};

// decomp: _score (_score@src/MENU/MENUF.PRG/3B8.c)
std::uint32_t score;
}  // namespace VS::Menu::Results
