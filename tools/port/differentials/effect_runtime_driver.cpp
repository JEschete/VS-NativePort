// Differential driver for the effect runtime (effect_runtime_differential.py).
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#include "Effects/EffectRuntime.hpp"

// The core sees only incomplete records; the driver numbers them.
struct Effects_EffectInstance {
    int index;
};
struct Effects_RendererSlot {
    int index;
};
struct Content_DiscQueueSlot {
    int index;
};

namespace {
namespace fx = VS::Effects;

enum Mode : int {
    kInit,
    kReset,
    kUpdatePrimary,
    kUpdateSecondary,
    kStartTrack,
    kUpdateRuntime,
    kBeginAction,
    kClearPrimary,
    kSpawnSecondary,
    kBeginSecondary,
    kSpawnRequest,
    kSourceActor,
    kClearMode,
};

enum Call : int {
    kSelect = 1,
    kResetCall,
    kResources,
    kPoolReferences,
    kPoll,
    kWait,
    kConfigure,
    kWorkspace,
    kBind,
    kMenuSector,
    kAllocateDisc,
    kEnqueue,
    kSample,
    kParticles,
    kScript,
    kDestroyPrimary,
    kUnbindSfx,
    kUnbindStream,
    kVibration,
    kReleasePackage,
    kReleasePool,
    kReleaseToken,
    kResetCamera,
    kReleaseDisc,
    kAcquirePool,
    kRenderer,
    kDetach,
    kDestroySecondary,
    kCreate,
    kUpdatePrimaryCall,
    kUpdateSecondaryCall,
    kCameraMotion,
    kAllocateToken,
    kCapture,
    kFree,
    kSecondary,
    kSecondaryScript,
    kSource,
    kSourceEndpoint,
    kSourceTarget,
};

constexpr int kNull = -1;
constexpr int kSlotRef = 100;
constexpr int kDiscRef = 300;
constexpr int kInstances = 7;
constexpr int kSlotsPerInstance = 3;
constexpr int kSlots = kInstances * kSlotsPerInstance;
constexpr int kPolls = 4;
constexpr int kLoadedDisc = 4;
constexpr int kUpdateCommand = 2;
constexpr int kAllPatterns = 0;
constexpr int kBattleOwner = 1;
constexpr int kPurgeOwner = 2;
constexpr int kSecondaryParticles = 0xFF;
constexpr int kTargets = 16;
constexpr std::uint32_t kBusyMask = 0x700U;
constexpr unsigned kBusyShift = 8;
// LiveSpawnSourceActorEffect begins its request with parameter 0.
constexpr int kSourceActorParameter = 0;
constexpr unsigned kByteShift = 8;

std::array<Effects_EffectInstance, kInstances> instances { { { 0 }, { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 6 } } };
std::array<Effects_RendererSlot, kSlots> slots { };
Content_DiscQueueSlot disc { kDiscRef };

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
Content_DiscQueueSlot* DiscAt(int ref) { return ref == kDiscRef ? &disc : nullptr; }
int RefOf(const Content_DiscQueueSlot* slot) { return slot == nullptr ? kNull : slot->index; }

struct Recorded {
    std::vector<std::array<long long, 4>> calls;

    void Add(int kind, long long first = 0, long long second = 0, long long third = 0)
    {
        calls.push_back({ kind, first, second, third });
    }
    void Print() const
    {
        std::cout << ' ' << calls.size();
        for (const auto& call : calls)
            PrintAll(call);
        std::cout << '\n';
    }
};

// An instance chain and the playback of its instances.
template <typename Host> class Playback : public Host {
public:
    explicit Playback(Recorded& recorded) : recorded_(recorded) { }

    int first = kNull;
    std::array<int, kInstances> next { };
    int current = kNull;
    int curves = 0;
    std::array<std::uint16_t, kInstances> frames { };
    std::array<int, kInstances> scripts { };

    bool ReadChain()
    {
        return Read(first) && ReadAll(next);
    }
    void Unlink(Effects_EffectInstance& instance)
    {
        int* link = &first;
        while (*link != kNull && *link != instance.index)
            link = &next.at(*link);
        if (*link == instance.index)
            *link = next.at(instance.index);
    }

    Effects_EffectInstance* Next(Effects_EffectInstance& instance) override
    {
        return InstanceAt(next.at(instance.index));
    }
    void SetCurrent(Effects_EffectInstance& instance) override { current = instance.index; }
    bool HasCurves() override { return curves != 0; }
    void SampleCurves(std::uint16_t frame) override { recorded_.Add(kSample, frame); }
    void UpdateParticleForces() override { recorded_.Add(kParticles); }
    bool RunScript(Effects_EffectInstance& instance) override
    {
        recorded_.Add(kScript, instance.index);
        return scripts.at(instance.index) != 0;
    }
    std::uint16_t Frame(Effects_EffectInstance& instance) override { return frames.at(instance.index); }
    void SetFrame(Effects_EffectInstance& instance, std::uint16_t frame) override
    {
        frames.at(instance.index) = frame;
    }
    void ReleaseParticlePool() override { recorded_.Add(kReleasePool); }
    void SelectPackage(fx::ActivePackage package) override { recorded_.Add(kSelect, std::to_underlying(package)); }

protected:
    Recorded& recorded_;
};

class PrimaryHost final : public Playback<fx::PrimaryEffectRuntime> {
public:
    using Playback::Playback;

    fx::PrimaryEffectState state;
    std::uint8_t token = 0;
    int menu = kNull;
    int disc_state = 0;
    std::uint32_t purge = 0;
    int effect = 0;
    int lba = 0;
    std::uint8_t tracks = 0;
    std::array<std::uint32_t, kPolls> polls { };
    std::uint32_t sfx = 0;
    std::uint32_t stream = 0;
    std::uint32_t aux = 0;
    std::array<int, kInstances> timelines { };
    std::optional<int> moved;
    std::uint8_t allocated = 0;
    int stored = 0;
    std::uint8_t variant = 0;
    std::uint8_t argument = 0;
    // The update's destruction stub unlinks; the clear's only records (stale links may cycle).
    bool unlink = true;

    fx::PrimaryEffectState Primary() override { return state; }
    void SetPrimary(const fx::PrimaryEffectState& value) override { state = value; }
    std::uint8_t PrimaryToken() override { return token; }
    void SetPrimaryToken(std::uint8_t value) override { token = value; }
    Effects_EffectInstance* FirstPrimary() override { return InstanceAt(first); }
    Content_DiscQueueSlot* MenuReloadSlot() override { return DiscAt(menu); }
    void SetMenuReloadSlot(Content_DiscQueueSlot* slot) override { menu = RefOf(slot); }

    bool PollPurgeOverlay() override { return Poll(0); }
    bool PollTextures() override { return Poll(1); }
    bool PollPlugins() override { return Poll(2); }
    bool PollPackage() override { return Poll(3); }
    void WaitForOverlay() override { recorded_.Add(kWait); }
    void ConfigureResources() override { recorded_.Add(kConfigure); }
    bool ActionHasEffect() override { return effect != 0; }
    bool PackageOnDisc() override { return lba != 0; }
    bool PurgeOverlayHeld() override { return purge != 0; }
    void ReleasePurgeOverlay() override
    {
        recorded_.Add(kWorkspace, kPurgeOwner);
        purge = 0;
    }
    void BindPackage() override { recorded_.Add(kBind, 1); }
    std::uint8_t PackageTrackCount() override { return tracks; }
    Content_DiscQueueSlot* RequestMenuReload() override
    {
        recorded_.Add(kMenuSector);
        recorded_.Add(kAllocateDisc, 1);
        return &disc;
    }
    void EnqueueMenuReload(Content_DiscQueueSlot& slot) override { recorded_.Add(kEnqueue, RefOf(&slot), 0); }
    bool MenuReloadLoaded(Content_DiscQueueSlot&) override { return disc_state == kLoadedDisc; }
    void ReleaseMenuReload(Content_DiscQueueSlot& slot) override { recorded_.Add(kReleaseDisc, RefOf(&slot)); }

    bool SpawnTrack(std::int32_t script_offset, std::uint8_t spawn_token) override
    {
        recorded_.Add(kCreate, script_offset, spawn_token, 1);
        return true;
    }
    void DestroyPrimary(Effects_EffectInstance& instance) override
    {
        recorded_.Add(kDestroyPrimary, instance.index);
        if (unlink)
            Unlink(instance);
    }

    bool HasSfxBank() override { return sfx != 0; }
    void UnbindSfxBank() override
    {
        recorded_.Add(kUnbindSfx);
        sfx = 0;
    }
    bool HasCutsceneStream() override { return stream != 0; }
    void UnbindCutsceneStream() override
    {
        recorded_.Add(kUnbindStream);
        stream = 0;
        aux = 0;
    }
    void StopVibration() override
    {
        recorded_.Add(kVibration, kAllPatterns, kBattleOwner);
        if (moved)
            first = *moved;
    }
    void ReleasePackageResources() override { recorded_.Add(kReleasePackage); }
    void ReleaseToken(std::uint8_t released) override { recorded_.Add(kReleaseToken, released); }
    void ResetCameraMotion() override { recorded_.Add(kResetCamera); }

    void StoreActionRequest() override { ++stored; }
    void SetActionVariant(std::uint8_t new_variant, std::uint8_t new_argument) override
    {
        variant = new_variant;
        argument = new_argument;
    }
    std::uint8_t AllocateToken() override
    {
        recorded_.Add(kAllocateToken);
        return allocated;
    }
    void CaptureSavedCamera() override { recorded_.Add(kCapture, 0); }
    void CaptureEffectCamera() override { recorded_.Add(kCapture, 1); }

    bool HasTimeline(Effects_EffectInstance& instance) override { return timelines.at(instance.index) != 0; }
    void FreeTimeline(Effects_EffectInstance& instance) override
    {
        recorded_.Add(kFree, instance.index);
        timelines.at(instance.index) = 0;
    }

    void PrintState() const
    {
        std::cout << ' ' << std::to_underlying(state.phase) << ' ' << state.track_count << ' ' << state.started_tracks
                  << ' ' << state.pending_tracks << ' ' << (state.loaded ? 1 : 0) << ' ' << static_cast<int>(token);
    }

private:
    bool Poll(int which)
    {
        recorded_.Add(kPoll, which);
        return polls.at(static_cast<std::size_t>(which)) == 0;
    }
};

class SecondaryHost final : public Playback<fx::SecondaryEffectRuntime> {
public:
    using Playback::Playback;

    std::array<std::int16_t, kInstances> phases { };
    std::array<int, kInstances> busy { };
    std::array<int, kInstances> held { };
    std::array<int, kSlots> updates { };

    Effects_EffectInstance* FirstSecondary() override { return InstanceAt(first); }
    std::int16_t PhaseOf(Effects_EffectInstance& instance) override { return phases.at(instance.index); }
    void SetPhase(Effects_EffectInstance& instance, fx::SecondaryEffectPhase phase) override
    {
        phases.at(instance.index) = std::to_underlying(phase);
    }
    void AcquireParticlePool() override { recorded_.Add(kAcquirePool, kSecondaryParticles); }
    Effects_RendererSlot* FirstRenderer(Effects_EffectInstance& instance) override
    {
        return held.at(instance.index) > 0 ? &slots.at(instance.index * kSlotsPerInstance) : nullptr;
    }
    Effects_RendererSlot* NextRenderer(Effects_RendererSlot& slot) override
    {
        const int owner = slot.index / kSlotsPerInstance;
        const int position = slot.index % kSlotsPerInstance;
        return position + 1 < held.at(owner) ? &slots.at(slot.index + 1) : nullptr;
    }
    bool UpdateRenderer(Effects_RendererSlot& slot) override
    {
        recorded_.Add(kRenderer, kSlotRef + slot.index, kUpdateCommand, 0);
        return updates.at(slot.index) != 0;
    }
    void DetachRenderer(Effects_EffectInstance& instance, Effects_RendererSlot& slot) override
    {
        recorded_.Add(kDetach, instance.index, kSlotRef + slot.index);
    }
    std::uint8_t BusyBits(Effects_EffectInstance& instance) override
    {
        return static_cast<std::uint8_t>(busy.at(instance.index));
    }
    void DestroySecondary(Effects_EffectInstance& instance) override
    {
        recorded_.Add(kDestroySecondary, instance.index);
    }
};

// The secondary request, token pool and instances a secondary spawn uses.
class SpawnerHost final : public fx::SecondaryEffectSpawner {
public:
    explicit SpawnerHost(Recorded& recorded) : recorded_(recorded) { }

    std::uint8_t token = 0;
    int stored = 0;
    std::array<std::uint16_t, kTargets> types { };
    std::array<int, kTargets> created { };
    std::array<std::uint32_t, kInstances> busy { };
    std::array<std::int32_t, kInstances> parameters { };
    std::array<std::uint16_t, kInstances> effects { };
    std::array<std::uint16_t, kInstances> phases { };

    bool HasPackage() override { return true; }
    std::uint8_t AllocateToken() override
    {
        recorded_.Add(kAllocateToken);
        return token;
    }
    void StoreRequest() override { ++stored; }
    std::uint16_t TargetType(std::size_t target) override { return types.at(target); }
    Effects_EffectInstance* CreateSecondary(const fx::SecondarySpawn& spawn) override
    {
        recorded_.Add(kSecondary, static_cast<long long>(spawn.target), spawn.toward_source ? 1 : 0, spawn.priority);
        recorded_.Add(kSecondaryScript, spawn.script_offset, spawn.token, 1);
        return InstanceAt(created.at(spawn.target));
    }
    void StartSecondary(Effects_EffectInstance& instance, std::int32_t parameter, std::uint16_t effect_id,
        std::uint8_t busy_bits) override
    {
        parameters.at(instance.index) = parameter;
        effects.at(instance.index) = effect_id;
        std::uint32_t& word = busy.at(instance.index);
        word = (word & ~kBusyMask) | ((static_cast<std::uint32_t>(busy_bits) << kBusyShift) & kBusyMask);
    }
    void SetPhase(Effects_EffectInstance& instance, fx::SecondaryEffectPhase phase) override
    {
        phases.at(instance.index) = static_cast<std::uint16_t>(std::to_underlying(phase));
    }
    void SelectPackage(fx::ActivePackage package) override { recorded_.Add(kSelect, std::to_underlying(package)); }
    void CaptureSavedCamera() override { recorded_.Add(kCapture, 0); }
    void CaptureEffectCamera() override { recorded_.Add(kCapture, 1); }

private:
    Recorded& recorded_;
};

class StartupHost final : public fx::EffectRuntimeStartup {
public:
    explicit StartupHost(Recorded& recorded) : recorded_(recorded) { }

    void ResetInstances() override { recorded_.Add(kResetCall); }
    void InitializeResources() override { recorded_.Add(kResources); }
    void ResetParticlePoolReferences() override { recorded_.Add(kPoolReferences); }

private:
    Recorded& recorded_;
};

class FrameHost final : public fx::EffectRuntimeFrame {
public:
    explicit FrameHost(Recorded& recorded) : recorded_(recorded) { }

    std::uint8_t buffer = 0;
    int primary = 0;
    std::uint32_t phase_after = 0;
    std::uint8_t secondary = 0;
    std::uint32_t phase = 0;

    std::uint8_t BufferIndex() override { return buffer; }
    void SetBufferIndex(std::uint8_t index) override { buffer = index; }
    void SelectPackage(fx::ActivePackage package) override { recorded_.Add(kSelect, std::to_underlying(package)); }
    bool UpdatePrimary() override
    {
        recorded_.Add(kUpdatePrimaryCall);
        phase = phase_after;
        return primary != 0;
    }
    std::uint8_t UpdateSecondary() override
    {
        recorded_.Add(kUpdateSecondaryCall);
        return secondary;
    }
    fx::PrimaryEffectPhase PrimaryPhase() override { return static_cast<fx::PrimaryEffectPhase>(phase); }
    void UpdateCameraMotion() override { recorded_.Add(kCameraMotion); }

private:
    Recorded& recorded_;
};

bool Init()
{
    Recorded recorded;
    StartupHost startup(recorded);
    fx::InitEffectRuntime(startup);
    recorded.Print();
    return true;
}

bool Reset()
{
    fx::EffectRuntimeRecords records;
    std::uint32_t phase = 0;
    int loaded = 0;
    int secondary = 0;
    int current = 0;
    int primary = 0;
    int menu = 0;
    if (!Read(phase) || !Read(records.primary.started_tracks) || !Read(records.primary.track_count) || !Read(loaded)
        || !Read(records.primary_token) || !Read(secondary) || !Read(current) || !Read(records.unused_primary_word)
        || !Read(primary) || !Read(records.first_curve_sample) || !Read(records.action_variant) || !Read(menu)
        || !ReadAll(records.tokens) || !Read(records.primary.pending_tracks))
        return false;
    records.primary.phase = static_cast<fx::PrimaryEffectPhase>(phase);
    records.primary.loaded = loaded != 0;
    records.secondary_instances = InstanceAt(secondary);
    records.current_instance = InstanceAt(current);
    records.primary_instances = InstanceAt(primary);
    records.menu_reload_slot = DiscAt(menu);
    fx::ResetEffectInstances(records);
    std::cout << static_cast<std::int32_t>(std::to_underlying(records.primary.phase)) << ' '
              << records.primary.started_tracks << ' ' << records.primary.track_count << ' '
              << (records.primary.loaded ? 1 : 0) << ' ' << static_cast<int>(records.primary_token) << ' '
              << RefOf(records.secondary_instances) << ' ' << RefOf(records.current_instance) << ' '
              << records.unused_primary_word << ' ' << RefOf(records.primary_instances) << ' '
              << records.first_curve_sample << ' ' << static_cast<int>(records.action_variant) << ' '
              << RefOf(records.menu_reload_slot);
    for (const std::uint32_t word : records.tokens)
        std::cout << ' ' << static_cast<std::int32_t>(word);
    std::cout << ' ' << static_cast<std::int32_t>(records.primary.pending_tracks) << " 0\n";
    return true;
}

bool UpdatePrimary()
{
    Recorded recorded;
    PrimaryHost runtime(recorded);
    std::uint32_t phase = 0;
    int loaded = 0;
    if (!Read(phase) || !Read(runtime.state.track_count) || !Read(runtime.state.started_tracks)
        || !Read(runtime.state.pending_tracks) || !Read(loaded) || !Read(runtime.token) || !Read(runtime.menu)
        || !Read(runtime.disc_state) || !Read(runtime.purge) || !Read(runtime.effect) || !Read(runtime.lba)
        || !Read(runtime.tracks) || !ReadAll(runtime.polls) || !runtime.ReadChain() || !Read(runtime.current)
        || !ReadAll(runtime.frames) || !ReadAll(runtime.scripts) || !Read(runtime.curves) || !Read(runtime.sfx)
        || !Read(runtime.stream) || !Read(runtime.aux))
        return false;
    runtime.state.phase = static_cast<fx::PrimaryEffectPhase>(phase);
    runtime.state.loaded = loaded != 0;
    const bool busy = fx::UpdatePrimaryEffects(runtime);
    std::cout << (busy ? 1 : 0);
    runtime.PrintState();
    std::cout << ' ' << runtime.menu << ' ' << runtime.purge << ' ' << runtime.first << ' ' << runtime.current;
    PrintAll(runtime.frames);
    std::cout << ' ' << runtime.sfx << ' ' << runtime.stream << ' ' << runtime.aux;
    recorded.Print();
    return true;
}

bool UpdateSecondary()
{
    Recorded recorded;
    SecondaryHost runtime(recorded);
    for (int index = 0; index < kSlots; ++index)
        slots.at(index).index = index;
    if (!runtime.ReadChain() || !Read(runtime.current) || !Read(runtime.curves) || !ReadAll(runtime.phases)
        || !ReadAll(runtime.busy) || !ReadAll(runtime.frames) || !ReadAll(runtime.scripts)
        || !ReadAll(runtime.held) || !ReadAll(runtime.updates))
        return false;
    const std::uint8_t busy = fx::UpdateSecondaryEffects(runtime);
    std::cout << static_cast<int>(busy) << ' ' << runtime.current;
    PrintAll(runtime.phases);
    PrintAll(runtime.frames);
    recorded.Print();
    return true;
}

bool StartTrack()
{
    Recorded recorded;
    PrimaryHost runtime(recorded);
    int track = 0;
    std::uint32_t phase = 0;
    if (!Read(track) || !Read(phase) || !Read(runtime.state.track_count) || !Read(runtime.state.started_tracks)
        || !Read(runtime.state.pending_tracks) || !Read(runtime.token))
        return false;
    runtime.state.phase = static_cast<fx::PrimaryEffectPhase>(phase);
    const fx::TrackStart started = fx::StartEffectTrack(track, runtime);
    std::cout << (started == fx::TrackStart::kNotReady ? 0 : 1) << ' ' << std::to_underlying(runtime.state.phase)
              << ' ' << runtime.state.started_tracks << ' ' << runtime.state.pending_tracks;
    recorded.Print();
    return true;
}

bool UpdateRuntime()
{
    Recorded recorded;
    FrameHost frame(recorded);
    if (!Read(frame.buffer) || !Read(frame.primary) || !Read(frame.phase_after) || !Read(frame.secondary))
        return false;
    const std::uint8_t busy = fx::UpdateEffectRuntime(frame);
    std::cout << static_cast<int>(busy) << ' ' << static_cast<int>(frame.buffer);
    recorded.Print();
    return true;
}

bool BeginAction()
{
    Recorded recorded;
    PrimaryHost runtime(recorded);
    fx::ActionEffectStart start;
    std::uint32_t phase = 0;
    if (!Read(start.variant) || !Read(start.argument) || !Read(start.source_type) || !Read(runtime.allocated)
        || !Read(runtime.variant) || !Read(runtime.argument) || !Read(phase))
        return false;
    runtime.state.phase = static_cast<fx::PrimaryEffectPhase>(phase);
    const std::uint8_t token = fx::BeginActionEffect(start, runtime);
    std::cout << static_cast<int>(token) << ' ' << (runtime.stored == 1 ? 1 : 0) << ' '
              << static_cast<int>(runtime.variant) << ' ' << static_cast<int>(runtime.argument) << ' '
              << static_cast<int>(runtime.token) << ' ' << std::to_underlying(runtime.state.phase);
    recorded.Print();
    return true;
}

bool ClearPrimary()
{
    Recorded recorded;
    PrimaryHost runtime(recorded);
    int moved = 0;
    if (!runtime.ReadChain() || !ReadAll(runtime.timelines) || !Read(moved))
        return false;
    runtime.moved = moved;
    runtime.unlink = false;
    fx::ClearPrimaryEffects(runtime);
    for (int index = 0; index < kInstances; ++index)
        std::cout << (index == 0 ? "" : " ") << (runtime.timelines.at(index) != 0 ? index : kNull);
    recorded.Print();
    return true;
}

bool Spawn(int mode)
{
    Recorded recorded;
    SpawnerHost spawner(recorded);
    fx::SecondaryEffectRequest request;
    std::int32_t parameter = 0;
    if (!Read(request.effect_id) || !Read(request.target_count) || !Read(parameter) || !Read(spawner.token)
        || !ReadAll(spawner.types) || !ReadAll(spawner.created) || !ReadAll(spawner.busy)
        || !ReadAll(spawner.parameters) || !ReadAll(spawner.effects) || !ReadAll(spawner.phases))
        return false;
    const std::uint8_t token = mode == kBeginSecondary ? fx::BeginSecondaryEffects(request, parameter, spawner)
                                                       : fx::SpawnSecondaryEffects(request, parameter, spawner);
    std::cout << static_cast<int>(token) << ' ' << (spawner.stored == 1 ? 1 : 0);
    PrintAll(spawner.parameters);
    PrintAll(spawner.effects);
    PrintAll(spawner.busy);
    PrintAll(spawner.phases);
    recorded.Print();
    return true;
}

bool SourceActor()
{
    Recorded recorded;
    std::uint8_t actor = 0;
    if (!Read(actor))
        return false;
    const fx::SourceActorEffect effect = fx::MakeSourceActorEffect(actor);
    recorded.Add(kSource, effect.effect_id, effect.target_count | (effect.other_count << kByteShift),
        effect.source_type);
    recorded.Add(kSourceEndpoint, effect.actor | (effect.part << kByteShift), effect.target_type,
        kSourceActorParameter);
    recorded.Add(kSourceTarget, effect.target_x, effect.target_y, effect.target_z);
    recorded.Print();
    return true;
}

bool ClearMode()
{
    std::uint8_t variant = 0;
    if (!Read(variant))
        return false;
    fx::ClearActionEffectMode(variant);
    std::cout << static_cast<int>(variant) << " 0\n";
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
        case kReset:
            read = Reset();
            break;
        case kUpdatePrimary:
            read = UpdatePrimary();
            break;
        case kUpdateSecondary:
            read = UpdateSecondary();
            break;
        case kStartTrack:
            read = StartTrack();
            break;
        case kUpdateRuntime:
            read = UpdateRuntime();
            break;
        case kBeginAction:
            read = BeginAction();
            break;
        case kClearPrimary:
            read = ClearPrimary();
            break;
        case kSpawnSecondary:
        case kBeginSecondary:
        case kSpawnRequest:
            read = Spawn(mode);
            break;
        case kSourceActor:
            read = SourceActor();
            break;
        case kClearMode:
            read = ClearMode();
            break;
        default:
            return 1;
        }
        if (!read)
            return 1;
    }
    return 0;
}
