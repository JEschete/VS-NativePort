// Native side of particle_emitter.py; byte records are fixture I/O only.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Effects/ParticleEmitter.hpp"

namespace {
namespace fx = VS::Effects;
namespace math = VS::Core::Math;
constexpr std::size_t kRecords = 8;
constexpr std::uint32_t kPool = 0x80520000;
constexpr std::size_t kStride = 0x78;
using Bytes = std::vector<std::uint8_t>;

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
bool ReadMatrix(math::Mat3& value)
{
    for (auto& row : value.m)
        if (!ReadAll(row))
            return false;
    return true;
}
std::uint32_t Unsigned(const Bytes& raw, std::size_t at, std::size_t size = 4)
{
    std::uint32_t result { };
    for (std::size_t index = 0; index < size; ++index)
        result |= std::uint32_t(raw.at(at + index)) << (index * 8);
    return result;
}
std::int32_t Word(const Bytes& raw, std::size_t at)
{
    return static_cast<std::int32_t>(Unsigned(raw, at));
}
std::int16_t Half(const Bytes& raw, std::size_t at)
{
    return static_cast<std::int16_t>(Unsigned(raw, at, 2));
}
void Put(Bytes& raw, std::size_t at, std::int64_t value, std::size_t size = 4)
{
    for (std::size_t index = 0; index < size; ++index)
        raw.at(at + index) =
            static_cast<std::uint8_t>(static_cast<std::uint64_t>(value) >> (index * 8));
}
math::Vec3 Vector(const Bytes& raw, std::size_t at, std::size_t width = 4)
{
    if (width == 2)
        return { Half(raw, at), Half(raw, at + 2), Half(raw, at + 4) };
    return { Word(raw, at), Word(raw, at + 4), Word(raw, at + 8) };
}
void PutVector(Bytes& raw, std::size_t at, math::Vec3 value, std::size_t width = 4)
{
    Put(raw, at, value.x, width);
    Put(raw, at + width, value.y, width);
    Put(raw, at + 2 * width, value.z, width);
}
math::Vec2 Pair(const Bytes& raw, std::size_t at)
{
    return { Word(raw, at), Word(raw, at + 4) };
}
void PutPair(Bytes& raw, std::size_t at, math::Vec2 value)
{
    Put(raw, at, value.x);
    Put(raw, at + 4, value.y);
}
math::Mat3 Matrix(const Bytes& raw, std::size_t at)
{
    math::Mat3 result;
    for (auto& row : result.m)
        for (auto& value : row) {
            value = Half(raw, at);
            at += 2;
        }
    return result;
}
void PutMatrix(Bytes& raw, std::size_t at, const math::Mat3& matrix)
{
    for (const auto& row : matrix.m)
        for (auto value : row) {
            Put(raw, at, value, 2);
            at += 2;
        }
}
math::ScalarSpan Range(const Bytes& raw, std::size_t at)
{
    return { Half(raw, at), Half(raw, at + 2) };
}
fx::EmissionVectorRange VectorRange(const Bytes& raw, std::size_t at)
{
    return { Range(raw, at), Range(raw, at + 4), Range(raw, at + 8) };
}
fx::ParticleEmissionDefinition Definition(const Bytes& raw)
{
    fx::ParticleEmissionDefinition result;
    result.flags = Unsigned(raw, 4) & 0x3FFFFFF;
    result.transparency = static_cast<std::uint8_t>(Unsigned(raw, 4) >> 26);
    result.animation = raw[0];
    result.texture = raw[1];
    result.end_event = raw[2];
    result.tick_event = raw[3];
    std::copy_n(raw.begin() + 8, result.curves.size(), result.curves.begin());
    result.position = VectorRange(raw, 0x18);
    result.extent = VectorRange(raw, 0x24);
    result.inner_extent = Range(raw, 0x30);
    for (std::size_t index = 0; index < result.vectors.size(); ++index)
        result.vectors[index] = VectorRange(raw, 0x34 + index * 12);
    for (std::size_t index = 0; index < result.pairs.size(); ++index)
        result.pairs[index] = { Range(raw, 0x94 + index * 8),
            Range(raw, 0x98 + index * 8) };
    result.count = Range(raw, 0xBC);
    result.interval = Range(raw, 0xC0);
    result.color_curves = { raw[0xC4], raw[0xC5], raw[0xC6] };
    result.transform = raw[0xC8];
    result.attraction_transform = raw[0xC9];
    result.ring_segments = raw[0xCA];
    result.ring_positions = raw[0xCB];
    return result;
}
fx::ParticleEmissionScratch Scratch(const Bytes& raw)
{
    fx::ParticleEmissionScratch result;
    result.direction_matrix = Matrix(raw, 0);
    result.flags = Unsigned(raw, 0x24);
    result.saved_flags = static_cast<std::uint16_t>(Unsigned(raw, 0x28, 2));
    result.offset = { Half(raw, 0x2C), Half(raw, 0x2E), Half(raw, 0x30) };
    result.aim = Vector(raw, 0x34);
    result.spawn = Vector(raw, 0x44);
    result.aim_matrix = Matrix(raw, 0x64);
    result.position = Vector(raw, 0x98);
    result.extent = Vector(raw, 0xA8);
    result.direction = Vector(raw, 0xB8);
    result.direction_jitter = Vector(raw, 0xC8);
    result.inner_extent = Vector(raw, 0xD8);
    result.base_direction = Vector(raw, 0xE8);
    result.force_min = Vector(raw, 0xF8, 2);
    result.force_max = Vector(raw, 0x100, 2);
    result.acceleration_min = Vector(raw, 0x108, 2);
    result.acceleration_max = Vector(raw, 0x110, 2);
    result.speed_range = Pair(raw, 0x118);
    result.speed = Word(raw, 0x120);
    result.mass_range = Pair(raw, 0x124);
    result.gravity_range = Pair(raw, 0x12C);
    result.lifetime_range = Pair(raw, 0x134);
    result.attraction_offset = Vector(raw, 0x13C, 2);
    result.attraction_jitter = Vector(raw, 0x144, 2);
    result.attraction_range = Pair(raw, 0x14C);
    result.color_curves = { raw[0x154], raw[0x155], raw[0x156] };
    result.age = raw[0x157];
    result.transform = raw[0x168];
    result.attraction_transform = raw[0x16C];
    result.sin_angle0 = Word(raw, 0x170);
    result.sin_angle1 = Word(raw, 0x174);
    result.cos_angle0 = Word(raw, 0x178);
    result.cos_angle1 = Word(raw, 0x17C);
    result.transparency = raw[0x180];
    result.ring_segments = Word(raw, 0x184);
    result.ring_positions = Word(raw, 0x188);
    return result;
}
void StoreScratch(Bytes& raw, const fx::ParticleEmissionScratch& value)
{
    PutMatrix(raw, 0, value.direction_matrix);
    Put(raw, 0x24, value.flags);
    Put(raw, 0x28, value.saved_flags, 2);
    PutVector(raw, 0x2C, { value.offset.x, value.offset.y, value.offset.z }, 2);
    PutVector(raw, 0x34, value.aim);
    PutVector(raw, 0x44, value.spawn);
    PutMatrix(raw, 0x64, value.aim_matrix);
    PutVector(raw, 0x98, value.position);
    PutVector(raw, 0xA8, value.extent);
    PutVector(raw, 0xB8, value.direction);
    PutVector(raw, 0xC8, value.direction_jitter);
    PutVector(raw, 0xD8, value.inner_extent);
    PutVector(raw, 0xE8, value.base_direction);
    PutVector(raw, 0xF8, value.force_min, 2);
    PutVector(raw, 0x100, value.force_max, 2);
    PutVector(raw, 0x108, value.acceleration_min, 2);
    PutVector(raw, 0x110, value.acceleration_max, 2);
    PutPair(raw, 0x118, value.speed_range);
    Put(raw, 0x120, value.speed);
    PutPair(raw, 0x124, value.mass_range);
    PutPair(raw, 0x12C, value.gravity_range);
    PutPair(raw, 0x134, value.lifetime_range);
    PutVector(raw, 0x13C, value.attraction_offset, 2);
    PutVector(raw, 0x144, value.attraction_jitter, 2);
    PutPair(raw, 0x14C, value.attraction_range);
    std::copy(value.color_curves.begin(), value.color_curves.end(), raw.begin() + 0x154);
    raw[0x157] = value.age;
    Put(raw, 0x168, value.transform);
    Put(raw, 0x16C, value.attraction_transform);
    Put(raw, 0x170, value.sin_angle0);
    Put(raw, 0x174, value.sin_angle1);
    Put(raw, 0x178, value.cos_angle0);
    Put(raw, 0x17C, value.cos_angle1);
    raw[0x180] = value.transparency;
    Put(raw, 0x184, value.ring_segments);
    Put(raw, 0x188, value.ring_positions);
}
std::optional<fx::ParticleEmissionHandle> Handle(std::uint32_t address)
{
    if (address == 0)
        return std::nullopt;
    return fx::ParticleEmissionHandle { (address - kPool) / kStride };
}
std::uint32_t Address(std::optional<fx::ParticleEmissionHandle> handle)
{
    return handle ? kPool + static_cast<std::uint32_t>(handle->index * kStride) : 0;
}
fx::EmittedParticle Particle(const Bytes& raw)
{
    const auto target = Vector(raw, 0x62, 2);
    return { .next = Handle(Unsigned(raw, 4)),
        .motion = { .mass = Half(raw, 8),
            .gravity_scale = Half(raw, 10),
            .position = Vector(raw, 0xC),
            .velocity = Vector(raw, 0x18),
            .force = Vector(raw, 0x24),
            .acceleration = Vector(raw, 0x30),
            .target = { static_cast<std::int16_t>(target.x),
                static_cast<std::int16_t>(target.y),
                static_cast<std::int16_t>(target.z) },
            .attraction = Word(raw, 0x70) },
        .flags = Unsigned(raw, 0x5C),
        .transparency = raw[0x60],
        .variant = static_cast<std::uint8_t>(raw[0x61] & 15),
        .lifetime = Half(raw, 0x68),
        .transform = raw[0x6A],
        .attraction_transform = raw[0x6B],
        .end_event = raw[0x6C],
        .tick_event = raw[0x6D],
        .color_curves = { raw[0x74], raw[0x75], raw[0x76] },
        .age = raw[0x77] };
}
void StoreParticle(Bytes& raw, const fx::EmittedParticle& value)
{
    Put(raw, 8, value.motion.mass, 2);
    Put(raw, 10, value.motion.gravity_scale, 2);
    PutVector(raw, 0xC, value.motion.position);
    PutVector(raw, 0x18, value.motion.velocity);
    PutVector(raw, 0x24, value.motion.force);
    PutVector(raw, 0x30, value.motion.acceleration);
    Put(raw, 0x5C, value.flags);
    raw[0x60] = value.transparency;
    raw[0x61] = static_cast<std::uint8_t>((raw[0x61] & 0xF0) | value.variant);
    PutVector(raw, 0x62,
        { value.motion.target.x, value.motion.target.y, value.motion.target.z }, 2);
    Put(raw, 0x68, value.lifetime, 2);
    raw[0x6A] = value.transform;
    raw[0x6B] = value.attraction_transform;
    raw[0x6C] = value.end_event;
    raw[0x6D] = value.tick_event;
    Put(raw, 0x70, value.motion.attraction);
    std::copy(value.color_curves.begin(), value.color_curves.end(), raw.begin() + 0x74);
    raw[0x77] = value.age;
}

class Host final : public fx::ParticleEmissionHost {
public:
    std::array<Bytes, kRecords> records;
    std::optional<fx::ParticleEmissionHandle> first;
    std::optional<fx::ParticleEmissionHandle> free;
    int used { };
    std::uint8_t count { };
    std::vector<std::array<int, 3>> texture_calls;
    int releases { };
    std::optional<fx::ParticleEmissionHandle> Allocate() override
    {
        if (!free)
            return std::nullopt;
        const auto handle = *free;
        auto& raw = records.at(handle.index);
        free = Handle(Unsigned(raw, 4));
        Put(raw, 0, 0);
        Put(raw, 4, Address(first));
        if (first)
            Put(records.at(first->index), 0, Address(handle));
        first = handle;
        std::fill(raw.begin() + 0x3C, raw.begin() + 0x5C, 0);
        raw[0x77] = 0xFF;
        ++used;
        ++count;
        return handle;
    }
    std::optional<fx::EmittedParticle> Read(
        fx::ParticleEmissionHandle handle) const override
    {
        if (handle.index >= records.size())
            return std::nullopt;
        return Particle(records.at(handle.index));
    }
    void Store(
        fx::ParticleEmissionHandle handle, const fx::EmittedParticle& values) override
    {
        StoreParticle(records.at(handle.index), values);
    }
    void InitializeTexture(fx::ParticleEmissionHandle handle, std::uint8_t texture,
        std::uint8_t animation) override
    {
        texture_calls.push_back({ static_cast<int>(handle.index), texture, animation });
    }
    void Release(fx::ParticleEmissionHandle handle) override
    {
        ++releases;
        auto& raw = records.at(handle.index);
        first = Handle(Unsigned(raw, 4));
        if (first)
            Put(records.at(first->index), 0, 0);
        Put(raw, 4, Address(free));
        free = handle;
        --used;
        --count;
    }
};
bool Run()
{
    std::uint32_t seed { };
    std::int32_t counter { };
    fx::ParticleEmissionContext context;
    int has_parent { };
    fx::ParticleEmissionParent parent;
    math::Vec3 aim;
    std::uint32_t first { }, free { };
    Host host;
    Bytes definition(0xCC), scratch_bytes(0x18C);
    std::array<std::int32_t, 33> samples { };
    if (!Read(seed) || !Read(counter) || !Read(context.frame) || !Read(context.capacity)
        || !Read(context.used) || !Read(context.transparency_override)
        || !Read(context.variant) || !Read(has_parent) || !ReadVector(parent.position)
        || !Read(parent.transparency) || !Read(parent.variant)
        || !ReadVector(context.effect_origin) || !ReadMatrix(context.transform.rotation)
        || !ReadVector(context.transform.translation)
        || !ReadVector(context.attraction_translation) || !ReadVector(aim) || !Read(first)
        || !Read(free) || !Read(host.count) || !ReadAll(definition) || !ReadAll(samples)
        || !ReadAll(scratch_bytes))
        return false;
    for (auto& raw : host.records) {
        raw.resize(kStride);
        if (!ReadAll(raw))
            return false;
    }
    context.samples = samples;
    context.aim_target = aim;
    if (has_parent)
        context.parent = parent;
    host.first = Handle(first);
    host.free = Handle(free);
    host.used = context.used;
    VS::Core::Rand15 random(seed);
    auto scratch = Scratch(scratch_bytes);
    const auto result = fx::EmitParticleEvent(
        Definition(definition), context, scratch, counter, random, host);
    StoreScratch(scratch_bytes, scratch);
    std::cout << (result.status >= fx::ParticleEmissionStatus::kMissingSamples ? 1 : 0)
              << ' ' << random.State() << ' ' << counter << ' ' << host.used << ' '
              << int(host.count) << ' ' << Address(host.first) << ' '
              << Address(host.free) << ' ' << host.releases;
    for (auto value : scratch_bytes)
        std::cout << ' ' << int(value);
    for (const auto& raw : host.records)
        for (auto value : raw)
            std::cout << ' ' << int(value);
    std::cout << ' ' << host.texture_calls.size();
    for (const auto& call : host.texture_calls)
        for (auto value : call)
            std::cout << ' ' << value;
    std::cout << '\n';
    return true;
}
} // namespace

int main()
{
    while (Run()) { }
    return std::cin.eof() ? 0 : 1;
}
