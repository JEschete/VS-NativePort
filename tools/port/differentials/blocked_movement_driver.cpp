// Native side of tools\port\differentials\blocked_movement.py (F01636, F01641, F01642, F01644,
// F01660): one vector per stdin line, one line of results per vector. The worlds hold the vector's
// direction codes, arena orbit and random draw; the movement primitives pop the vector's queues,
// and every call is recorded with its arguments and a snapshot of the fields the detour writes.
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

#include "Battle/Ai/BlockedMovement.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

enum Kind { kAdjacent, kStalled, kRepeats, kSave, kRecover };
enum Code { kEdge, kOccupancy, kGap, kRandom, kForget, kDispatch };
constexpr std::size_t kRecord = 10;
using Record = std::array<long long, kRecord>;

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}

std::deque<long long> ReadQueue(std::istringstream& in)
{
    std::deque<long long> queue(Read<std::size_t>(in));
    for (auto& answer : queue)
        answer = Read<long long>(in);
    return queue;
}

long long Pop(std::deque<long long>& queue)
{
    if (queue.empty())
        return 0;
    const auto answer = queue.front();
    queue.pop_front();
    return answer;
}

NavigationDirectionCodes ReadCodes(std::istringstream& in)
{
    NavigationDirectionCodes codes { };
    for (auto& code : codes)
        code = Read<std::uint8_t>(in);
    return codes;
}

long long Directions(const BlockedMover& m)
{
    return m.detour_directions[0] | m.detour_directions[1] << 8 | m.detour_directions[2] << 16
        | static_cast<long long>(m.detour_directions[3]) << 24;
}

// BlockedMover in declaration order, the directions one per byte.
std::array<long long, 14> Fields(const BlockedMover& m)
{
    return { m.is_stuck, m.holds_stall_tile, m.tile, m.previous_tile, m.x, m.z, m.detour_step,
        m.detour_target, m.detour_directions[0], m.detour_directions[1], m.detour_directions[2],
        m.detour_directions[3], m.movement_target, m.destination };
}

BlockedMover ReadMover(std::istringstream& in)
{
    BlockedMover m;
    m.is_stuck = Read<std::uint8_t>(in);
    m.holds_stall_tile = Read<std::uint8_t>(in);
    m.tile = Read<std::uint32_t>(in);
    m.previous_tile = Read<std::uint32_t>(in);
    m.x = Read<std::int16_t>(in);
    m.z = Read<std::int16_t>(in);
    m.detour_step = Read<std::uint8_t>(in);
    m.detour_target = Read<std::uint8_t>(in);
    for (auto& direction : m.detour_directions)
        direction = Read<std::uint8_t>(in);
    m.movement_target = Read<std::uint8_t>(in);
    m.destination = Read<std::uint32_t>(in);
    return m;
}

class AdjacentWorld final : public AdjacentDestinationWorld {
public:
    NavigationDirectionCodes codes { };
    std::deque<long long> edges, occupancy, landings;
    std::vector<Record> records;

    [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override { return codes; }
    [[nodiscard]] std::optional<int> CanTraverseTileEdge(std::uint32_t from, std::uint32_t to, int direction) override
    {
        records.push_back({ kEdge, from, to, direction });
        return static_cast<int>(Pop(edges));
    }
    [[nodiscard]] std::optional<int> DestinationOccupancy(std::uint32_t tile) override
    {
        records.push_back({ kOccupancy, tile });
        return static_cast<int>(Pop(occupancy));
    }
    [[nodiscard]] std::optional<std::uint32_t> FindGapLandingTile(
        std::uint32_t tile, int direction, std::uint32_t from) override
    {
        records.push_back({ kGap, tile, direction, from });
        return static_cast<std::uint32_t>(Pop(landings));
    }
};

class RecoverWorld final : public BlockedMovementWorld {
public:
    NavigationDirectionCodes codes { };
    bool orbit = false;
    int random = 0;
    std::deque<long long> occupancy;
    std::vector<Record> records;

    [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override { return codes; }
    [[nodiscard]] bool HasArenaOrbit() const override { return orbit; }
    [[nodiscard]] int NextRandomMasked(int range) override
    {
        records.push_back({ kRandom, range });
        return random;
    }
    [[nodiscard]] std::optional<int> DestinationOccupancy(BlockedMover& mover, std::uint32_t tile) override
    {
        Note(kOccupancy, tile, mover);
        return static_cast<int>(Pop(occupancy));
    }
    void ForgetActionTarget(BlockedMover& mover) override { Note(kForget, 0, mover); }
    void DispatchLocomotion(BlockedMover& mover) override { Note(kDispatch, 0, mover); }

private:
    void Note(int code, long long argument, const BlockedMover& m)
    {
        records.push_back({ code, argument, 0, 0, m.is_stuck, m.holds_stall_tile, m.detour_step, Directions(m),
            m.movement_target, m.destination });
    }
};

void Print(bool refused, long long result, const std::vector<long long>& fields, const std::vector<Record>& records)
{
    std::cout << (refused ? 1 : 0) << ' ' << result << ' ' << fields.size();
    for (const auto value : fields)
        std::cout << ' ' << value;
    std::cout << ' ' << records.size();
    for (const auto& record : records)
        for (const auto value : record)
            std::cout << ' ' << value;
    std::cout << '\n';
}

void Adjacent(std::istringstream& in)
{
    AdjacentWorld world;
    world.codes = ReadCodes(in);
    AdjacentMover mover;
    mover.movement_disabled = Read<int>(in) != 0;
    mover.tile = Read<std::uint32_t>(in);
    const auto first = Read<int>(in);
    const auto second = Read<int>(in);
    const auto direction = Read<std::uint32_t>(in);
    world.edges = ReadQueue(in);
    world.occupancy = ReadQueue(in);
    world.landings = ReadQueue(in);
    const auto result = FindAdjacentMovementDestination(world, mover, first, second, direction);
    Print(!result.has_value(), result.value_or(0), { }, world.records);
}

MovementHistory ReadHistory(std::istringstream& in)
{
    MovementHistory history;
    for (auto& word : history.x_words)
        word = Read<std::uint32_t>(in);
    for (auto& word : history.z_words)
        word = Read<std::uint32_t>(in);
    history.mask = Read<std::uint8_t>(in);
    return history;
}

void Recover(std::istringstream& in)
{
    RecoverWorld world;
    world.codes = ReadCodes(in);
    world.orbit = Read<int>(in) != 0;
    world.random = Read<int>(in);
    auto mover = ReadMover(in);
    world.occupancy = ReadQueue(in);
    const auto result = TryRecoverBlockedMovement(world, mover);
    const auto fields = Fields(mover);
    Print(!result.has_value(), result.value_or(false) ? 1 : 0, { fields.begin(), fields.end() }, world.records);
}
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        switch (Read<int>(in)) {
        case kAdjacent:
            Adjacent(in);
            break;
        case kStalled:
            Print(false, IsMovementHistoryStalled(ReadHistory(in)) ? 1 : 0, { }, { });
            break;
        case kRepeats:
            Print(false, HasRepeatingMovementHistory(ReadHistory(in)) ? 1 : 0, { }, { });
            break;
        case kSave: {
            MovementRandomStates states;
            states.frame = Read<std::uint32_t>(in);
            states.saved = Read<std::int32_t>(in);
            SaveMovementRandomState(states);
            Print(false, static_cast<std::uint32_t>(states.saved), { }, { });
            break;
        }
        default:
            Recover(in);
            break;
        }
    }
}
