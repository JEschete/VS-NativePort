// Native side of tools\port\differentials\flight_movement.py (F01729, F01730, F01732-F01734): one
// vector per stdin line, one line of results per vector. The world holds the vector's direction
// codes and cache, room, terrain, tile flags, platforms, actors and target records; the vertical
// step, the movement resolution and occupancy pop the vector's queues, and every call is recorded
// with its arguments and a snapshot of the fields the routines write.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Battle/Ai/FlightMovement.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Field/TerrainHeight.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

constexpr int kGrid = 32;
constexpr std::size_t kSlots = 16;
constexpr std::size_t kTargets = 17;
enum Kind { kObstruction, kCanFly, kTransition, kMove, kDirectional };
enum Code {
    kPlatforms, kOccupancy, kCommand, kToward, kGround, kRelease, kBegin, kAlternate, kStep, kResolve,
    kRecover
};
constexpr std::size_t kRecord = 9;
using Record = std::array<long long, kRecord>;

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}

// FlightMover in declaration order, less the model's flag.
std::array<long long, 21> Fields(const FlightMover& m)
{
    return { m.close_approach, m.movement_disabled, m.tile, m.previous_tile, m.x, m.y, m.z, m.actor_id,
        m.radius, m.max_range, m.body_height, m.ground_clearance, m.flight_height, m.approach_target,
        m.movement_flags, m.destination, m.move_x, m.move_z, m.move_y, m.heading, m.stall_counter };
}

FlightMover ReadMover(std::istringstream& in)
{
    FlightMover m;
    m.close_approach = Read<std::uint8_t>(in);
    m.movement_disabled = Read<std::uint8_t>(in);
    m.tile = Read<std::uint32_t>(in);
    m.previous_tile = Read<std::uint32_t>(in);
    m.x = Read<std::int16_t>(in);
    m.y = Read<std::int16_t>(in);
    m.z = Read<std::int16_t>(in);
    m.actor_id = Read<std::uint8_t>(in);
    m.radius = Read<std::uint16_t>(in);
    m.max_range = Read<std::uint8_t>(in);
    m.body_height = Read<std::int16_t>(in);
    m.ground_clearance = Read<std::int16_t>(in);
    m.flight_height = Read<std::int16_t>(in);
    m.approach_target = Read<std::uint8_t>(in);
    m.movement_flags = Read<std::uint16_t>(in);
    m.destination = Read<std::uint32_t>(in);
    m.move_x = Read<std::int32_t>(in);
    m.move_z = Read<std::int32_t>(in);
    m.move_y = Read<std::int32_t>(in);
    m.heading = Read<std::int16_t>(in);
    m.stall_counter = Read<std::int32_t>(in);
    m.flies = Read<int>(in) != 0;
    return m;
}

class VectorWorld final : public FlightMovementWorld {
public:
    const FlightMover* mover = nullptr;
    NavigationDirectionCodes codes { };
    FlightDirectionCache cache { };
    FlightRoom room { };
    VS::Field::TerrainRow heights[kGrid] { };
    std::uint16_t flags[kGrid][kGrid] { };
    std::vector<VS::Field::PlatformContact> platforms;
    std::vector<int> actors;
    std::size_t next = 0;
    std::array<FlightObstacle, kSlots> obstacles { };
    std::array<FlightTargetRecord, kTargets> targets { };
    std::deque<std::optional<FlightStep>> steps;
    std::deque<std::pair<int, std::int16_t>> resolutions;
    std::deque<int> occupancy;
    mutable std::vector<Record> records;

    void Read(std::istringstream& in)
    {
        for (auto& code : codes)
            code = ::Read<std::uint8_t>(in);
        room.bounds.min_x = ::Read<std::uint8_t>(in);
        room.bounds.min_z = ::Read<std::uint8_t>(in);
        room.bounds.max_x = ::Read<std::uint8_t>(in);
        room.bounds.max_z = ::Read<std::uint8_t>(in);
        room.ceiling = ::Read<std::int32_t>(in);
        for (auto& row : heights)
            for (auto& cell : row)
                cell = ::Read<std::uint16_t>(in);
        for (auto& row : flags)
            for (auto& cell : row)
                cell = ::Read<std::uint16_t>(in);
        cache.fly_tested = ::Read<std::uint8_t>(in);
        cache.fly_open = ::Read<std::uint8_t>(in);
        cache.obstruction_tested = ::Read<std::uint8_t>(in);
        for (auto& answer : cache.obstruction)
            answer = ::Read<std::uint8_t>(in);
    }

    void ReadActors(std::istringstream& in)
    {
        platforms.resize(::Read<std::size_t>(in));
        for (auto& platform : platforms) {
            platform.x = ::Read<std::int16_t>(in);
            platform.height = ::Read<std::int16_t>(in);
            platform.z = ::Read<std::int16_t>(in);
            platform.platform_index = ::Read<std::int16_t>(in);
        }
        actors.resize(::Read<std::size_t>(in));
        for (auto& actor : actors)
            actor = ::Read<int>(in);
        for (auto& obstacle : obstacles) {
            obstacle.actor_id = ::Read<std::uint8_t>(in);
            obstacle.radius = ::Read<std::uint16_t>(in);
            obstacle.max_range = ::Read<std::uint8_t>(in);
            obstacle.body_height = ::Read<std::int16_t>(in);
            FlightPoint point;
            point.x = ::Read<std::int16_t>(in);
            point.y = ::Read<std::int16_t>(in);
            point.z = ::Read<std::int16_t>(in);
            obstacle.position = point;
        }
        for (auto& target : targets) {
            target.y = ::Read<std::int16_t>(in);
            target.tile_x = ::Read<std::uint8_t>(in);
            target.tile_z = ::Read<std::uint8_t>(in);
            target.collision_height = ::Read<std::uint16_t>(in);
        }
    }

    void ReadQueues(std::istringstream& in)
    {
        steps.resize(::Read<std::size_t>(in));
        for (auto& step : steps) {
            const bool present = ::Read<int>(in) != 0;
            FlightStep value;
            value.step = ::Read<std::int32_t>(in);
            value.intent = ::Read<int>(in);
            step = present ? std::optional<FlightStep>(value) : std::nullopt;
        }
        resolutions.resize(::Read<std::size_t>(in));
        for (auto& [result, heading] : resolutions) {
            result = ::Read<int>(in);
            heading = ::Read<std::int16_t>(in);
        }
        occupancy.resize(::Read<std::size_t>(in));
        for (auto& answer : occupancy)
            answer = ::Read<int>(in);
    }

    [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override { return codes; }
    [[nodiscard]] FlightDirectionCache DirectionCache() const override { return cache; }
    void SetDirectionCache(const FlightDirectionCache& value) override { cache = value; }
    [[nodiscard]] std::vector<VS::Field::PlatformContact> PlatformPositions() const override
    {
        Note(kPlatforms, 0, 0);
        return platforms;
    }
    void RewindActors() override { next = 0; }
    [[nodiscard]] std::optional<int> NextActor() override
    {
        if (next >= actors.size())
            return std::nullopt;
        return actors[next++];
    }
    [[nodiscard]] std::optional<FlightObstacle> Obstacle(int actor_id) const override
    {
        if (actor_id < 0 || static_cast<std::size_t>(actor_id) >= kSlots)
            return std::nullopt;
        return obstacles[static_cast<std::size_t>(actor_id)];
    }
    [[nodiscard]] std::optional<FlightRoom> Room() const override { return room; }
    [[nodiscard]] std::optional<std::uint16_t> TileFlags(int x, int z) const override
    {
        if (x < 0 || z < 0 || x >= kGrid || z >= kGrid)
            return std::nullopt;
        return flags[z][x];
    }
    [[nodiscard]] VS::Field::TerrainGrid Terrain() const override
    {
        return VS::Field::TerrainGrid(std::span<const VS::Field::TerrainRow, kGrid>(heights));
    }
    [[nodiscard]] std::optional<FlightTargetRecord> TargetRecord(int actor_id) const override
    {
        if (actor_id < 0 || static_cast<std::size_t>(actor_id) >= kTargets)
            return std::nullopt;
        return targets[static_cast<std::size_t>(actor_id)];
    }
    [[nodiscard]] std::optional<FlightStep> SelectVerticalStep(FlightMover&) override
    {
        Note(kStep, 0, 0);
        if (steps.empty())
            return std::nullopt;
        const auto step = steps.front();
        steps.pop_front();
        return step;
    }
    [[nodiscard]] int ResolveDirectionalMovement(FlightMover& m, int angle, int mode) override
    {
        Note(kResolve, angle, mode);
        if (resolutions.empty())
            return 0;
        const auto [result, heading] = resolutions.front();
        resolutions.pop_front();
        m.heading = heading;
        return result;
    }
    [[nodiscard]] int DestinationOccupancy(FlightMover&, std::uint32_t tile) override
    {
        Note(kOccupancy, tile, 0);
        if (occupancy.empty())
            return 0;
        const auto answer = occupancy.front();
        occupancy.pop_front();
        return answer;
    }
    void IssueCommand(FlightMover&, int intent) override { Note(kCommand, intent, 0); }
    void Recover(FlightMover&) override { Note(kRecover, 0, 0); }
    void ReleaseReservations(FlightMover&) override { Note(kRelease, 0, 0); }
    void BeginTerrainMovement(FlightMover&) override { Note(kBegin, 0, 0); }
    void BeginAlternateTerrainMovement(FlightMover&) override { Note(kAlternate, 0, 0); }
    void MoveTowardDestinationTile(FlightMover&, int angle) override { Note(kToward, angle, 0); }
    void BeginGroundMovement(FlightMover&, int angle) override { Note(kGround, angle, 0); }

private:
    void Note(int code, long long argument, long long extra) const
    {
        const auto& m = *mover;
        records.push_back({ code, argument, extra, m.movement_disabled, m.move_x, m.move_z, m.move_y,
            m.heading, m.stall_counter });
    }
};
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        const auto kind = Read<int>(in);
        const auto argument = Read<int>(in);
        const auto flag = Read<int>(in) != 0;
        VectorWorld world;
        world.Read(in);
        auto mover = ReadMover(in);
        world.mover = &mover;
        world.ReadActors(in);
        world.ReadQueues(in);
        bool done = true;
        long long result = 0;
        switch (kind) {
        case kObstruction: {
            const auto answer = FlightDirectionObstruction(world, mover, argument, flag);
            done = answer.has_value();
            result = answer.value_or(0);
            break;
        }
        case kCanFly: {
            const auto open = CanFlyInDirection(world, mover, argument);
            done = open.has_value();
            result = open.value_or(false) ? 1 : 0;
            break;
        }
        case kTransition: {
            const auto changed = UpdateFlightGroundTransition(world, mover, argument, flag);
            done = changed.has_value();
            result = changed.value_or(false) ? 1 : 0;
            break;
        }
        case kMove:
            done = UpdateFlightMovement(world, mover, argument);
            break;
        default:
            done = UpdateFlightDirectionalMovement(world, mover, argument);
            break;
        }
        std::cout << (done ? 0 : 1) << ' ' << result;
        for (const auto value : Fields(mover))
            std::cout << ' ' << value;
        const auto& c = world.cache;
        std::cout << ' ' << int { c.fly_tested } << ' ' << int { c.fly_open } << ' '
                  << int { c.obstruction_tested };
        for (const auto answer : c.obstruction)
            std::cout << ' ' << int { answer };
        std::cout << ' ' << world.records.size();
        for (const auto& record : world.records)
            for (const auto value : record)
                std::cout << ' ' << value;
        std::cout << '\n';
    }
}
