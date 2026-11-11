// Native side of tools\port\differentials\height_filter.py (F01738-F01739): one vector per
// stdin line, one line of results per vector. The world holds the vector's direction codes,
// height map and terrain regions; the directional move reports the vector's movement result
// and heading and records the angle and the actor's region cell at the time of the move.
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

#include "Battle/Ai/AreaRoute.hpp"
#include "Field/TerrainHeight.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

constexpr int kGrid = 32;
enum Kind { kFilter, kMove };

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}

class VectorWorld final : public HeightFilteredMoveWorld {
public:
    NavigationDirectionCodes codes { };
    VS::Field::TerrainRow rows[kGrid] { };
    int width = 0;
    std::vector<std::uint8_t> regions;
    int result = 0;
    std::uint16_t heading = 0;
    std::size_t tile_cell = 0;
    std::vector<std::array<int, 2>> moves;

    void Read(std::istringstream& in)
    {
        for (auto& code : codes)
            code = ::Read<std::uint8_t>(in);
        for (auto& row : rows)
            for (auto& cell : row)
                cell = ::Read<std::uint16_t>(in);
        width = ::Read<int>(in);
        regions.resize(static_cast<std::size_t>(width * kGrid));
        for (auto& cell : regions)
            cell = ::Read<std::uint8_t>(in);
    }

    [[nodiscard]] std::optional<std::uint8_t> TerrainRegionCell(int x, int z) const override
    {
        const auto cell = static_cast<std::size_t>((z * width) + x);
        if (x < 0 || z < 0 || x >= width || cell >= regions.size())
            return std::nullopt;
        return regions[cell];
    }
    void SetTerrainRegionCell(int x, int z, std::uint8_t cell) override
    {
        regions.at(static_cast<std::size_t>((z * width) + x)) = cell;
    }
    [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override { return codes; }
    [[nodiscard]] VS::Field::TerrainGrid Terrain() const override
    {
        return VS::Field::TerrainGrid(std::span<const VS::Field::TerrainRow, kGrid>(rows));
    }
    [[nodiscard]] std::optional<DirectionalMoveOutcome> MoveInDirection(int angle) override
    {
        moves.push_back({ angle, regions.at(tile_cell) });
        return DirectionalMoveOutcome { .movement_result = result, .heading = heading };
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
        std::ostringstream out;
        if (kind == kFilter) {
            const auto mask = Read<std::uint8_t>(in);
            const auto tile = Read<std::uint32_t>(in);
            const auto limit = Read<int>(in);
            const auto filtered
                = FilterDirectionsByGroundHeight(mask, tile, limit, world.codes, world.Terrain());
            out << (filtered ? 0 : 1) << ' ' << (filtered ? int { *filtered } : 0);
        } else {
            HeightFilteredMover actor;
            actor.tile = Read<std::uint32_t>(in);
            actor.x = Read<std::int16_t>(in);
            actor.y = Read<std::int16_t>(in);
            actor.z = Read<std::int16_t>(in);
            actor.move_x = Read<std::int32_t>(in);
            actor.move_z = Read<std::int32_t>(in);
            world.result = Read<int>(in);
            world.heading = Read<std::uint16_t>(in);
            world.tile_cell = static_cast<std::size_t>(
                (((actor.tile >> 16) & 0xFF) * world.width) + (actor.tile & 0xFF));
            const auto move = MoveWithHeightFilteredDirections(world, actor);
            const bool walks = move && move->walk_heading.has_value();
            out << (move ? 0 : 1) << ' ' << (walks ? 1 : 0) << ' '
                << (walks ? int { *move->walk_heading } : 0) << ' ' << world.moves.size();
            for (const auto& [angle, cell] : world.moves)
                out << ' ' << angle << ' ' << cell;
            out << ' ' << int { world.regions.at(world.tile_cell) };
        }
        std::cout << out.str() << '\n';
    }
    return 0;
}
