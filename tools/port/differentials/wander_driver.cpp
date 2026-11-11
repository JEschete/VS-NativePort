// Native side of tools\port\differentials\wander.py (F01741-F01745): one vector per stdin
// line, one line of results per vector. The world holds the vector's direction codes, regions,
// tile flags and room bounds; the random stream and the checks pop the vector's queues, and
// every call is recorded with its arguments and, for the calls taking the actor, a snapshot of
// the fields the wander writes.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Battle/Ai/Wander.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

constexpr int kGrid = 32;
enum Kind { kTile, kMove };
enum Code { kRandom, kAdjacent, kTraverse, kFly, kWalk, kProject, kDispatch, kRecover };
constexpr std::size_t kRecord = 16;
using Record = std::array<long long, kRecord>;

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}

std::deque<std::uint32_t> ReadQueue(std::istringstream& in)
{
    std::deque<std::uint32_t> queue(Read<std::size_t>(in));
    for (auto& value : queue)
        value = Read<std::uint32_t>(in);
    return queue;
}

// WanderActor in declaration order, less flies.
std::array<long long, 21> Fields(const WanderActor& a)
{
    return { a.is_stuck, a.height_filter, a.stayed_on_tile, a.tile, a.saved_destination, a.x, a.z,
        a.move_distance, a.phase, a.timer, a.range_squared, a.heading, a.cooldown, a.rest_period,
        a.move_period, a.rate, a.goal, a.behavior_mode, a.count, a.movement_flags, a.destination };
}

WanderActor ReadActor(std::istringstream& in)
{
    WanderActor a;
    a.is_stuck = Read<std::uint8_t>(in);
    a.height_filter = Read<std::uint8_t>(in);
    a.stayed_on_tile = Read<std::uint8_t>(in);
    a.tile = Read<std::uint32_t>(in);
    a.saved_destination = Read<std::uint32_t>(in);
    a.x = Read<std::int16_t>(in);
    a.z = Read<std::int16_t>(in);
    a.move_distance = Read<std::uint32_t>(in);
    a.phase = Read<std::uint16_t>(in);
    a.timer = Read<std::uint16_t>(in);
    a.range_squared = Read<std::int32_t>(in);
    a.heading = Read<std::int16_t>(in);
    a.cooldown = Read<std::uint16_t>(in);
    a.rest_period = Read<std::uint16_t>(in);
    a.move_period = Read<std::uint16_t>(in);
    a.rate = Read<std::uint8_t>(in);
    a.goal = Read<std::uint32_t>(in);
    a.behavior_mode = Read<std::uint8_t>(in);
    a.count = Read<std::uint8_t>(in);
    a.movement_flags = Read<std::uint16_t>(in);
    a.destination = Read<std::uint32_t>(in);
    a.flies = Read<int>(in) != 0;
    return a;
}

class VectorWorld final : public WanderWorld {
public:
    NavigationDirectionCodes codes { };
    bool room_data = false;
    int width = 0;
    std::vector<std::uint8_t> regions;
    std::array<std::uint16_t, kGrid * kGrid> flags { };
    WanderRoomBounds bounds { };
    std::uint8_t cache = 0;
    std::deque<std::uint32_t> draws, adjacent, traverse, checks;
    std::vector<Record> records;

    void Read(std::istringstream& in)
    {
        for (auto& code : codes)
            code = ::Read<std::uint8_t>(in);
        room_data = ::Read<int>(in) != 0;
        width = ::Read<int>(in);
        regions.resize(static_cast<std::size_t>(width * kGrid));
        for (auto& cell : regions)
            cell = ::Read<std::uint8_t>(in);
        for (auto& cell : flags)
            cell = ::Read<std::uint16_t>(in);
        bounds.min_x = ::Read<std::uint8_t>(in);
        bounds.min_z = ::Read<std::uint8_t>(in);
        bounds.max_x = ::Read<std::uint8_t>(in);
        bounds.max_z = ::Read<std::uint8_t>(in);
        cache = ::Read<std::uint8_t>(in);
    }

    void ReadQueues(std::istringstream& in)
    {
        draws = ReadQueue(in);
        adjacent = ReadQueue(in);
        traverse = ReadQueue(in);
        checks = ReadQueue(in);
    }

    [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override { return codes; }
    [[nodiscard]] bool HasRoomData() const override { return room_data; }
    [[nodiscard]] std::optional<std::uint8_t> TerrainRegionCell(int x, int z) const override
    {
        const auto cell = static_cast<std::size_t>((z * width) + x);
        if (x < 0 || z < 0 || x >= width || cell >= regions.size())
            return std::nullopt;
        return regions[cell];
    }
    [[nodiscard]] std::optional<std::uint16_t> TileFlags(int x, int z) const override
    {
        if (x < 0 || z < 0 || x >= kGrid || z >= kGrid)
            return std::nullopt;
        return flags[static_cast<std::size_t>((z * kGrid) + x)];
    }
    [[nodiscard]] std::optional<WanderRoomBounds> RoomBounds() const override { return bounds; }
    int NextRandomMasked(int range) override
    {
        Note(kRandom, range, 0, 0, nullptr);
        return static_cast<int>(Pop(draws) & (static_cast<std::uint32_t>(range) - 1));
    }
    [[nodiscard]] bool IsAdjacentStepBlocked(std::uint32_t tile, int direction) override
    {
        Note(kAdjacent, tile, direction, 0, nullptr);
        return Pop(adjacent) != 0;
    }
    [[nodiscard]] bool CanTraverseEdge(
        WanderActor& actor, std::uint32_t from, std::uint32_t to, int direction) override
    {
        Note(kTraverse, from, to, direction, &actor);
        return Pop(traverse) != 0;
    }
    [[nodiscard]] bool CanFlyInDirection(WanderActor& actor, int direction) override
    {
        Note(kFly, direction, 0, 0, &actor);
        return Pop(checks) != 0;
    }
    [[nodiscard]] bool CanWalkInDirection(WanderActor& actor, int direction) override
    {
        Note(kWalk, direction, 0, 0, &actor);
        return Pop(checks) != 0;
    }
    void ClearDirectionCache() override { cache = 0; }
    void ProjectClearRadialDestination(
        WanderActor& actor, std::uint32_t source, int angle, std::uint32_t distance) override
    {
        Note(kProject, source, angle, distance, &actor);
    }
    void DispatchLocomotion(WanderActor& actor, int kind, int angle) override
    {
        Note(kDispatch, kind, angle, 0, &actor);
    }
    void RecoverMovement(WanderActor& actor) override { Note(kRecover, 0, 0, 0, &actor); }

private:
    static std::uint32_t Pop(std::deque<std::uint32_t>& queue)
    {
        if (queue.empty())
            return 0;
        const auto value = queue.front();
        queue.pop_front();
        return value;
    }

    void Note(int code, long long a, long long b, long long c, const WanderActor* actor)
    {
        Record record { code, a, b, c };
        if (actor != nullptr) {
            const auto& s = *actor;
            const std::array<long long, 12> snapshot { s.is_stuck, s.height_filter, s.saved_destination,
                s.phase, s.timer, s.range_squared, s.heading, s.cooldown, s.goal, s.behavior_mode,
                s.count, s.destination };
            for (std::size_t i = 0; i < snapshot.size(); ++i)
                record[4 + i] = snapshot[i];
        }
        records.push_back(record);
    }
};
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        const auto kind = Read<int>(in);
        VectorWorld world;
        world.Read(in);
        auto actor = ReadActor(in);
        const auto tile = Read<std::uint32_t>(in);
        const auto range = Read<std::uint32_t>(in);
        world.ReadQueues(in);
        bool refused = false;
        int result = 0;
        if (kind == kTile) {
            const auto choice = ChooseNextWanderTile(world, actor, tile, range);
            refused = !choice;
            result = choice ? static_cast<int>(*choice) : 0;
        } else {
            refused = !UpdateWanderMovement(world, actor, tile, range);
        }
        std::ostringstream out;
        out << (refused ? 1 : 0) << ' ' << result;
        for (const auto value : Fields(actor))
            out << ' ' << value;
        out << ' ' << int { world.cache } << ' ' << world.records.size();
        for (const auto& record : world.records)
            for (const auto value : record)
                out << ' ' << value;
        std::cout << out.str() << '\n';
    }
    return 0;
}
