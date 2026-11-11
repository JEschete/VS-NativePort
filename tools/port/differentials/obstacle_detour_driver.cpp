// Native side of tools\port\differentials\obstacle_detour.py (F01751): one vector per stdin
// line, one line of results per vector. The world holds the vector's room grid, height and
// flag maps, blocker cache, actor list, records and AI states; the probe answers from the
// vector's per-direction table and records each call with the navigator's state, and the
// random stream answers from the vector's queue and records each range.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "Battle/Ai/GroundNavigation.hpp"
#include "Field/TerrainHeight.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

constexpr int kGrid = 32;
constexpr int kActors = 16;
constexpr int kDirections = 8;

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}

[[noreturn]] void Unused() { std::abort(); }

struct Actor {
    std::uint8_t moving = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t z = 0;
    std::int16_t planned_x = 0;
    std::int16_t planned_y = 0;
    std::int16_t planned_z = 0;
    std::uint16_t radius = 0;
    std::uint16_t movement_flags = 0;
    std::uint8_t wait = 0;
};

class VectorWorld final : public GroundNavigationWorld {
public:
    NavigationDirectionCodes codes { };
    int width = 0;
    int depth = 0;
    RoomTileBounds limits { };
    VS::Field::TerrainRow heights[kGrid] { };
    std::uint16_t flags[kGrid][kGrid] { };
    std::uint8_t blocker_valid = 0xFF;
    std::array<std::uint8_t, kDirections> blockers { };
    std::array<Actor, kActors> actors { };
    std::vector<int> members;
    std::size_t cursor = 0;
    std::vector<int> randoms;
    std::size_t next_random = 0;
    std::vector<int> ranges;

    [[nodiscard]] std::uint32_t RoomBounds() const override
    {
        return static_cast<std::uint32_t>(width) | (static_cast<std::uint32_t>(depth) << 8U);
    }
    [[nodiscard]] std::optional<RoomTileBounds> RoomTileLimits() const override { return limits; }
    [[nodiscard]] bool HasTileMaps() const override { return true; }
    [[nodiscard]] std::uint16_t TileFlags(int x, int z) const override
    {
        if (x < 0 || z < 0 || x >= kGrid || z >= kGrid)
            std::abort();
        return flags[z][x];
    }
    [[nodiscard]] VS::Field::TerrainGrid Terrain() const override
    {
        return VS::Field::TerrainGrid(std::span<const VS::Field::TerrainRow, kGrid>(heights));
    }
    [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override { return codes; }
    [[nodiscard]] bool HasRoomData() const override { Unused(); }
    [[nodiscard]] std::optional<std::uint8_t> TerrainRegionCell(int, int) const override { Unused(); }

    [[nodiscard]] std::uint8_t WalkValid() const override { Unused(); }
    void SetWalkValid(std::uint8_t) override { Unused(); }
    [[nodiscard]] std::uint8_t WalkPassable() const override { Unused(); }
    void SetWalkPassable(std::uint8_t) override { Unused(); }
    [[nodiscard]] std::uint8_t BlockerValid() const override { return blocker_valid; }
    void SetBlockerValid(std::uint8_t bits) override { blocker_valid = bits; }
    [[nodiscard]] std::uint8_t BlockerActor(int direction) const override
    {
        return blockers.at(static_cast<std::size_t>(direction));
    }
    void SetBlockerActor(int direction, std::uint8_t actor) override
    {
        blockers.at(static_cast<std::size_t>(direction)) = actor;
    }

    void RewindCollisionActors() override { cursor = 0; }
    [[nodiscard]] std::optional<int> NextCollisionActor() override
    {
        if (cursor >= members.size())
            return std::nullopt;
        return members[cursor++];
    }
    [[nodiscard]] std::optional<GroundCollisionActor> CollisionActor(int actor_id) const override
    {
        const auto& actor = actors.at(static_cast<std::size_t>(actor_id));
        GroundCollisionActor collision;
        collision.has_ai_state = true;
        collision.model_moving = (actor.moving & 0x1F) != 0;
        collision.model_x = actor.x;
        collision.model_y = actor.y;
        collision.model_z = actor.z;
        collision.planned_x = actor.planned_x;
        collision.planned_y = actor.planned_y;
        collision.planned_z = actor.planned_z;
        collision.collision_radius = actor.radius;
        return collision;
    }
    [[nodiscard]] std::optional<std::int16_t> ModelFacing(int) const override { Unused(); }
    [[nodiscard]] std::optional<GroundPoint> ModelPosition(int) const override { Unused(); }
    [[nodiscard]] std::optional<ActorDetourWait> OtherActorDetourWait(int actor_id) const override
    {
        const auto& actor = actors.at(static_cast<std::size_t>(actor_id));
        return ActorDetourWait { .movement_flags = actor.movement_flags, .repeat_wait = actor.wait };
    }
    void SetOtherActorRepeatWait(int actor_id, std::uint8_t wait) override
    {
        actors.at(static_cast<std::size_t>(actor_id)).wait = wait;
    }

    int NextRandomMasked(int range) override
    {
        ranges.push_back(range);
        return next_random < randoms.size() ? randoms[next_random++] : 0;
    }
    [[nodiscard]] std::uint8_t HesitationLength(int) const override { Unused(); }
    [[nodiscard]] bool HasRoomAiState() const override { Unused(); }
    [[nodiscard]] std::uint8_t ContactMask() const override { Unused(); }
    void SetContactMask(std::uint8_t) override { Unused(); }
    [[nodiscard]] bool HasRouteGates() const override { Unused(); }
    int FindRouteGateAtTile(GroundNavigator&, std::uint32_t) override { Unused(); }
    [[nodiscard]] RouteGateEnd RouteLinkStart(int) const override { Unused(); }
    void Recover(GroundNavigator&) override { Unused(); }
    void IssueCommand(GroundNavigator&, int) override { Unused(); }
    void UpdateNavigationAreaMovement(GroundNavigator&) override { Unused(); }
    void MoveWithHeightFilteredDirections(GroundNavigator&) override { Unused(); }
    void UpdateFlightMovement(GroundNavigator&, int) override { Unused(); }
    void UpdateFlightDirectionalMovement(GroundNavigator&, int) override { Unused(); }
    void MoveToRadialDestination(GroundNavigator&, int) override { Unused(); }
    void MoveAwayFromReference(GroundNavigator&, int) override { Unused(); }
    void RefreshMovementCooldown(GroundNavigator&) override { Unused(); }
    std::uint32_t FindAdjacentMovementDestination(GroundNavigator&, int, int, std::uint32_t) override
    {
        Unused();
    }
    int CanFlyInDirection(GroundNavigator&, int) override { Unused(); }
};

class VectorProbe final : public GroundDirectionProbe {
public:
    std::array<int, kDirections> open { };
    std::vector<std::array<long long, 5>> calls;

    [[nodiscard]] std::optional<int> Probe(GroundNavigator& navigator, int direction) override
    {
        if (direction < 0 || direction >= kDirections)
            std::abort();
        calls.push_back({ direction, navigator.gait, navigator.target_distance_filter, navigator.repeat_wait,
            navigator.navigation_flags });
        return open[static_cast<std::size_t>(direction)];
    }
};

void ReadNavigator(std::istringstream& in, GroundNavigator& navigator)
{
    navigator.actor_id = Read<std::uint8_t>(in);
    navigator.gait = Read<std::uint8_t>(in);
    navigator.is_stuck = Read<std::uint8_t>(in);
    navigator.height_filter = Read<std::uint8_t>(in);
    navigator.near_target_detour = Read<std::uint8_t>(in);
    navigator.target_distance_filter = Read<std::uint8_t>(in);
    navigator.movement_disabled = Read<std::uint8_t>(in);
    navigator.terrain_reject_mask = Read<std::uint8_t>(in);
    navigator.tile = Read<std::uint32_t>(in);
    navigator.previous_tile = Read<std::int32_t>(in);
    navigator.x = Read<std::int16_t>(in);
    navigator.y = Read<std::int16_t>(in);
    navigator.z = Read<std::int16_t>(in);
    navigator.requested_movement_mode = Read<std::uint8_t>(in);
    navigator.collision_radius = Read<std::uint16_t>(in);
    navigator.step_height = Read<std::uint16_t>(in);
    navigator.tile_step_limit = Read<std::uint8_t>(in);
    navigator.max_range = Read<std::uint8_t>(in);
    navigator.ground_area = Read<std::uint8_t>(in);
    navigator.movement_flags = Read<std::uint16_t>(in);
    navigator.destination = Read<std::uint32_t>(in);
    navigator.next_tile = Read<std::uint32_t>(in);
    navigator.navigation_flags = Read<std::uint32_t>(in);
    navigator.detour_run = Read<std::uint8_t>(in);
    navigator.repeat_wait = Read<std::uint8_t>(in);
    navigator.pursuit_e4 = Read<std::uint8_t>(in);
    navigator.pursuit_e7 = Read<std::uint8_t>(in);
    navigator.pursuit_e8 = Read<std::uint8_t>(in);
}
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        VectorWorld world;
        for (auto& code : world.codes)
            code = Read<std::uint8_t>(in);
        world.width = Read<int>(in);
        world.depth = Read<int>(in);
        world.limits.min_x = Read<std::uint8_t>(in);
        world.limits.min_z = Read<std::uint8_t>(in);
        world.limits.max_x = Read<std::uint8_t>(in);
        world.limits.max_z = Read<std::uint8_t>(in);
        for (auto& row : world.heights)
            for (auto& cell : row)
                cell = Read<std::uint16_t>(in);
        for (auto& row : world.flags)
            for (auto& cell : row)
                cell = Read<std::uint16_t>(in);
        for (auto& blocker : world.blockers)
            blocker = Read<std::uint8_t>(in);
        GroundNavigator navigator;
        ReadNavigator(in, navigator);
        for (auto& actor : world.actors) {
            actor.moving = Read<std::uint8_t>(in);
            actor.x = Read<std::int16_t>(in);
            actor.y = Read<std::int16_t>(in);
            actor.z = Read<std::int16_t>(in);
            actor.planned_x = Read<std::int16_t>(in);
            actor.planned_y = Read<std::int16_t>(in);
            actor.planned_z = Read<std::int16_t>(in);
            actor.radius = Read<std::uint16_t>(in);
            actor.movement_flags = Read<std::uint16_t>(in);
            actor.wait = Read<std::uint8_t>(in);
        }
        world.members.resize(Read<std::size_t>(in));
        for (auto& member : world.members)
            member = Read<int>(in);
        VectorProbe probe;
        for (auto& open : probe.open)
            open = Read<int>(in);
        world.randoms.resize(Read<std::size_t>(in));
        for (auto& value : world.randoms)
            value = Read<int>(in);
        const auto bit = Read<int>(in);

        const auto result = FollowObstacleDetour(world, navigator, probe, bit);
        std::ostringstream out;
        out << (result ? 0 : 1) << ' ' << (result ? *result : 0) << ' ' << int { navigator.gait } << ' '
            << int { navigator.target_distance_filter } << ' ' << int { navigator.pursuit_e4 } << ' '
            << int { navigator.pursuit_e7 } << ' ' << int { navigator.pursuit_e8 } << ' ' << navigator.next_tile
            << ' ' << navigator.navigation_flags << ' ' << int { navigator.repeat_wait };
        for (const auto& actor : world.actors)
            out << ' ' << int { actor.wait };
        out << ' ' << probe.calls.size();
        for (const auto& call : probe.calls)
            for (const auto value : call)
                out << ' ' << value;
        out << ' ' << world.ranges.size();
        for (const auto range : world.ranges)
            out << ' ' << range;
        std::cout << out.str() << '\n';
    }
    return 0;
}
