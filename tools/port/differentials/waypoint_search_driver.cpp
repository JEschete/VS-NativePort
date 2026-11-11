// Native side of tools\port\differentials\waypoint_search.py (F01710): one vector per
// stdin line, one line of results per vector. The tile word is written as
// FindLiveNearestActiveWaypoint writes it: bytes 0 and 2 only, and only when a waypoint
// is found.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <sstream>
#include <string>

#include "Battle/Ai/Waypoints.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::array<std::uint16_t, kRoomWaypointCount> waypoints { };
        for (auto& value : waypoints)
            value = Read<std::uint16_t>(in);
        const VS::Core::Math::Position3 position { .x = Read<std::int16_t>(in),
            .y = Read<std::int16_t>(in),
            .z = Read<std::int16_t>(in) };
        std::array<VS::Field::TerrainRow, VS::Field::kTerrainGridSide> rows { };
        for (auto& row : rows) {
            for (auto& cell : row)
                cell = Read<std::uint16_t>(in);
        }
        auto tile = Read<std::uint32_t>(in);
        const auto search = FindNearestActiveWaypoint(
            waypoints, position, VS::Field::TerrainGrid { rows });
        if (!search)
            std::abort();
        std::ostringstream out;
        if (search->nearest) {
            const auto& nearest = *search->nearest;
            tile = (tile & 0xFF00FF00u) | (std::uint32_t { nearest.tile.z } << 16)
                 | nearest.tile.x;
            out << 1 << ' ' << int { nearest.tile.x } << ' ' << int { nearest.tile.z }
                << ' ' << nearest.squared_distance;
        } else {
            out << "0 0 0 0";
        }
        out << ' ' << tile;
        std::cout << out.str() << '\n';
    }
    return 0;
}
