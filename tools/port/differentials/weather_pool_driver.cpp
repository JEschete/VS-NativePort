// Native side of the weather pool differentials (weather_pool_common.py, #276).
// One vector per line: mode, whether a room is loaded, the room record's weather
// kind and area, the two argument words, the current and allocated buffer
// pointers, the 16 pool bytes from D_800F1DC0, the eight D_800F1D98 halfwords,
// the reset particle's index, the random words (count, then words), the five
// trigonometry results and the buffer's 0x800 bytes. Prints the result, the
// buffer pointer, the pool globals, func_8008EB04's outputs, the velocity
// words, the random words used, the buffer as words and the calls.
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Effects/WeatherPool.hpp"

namespace {
namespace fx = VS::Effects;
using VS::Field::WeatherArea;
using VS::Field::WeatherParticlesId;

enum Mode : int {
    kAllocate,
    kRoomWeather,
    kWeatherState,
    kFalling,
    kScreen,
    kReset,
    kVelocity,
};
enum CallKind : int {
    kCallFree = 1,
    kCallAllocate,
    kCallWeather,
    kCallSine,
    kCallCosine,
};

constexpr std::size_t kPoolBytes = 16;
constexpr std::size_t kParameterHalves = 8;
constexpr std::size_t kTrigResults = 5;
constexpr std::size_t kBufferBytes = 0x800;
constexpr std::size_t kBufferWords = kBufferBytes / 4;

class DriverHost final : public fx::WeatherPoolHost {
public:
    std::vector<std::int32_t> randoms;
    std::deque<std::int32_t> trig;
    std::uint32_t allocated = 0;
    std::size_t used = 0;
    std::vector<std::array<std::int64_t, 2>> calls;

    std::int32_t Random() override
    {
        const std::size_t index = used++;
        return index < randoms.size() ? randoms[index] : 0;
    }
    std::int32_t Sine(std::int32_t angle) override
    {
        calls.push_back({ kCallSine, angle });
        return NextTrig();
    }
    std::int32_t Cosine(std::int32_t angle) override
    {
        calls.push_back({ kCallCosine, angle });
        return NextTrig();
    }
    WeatherParticlesId AllocateParticles(std::int32_t bytes) override
    {
        calls.push_back({ kCallAllocate, bytes });
        return static_cast<WeatherParticlesId>(allocated);
    }
    void FreeParticles(WeatherParticlesId particles) override
    {
        calls.push_back({ kCallFree,
            static_cast<std::int32_t>(static_cast<std::uint32_t>(particles)) });
    }
    void SetRoomWeather(std::int32_t kind) override
    {
        calls.push_back({ kCallWeather, kind });
    }

private:
    std::int32_t NextTrig()
    {
        if (trig.empty())
            return 0;
        const std::int32_t value = trig.front();
        trig.pop_front();
        return value;
    }
};

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
    for (Value& value : values) {
        if (!Read(value))
            return false;
    }
    return true;
}

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

int RunVector(int mode)
{
    DriverHost host;
    int room = 0;
    std::int16_t room_kind = 0;
    std::array<std::int16_t, 4> area_halves { };
    std::array<std::int32_t, 2> arguments { };
    std::uint32_t current = 0;
    std::array<std::uint8_t, kPoolBytes> pool { };
    std::array<std::int16_t, kParameterHalves> parameters { };
    std::size_t index = 0;
    std::size_t random_count = 0;
    if (!Read(room) || !Read(room_kind) || !ReadAll(area_halves) || !ReadAll(arguments)
        || !Read(current) || !Read(host.allocated) || !ReadAll(pool)
        || !ReadAll(parameters) || !Read(index) || !Read(random_count))
        return 1;
    host.randoms.resize(random_count);
    for (std::int32_t& random : host.randoms) {
        if (!Read(random))
            return 1;
    }
    std::array<std::int32_t, kTrigResults> trig { };
    std::vector<std::uint8_t> buffer(kBufferBytes);
    if (!ReadAll(trig))
        return 1;
    for (std::uint8_t& byte : buffer) {
        if (!Read(byte))
            return 1;
    }
    host.trig.assign(trig.begin(), trig.end());

    std::vector<fx::WeatherParticle> particles(fx::kWeatherParticleCapacity);
    for (std::size_t slot = 0; slot < particles.size(); ++slot) {
        particles[slot] =
            ParticleAt(std::span<const std::uint8_t>(buffer).subspan(slot * 16, 16));
    }
    fx::WeatherPoolState state { .particle_count = HalfAt(pool, 0),
        .particle_capacity = HalfAt(pool, 2),
        .kind = pool[0xA],
        .screen_fade = pool[7] };
    const std::int16_t intensity = HalfAt(pool, 4);
    std::optional<WeatherArea> area;
    if (room != 0) {
        area = WeatherArea { .origin_x = area_halves[0],
            .origin_z = area_halves[1],
            .extent_x = area_halves[2],
            .extent_z = area_halves[3] };
    }
    std::int32_t result = 0;
    fx::RoomWeatherReport report { };
    switch (mode) {
    case kAllocate:
        current = static_cast<std::uint32_t>(
            fx::AllocateWeatherParticles(host, static_cast<WeatherParticlesId>(current)));
        break;
    case kRoomWeather:
        result = fx::InitializeRoomWeather(host,
                     room != 0 ? std::optional<std::int16_t>(room_kind) : std::nullopt,
                     arguments[0])
                   ? 1
                   : 0;
        break;
    case kWeatherState:
        report = fx::RoomWeatherStateOf(state.kind, intensity);
        break;
    case kFalling:
        (void)fx::InitializeFallingWeatherParticles(
            host, state, particles, area, arguments[0], arguments[1]);
        break;
    case kScreen:
        (void)fx::InitializeScreenWeatherParticles(
            host, state, particles, arguments[0], arguments[1]);
        break;
    case kReset:
        if (index >= particles.size())
            return 1;
        particles[index] = fx::ResetFallingWeatherParticle(host, particles[index], area);
        break;
    case kVelocity: {
        const fx::WeatherVelocity velocity =
            fx::WeatherVelocityOf(host, { .elevation = parameters[4],
                                            .azimuth = parameters[5],
                                            .scale = parameters[6] });
        parameters[0] = velocity.x;
        parameters[1] = velocity.y;
        parameters[2] = velocity.z;
        break;
    }
    default:
        return 1;
    }
    for (std::size_t slot = 0; slot < particles.size(); ++slot) {
        StoreParticle(
            std::span<std::uint8_t>(buffer).subspan(slot * 16, 16), particles[slot]);
    }

    std::cout << result << ' ' << current << ' ' << state.particle_count << ' '
              << state.particle_capacity << ' ' << intensity << ' '
              << int { state.screen_fade } << ' ' << int { state.kind } << ' '
              << report.kind << ' ' << report.intensity << ' ' << parameters[0] << ' '
              << parameters[1] << ' ' << parameters[2] << ' ' << host.used;
    for (std::size_t word = 0; word < kBufferWords; ++word) {
        std::uint32_t bits = 0;
        for (std::size_t byte = 0; byte < 4; ++byte) {
            bits |= std::uint32_t { buffer[word * 4 + byte] } << (8 * byte);
        }
        std::cout << ' ' << bits;
    }
    std::cout << ' ' << host.calls.size();
    for (const auto& call : host.calls) {
        std::cout << ' ' << call[0] << ' ' << call[1];
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
