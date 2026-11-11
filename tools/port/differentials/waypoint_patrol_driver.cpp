// Native side of tools\port\differentials\waypoint_patrol.py (F01747): one vector per
// stdin line, one line of results per vector. The random stub masks a fixed raw draw the
// way the original's actor random stream masks its next value.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "Battle/Ai/Waypoints.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

constexpr std::size_t kRows = 4;

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}

class FixedRandom final : public PatrolRandom {
public:
    explicit FixedRandom(std::uint32_t raw)
        : raw_(raw)
    {
    }
    int NextMasked(int range) override
    {
        ++draws;
        last_range = range;
        return static_cast<int>(raw_ & (static_cast<std::uint32_t>(range) - 1U));
    }
    int draws = 0;
    int last_range = 0;

private:
    std::uint32_t raw_;
};
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        PatrolCursor cursor { .index = Read<std::uint8_t>(in),
            .row = Read<std::uint8_t>(in),
            .rate = Read<std::uint8_t>(in),
            .direction = Read<std::uint8_t>(in),
            .cooldown = Read<std::uint8_t>(in) };
        const WaypointTile tile { .x = Read<std::uint8_t>(in),
            .z = Read<std::uint8_t>(in) };
        FixedRandom random(Read<std::uint32_t>(in));
        std::array<std::uint16_t, kRows * kPatrolWaypointsPerRow> routes { };
        for (auto& value : routes)
            value = Read<std::uint16_t>(in);
        const auto target = UpdateWaypointPatrol(cursor, routes, tile, random);
        std::cout << (target ? 0 : 1) << ' ' << int { cursor.index } << ' '
                  << int { cursor.row } << ' ' << int { cursor.rate } << ' '
                  << int { cursor.direction } << ' ' << int { cursor.cooldown } << ' '
                  << (target ? int { target->x } : 0) << ' '
                  << (target ? int { target->z } : 0) << ' ' << random.draws << ' '
                  << random.last_range << '\n';
    }
    return 0;
}
