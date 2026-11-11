// Native side of tools\port\differentials\radial_movement.py (F01735-F01737, F01750): one vector
// per stdin line, one line of results per vector. The world holds the vector's direction codes,
// room and grid bounds, terrain and actor positions; occupancy pops the vector's queue, and every
// call is recorded with its argument and a snapshot of the fields the routines write.
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
#include <vector>

#include "Battle/Ai/RadialMovement.hpp"
#include "Field/TerrainHeight.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

constexpr int kGrid = 32;
constexpr std::size_t kActors = 17;
enum Kind { kProject, kMove, kAway, kRecover };
enum Code { kOccupancy, kCommand, kToward, kRelease, kBegin };
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

// RadialMover in declaration order, less the model's fields.
std::array<long long, 21> Fields(const RadialMover& m)
{
    return { m.toward_point, m.approach_directly, m.tile, m.saved_destination, m.x, m.z, m.move_attempts,
        m.move_distance, m.turn_timer, m.turn_period, m.target_x, m.target_z, m.movement_target,
        m.movement_flags, m.destination, m.next_tile, m.move_x, m.move_z, m.heading, m.angle,
        m.stall_counter };
}

RadialMover ReadMover(std::istringstream& in)
{
    RadialMover m;
    m.toward_point = Read<std::uint8_t>(in);
    m.approach_directly = Read<std::uint8_t>(in);
    m.tile = Read<std::uint32_t>(in);
    m.saved_destination = Read<std::uint32_t>(in);
    m.x = Read<std::int16_t>(in);
    m.z = Read<std::int16_t>(in);
    m.move_attempts = Read<std::uint16_t>(in);
    m.move_distance = Read<std::int32_t>(in);
    m.turn_timer = Read<std::uint16_t>(in);
    m.turn_period = Read<std::uint16_t>(in);
    m.target_x = Read<std::int16_t>(in);
    m.target_z = Read<std::int16_t>(in);
    m.movement_target = Read<std::uint8_t>(in);
    m.movement_flags = Read<std::uint16_t>(in);
    m.destination = Read<std::uint32_t>(in);
    m.next_tile = Read<std::uint32_t>(in);
    m.move_x = Read<std::int32_t>(in);
    m.move_z = Read<std::int32_t>(in);
    m.heading = Read<std::int16_t>(in);
    m.angle = Read<std::int16_t>(in);
    m.stall_counter = Read<std::int32_t>(in);
    m.flies = Read<int>(in) != 0;
    m.model_look_yaw = Read<std::int16_t>(in);
    m.model_facing = Read<std::int16_t>(in);
    return m;
}

class VectorWorld final : public RadialMovementWorld {
public:
    NavigationDirectionCodes codes { };
    RoomTileBounds bounds { };
    std::uint32_t grid = 0;
    VS::Field::TerrainRow rows[kGrid] { };
    std::array<RadialPoint, kActors> actors { };
    std::deque<int> occupancy;
    std::vector<Record> records;

    void Read(std::istringstream& in)
    {
        for (auto& code : codes)
            code = ::Read<std::uint8_t>(in);
        bounds.min_x = ::Read<std::uint8_t>(in);
        bounds.min_z = ::Read<std::uint8_t>(in);
        bounds.max_x = ::Read<std::uint8_t>(in);
        bounds.max_z = ::Read<std::uint8_t>(in);
        const auto width = ::Read<std::uint32_t>(in);
        const auto depth = ::Read<std::uint32_t>(in);
        grid = width | (depth << 8);
        for (auto& row : rows)
            for (auto& cell : row)
                cell = ::Read<std::uint16_t>(in);
    }

    void ReadActors(std::istringstream& in)
    {
        for (auto& actor : actors) {
            actor.x = ::Read<std::int16_t>(in);
            actor.z = ::Read<std::int16_t>(in);
        }
        occupancy.resize(::Read<std::size_t>(in));
        for (auto& answer : occupancy)
            answer = ::Read<int>(in);
    }

    [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override { return codes; }
    [[nodiscard]] std::optional<RoomTileBounds> RoomBounds() const override { return bounds; }
    [[nodiscard]] std::uint32_t GridBounds() const override { return grid; }
    [[nodiscard]] VS::Field::TerrainGrid Terrain() const override
    {
        return VS::Field::TerrainGrid(std::span<const VS::Field::TerrainRow, kGrid>(rows));
    }
    [[nodiscard]] std::optional<int> DestinationOccupancy(RadialMover& mover, std::uint32_t tile) override
    {
        Note(kOccupancy, tile, mover);
        if (occupancy.empty())
            return 0;
        const auto answer = occupancy.front();
        occupancy.pop_front();
        return answer;
    }
    [[nodiscard]] std::optional<RadialPoint> ActorPosition(int actor_id) const override
    {
        if (actor_id < 0 || static_cast<std::size_t>(actor_id) >= kActors)
            return std::nullopt;
        return actors[static_cast<std::size_t>(actor_id)];
    }
    void IssueCommand(RadialMover& mover, CommandIntent intent) override
    {
        Note(kCommand, static_cast<long long>(intent), mover);
    }
    void MoveTowardDestinationTile(RadialMover& mover, int angle) override { Note(kToward, angle, mover); }
    void ReleaseReservations(RadialMover& mover) override { Note(kRelease, 0, mover); }
    void BeginTerrainMovement(RadialMover& mover) override { Note(kBegin, 0, mover); }

private:
    void Note(int code, long long argument, const RadialMover& m)
    {
        records.push_back({ code, argument, m.destination, m.next_tile, m.move_attempts, m.turn_timer,
            m.heading, m.angle, m.stall_counter });
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
        auto mover = ReadMover(in);
        world.ReadActors(in);
        const auto source = Read<std::uint32_t>(in);
        const auto angle = Read<int>(in);
        const auto distance = Read<std::uint32_t>(in);
        bool done = false;
        switch (kind) {
        case kProject:
            done = ProjectClearRadialDestination(world, mover, source, angle, distance);
            break;
        case kMove:
            done = MoveToRadialDestination(world, mover, angle);
            break;
        case kAway:
            done = MoveAwayFromReference(world, mover, angle);
            break;
        default:
            done = RecoverMovement(world, mover);
            break;
        }
        std::cout << (done ? 0 : 1);
        for (const auto value : Fields(mover))
            std::cout << ' ' << value;
        std::cout << ' ' << world.records.size();
        for (const auto& record : world.records)
            for (const auto value : record)
                std::cout << ' ' << value;
        std::cout << '\n';
    }
}
