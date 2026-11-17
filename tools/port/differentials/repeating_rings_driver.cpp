// Native host for the seven independently loaded original ring overlays.
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Core/Gte.h"
#include "Core/Math/Matrix.hpp"
#include "Core/Random.hpp"
#include "Effects/Templates/RepeatingRings.hpp"

namespace {
namespace rings = VS::Effects::Templates;
namespace math = VS::Core::Math;
using Bytes = std::vector<std::uint8_t>;
constexpr std::uint32_t kState = 0x80510000;
constexpr std::uint32_t kPackets = 0x80600000;
constexpr std::size_t kNegativeEntries = 16;

template <typename T> bool Read(T& value)
{
    long long input { };
    if (!(std::cin >> input))
        return false;
    value = static_cast<T>(input);
    return true;
}
template <typename Range> bool ReadAll(Range& range)
{
    for (auto& value : range)
        if (!Read(value))
            return false;
    return true;
}
bool ReadVector(math::Vec3& value)
{
    return Read(value.x) && Read(value.y) && Read(value.z);
}
bool ReadTransform(math::Transform3& value)
{
    for (auto& row : value.rotation.m)
        if (!ReadAll(row))
            return false;
    return ReadVector(value.translation);
}
std::uint32_t Word(const Bytes& raw, std::size_t at, std::size_t width = 4)
{
    std::uint32_t value { };
    for (std::size_t i = 0; i < width; ++i)
        value |= std::uint32_t(raw.at(at + i)) << (8 * i);
    return value;
}
std::int16_t Half(const Bytes& raw, std::size_t at)
{
    return static_cast<std::int16_t>(Word(raw, at, 2));
}
void Put(Bytes& raw, std::size_t at, std::int64_t value, std::size_t width = 4)
{
    for (std::size_t i = 0; i < width; ++i)
        raw.at(at + i) =
            static_cast<std::uint8_t>(static_cast<std::uint64_t>(value) >> (8 * i));
}
void PutVector(Bytes& raw, std::size_t at, math::Vec3 value, std::size_t width = 4)
{
    Put(raw, at, value.x, width);
    Put(raw, at + width, value.y, width);
    Put(raw, at + 2 * width, value.z, width);
}
math::ScalarSpan Range(const Bytes& raw, std::size_t at)
{
    return { Half(raw, at), Half(raw, at + 2) };
}
rings::RingVectorRange VectorRange(const Bytes& raw, std::size_t at)
{
    return { Range(raw, at), Range(raw, at + 4), Range(raw, at + 8) };
}
rings::RingDefinition Definition(const Bytes& raw)
{
    return { .flags = Word(raw, 4) & 0x3ffffff,
        .palette_index = static_cast<std::uint8_t>(Word(raw, 4) >> 26),
        .tick_event = raw[3],
        .curves = { .position = raw[8],
            .extent = raw[9],
            .inner_extent = raw[23],
            .direction = raw[10],
            .direction_jitter = raw[11],
            .speed = raw[12],
            .force = raw[13],
            .acceleration = raw[14],
            .color = { raw[0xc4], raw[0xc5], raw[0xc6] } },
        .position = VectorRange(raw, 0x18),
        .extent = VectorRange(raw, 0x24),
        .inner_extent = Range(raw, 0x30),
        .direction = VectorRange(raw, 0x34),
        .direction_jitter = VectorRange(raw, 0x40),
        .force_min = VectorRange(raw, 0x4c),
        .force_max = VectorRange(raw, 0x58),
        .acceleration_min = VectorRange(raw, 0x64),
        .acceleration_max = VectorRange(raw, 0x70),
        .speed = { Range(raw, 0xa4), Range(raw, 0xa8) } };
}
void LoadGte(const math::Transform3& value)
{
    MATRIX matrix { };
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
            matrix.m[row][column] = value.rotation.m[row][column];
    matrix.t[0] = value.translation.x;
    matrix.t[1] = value.translation.y;
    matrix.t[2] = value.translation.z;
    Core_Gte_SetRotMatrix(&matrix);
    Core_Gte_SetTransMatrix(&matrix);
}

class Host final : public rings::RingHost {
public:
    Bytes definition = Bytes(0xcc), state = Bytes(0x3c), scratch = Bytes(0x18c);
    std::array<std::int32_t, 33> samples { };
    rings::RingTickInputs inputs { };
    math::Transform3 world { }, effect_view { }, frame_view { };
    rings::RingTexture texture { };
    VS::Core::Rand15 random { };
    std::vector<std::int64_t> sample_calls, projections, texture_calls, packets, events;
    std::array<std::uint32_t, 2048 + kNegativeEntries> ordering { };
    std::int32_t allocations { }, advances { };
    Host()
    {
        for (std::size_t i = 0; i < ordering.size(); ++i)
            ordering[i] = 0xab000000 | static_cast<std::uint32_t>(i * 13 + 5);
    }
    void Create(const rings::RingState& value, rings::RingTextureSource source) override
    {
        ++allocations;
        StoreState(value);
        Put(state, 0x1c, 0);
        Put(state, 0x20, 0);
        Put(state, 0x24, 0);
        Put(state, 0x28, 0);
        Put(state, 0x2c, 0);
        Put(state, 0x30, 0);
        Put(state, 0x34, 0);
        Put(state, 0x38, 0);
        const auto at =
            source == rings::RingTextureSource::kSegmentsAndPositions ? 0xca : 1;
        const auto program =
            source == rings::RingTextureSource::kSegmentsAndPositions ? 0xcb : 0;
        texture_calls.insert(
            texture_calls.end(), { kState + 0x1c, definition[at], definition[program] });
    }
    std::optional<rings::RingState> State() const override
    {
        return rings::RingState { state[0], state[1],
            static_cast<std::int32_t>(Word(state, 4)),
            static_cast<std::int32_t>(Word(state, 8)),
            static_cast<std::int32_t>(Word(state, 12)),
            static_cast<std::int32_t>(Word(state, 16)),
            static_cast<std::int32_t>(Word(state, 20)),
            static_cast<std::int32_t>(Word(state, 24)) };
    }
    void StoreState(const rings::RingState& value) override
    {
        state[0] = value.lifetime;
        state[1] = value.definition_index;
        Put(state, 4, value.radius);
        Put(state, 8, value.radius_speed);
        Put(state, 12, value.height);
        Put(state, 16, value.height_speed);
        Put(state, 20, value.angle);
        Put(state, 24, value.age);
    }
    std::optional<rings::RingTickInputs> BeginTick(std::uint8_t index) override
    {
        sample_calls.insert(sample_calls.end(), { -1, index });
        return inputs;
    }
    std::int32_t SampleCurve(std::uint8_t curve, std::int32_t age) override
    {
        sample_calls.insert(sample_calls.end(), { curve, age });
        return samples.at(curve);
    }
    std::int32_t Rand() override { return random.Next(); }
    void PublishScratch(const rings::RingScratch& value) override
    {
        Put(scratch, 0x24, value.flags);
        PutVector(scratch, 0x98, value.position);
        PutVector(scratch, 0xa8, value.extent);
        PutVector(scratch, 0xe8, value.base_direction);
        PutVector(scratch, 0xf8, value.force_min, 2);
        PutVector(scratch, 0x100, value.force_max, 2);
        PutVector(scratch, 0x108, value.acceleration_min, 2);
        PutVector(scratch, 0x110, value.acceleration_max, 2);
        Put(scratch, 0x118, value.speed.x);
        Put(scratch, 0x11c, value.speed.y);
    }
    void BeginProjection(bool effect) override
    {
        LoadGte(effect ? effect_view : frame_view);
    }
    rings::RingProjection Project(rings::RingPoint point) override
    {
        const SVECTOR input { point.x, point.y, point.z, 0 };
        Core_Gte_LoadV0(&input);
        Core_Gte_Rtps();
        Core_GteWord screen { }, depth { };
        Core_Gte_StoreSxy2(&screen);
        Core_Gte_StoreSz3Otz(&depth);
        projections.insert(projections.end(),
            { point.x, point.y, point.z, static_cast<std::uint32_t>(screen), depth });
        return { static_cast<std::uint32_t>(screen), static_cast<std::int32_t>(depth) };
    }
    std::optional<rings::RingTexture> AdvanceTexture() override
    {
        ++advances;
        return texture;
    }
    void Submit(const rings::RingQuad& quad, std::int32_t depth) override
    {
        auto& link = ordering.at(static_cast<std::size_t>(depth + kNegativeEntries));
        const auto encoded = rings::EncodeRingPacket(quad, link);
        link = (link & 0xff000000)
             | ((kPackets + static_cast<std::uint32_t>(packets.size() / 16) * 60)
                 & 0xffffff);
        packets.push_back(depth);
        for (auto word : encoded)
            packets.push_back(word);
    }
    math::Vec3 TransformByActor(rings::RingPoint point) override
    {
        const math::Vec3 wide { point.x, point.y, point.z };
        const auto result = math::RotateTranslate(world, wide);
        PutVector(scratch, 0x2c, wide, 2);
        PutVector(scratch, 0x34, result);
        return result;
    }
    void EmitParticleEvent(std::uint8_t event, math::Vec3 position) override
    {
        events.insert(events.end(),
            { 0x80520000, event, 0, 0, position.x, position.y, position.z, 0 });
    }
};

using Instance = bool (*)(rings::RingCommand, std::int32_t,
    std::span<const rings::RingPaletteEntry>, rings::RingHost&);
constexpr std::array<Instance, 7> kInstances { &VS::Effects::Plg052::RepeatingRing,
    &VS::Effects::Plg077::RepeatingRing, &VS::Effects::Plg162::TransposedRing,
    &VS::Effects::Plg236::TransposedRing, &VS::Effects::Plg263::TransposedRing,
    &VS::Effects::Plg300::RepeatingRing, &VS::Effects::Plg300::ForegroundRepeatingRing };

template <typename Range> void Print(const Range& values)
{
    for (auto value : values)
        std::cout << ' ' << static_cast<std::int64_t>(value);
}
void PrintSized(const std::vector<std::int64_t>& values, std::size_t width)
{
    std::cout << ' ' << values.size() / width;
    Print(values);
}
bool Run()
{
    Host host;
    std::size_t ring { };
    std::uint32_t seed { }, command { };
    std::int32_t packed { };
    long ofx { }, ofy { }, h { }, dqa { }, dqb { };
    std::array<std::uint8_t, 32> palette_bytes { };
    if (!Read(ring) || !Read(seed) || !Read(command) || !Read(packed)
        || !ReadAll(host.definition) || !ReadAll(host.state) || !ReadAll(host.scratch)
        || !ReadVector(host.inputs.effect_origin) || !ReadTransform(host.world)
        || !ReadTransform(host.effect_view) || !ReadTransform(host.frame_view)
        || !Read(ofx) || !Read(ofy) || !Read(h) || !Read(dqa) || !Read(dqb)
        || !Read(host.inputs.near_clip) || !ReadAll(host.samples)
        || !Read(host.texture.clut) || !Read(host.texture.tpage) || !Read(host.texture.u)
        || !Read(host.texture.v) || !Read(host.texture.w) || !Read(host.texture.h)
        || !ReadAll(palette_bytes))
        return false;
    host.inputs.definition = Definition(host.definition);
    host.inputs.actor_translation = host.world.translation;
    host.random = VS::Core::Rand15(seed);
    std::array<rings::RingPaletteEntry, 8> palette { };
    for (std::size_t i = 0; i < palette.size(); ++i)
        palette[i].rows = { palette_bytes[i * 4], palette_bytes[i * 4 + 1],
            palette_bytes[i * 4 + 2] };
    Core_Gte_Reset();
    Core_Gte_SetGeomOffset(ofx, ofy);
    Core_Gte_SetGeomScreen(h);
    Core_Gte_SetDepthCue(dqa, dqb);
    const bool alive = kInstances.at(ring)(
        static_cast<rings::RingCommand>(command), packed, palette, host);
    std::cout << alive << ' ' << host.random.State() << ' ' << host.allocations << ' '
              << host.advances;
    Print(host.state);
    Print(host.scratch);
    PrintSized(host.sample_calls, 2);
    PrintSized(host.projections, 5);
    PrintSized(host.texture_calls, 3);
    PrintSized(host.packets, 16);
    PrintSized(host.events, 8);
    std::cout << '\n';
    return true;
}
} // namespace

int main()
{
    while (Run()) { }
    return std::cin.eof() ? 0 : 1;
}
