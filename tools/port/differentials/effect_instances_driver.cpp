// Differential driver for the effect instance lifecycle (effect_instances_differential.py).
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#include "Effects/EffectInstances.hpp"

// The core sees only incomplete records; the driver numbers them.
struct Effects_EffectInstance {
    int index;
};
struct Effects_RendererSlot {
    int index;
};

namespace {
namespace fx = VS::Effects;

enum Mode : int {
    kInit,
    kCreatePrimary,
    kCreateSecondary,
    kDestroyPrimary,
    kDestroySecondary,
    kAttach,
    kDetach,
    kPhase,
};

constexpr int kNull = -1;
constexpr int kSlotRef = 100;
constexpr int kStateRef = 200;
constexpr int kOther = 999;
constexpr int kInstances = 7;
constexpr int kSlots = 6;
constexpr int kOutside = 6;
constexpr int kFresh = 5;
constexpr std::uintptr_t kRenderers = 0x80520000;
constexpr std::uintptr_t kRendererStride = 0x40;
constexpr std::size_t kRecordBytes = 0xD5C;
constexpr std::size_t kTransforms = 0x1C;
constexpr std::size_t kTransformBytes = 0xD0;
constexpr std::size_t kContext = 0xD1C;

enum Call : int { kParticles = 1, kRenderer = 2, kFree = 3, kToken = 4 };

std::array<Effects_EffectInstance, kInstances> instances { { { 0 }, { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 6 } } };
std::array<Effects_RendererSlot, kSlots> slots { { { 0 }, { 1 }, { 2 }, { 3 }, { 4 }, { 5 } } };

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

Effects_EffectInstance* InstanceAt(int ref) { return ref == kNull ? nullptr : &instances.at(ref); }
int RefOf(const Effects_EffectInstance* instance) { return instance == nullptr ? kNull : instance->index; }
Effects_RendererSlot* SlotAt(int ref) { return ref == kNull ? nullptr : &slots.at(ref - kSlotRef); }
int RefOf(const Effects_RendererSlot* slot) { return slot == nullptr ? kNull : kSlotRef + slot->index; }

struct Recorded {
    std::vector<std::array<long long, 5>> calls;

    void Add(int kind, long long first, long long second = 0, long long third = 0, long long fourth = 0)
    {
        calls.push_back({ kind, first, second, third, fourth });
    }
    void Print() const
    {
        std::cout << ' ' << calls.size();
        for (const auto& call : calls)
            PrintAll(call);
        std::cout << '\n';
    }
};

class DriverRenderers final : public fx::RendererChain {
public:
    explicit DriverRenderers(Recorded& recorded) : recorded_(recorded) { }

    int first = kNull;
    std::array<int, kSlots> next { };
    std::array<int, kSlots> state { };
    std::array<int, kSlots> renderer { 0, 1, 2, 3, 4, 5 };
    int allocations = 0;

    Effects_RendererSlot* First() override { return SlotAt(first); }
    void SetFirst(Effects_RendererSlot* slot) override { first = RefOf(slot); }
    Effects_RendererSlot* Next(Effects_RendererSlot& slot) override { return SlotAt(next.at(slot.index)); }
    void SetNext(Effects_RendererSlot& slot, Effects_RendererSlot* link) override
    {
        next.at(slot.index) = RefOf(link);
    }
    Effects_RendererSlot* NewSlot(fx::EffectRenderer created) override
    {
        ++allocations;
        const auto address = reinterpret_cast<std::uintptr_t>(created);
        const bool known = address >= kRenderers && (address - kRenderers) % kRendererStride == 0
            && (address - kRenderers) / kRendererStride < kSlots;
        renderer.at(kFresh) = known ? static_cast<int>((address - kRenderers) / kRendererStride) : kOther;
        state.at(kFresh) = 0;
        return &slots.at(kFresh);
    }
    void Run(Effects_RendererSlot& slot, fx::RendererCommand command, std::int32_t parameters) override
    {
        recorded_.Add(kRenderer, RefOf(&slot), renderer.at(slot.index), std::to_underlying(command), parameters);
    }
    void Free(Effects_RendererSlot& slot) override
    {
        if (state.at(slot.index) != 0)
            recorded_.Add(kFree, kStateRef + slot.index);
        recorded_.Add(kFree, RefOf(&slot));
    }

private:
    Recorded& recorded_;
};

class DriverInstances final : public fx::EffectInstanceChain {
public:
    DriverInstances(Recorded& recorded, DriverRenderers& renderers) : recorded_(recorded), renderers_(renderers) { }

    int first = kNull;
    std::array<int, kInstances> next { };
    std::array<std::uint8_t, kInstances> priority { };
    std::array<int, kInstances> token { };
    int starts = 0;

    Effects_EffectInstance* First() override { return InstanceAt(first); }
    void SetFirst(Effects_EffectInstance* instance) override { first = RefOf(instance); }
    Effects_EffectInstance* Next(Effects_EffectInstance& instance) override
    {
        return InstanceAt(next.at(instance.index));
    }
    void SetNext(Effects_EffectInstance& instance, Effects_EffectInstance* link) override
    {
        next.at(instance.index) = RefOf(link);
    }
    std::uint8_t PriorityOf(Effects_EffectInstance& instance) override { return priority.at(instance.index); }
    Effects_EffectInstance* StartInstance() override
    {
        ++starts;
        return &instances.at(kOutside);
    }
    void ReleaseParticles(Effects_EffectInstance& instance) override { recorded_.Add(kParticles, instance.index); }
    fx::RendererChain& Renderers(Effects_EffectInstance&) override { return renderers_; }
    void ReleaseToken(Effects_EffectInstance& instance) override
    {
        recorded_.Add(kToken, token.at(instance.index));
    }
    void Free(Effects_EffectInstance& instance) override { recorded_.Add(kFree, instance.index); }

private:
    Recorded& recorded_;
    DriverRenderers& renderers_;
};

// The PS1 instance record, written at the offsets of the fields EffectInstancesLive.cpp writes.
class Record final : public fx::EffectInstanceStart {
public:
    std::array<std::uint32_t, kRecordBytes / 4> words { };
    std::uint32_t parent = 0;
    std::uint32_t script = 0;
    std::array<std::uint32_t, 12> endpoints { };

    void Begin(const fx::EffectInstanceIdentity& identity) override
    {
        Put(0x04, parent, 4);
        Put(0x08, 0, 1);
        Put(0x09, identity.priority, 1);
        Put(0x0A, identity.script_offset, 2);
        Put(0x0C, script, 4);
        Put(0x10, 0, 4);
        Put(0x14, identity.token, 1);
        words.at(0x14 / 4) &= ~(0x1Fu << 11);
        Put(0x16, 0, 2);
        Put(0x18, 0, 4);
    }
    void ClearTransform(std::size_t transform) override
    {
        const std::size_t base = kTransforms + transform * kTransformBytes;
        for (const std::size_t offset : { 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x28, 0x2C, 0x30, 0x34 })
            Put(base + offset, 0, 4);
    }
    void TakeEndpoints() override
    {
        for (std::size_t word = 0; word < endpoints.size(); ++word)
            Put(kContext + word * 4, endpoints.at(word), 4);
        Put(0xD4C, 0, 4);
        Put(0xD50, 0, 4);
        Put(0xD58, 0, 4);
    }

private:
    void Put(std::size_t offset, std::uint32_t value, std::size_t size)
    {
        for (std::size_t byte = 0; byte < size; ++byte) {
            const std::size_t at = offset + byte;
            const unsigned shift = static_cast<unsigned>(at % 4) * 8;
            std::uint32_t& word = words.at(at / 4);
            word = (word & ~(0xFFu << shift)) | (((value >> (byte * 8)) & 0xFFu) << shift);
        }
    }
};

bool Init()
{
    fx::EffectSpawn spawn;
    int token = 0;
    Record record;
    if (!Read(spawn.priority) || !Read(spawn.script_offset) || !Read(token) || !Read(record.parent)
        || !Read(record.script) || !ReadAll(record.endpoints) || !ReadAll(record.words))
        return false;
    spawn.token = static_cast<std::uint8_t>(token);
    fx::InitEffectInstance(spawn, record);
    PrintAll(record.words);
    std::cout << " 0\n";
    return true;
}

bool ReadInstances(DriverInstances& chain)
{
    std::array<int, kInstances> priority { };
    if (!Read(chain.first) || !ReadAll(chain.next) || !ReadAll(priority))
        return false;
    for (std::size_t index = 0; index < priority.size(); ++index)
        chain.priority.at(index) = static_cast<std::uint8_t>(priority.at(index));
    return true;
}

bool Create(int mode)
{
    Recorded recorded;
    DriverRenderers renderers(recorded);
    DriverInstances chain(recorded, renderers);
    std::int32_t priority = 0;
    if (!ReadInstances(chain) || !Read(priority))
        return false;
    Effects_EffectInstance* created = mode == kCreatePrimary ? fx::CreatePrimaryEffectInstance(priority, chain)
                                                             : fx::CreateSecondaryEffectInstance(priority, chain);
    std::cout << RefOf(created) << ' ' << chain.first;
    PrintAll(chain.next);
    std::cout << ' ' << (chain.starts == 1 ? 1 : 0);
    recorded.Print();
    return true;
}

bool ReadRenderers(DriverRenderers& renderers, bool states)
{
    if (!Read(renderers.first) || !ReadAll(renderers.next))
        return false;
    return !states || ReadAll(renderers.state);
}

bool Destroy(int mode)
{
    Recorded recorded;
    DriverRenderers renderers(recorded);
    DriverInstances chain(recorded, renderers);
    int target = 0;
    if (!Read(chain.first) || !ReadAll(chain.next) || !ReadAll(chain.token) || !Read(target)
        || !ReadRenderers(renderers, true))
        return false;
    if (mode == kDestroyPrimary)
        fx::DestroyPrimaryEffectInstance(instances.at(target), chain);
    else
        fx::DestroySecondaryEffectInstance(instances.at(target), chain);
    std::cout << chain.first;
    PrintAll(chain.next);
    std::cout << ' ' << renderers.first;
    PrintAll(renderers.next);
    recorded.Print();
    return true;
}

bool Attach()
{
    Recorded recorded;
    DriverRenderers renderers(recorded);
    int renderer = 0;
    std::int32_t parameters = 0;
    if (!ReadRenderers(renderers, false) || !Read(renderer) || !Read(parameters))
        return false;
    renderers.renderer.at(kFresh) = kOther;
    const auto address = kRenderers + static_cast<std::uintptr_t>(renderer) * kRendererStride;
    Effects_RendererSlot* attached
        = fx::AttachEffectRenderer(renderers, reinterpret_cast<fx::EffectRenderer>(address), parameters);
    std::cout << RefOf(attached) << ' ' << renderers.first;
    PrintAll(renderers.next);
    std::cout << ' ' << renderers.renderer.at(kFresh) << ' ' << renderers.state.at(kFresh) << ' '
              << (renderers.allocations == 1 ? 1 : 0);
    recorded.Print();
    return true;
}

bool Detach()
{
    Recorded recorded;
    DriverRenderers renderers(recorded);
    int target = 0;
    if (!ReadRenderers(renderers, true) || !Read(target))
        return false;
    const bool detached = fx::DetachEffectRenderer(renderers, *SlotAt(target));
    std::cout << (detached ? 0 : 1) << ' ' << renderers.first;
    PrintAll(renderers.next);
    recorded.Print();
    return true;
}

bool Phase()
{
    int phase = 0;
    std::uint32_t effect = 0;
    std::uint32_t old = 0;
    std::uint32_t parameter = 0;
    if (!Read(phase) || !Read(effect) || !Read(old) || !Read(parameter))
        return false;
    auto context_phase = static_cast<std::int16_t>(old);
    const std::optional<fx::SecondaryEffectPhase> known = fx::ToSecondaryEffectPhase(phase);
    if (known)
        fx::SetSecondaryEffectPhase(context_phase, *known);
    std::cout << effect << ' ' << static_cast<std::uint16_t>(context_phase) << ' ' << parameter << " 0\n";
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
        case kInit:
            read = Init();
            break;
        case kCreatePrimary:
        case kCreateSecondary:
            read = Create(mode);
            break;
        case kDestroyPrimary:
        case kDestroySecondary:
            read = Destroy(mode);
            break;
        case kAttach:
            read = Attach();
            break;
        case kDetach:
            read = Detach();
            break;
        case kPhase:
            read = Phase();
            break;
        default:
            return 1;
        }
        if (!read)
            return 1;
    }
    return 0;
}
