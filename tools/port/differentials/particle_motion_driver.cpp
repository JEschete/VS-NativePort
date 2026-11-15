// Differential driver for the particle motion and texture animation (particle_motion_differential.py).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Effects/EffectResources.hpp"
#include "Effects/ParticleMotion.hpp"
#include "Effects/ParticleTextureAnimation.hpp"

// The cores see only incomplete records; the driver numbers them.
struct Effects_Particle {
    int index;
};

namespace {
namespace fx = VS::Effects;
using VS::Core::Math::Vec3;

enum Mode : int {
    kStepPoint,
    kSpawn,
    kTick,
    kIntegrate,
    kClear,
    kInit,
    kAdvance,
};

constexpr int kNull = -1;
constexpr int kRecords = 8;
constexpr std::size_t kWordBytes = 4;
constexpr unsigned kByteBits = 8;

enum Call : int { kRotateCall = 0 };
enum TickCall : int { kIntegrateCall, kDrawCall, kEmitCall, kReleaseCall };

std::array<Effects_Particle, kRecords> particles { { { 0 }, { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 6 }, { 7 } } };

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

template <typename Range> bool ReadAll(Range& range)
{
    for (auto& value : range) {
        if (!Read(value))
            return false;
    }
    return true;
}

template <typename Range> void PrintAll(const Range& range)
{
    for (const auto value : range)
        std::cout << ' ' << static_cast<long long>(value);
}

Effects_Particle* ParticleAt(int ref) { return ref == kNull ? nullptr : &particles.at(ref); }
int RefOf(const Effects_Particle* particle) { return particle == nullptr ? kNull : particle->index; }

struct Recorded {
    std::vector<std::array<long long, 5>> calls;

    void Add(int kind, long long first, long long second = 0, long long third = 0)
    {
        calls.push_back({ kind, first, second, third, 0 });
    }
    void Print() const
    {
        std::cout << ' ' << calls.size();
        for (const auto& call : calls)
            PrintAll(call);
        std::cout << '\n';
    }
};

bool ReadVec(Vec3& vector) { return Read(vector.x) && Read(vector.y) && Read(vector.z); }
void PrintVec(Vec3 vector) { std::cout << ' ' << vector.x << ' ' << vector.y << ' ' << vector.z; }

bool StepPoint()
{
    std::uint32_t point = 0;
    std::uint32_t target = 0;
    if (!Read(point) || !Read(target))
        return false;
    std::cout << fx::StepScreenPointToward(point, target);
    Recorded().Print();
    return true;
}

// The stand-in rotation particle_motion_differential.py fake_rotation applies.
class FakeRotation final : public fx::ParticleRotation {
public:
    explicit FakeRotation(Recorded& recorded) : recorded_(recorded) { }

    Vec3 Rotate(fx::ParticleOffset offset) override
    {
        recorded_.Add(kRotateCall, 1, 1);
        const auto wrap = [](std::int64_t value) { return static_cast<std::int32_t>(value); };
        return { .x = wrap((std::int64_t { offset.x } * 3) - offset.y),
            .y = wrap((std::int64_t { offset.y } * 2) + (std::int64_t { offset.z } * 5)),
            .z = wrap(offset.z - (std::int64_t { offset.x } * 7)) };
    }

private:
    Recorded& recorded_;
};

bool Spawn()
{
    std::uint32_t flags = 0;
    std::array<std::int16_t, 3> offset { };
    Vec3 origin { };
    if (!Read(flags) || !ReadAll(offset) || !ReadVec(origin))
        return false;
    Recorded recorded;
    FakeRotation rotation(recorded);
    const fx::ParticleSpawn spawn
        = fx::PlaceParticleSpawn(flags, { .x = offset[0], .y = offset[1], .z = offset[2] }, origin, rotation);
    PrintVec(spawn.aim);
    PrintVec(spawn.position);
    recorded.Print();
    return true;
}

// One record's lifecycle and the rewrite an emit for it makes.
struct TickRecord {
    fx::ParticleLifecycle lifecycle;
    bool poke = false;
    std::int16_t poke_lifetime = 0;
    std::uint8_t poke_end = 0;
    std::uint8_t poke_timer = 0;
};

class TickHost final : public fx::ParticleTickHost {
public:
    TickHost(Recorded& recorded, Effects_Particle* first) : recorded_(recorded), first_(first) { }

    std::array<int, kRecords> next { };
    std::array<TickRecord, kRecords> records { };

    void Integrate(Effects_Particle* first) override { recorded_.Add(kIntegrateCall, RefOf(first)); }
    // The Live host draws the instance's list, whose first particle the original passed.
    void Draw() override { recorded_.Add(kDrawCall, RefOf(first_)); }
    Effects_Particle* Next(Effects_Particle& particle) override { return ParticleAt(next.at(particle.index)); }
    fx::ParticleLifecycle Lifecycle(Effects_Particle& particle) override
    {
        return records.at(particle.index).lifecycle;
    }
    void StoreAge(Effects_Particle& particle, std::uint8_t age) override
    {
        records.at(particle.index).lifecycle.age = age;
    }
    void StoreLifetime(Effects_Particle& particle, std::int16_t lifetime) override
    {
        records.at(particle.index).lifecycle.lifetime = lifetime;
    }
    void Emit(Effects_Particle& particle, std::int32_t event) override
    {
        recorded_.Add(kEmitCall, event, particle.index, 1);
        TickRecord& record = records.at(particle.index);
        if (record.poke) {
            record.lifecycle.lifetime = record.poke_lifetime;
            record.lifecycle.end_event = record.poke_end;
            record.lifecycle.texture_timer = record.poke_timer;
        }
    }
    void Release(Effects_Particle& particle) override { recorded_.Add(kReleaseCall, particle.index, 1); }

private:
    Recorded& recorded_;
    Effects_Particle* first_;
};

bool ReadTickRecord(TickRecord& record)
{
    fx::ParticleLifecycle& state = record.lifecycle;
    return Read(state.age) && Read(state.lifetime) && Read(state.end_event) && Read(state.tick_event)
        && Read(state.texture_timer) && Read(record.poke) && Read(record.poke_lifetime) && Read(record.poke_end)
        && Read(record.poke_timer);
}

bool Tick()
{
    int first = 0;
    Recorded recorded;
    if (!Read(first))
        return false;
    TickHost host(recorded, ParticleAt(first));
    if (!ReadAll(host.next))
        return false;
    for (TickRecord& record : host.records) {
        if (!ReadTickRecord(record))
            return false;
    }
    fx::TickEffectParticles(ParticleAt(first), host);
    // LiveTickEffectParticles returns 1, as the original did.
    std::cout << 1;
    for (const TickRecord& record : host.records)
        std::cout << ' ' << static_cast<int>(record.lifecycle.age) << ' ' << record.lifecycle.lifetime;
    recorded.Print();
    return true;
}

class MotionRecords final : public fx::ParticleMotionRecords {
public:
    std::array<int, kRecords> next { };
    std::array<fx::ParticleMotionState, kRecords> motion { };

    Effects_Particle* Next(Effects_Particle& particle) override { return ParticleAt(next.at(particle.index)); }
    fx::ParticleMotionState Motion(Effects_Particle& particle) override { return motion.at(particle.index); }
    void StoreMotion(Effects_Particle& particle, const fx::ParticleMotionState& state) override
    {
        // As LiveMotionRecords: the step changes only the position, velocity and force.
        fx::ParticleMotionState& stored = motion.at(particle.index);
        stored.position = state.position;
        stored.velocity = state.velocity;
        stored.force = state.force;
    }
};

bool ReadMotion(fx::ParticleMotionState& state)
{
    return Read(state.mass) && Read(state.gravity_scale) && ReadVec(state.position) && ReadVec(state.velocity)
        && ReadVec(state.force) && ReadVec(state.acceleration) && Read(state.target.x) && Read(state.target.y)
        && Read(state.target.z) && Read(state.attraction);
}

bool Integrate()
{
    fx::ParticleForces forces { };
    int first = 0;
    MotionRecords records;
    if (!Read(forces.drag) || !ReadVec(forces.gravity) || !Read(first) || !ReadAll(records.next))
        return false;
    for (fx::ParticleMotionState& state : records.motion) {
        if (!ReadMotion(state))
            return false;
    }
    fx::IntegrateParticleMotion(ParticleAt(first), records, forces);
    for (const fx::ParticleMotionState& state : records.motion) {
        PrintVec(state.position);
        PrintVec(state.velocity);
        PrintVec(state.force);
    }
    Recorded().Print();
    return true;
}

bool ReadAnimation(fx::TextureAnimationState& state)
{
    return Read(state.frame_timer) && Read(state.frame_flags) && Read(state.frame_offset_x)
        && Read(state.frame_offset_y) && Read(state.cursor) && Read(state.texture_index);
}

void PrintAnimation(const fx::TextureAnimationState& state)
{
    std::cout << static_cast<int>(state.frame_timer) << ' ' << static_cast<int>(state.frame_flags) << ' '
              << state.frame_offset_x << ' ' << state.frame_offset_y << ' ' << state.cursor << ' '
              << static_cast<int>(state.texture_index);
}

std::vector<std::byte> WordBytes(const std::vector<std::uint32_t>& words)
{
    std::vector<std::byte> bytes;
    for (const std::uint32_t word : words) {
        for (std::size_t index = 0; index < kWordBytes; ++index)
            bytes.push_back(static_cast<std::byte>(word >> (index * kByteBits)));
    }
    return bytes;
}

bool Clear()
{
    fx::TextureAnimationState state;
    if (!ReadAnimation(state))
        return false;
    fx::ClearTextureAnimation(state);
    PrintAnimation(state);
    Recorded().Print();
    return true;
}

// As LiveInitializeParticleTextureAnimation: the texture index is narrowed to the record's byte
// and the program is the animation table entry ParticleAnimationData selects.
bool Init()
{
    fx::TextureAnimationState state;
    int texture = 0;
    std::int32_t index = 0;
    std::size_t size = 0;
    std::uint32_t count = 0;
    if (!ReadAnimation(state) || !Read(texture) || !Read(index) || !Read(size) || !Read(count))
        return false;
    std::vector<std::byte> table(size);
    const std::vector<std::byte> header = WordBytes({ count });
    std::copy(header.begin(), header.end(), table.begin());
    for (std::uint32_t entry = 0; entry < count; ++entry) {
        std::uint16_t offset = 0;
        if (!Read(offset))
            return false;
        table.at(kWordBytes + (2 * entry)) = static_cast<std::byte>(offset);
        table.at(kWordBytes + (2 * entry) + 1) = static_cast<std::byte>(offset >> kByteBits);
    }
    fx::RestartTextureAnimation(state, static_cast<std::uint8_t>(texture));
    fx::EffectPackageBlocks blocks { };
    fx::BindParticleAnimationTable(blocks, table);
    const std::optional<std::span<std::byte>> program = fx::ParticleAnimationData(blocks, index);
    PrintAnimation(state);
    std::cout << ' ' << (program ? program->data() - table.data() : -1);
    Recorded().Print();
    return true;
}

bool Advance()
{
    fx::TextureAnimationState state;
    std::size_t length = 0;
    if (!ReadAnimation(state) || !Read(length))
        return false;
    std::vector<std::uint32_t> words(length);
    if (!ReadAll(words))
        return false;
    const std::vector<std::byte> program = WordBytes(words);
    const fx::TextureAnimationAdvance advance = fx::AdvanceTextureAnimation(state, program);
    PrintAnimation(state);
    std::cout << ' ' << (advance.step == fx::TextureAnimationStep::kFrame ? advance.frame : kNull);
    Recorded().Print();
    return true;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int mode = 0;
    while (Read(mode)) {
        bool read = true;
        switch (mode) {
        case kStepPoint:
            read = StepPoint();
            break;
        case kSpawn:
            read = Spawn();
            break;
        case kTick:
            read = Tick();
            break;
        case kIntegrate:
            read = Integrate();
            break;
        case kClear:
            read = Clear();
            break;
        case kInit:
            read = Init();
            break;
        case kAdvance:
            read = Advance();
            break;
        default:
            return 1;
        }
        if (!read)
            return 1;
    }
    return 0;
}
