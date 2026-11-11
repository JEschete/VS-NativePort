// Native side of tools\port\differentials\area_route.py (F01721-F01728): one vector per
// stdin line, one line of results per vector. The world is the vector's room, maps and
// platforms; the tile and exit words are composed as AreaRouteLive.cpp composes them.
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "area_route_world.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace differential;

constexpr int kRefused = -2;

std::string AreaRoute(std::istringstream& in)
{
    VectorWorld world;
    world.ends = ReadEnds(in);
    world.contacts = ReadContacts(in);
    const int area = Read<int>(in);
    const int index = Read<int>(in);
    world.ReadMaps(in);
    auto actor = ReadActor(in);
    const auto intent = ResolveNavigationAreaRoute(world, actor, index, area);
    std::ostringstream out;
    out << (intent ? static_cast<int>(*intent) : kRefused);
    WriteActor(out, actor);
    world.WriteCalls(out);
    return out.str();
}
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        const int kind = Read<int>(in);
        std::ostringstream out;
        if (kind == 0) {
            // ResolveLiveRouteGateExit: the exit's three bytes, or the whole entry word.
            const auto ends = ReadEnds(in);
            const auto entry = Read<std::uint32_t>(in);
            auto exit = Read<std::uint32_t>(in);
            if (const auto resolved = ResolveRouteGateExit(ends, entry)) {
                exit = (exit & 0xFF000000u) | (std::uint32_t { resolved->z } << 16)
                     | (std::uint32_t { resolved->platform } << 8) | resolved->x;
            } else {
                exit = entry;
            }
            out << exit;
        } else if (kind == 1) {
            // FindLiveNearestRouteGateEntry: bytes 0 and 2 only.
            const auto ends = ReadEnds(in);
            const int platform = Read<int>(in);
            const auto world_x = Read<std::int32_t>(in);
            const auto world_z = Read<std::int32_t>(in);
            auto tile = Read<std::uint32_t>(in);
            const auto end = FindNearestRouteGateEntry(ends, platform, world_x, world_z);
            if (!end)
                std::abort();
            tile = (tile & 0xFF00FF00u) | (std::uint32_t { end->z } << 16) | end->x;
            out << tile;
        } else if (kind == 2) {
            const auto ends = ReadEnds(in);
            const auto contacts = ReadContacts(in);
            const auto tile = Read<std::uint32_t>(in);
            const auto match = Read<std::uint32_t>(in);
            const auto link = FindRouteGateAtTile(ends, contacts, tile, match != 0);
            out << (link ? *link : kRefused) << ' ' << 1;
        } else {
            out << AreaRoute(in);
        }
        std::cout << out.str() << '\n';
    }
    return 0;
}
