// Native side of tools\port\differentials\special_terrain.py (F01814-F01816): one vector per
// stdin line, one line of results per vector. argv[1] is BATTLE.PRG, read for the direction codes.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Battle/Ai/SpecialTerrain.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) {}

namespace {
constexpr std::uint32_t kBase = 0x80068800u;
constexpr std::uint32_t kDirectionCodes = 0x800F16ECu;
std::vector<unsigned char> image;

// The same mix as floor_height() in the case: the stubbed floor callee's value at a world point.
std::uint32_t FloorHeight(std::uint32_t seed, std::uint32_t mode, int x, int z) {
  std::uint32_t value = seed ^ (static_cast<std::uint32_t>(x) * 0x9E3779B1u) ^
                        (static_cast<std::uint32_t>(z) * 0x85EBCA77u);
  value ^= value >> 16;
  value *= 0x7FEB352Du;
  value ^= value >> 15;
  value *= 0x846CA68Bu;
  value ^= value >> 16;
  if (mode == 1) return value & 0x7FFFu;  // never negative
  if (mode == 2) return static_cast<std::uint32_t>(static_cast<std::int16_t>(value));
  return value;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::ifstream file(argv[1], std::ios::binary);
  image.assign(std::istreambuf_iterator<char>(file), {});
  VS::Battle::Ai::NavigationDirectionCodes directions{};
  for (std::size_t i = 0; i < directions.size(); ++i) directions[i] = image.at(kDirectionCodes - kBase + i);
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    int kind = 0, x = 0, z = 0, height = 0;
    std::uint32_t seed = 0, mode = 0;
    in >> kind >> x >> z >> height >> seed >> mode;
    std::array<std::uint16_t, VS::Battle::Ai::kSpecialTerrainCells> flags{}, heights{};
    for (auto* map : {&flags, &heights}) {
      for (auto& cell : *map) {
        unsigned value = 0;
        in >> value;
        cell = static_cast<std::uint16_t>(value);
      }
    }
    if (!in) std::abort();
    std::vector<std::pair<int, int>> queries;
    const VS::Battle::Ai::FloorHeightQuery floor = [&](int world_x, int world_z) {
      queries.emplace_back(world_x, world_z);
      return static_cast<int>(FloorHeight(seed, mode, world_x, world_z));
    };
    const VS::Battle::Ai::SpecialTerrainMaps maps{flags, heights};
    int result = 0;
    if (kind == 0) {
      VS::Battle::Ai::BuildAiSpecialTerrainHeights(maps, floor, directions);
    } else if (kind == 1) {
      result = VS::Battle::Ai::FloodFillAiTerrainHeights(maps, directions, x, z, height) ? 1 : 0;
    } else {
      result = VS::Battle::Ai::FindAdjacentAiGroundHeight(flags, floor, directions, x, z).value_or(-1);
    }
    std::ostringstream out;
    out << result << ' ' << queries.size();
    for (const auto& [world_x, world_z] : queries) out << ' ' << world_x << ' ' << world_z;
    for (const auto cell : heights) out << ' ' << cell;
    std::cout << out.str() << '\n';
  }
}
