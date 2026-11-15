// Differential driver for the particle pool and forces (particle_pool_differential.py).
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Effects/ParticlePool.hpp"

// The core sees only incomplete records; the driver numbers them.
struct Effects_Particle {
    int index;
};

namespace {
namespace fx = VS::Effects;

enum Mode : int {
    kReset,
    kAcquire,
    kRelease,
    kAllocate,
    kReleaseParticle,
    kReleaseAll,
    kGravity,
    kDrag,
};

constexpr int kNull = -1;
constexpr int kRecords = 8;
constexpr std::int32_t kParticleBytes = 0x78;
constexpr int kUnaged = 0xFF;

enum Call : int { kAllocateCall, kFreeCall, kTextureCall };

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

    void Add(int kind, long long first) { calls.push_back({ kind, first, 0, 0, 0 }); }
    void Print() const
    {
        std::cout << ' ' << calls.size();
        for (const auto& call : calls)
            PrintAll(call);
        std::cout << '\n';
    }
};

class DriverRecords final : public fx::ParticleRecords {
public:
    explicit DriverRecords(Recorded& recorded) : recorded_(recorded) { }

    std::array<int, kRecords> previous { };
    std::array<int, kRecords> next { };
    std::array<int, kRecords> age { };

    Effects_Particle* Reserve(std::int32_t count) override
    {
        recorded_.Add(kAllocateCall, static_cast<long long>(count) * kParticleBytes);
        return particles.data();
    }
    void Free(Effects_Particle& block) override { recorded_.Add(kFreeCall, block.index); }
    Effects_Particle& At(Effects_Particle& block, std::int32_t index) override
    {
        return particles.at(static_cast<std::size_t>(block.index + index));
    }
    Effects_Particle* Previous(Effects_Particle& particle) override { return ParticleAt(previous.at(particle.index)); }
    void SetPrevious(Effects_Particle& particle, Effects_Particle* link) override
    {
        previous.at(particle.index) = RefOf(link);
    }
    Effects_Particle* Next(Effects_Particle& particle) override { return ParticleAt(next.at(particle.index)); }
    void SetNext(Effects_Particle& particle, Effects_Particle* link) override { next.at(particle.index) = RefOf(link); }
    void Renew(Effects_Particle& particle) override
    {
        recorded_.Add(kTextureCall, particle.index);
        age.at(particle.index) = kUnaged;
    }

    void PrintLinks() const
    {
        PrintAll(previous);
        PrintAll(next);
    }

private:
    Recorded& recorded_;
};

bool ReadPool(fx::ParticlePool& pool, DriverRecords& records)
{
    int block = 0;
    int free = 0;
    if (!Read(pool.references) || !Read(block) || !Read(free) || !Read(pool.capacity) || !Read(pool.used)
        || !ReadAll(records.previous) || !ReadAll(records.next))
        return false;
    pool.block = ParticleAt(block);
    pool.free = ParticleAt(free);
    return true;
}

void PrintPool(const fx::ParticlePool& pool)
{
    std::cout << ' ' << pool.references << ' ' << RefOf(pool.block) << ' ' << RefOf(pool.free) << ' ' << pool.capacity
              << ' ' << pool.used;
}

bool Pool(int mode)
{
    Recorded recorded;
    DriverRecords records(recorded);
    fx::ParticlePool pool;
    if (!ReadPool(pool, records))
        return false;
    if (mode == kReset) {
        fx::ResetParticlePoolReferences(pool);
    } else if (mode == kAcquire) {
        std::int32_t count = 0;
        if (!Read(count))
            return false;
        static_cast<void>(fx::AcquireParticlePool(pool, count, records));
    } else {
        static_cast<void>(fx::ReleaseParticlePool(pool, records));
    }
    PrintPool(pool);
    records.PrintLinks();
    recorded.Print();
    return true;
}

bool List(int mode)
{
    Recorded recorded;
    DriverRecords records(recorded);
    fx::ParticlePool pool;
    int first = 0;
    int count = 0;
    if (!ReadPool(pool, records) || !Read(first) || !Read(count) || !ReadAll(records.age))
        return false;
    fx::ParticleList list { .first = ParticleAt(first), .count = static_cast<std::uint8_t>(count) };
    if (mode == kAllocate) {
        std::cout << RefOf(fx::AllocateEffectParticle(pool, list, records));
    } else if (mode == kReleaseParticle) {
        int target = 0;
        if (!Read(target))
            return false;
        fx::ReleaseEffectParticle(particles.at(target), pool, list, records);
    } else {
        fx::ReleaseAllEffectParticles(pool, list, records);
    }
    PrintPool(pool);
    std::cout << ' ' << RefOf(list.first) << ' ' << static_cast<int>(list.count);
    records.PrintLinks();
    PrintAll(records.age);
    recorded.Print();
    return true;
}

// The gravity record: the core sets x, y and z and ParticlePoolLive.cpp copies the fourth word.
bool Gravity()
{
    std::array<std::int32_t, 4> gravity { };
    std::array<std::int32_t, 5> old { };
    if (!ReadAll(gravity) || !ReadAll(old))
        return false;
    fx::ParticleForces forces { .gravity = { .x = old[0], .y = old[1], .z = old[2] }, .drag = old[4] };
    fx::SetParticleGravity(forces, { .x = gravity[0], .y = gravity[1], .z = gravity[2] });
    std::cout << forces.gravity.x << ' ' << forces.gravity.y << ' ' << forces.gravity.z << ' ' << gravity[3] << " 0\n";
    return true;
}

bool Drag()
{
    std::array<std::int32_t, 2> drag { };
    std::int32_t old = 0;
    if (!ReadAll(drag) || !Read(old))
        return false;
    fx::ParticleForces forces { .drag = old };
    fx::SetParticleDrag(forces, drag[0]);
    std::cout << forces.drag << " 0\n";
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
        case kReset:
        case kAcquire:
        case kRelease:
            read = Pool(mode);
            break;
        case kAllocate:
        case kReleaseParticle:
        case kReleaseAll:
            read = List(mode);
            break;
        case kGravity:
            read = Gravity();
            break;
        case kDrag:
            read = Drag();
            break;
        default:
            return 1;
        }
        if (!read)
            return 1;
    }
    return 0;
}
