// Native side of the weather draw differentials (weather_render_common.py,
// #276). One vector per line: mode, room updates paused, tick speed, the
// velocity and reference halves, the 16 pool bytes from D_800F1DC0, whether a
// room is loaded and its area, the two argument words, the reset particle's
// index, the random, trigonometry, collision cell and floor replies (each a
// count, then the replies), the projections, quad projections and transforms
// the original's GTE produced (each a count, then the results) and the
// buffer's 0x800 bytes. Prints the pool globals, the primitive cursor's
// advance, the random words used, the buffer as words, the packets in
// ordering table order and the calls.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Effects/WeatherPool.hpp"
#include "Effects/WeatherRender.hpp"

namespace {
namespace fx = VS::Effects;
using VS::Field::WeatherArea;
using VS::Field::WeatherParticlesId;

enum Mode : int {
    kRain,
    kEmberInit,
    kEmberReset,
    kEmberDraw,
    kSnow,
    kScreen,
};
enum CallKind : std::int64_t {
    kCallSine = 1,
    kCallCosine,
    kCallBegin,
    kCallProject,
    kCallQuad,
    kCallTransform,
    kCallCell,
    kCallFloor,
};

constexpr std::size_t kPoolBytes = 16;
constexpr std::size_t kBufferBytes = 0x800;
constexpr std::size_t kBufferWords = kBufferBytes / 4;
constexpr std::size_t kParticleBytes = 16;
constexpr std::uint32_t kSurfaceMask = 0x1F;

using Call = std::vector<std::int64_t>;

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

template <typename Value, std::size_t Size> bool ReadAll(std::array<Value, Size>& values)
{
    return std::ranges::all_of(values, [](Value& value) { return Read(value); });
}

template <typename Value> bool ReadList(std::vector<Value>& values, std::size_t width = 1)
{
    std::size_t count = 0;
    if (!Read(count))
        return false;
    values.resize(count * width);
    return std::ranges::all_of(values, [](Value& value) { return Read(value); });
}

template <typename Value> Value Next(const std::vector<Value>& values, std::size_t& used)
{
    const std::size_t index = used++;
    return index < values.size() ? values[index] : Value { };
}

class DriverPoolHost final : public fx::WeatherPoolHost {
public:
    explicit DriverPoolHost(std::vector<Call>& calls)
        : calls_(calls)
    {
    }

    std::vector<std::int32_t> randoms;
    std::vector<std::int32_t> trig;
    std::size_t used = 0;

    std::int32_t Random() override { return Next(randoms, used); }
    std::int32_t Sine(std::int32_t angle) override
    {
        calls_.push_back({ kCallSine, angle });
        return Next(trig, trig_used_);
    }
    std::int32_t Cosine(std::int32_t angle) override
    {
        calls_.push_back({ kCallCosine, angle });
        return Next(trig, trig_used_);
    }
    WeatherParticlesId AllocateParticles(std::int32_t /*bytes*/) override
    {
        return WeatherParticlesId { };
    }
    void FreeParticles(WeatherParticlesId /*particles*/) override { }
    void SetRoomWeather(std::int32_t /*kind*/) override { }

private:
    std::vector<Call>& calls_;
    std::size_t trig_used_ = 0;
};

struct Submission {
    std::int32_t slot = 0;
    fx::WeatherPacket packet;
};

class DriverRenderHost final : public fx::WeatherRenderHost {
public:
    explicit DriverRenderHost(std::vector<Call>& calls)
        : calls_(calls)
    {
    }

    std::vector<std::int64_t> cells;
    std::vector<std::int32_t> floors;
    std::vector<std::int32_t> projections;
    std::vector<std::int32_t> quads;
    std::vector<std::int32_t> transforms;
    std::vector<Submission> submissions;

    void BeginProjection() override { calls_.push_back({ kCallBegin }); }

    fx::WeatherProjection Project(const fx::WeatherPoint& point) override
    {
        calls_.push_back({ kCallProject, point.x, point.y, point.z });
        fx::WeatherProjection projection;
        projection.screen.x = static_cast<std::int16_t>(Next(projections, projected_));
        projection.screen.y = static_cast<std::int16_t>(Next(projections, projected_));
        projection.depth = Next(projections, projected_);
        return projection;
    }

    fx::WeatherQuadProjection ProjectQuad(
        const std::array<fx::WeatherPoint, fx::kWeatherQuadCorners>& corners) override
    {
        Call call { kCallQuad };
        for (const fx::WeatherPoint& corner : corners) {
            call.insert(call.end(), { corner.x, corner.y, corner.z });
        }
        calls_.push_back(call);
        fx::WeatherQuadProjection projection;
        for (fx::WeatherScreenPoint& screen : projection.screens) {
            screen.x = static_cast<std::int16_t>(Next(quads, quaded_));
            screen.y = static_cast<std::int16_t>(Next(quads, quaded_));
        }
        projection.depth = Next(quads, quaded_);
        return projection;
    }

    fx::WeatherTransform Transform(const fx::WeatherPoint& point) override
    {
        calls_.push_back({ kCallTransform, point.x, point.y, point.z });
        fx::WeatherTransform transform;
        transform.x = Next(transforms, transformed_);
        transform.y = Next(transforms, transformed_);
        transform.z = Next(transforms, transformed_);
        return transform;
    }

    std::optional<std::uint8_t> CollisionSurface(
        std::int32_t tile_x, std::int32_t tile_z) override
    {
        calls_.push_back({ kCallCell, tile_x, tile_z, 0 });
        const std::int64_t present = Next(cells, celled_);
        const std::int64_t word = Next(cells, celled_);
        if (present == 0)
            return std::nullopt;
        return static_cast<std::uint8_t>(static_cast<std::uint32_t>(word) & kSurfaceMask);
    }

    std::int32_t FloorHeight(std::int16_t x, std::int16_t z) override
    {
        calls_.push_back({ kCallFloor, x, z });
        return Next(floors, floored_);
    }

    void Submit(std::int32_t slot, const fx::WeatherPacket& packet) override
    {
        submissions.push_back({ .slot = slot, .packet = packet });
    }

private:
    std::vector<Call>& calls_;
    std::size_t projected_ = 0;
    std::size_t quaded_ = 0;
    std::size_t transformed_ = 0;
    std::size_t celled_ = 0;
    std::size_t floored_ = 0;
};

std::int16_t HalfAt(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::int16_t>(
        static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8)));
}

void PutHalf(std::span<std::uint8_t> bytes, std::size_t offset, std::int16_t value)
{
    const auto bits = static_cast<std::uint16_t>(value);
    bytes[offset] = static_cast<std::uint8_t>(bits & 0xFF);
    bytes[offset + 1] = static_cast<std::uint8_t>(bits >> 8);
}

fx::WeatherParticle ParticleAt(std::span<const std::uint8_t> bytes)
{
    return { .x = HalfAt(bytes, 0),
        .y = HalfAt(bytes, 2),
        .z = HalfAt(bytes, 4),
        .pad = HalfAt(bytes, 6),
        .screen_x = HalfAt(bytes, 8),
        .screen_y = HalfAt(bytes, 0xA),
        .opaque_c = bytes[0xC],
        .sway_phase = bytes[0xD],
        .fall_shift = bytes[0xE],
        .speed = bytes[0xF] };
}

void StoreParticle(std::span<std::uint8_t> bytes, const fx::WeatherParticle& particle)
{
    PutHalf(bytes, 0, particle.x);
    PutHalf(bytes, 2, particle.y);
    PutHalf(bytes, 4, particle.z);
    PutHalf(bytes, 6, particle.pad);
    PutHalf(bytes, 8, particle.screen_x);
    PutHalf(bytes, 0xA, particle.screen_y);
    bytes[0xC] = particle.opaque_c;
    bytes[0xD] = particle.sway_phase;
    bytes[0xE] = particle.fall_shift;
    bytes[0xF] = particle.speed;
}

// The packets as the ordering table holds them: each entry's chain from its
// head, the last submitted first.
std::vector<std::vector<std::uint32_t>> ChainsOf(
    const std::vector<Submission>& submissions)
{
    std::vector<std::int32_t> slots;
    for (const Submission& submission : submissions) {
        slots.push_back(submission.slot);
    }
    std::ranges::sort(slots);
    const auto [first, last] = std::ranges::unique(slots);
    slots.erase(first, last);
    std::vector<std::vector<std::uint32_t>> chains;
    for (const std::int32_t slot : slots) {
        for (auto it = submissions.rbegin(); it != submissions.rend(); ++it) {
            if (it->slot != slot)
                continue;
            std::vector<std::uint32_t> packet { static_cast<std::uint32_t>(slot),
                it->packet.size };
            packet.insert(packet.end(), it->packet.words.begin(),
                it->packet.words.begin() + it->packet.size);
            chains.push_back(std::move(packet));
        }
    }
    return chains;
}

int RunVector(int mode)
{
    std::vector<Call> calls;
    DriverPoolHost pool(calls);
    DriverRenderHost render(calls);
    int paused = 0;
    std::int32_t tick = 0;
    std::array<std::int16_t, 3> velocity { };
    std::array<std::int16_t, 3> reference { };
    std::array<std::uint8_t, kPoolBytes> pool_bytes { };
    int room = 0;
    std::array<std::int16_t, 4> area_halves { };
    std::array<std::int32_t, 2> arguments { };
    std::size_t index = 0;
    if (!Read(paused) || !Read(tick) || !ReadAll(velocity) || !ReadAll(reference)
        || !ReadAll(pool_bytes) || !Read(room) || !ReadAll(area_halves)
        || !ReadAll(arguments) || !Read(index) || !ReadList(pool.randoms)
        || !ReadList(pool.trig) || !ReadList(render.cells, 2) || !ReadList(render.floors)
        || !ReadList(render.projections, 3) || !ReadList(render.quads, 9)
        || !ReadList(render.transforms, 3))
        return 1;
    std::vector<std::uint8_t> buffer(kBufferBytes);
    if (!std::ranges::all_of(buffer, [](std::uint8_t& byte) { return Read(byte); }))
        return 1;

    std::vector<fx::WeatherParticle> particles(fx::kWeatherParticleCapacity);
    for (std::size_t slot = 0; slot < particles.size(); ++slot) {
        particles[slot] = ParticleAt(std::span<const std::uint8_t>(buffer).subspan(
            slot * kParticleBytes, kParticleBytes));
    }
    fx::WeatherPoolState state { .particle_count = HalfAt(pool_bytes, 0),
        .particle_capacity = HalfAt(pool_bytes, 2),
        .kind = pool_bytes[0xA],
        .screen_fade = pool_bytes[7] };
    const fx::WeatherFrame frame { .paused = paused != 0,
        .tick_speed = tick,
        .velocity = { .x = velocity[0], .y = velocity[1], .z = velocity[2] } };
    std::optional<WeatherArea> area;
    if (room != 0) {
        area = WeatherArea { .origin_x = area_halves[0],
            .origin_z = area_halves[1],
            .extent_x = area_halves[2],
            .extent_z = area_halves[3] };
    }
    const std::span<fx::WeatherParticle> span(particles);
    bool fitted = true;
    switch (mode) {
    case kRain:
        fitted =
            fx::DrawRainParticles(pool, render, frame, span, state.particle_count, area);
        break;
    case kEmberInit:
        if (!area)
            return 1;
        fitted = fx::InitializeRisingEmberParticles(
            pool, state, span, *area, arguments[0], arguments[1]);
        break;
    case kEmberReset:
        if (!area || index >= particles.size())
            return 1;
        particles[index] = fx::ResetRisingEmberParticle(pool, particles[index], *area);
        break;
    case kEmberDraw:
        if (!area)
            return 1;
        fitted = fx::DrawRisingEmberParticles(
            pool, render, frame, span, state.particle_count, *area);
        break;
    case kSnow:
        fitted =
            fx::DrawSnowParticles(pool, render, frame, span, state.particle_count, area);
        break;
    case kScreen:
        fitted = fx::DrawScreenWeatherParticles(pool, render, frame,
            { .x = reference[0], .y = reference[1], .z = reference[2] }, state, span);
        break;
    default:
        return 1;
    }
    if (!fitted)
        return 1;
    for (std::size_t slot = 0; slot < particles.size(); ++slot) {
        StoreParticle(std::span<std::uint8_t>(buffer).subspan(
                          slot * kParticleBytes, kParticleBytes),
            particles[slot]);
    }

    std::int64_t advance = 0;
    for (const Submission& submission : render.submissions) {
        advance += 4 * (std::int64_t { submission.packet.size } + 1);
    }
    std::cout << state.particle_count << ' ' << state.particle_capacity << ' '
              << int { state.kind } << ' ' << int { state.screen_fade } << ' ' << advance
              << ' ' << pool.used;
    for (std::size_t word = 0; word < kBufferWords; ++word) {
        std::uint32_t bits = 0;
        for (std::size_t byte = 0; byte < 4; ++byte) {
            bits |= std::uint32_t { buffer[word * 4 + byte] } << (8 * byte);
        }
        std::cout << ' ' << bits;
    }
    const auto chains = ChainsOf(render.submissions);
    std::cout << ' ' << chains.size();
    for (const auto& packet : chains) {
        for (const std::uint32_t word : packet)
            std::cout << ' ' << word;
    }
    std::cout << ' ' << calls.size();
    for (const Call& call : calls) {
        for (const std::int64_t word : call)
            std::cout << ' ' << word;
    }
    std::cout << '\n';
    return 0;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int mode = 0;
    while (Read(mode)) {
        if (RunVector(mode) != 0)
            return 1;
    }
    return 0;
}
