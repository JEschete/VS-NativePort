// Native side of tools\port\differentials\sphere_draw_*.py (A32-07): the
// sphere wire line, traversal, camera basis, position cursor and terrain row
// typed cores. One vector per stdin line, its first integer the function
// (1287, 1289, 1306, 1307 or 1308); one line of integers per vector. Projected
// inputs go through Core_Gte the way SphereDrawLive.cpp does.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "Battle/SphereDraw.hpp"
#include "Core/Math/ScalarMath.hpp"

extern "C" {
#include "Core/Gte.h"
}

namespace {
namespace battle = VS::Battle;
namespace math = VS::Core::Math;

constexpr std::size_t kEventWords = 7;
constexpr std::size_t kFloorTable = 64;

std::istringstream input;

std::int64_t Next()
{
    std::int64_t value = 0;
    if (!(input >> value)) {
        std::abort();
    }
    return value;
}

std::int16_t Short() { return static_cast<std::int16_t>(Next()); }
std::uint32_t Word() { return static_cast<std::uint32_t>(Next()); }

std::vector<std::uint32_t> output;

void Print()
{
    for (const std::uint32_t value : output) {
        std::printf("%u ", value);
    }
    std::printf("\n");
    output.clear();
}

std::uint32_t Pack(std::int32_t low, std::int32_t high)
{
    return (static_cast<std::uint32_t>(low) & 0xFFFFU)
         | (static_cast<std::uint32_t>(high) << 16U);
}

// Rotation, translation, OFX, OFY and H, as the frame view leaves the GTE.
struct View {
    MATRIX matrix { };
    long ofx = 0;
    long ofy = 0;
    long h = 0;
};

View ReadView()
{
    View view;
    for (auto& row : view.matrix.m) {
        for (auto& element : row) {
            element = Short();
        }
    }
    for (auto& translation : view.matrix.t) {
        translation = static_cast<long>(Next());
    }
    view.ofx = static_cast<long>(Next());
    view.ofy = static_cast<long>(Next());
    view.h = static_cast<long>(Next());
    return view;
}

void LoadMatrices(const View& view)
{
    Core_Gte_SetRotMatrix(&view.matrix);
    Core_Gte_SetTransMatrix(&view.matrix);
}

void LoadProjection(const View& view)
{
    Core_Gte_Reset();
    Core_Gte_SetGeomOffset(view.ofx, view.ofy);
    Core_Gte_SetGeomScreen(view.h);
}

battle::TargetWirePoint Point()
{
    battle::TargetWirePoint point;
    point.x = Short();
    point.y = Short();
    point.depth = Short();
    return point;
}

void WireLine()
{
    const battle::TargetWirePoint start = Point();
    const battle::TargetWirePoint end = Point();
    const std::uint32_t color = Word();
    const auto intensity = static_cast<std::int32_t>(Next());
    const auto line = battle::DrawSphereWireLine(start, end, color, intensity);
    if (line.has_value()) {
        output = { 1, line->slot, line->mode, line->color, line->start_xy, line->end_xy };
    } else {
        output = { 0, 0, 0, 0, 0, 0 };
    }
}

std::uint32_t VolumeWord(const battle::SphereDrawVolume& volume, std::size_t word)
{
    switch (word) {
    case 0:
        return volume.kind | (std::uint32_t { volume.reserved } << 8U)
             | (static_cast<std::uint32_t>(volume.angle) << 16U);
    case 1:
        return volume.extents[0] | (std::uint32_t { volume.extents[1] } << 8U)
             | (std::uint32_t { volume.extents[2] } << 16U)
             | (std::uint32_t { volume.extents[3] } << 24U);
    case 2:
        return Pack(volume.position[0], volume.position[1]);
    default:
        return Pack(volume.position[2], volume.position_pad);
    }
}

class Port final : public battle::SphereDrawPort {
public:
    Port(View view, std::array<math::Mat3, 2> matrices, std::uint32_t depth)
        : view_(view)
        , matrices_(matrices)
        , depth_(depth)
    {
    }

    std::array<std::int16_t, battle::kSphereSineSamples> sine { };
    std::vector<std::array<std::uint32_t, kEventWords>> events;

    void StoreSineSamples(
        std::span<const std::int16_t, battle::kSphereSineSamples> samples) override
    {
        std::copy(samples.begin(), samples.end(), sine.begin());
    }
    void StageVolume(std::size_t slot, const battle::SphereDrawVolume& volume) override
    {
        slot_ = slot;
        staged_ = volume;
    }
    void LoadFrameView() override
    {
        LoadMatrices(view_);
        events.push_back({ 1, static_cast<std::uint32_t>(slot_), VolumeWord(staged_, 0),
            VolumeWord(staged_, 1), VolumeWord(staged_, 2), VolumeWord(staged_, 3),
            depth_ });
    }
    std::int32_t ProjectCenterDepth(const battle::TargetWorldPoint& position) override
    {
        const SVECTOR point = { position[0], position[1], position[2], 0 };
        long depth = 0;
        Core_Gte_LoadV0(&point);
        Core_Gte_Rtps();
        Core_Gte_StoreSz3Otz(&depth);
        return static_cast<std::int32_t>(depth);
    }
    void StoreCenterDepth(std::int32_t depth) override
    {
        depth_ = static_cast<std::uint32_t>(depth);
    }
    math::Mat3 VolumeTransform(
        const battle::TargetVolumeExtents& extents, std::int16_t angle) override
    {
        const std::uint32_t packed = extents[0] | (std::uint32_t { extents[1] } << 8U)
                                   | (std::uint32_t { extents[2] } << 16U)
                                   | (std::uint32_t { extents[3] } << 24U);
        events.push_back({ 2, packed, static_cast<std::uint32_t>(angle), 0, 0, 0, 0 });
        return matrices_.at(transforms_++);
    }
    void BuildWireframe(std::int32_t type) override
    {
        events.push_back({ 3, static_cast<std::uint32_t>(type), 0, 0, 0, 0, 0 });
    }
    void DrawVolumeWireframe(std::size_t slot, const battle::SphereDrawVolume& volume,
        std::uint32_t color) override
    {
        events.push_back({ 4, static_cast<std::uint32_t>(slot), VolumeWord(volume, 0),
            VolumeWord(volume, 1), VolumeWord(volume, 2), VolumeWord(volume, 3), color });
    }
    void UpdateTargetLabels() override { events.push_back({ 5, depth_, 0, 0, 0, 0, 0 }); }
    // Native only; the original has no counterpart to compare.
    void DrawTargetPageHint() override { }

private:
    View view_;
    std::array<math::Mat3, 2> matrices_;
    std::uint32_t depth_;
    std::size_t slot_ = 0;
    std::size_t transforms_ = 0;
    battle::SphereDrawVolume staged_ { };
};

void Sphere()
{
    const bool present = Next() != 0;
    const View view = ReadView();
    battle::SphereDrawVolumes volumes { };
    for (auto& volume : volumes) {
        volume.kind = static_cast<std::uint8_t>(Next());
        volume.reserved = static_cast<std::uint8_t>(Next());
        volume.angle = Short();
        for (auto& extent : volume.extents) {
            extent = static_cast<std::uint8_t>(Next());
        }
        for (auto& axis : volume.position) {
            axis = Short();
        }
        volume.position_pad = Short();
    }
    std::array<math::Mat3, 2> matrices { };
    for (auto& matrix : matrices) {
        for (auto& row : matrix.m) {
            for (auto& element : row) {
                element = Short();
            }
        }
    }
    const std::uint32_t depth = Word();
    LoadProjection(view);
    Port port(view, matrices, depth);
    battle::DrawSphere(present ? std::optional { volumes } : std::nullopt, port);
    for (const std::int16_t sample : port.sine) {
        output.push_back(static_cast<std::uint32_t>(sample));
    }
    output.push_back(static_cast<std::uint32_t>(port.events.size()));
    for (const auto& event : port.events) {
        output.insert(output.end(), event.begin(), event.end());
    }
}

void CameraBasis()
{
    const auto yaw = static_cast<std::int32_t>(Next());
    const battle::SphereCameraBasis basis = battle::BuildSphereCameraBasis(yaw);
    output = { static_cast<std::uint32_t>(basis.cosine),
        static_cast<std::uint32_t>(basis.sine),
        static_cast<std::uint32_t>(basis.negated_sine),
        static_cast<std::uint32_t>(basis.cosine), static_cast<std::uint32_t>(basis.yaw) };
}

void Cursor()
{
    const View view = ReadView();
    SVECTOR position { };
    position.vx = Short();
    position.vy = Short();
    position.vz = Short();
    const auto lock = static_cast<std::int32_t>(Next());
    const auto blink = static_cast<std::uint8_t>(Next());
    battle::SphereCursorTables tables;
    for (auto& offset : tables.offsets) {
        offset = static_cast<std::int32_t>(Next());
    }
    for (auto& size : tables.sizes) {
        size = static_cast<std::int32_t>(Next());
    }
    for (auto& texcoord : tables.texcoords) {
        texcoord = static_cast<std::int32_t>(Next());
    }
    for (auto& brightness : tables.brightness) {
        brightness = static_cast<std::uint8_t>(Next());
    }
    LoadProjection(view);
    LoadMatrices(view);
    long screen = 0;
    Core_Gte_LoadV0(&position);
    Core_Gte_Rtps();
    Core_Gte_StoreSxy2(&screen);
    const auto frame = battle::DrawSpherePositionCursor(
        static_cast<std::uint32_t>(screen), lock, blink, tables);
    if (!frame.has_value()) {
        output = { static_cast<std::uint32_t>(screen), 0xFFFFFFFFU };
        return;
    }
    output = { static_cast<std::uint32_t>(screen), frame->blink,
        frame->pitch_quads.has_value() ? 1U : 0U };
    for (std::size_t quad = 0; quad < battle::kSphereCursorQuads; ++quad) {
        for (std::size_t word = 0; word < battle::kSphereCursorQuadWords; ++word) {
            output.push_back(
                frame->pitch_quads.has_value() ? (*frame->pitch_quads)[quad][word] : 0U);
        }
    }
    for (const battle::SphereCursorSprite& sprite : frame->sprites) {
        output.insert(output.end(), { static_cast<std::uint32_t>(sprite.brightness),
                                        sprite.xy, sprite.wh, sprite.texcoord });
    }
}

class Terrain final : public battle::SphereTerrainSource {
public:
    std::int32_t first_x = 0;
    std::int32_t dimensions = 0;
    std::array<std::uint32_t, battle::kSphereTerrainColumns> cells { };
    std::array<std::int32_t, battle::kSphereTerrainColumns> offsets { };
    std::array<std::uint8_t, battle::kSphereTerrainColumns> objects { };
    std::array<std::uint32_t, kFloorTable> floors { };

    std::int32_t MapDimensions() override { return dimensions; }
    std::uint32_t CellWord(std::int32_t tile_x, std::int32_t) override
    {
        return cells.at(Column(tile_x));
    }
    std::int32_t TileStackOffset(std::int32_t tile_x, std::int32_t) override
    {
        return offsets.at(Column(tile_x));
    }
    std::optional<std::uint8_t> TopGimmickObject(
        std::int32_t tile_x, std::int32_t) override
    {
        return objects.at(Column(tile_x));
    }
    std::int32_t FloorAndCeiling(std::int32_t world_x, std::int32_t world_z) override
    {
        const auto x = static_cast<std::uint32_t>(world_x) >> 5U;
        const auto z = static_cast<std::uint32_t>(world_z) >> 5U;
        return static_cast<std::int32_t>(floors.at(((x * 7U) + (z * 13U)) & 63U));
    }

private:
    [[nodiscard]] std::size_t Column(std::int32_t tile_x) const
    {
        return static_cast<std::size_t>(tile_x - first_x);
    }
};

void TerrainRow()
{
    const std::int16_t x = Short();
    const std::int16_t z = Short();
    const std::uint32_t row = Word();
    Terrain terrain;
    terrain.first_x = (x >> 7) - 4;
    terrain.dimensions = static_cast<std::int32_t>(Next());
    for (auto& cell : terrain.cells) {
        cell = Word();
    }
    for (auto& offset : terrain.offsets) {
        offset = static_cast<std::int32_t>(Next());
    }
    for (auto& object : terrain.objects) {
        object = static_cast<std::uint8_t>(Next());
    }
    for (auto& floor : terrain.floors) {
        floor = Word();
    }
    const battle::SphereTerrainRow cached =
        battle::CacheSphereTerrainRow(x, z, row, terrain);
    output = { static_cast<std::uint32_t>(cached.status) };
    for (const std::int32_t height : cached.heights) {
        output.push_back(static_cast<std::uint32_t>(height));
    }
    for (const std::uint8_t flag : cached.flags) {
        output.push_back(flag);
    }
}
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        input = std::istringstream(line);
        switch (Next()) {
        case 1287:
            WireLine();
            break;
        case 1289:
            Sphere();
            break;
        case 1306:
            CameraBasis();
            break;
        case 1307:
            Cursor();
            break;
        case 1308:
            TerrainRow();
            break;
        default:
            std::abort();
        }
        Print();
    }
}
