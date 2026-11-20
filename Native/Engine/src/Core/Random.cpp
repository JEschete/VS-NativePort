// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Core/RandomLive.hpp"

// decomp: _keystreamState (_keystreamState@BATTLE/BATTLE.PRG)
// contents: module image (ModuleData.cpp)
VS::u32 VS::Core::interfaceRandomState;

// decomp: randArr (randArr@SLUS_010.40)
VS::i32 VS::Core::randomShuffleTable[VS::Core::Random::TableSize];

// decomp: randIndex (randIndex@SLUS_010.40)
VS::i32 VS::Core::randomShuffleIndex;
