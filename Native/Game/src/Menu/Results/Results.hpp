#pragma once
// The MENUF.PRG results screens' tables and shared statics
// (src/MENU/MENUF.PRG/3B8.c): the bonus-stat reels, the results atlas, the
// Riskbreaker rank names and the game-clear score state.
#include <array>
#include <cstdint>

#include "Menu/Results/GameClearLifecycle.hpp"
#include "Menu/Results/GameClearScore.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Menu/Score/ScoreRank.hpp"
#include "Render/Images.hpp"

namespace VS::Menu::Results {
// decomp: disIndices (src/MENU/MENUF.PRG/3B8.c:505): named entries of the
// results atlas (kDisMap). Entries 0-9 are the digits.
enum class DisIndex : std::uint8_t {
  kCommonZero,
  kRank0Comma = 10,
  kRank0Bonus,
  kRank0RiskbreakerRank,
  kRank0Str,
  kRank0Agl,
  kRank0Int,
  kRank0Hp,
  kRank0Mp,
  kRank0Score,
  kRank0Percent,
  kRank0Pts,
  kRank0MapCompleted,
  kRank0Congratulations,
  kRank0Plus = 24,
  kRank0Minus,
  kRank0Colon,
  kRank1Grand,
  kRank1Master,
  kRank1Breaker,
  kRank1Paladin,
  kRank1Gladiator,
  kRank1Raging,
  kRank1Berserker,
  kRank1Radiant,
  kRank1Couragous,
  kRank1Adventurer,
  kRank1Knight,
  kRank1Archer = 39,
  kRank1DareDevil = 42,
  kRank1Blade = 44,
  kRank2Of = 48,
  kRank2Mystic = 55,
  kRank2Wanderer,
  kRank2Destroyer,
  kRank2Gladiator,
  kRank2Spectrebane,
  kRank2Normal,
  kRank2Phantom,
  kRank2Fighter,
  kRank2Demon,
  kRank2Eater,
  kRank2Agent,
  kRank2Dragon,
  kRank2Slayer,
  kRank2Cartographer,
  kTime0NewRecord = 69,
  kTime0TimeAttackResults,
  kTime0Time = 72,
  kTime0Prime,
  kTime0DoublePrime,
  kTime0First,
  kTime0Excellent = 78,
  kTime0Dash = 83,
  kAttack0Course,
  kAttack0Minotaur,
  kAttack0Death = 91,
  kAttack0OgreZombie = 93,
  kAttack0BestTime = 95,
  kAttack0Three,
  kAttack0TimeAttack = 100,
  kIq0EvolveOrDie = 102,
  kIq0AverageTime = 104,
  kIq0Foodchain = 105,
  kIq1GameDesigner = 107,
  kEsc0GivingUp = 123,
  kEsc0Chicken = 125,
};

using BonusReels = std::array<BonusReel, kBonusReelCount>;
using ResultsAtlas = std::array<Render::TextureRegion, kResultsAtlasEntries>;
using RankTitles = std::array<RiskbreakerRankRow, Score::kRiskbreakerRankCount>;

// decomp: _buffReelIndex (_buffReelIndex@src/MENU/MENUF.PRG/3B8.c): the reel
// the bonus roulette spins.
extern std::int32_t buffReelIndex;
// decomp: _buffReelSelection (_buffReelSelection@src/MENU/MENUF.PRG/3B8.c):
// the reel position the roulette has reached.
extern std::uint32_t buffReelSelection;
// decomp: _buffReels (_buffReels@src/MENU/MENUF.PRG/3B8.c): the three
// bonus-stat reels of sixteen awards each. The original only reads them.
extern const BonusReels kBuffReels;
// decomp: _clearCount (_clearCount@src/MENU/MENUF.PRG/3B8.c): the game-clear
// count the score calculation uses.
extern std::int32_t clearCount;
// decomp: _disMap (_disMap@src/MENU/MENUF.PRG/3B8.c): where each results
// image sits in the loaded DIS textures, indexed by DisIndex.
extern const ResultsAtlas kDisMap;
// decomp: _elementAnimationState
// (_elementAnimationState@src/MENU/MENUF.PRG/3B8.c): the results screens'
// animation state; the game-clear roulette keeps its reel speed here.
extern std::uint32_t elementAnimationState;
// decomp: _incrementingMapCompletion
// (_incrementingMapCompletion@src/MENU/MENUF.PRG/3B8.c): whether the map
// completion counting sound is playing.
extern std::int32_t incrementingMapCompletion;
// decomp: _incrementingScore (_incrementingScore@src/MENU/MENUF.PRG/3B8.c):
// whether the score counting sound is playing.
extern std::int32_t incrementingScore;
// decomp: _mapCompletion (_mapCompletion@src/MENU/MENUF.PRG/3B8.c): the final
// map completion percentage.
extern std::uint32_t mapCompletion;
// decomp: _onScreenMapCompletion
// (_onScreenMapCompletion@src/MENU/MENUF.PRG/3B8.c): the counting map
// completion on screen.
extern std::int32_t onScreenMapCompletion;
// decomp: _onScreenScore (_onScreenScore@src/MENU/MENUF.PRG/3B8.c): the
// counting score on screen.
extern std::int32_t onScreenScore;
// decomp: _rank (_rank@src/MENU/MENUF.PRG/3B8.c): the Riskbreaker rank row.
extern std::int32_t rank;
// decomp: _riskbreakerRanks (_riskbreakerRanks@src/MENU/MENUF.PRG/3B8.c): each
// rank's word count and atlas entries.
extern const RankTitles kRiskbreakerRanks;
// decomp: _score (_score@src/MENU/MENUF.PRG/3B8.c): the final score.
extern std::uint32_t score;
}  // namespace VS::Menu::Results
