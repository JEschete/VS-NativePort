// Differential driver for the bone trails (bone_trail_differential.py).
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <utility>
#include <vector>

#include "Effects/BoneTrail.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/Templates/RingCore.hpp"

// The cores see only incomplete records; the driver numbers them.
struct Effects_EffectInstance {
    int index;
};
struct Effects_RendererSlot {
    int index;
};

namespace {
namespace fx = VS::Effects;
namespace tp = VS::Effects::Templates;

enum Mode : int { kDraw, kUpdate, kSpawn, kFade, kClamp, kMap, kWeaponSpawn, kWeaponFade };
enum Call : long long { kProjectCall, kSampleCall, kLoadViewCall, kDrawCall, kAllocateCall, kFreeCall, kBeginCall, kFadeCall, kRefuseCall };

constexpr int kNull = -1;
constexpr std::size_t kCallWords = 16;
constexpr std::size_t kInstances = 4;
constexpr std::size_t kSlots = 12;
constexpr std::size_t kPaletteEntries = fx::kAdditiveTrailColors + fx::kDistortionTrailColors;
// The fixture packet the harness's cursor points at, the bytes of a trail
// state and the offsets of its history halves.
constexpr std::uint32_t kPacket = 0x80520000;
constexpr std::uint32_t kPacketBytes = tp::kRingPacketWords * 4;
constexpr long long kTrailBytes = 0xB4;
constexpr long long kHistoryBases = 0x0C;
constexpr long long kHistoryTips = 0x4C;
constexpr long long kEntryBytes = 8;
constexpr std::uint32_t kCodeByteSpare = 0x00FFFFFF;
constexpr std::uint32_t kHighHalfSpare = 0x0000FFFF;
constexpr unsigned kByteBits = 8;

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

struct Recorded {
    std::vector<std::array<long long, kCallWords>> calls;

    void Add(std::initializer_list<long long> words)
    {
        std::array<long long, kCallWords> call {};
        std::size_t index = 0;
        for (const long long word : words)
            call.at(index++) = word;
        calls.push_back(call);
    }
    void Print() const
    {
        std::cout << ' ' << calls.size();
        for (const auto& call : calls)
            PrintAll(call);
        std::cout << '\n';
    }
};

bool ReadPoint(fx::TrailPoint& point) { return Read(point.x) && Read(point.y) && Read(point.z); }

void AddPoint(std::vector<long long>& words, fx::TrailPoint point)
{
    words.insert(words.end(), { point.x, point.y, point.z });
}

void PrintPoint(fx::TrailPoint point) { std::cout << ' ' << point.x << ' ' << point.y << ' ' << point.z; }

bool ReadEdges(fx::BoneTrail& trail)
{
    return ReadPoint(trail.previous.base) && ReadPoint(trail.current.base) && ReadPoint(trail.previous.tip)
        && ReadPoint(trail.current.tip);
}

std::vector<long long> EdgeWords(const fx::BoneTrail& trail)
{
    std::vector<long long> words;
    for (const fx::TrailPoint point : { trail.previous.base, trail.current.base, trail.previous.tip, trail.current.tip })
        AddPoint(words, point);
    return words;
}

void AddCall(Recorded& recorded, std::vector<long long> words)
{
    std::array<long long, kCallWords> call {};
    for (std::size_t index = 0; index < words.size(); ++index)
        call.at(index) = words.at(index);
    recorded.calls.push_back(call);
}

std::array<std::uint32_t, tp::kRingPacketWords> PacketMask(bool distortion)
{
    std::array<std::uint32_t, tp::kRingPacketWords> mask {};
    mask.fill(0xFFFFFFFF);
    for (const std::size_t index : { 5, 8, 11 })
        mask.at(index) = kCodeByteSpare;
    for (const std::size_t index : { 10, 13 })
        mask.at(index) = kHighHalfSpare;
    if (distortion)
        mask.at(4) = kHighHalfSpare;
    return mask;
}

class SegmentHost final : public fx::TrailSegmentHost {
public:
    fx::TrailProjection projection {};
    fx::TrailPalettes palettes {};
    std::uint32_t frame {};
    std::uint32_t link {};
    std::array<std::uint32_t, tp::kRingPacketWords> packet {};
    long long advance {};
    long long depth { kNull };
    Recorded recorded;

    fx::TrailProjection Project(const std::array<fx::TrailPoint, fx::kTrailCorners>& corners) override
    {
        std::vector<long long> words { kProjectCall, 1 };
        for (const fx::TrailPoint corner : corners)
            AddPoint(words, corner);
        AddCall(recorded, std::move(words));
        return projection;
    }
    const fx::TrailPalettes& Palettes() override { return palettes; }
    std::uint32_t DrawBuffer() override { return frame & 1U; }
    void Submit(const tp::RingQuad& quad, std::uint8_t code, std::int32_t at) override
    {
        packet = tp::EncodeRingPacket(quad, link, code);
        link = (link & ~tp::kRingLinkMask) | (kPacket & tp::kRingLinkMask);
        advance += kPacketBytes;
        depth = at;
    }
};

bool ReadColor(tp::RingColor& color) { return Read(color.r) && Read(color.g) && Read(color.b); }

bool Draw()
{
    std::uint32_t config = 0;
    fx::BoneTrail trail {};
    fx::TrailSegmentWeights weights {};
    SegmentHost host;
    if (!Read(config) || !ReadEdges(trail) || !Read(weights.base) || !Read(weights.tip)
        || !ReadAll(host.projection.screens) || !Read(host.projection.depth) || !Read(host.frame))
        return false;
    trail.config = fx::BoneTrailConfig::Unpack(config);
    for (tp::RingColor& color : host.palettes.additive) {
        if (!ReadColor(color))
            return false;
    }
    for (tp::RingColor& color : host.palettes.distortion) {
        if (!ReadColor(color))
            return false;
    }
    if (!ReadAll(host.packet) || !Read(host.link))
        return false;
    static_cast<void>(fx::DrawTrailSegment(trail, weights, host));
    const auto mask = PacketMask(trail.config.texture == fx::kDistortionTrailTexture);
    for (std::size_t index = 0; index < host.packet.size(); ++index)
        std::cout << ' ' << (host.packet.at(index) & mask.at(index));
    std::cout << ' ' << host.link << ' ' << host.advance << ' ' << host.depth;
    host.recorded.Print();
    return true;
}

class UpdateHost final : public fx::BoneTrailHost {
public:
    fx::BoneTrail trail {};
    bool held {};
    std::array<std::array<int, 4>, 2> samples {};
    std::size_t sampled {};
    Recorded recorded;

    fx::BoneTrail* Trail() override { return held ? &trail : nullptr; }
    fx::BoneTrail* Allocate() override
    {
        recorded.Add({ kAllocateCall, kTrailBytes });
        trail = {};
        held = true;
        return &trail;
    }
    void Release() override
    {
        recorded.Add({ kFreeCall, held ? 1 : 0 });
        held = false;
    }
    void SampleBone(std::uint8_t actor, std::uint8_t bone, VS::Actors::BoneEndpoint endpoint, fx::TrailPoint& point) override
    {
        recorded.Add({ kSampleCall, actor, bone, std::to_underlying(endpoint), Offset(point) });
        if (sampled >= samples.size())
            return;
        const auto& [missing, x, y, z] = samples.at(sampled++);
        if (missing == 0)
            point = fx::TrailPoint { .x = static_cast<std::int16_t>(x), .y = static_cast<std::int16_t>(y), .z = static_cast<std::int16_t>(z) };
    }
    void LoadView() override { recorded.Add({ kLoadViewCall, 1, 1 }); }
    void DrawSegment(const fx::BoneTrail& drawn, fx::TrailSegmentWeights weights) override
    {
        std::vector<long long> words { kDrawCall, &drawn == &trail ? 1 : 0, weights.base, weights.tip };
        const std::vector<long long> edges = EdgeWords(drawn);
        words.insert(words.end(), edges.begin(), edges.end());
        AddCall(recorded, std::move(words));
    }
    void Refuse(fx::RendererCommand command, fx::BoneTrailRefusal refusal) override
    {
        recorded.Add({ kRefuseCall, std::to_underlying(command), std::to_underlying(refusal) });
    }

private:
    // The original's byte offset of `point` in the trail state.
    long long Offset(const fx::TrailPoint& point) const
    {
        for (std::size_t entry = 0; entry < trail.history.size(); ++entry) {
            const long long at = static_cast<long long>(entry) * kEntryBytes;
            if (&point == &trail.history.at(entry).base)
                return kHistoryBases + at;
            if (&point == &trail.history.at(entry).tip)
                return kHistoryTips + at;
        }
        return kNull;
    }
};

bool Update()
{
    std::uint32_t command = 0;
    std::uint32_t parameter = 0;
    std::uint32_t config = 0;
    UpdateHost host;
    fx::BoneTrail& trail = host.trail;
    if (!Read(command) || !Read(parameter) || !Read(host.held) || !Read(config) || !Read(trail.newest) || !Read(trail.age))
        return false;
    trail.config = fx::BoneTrailConfig::Unpack(config);
    for (fx::TrailEdge& edge : trail.history) {
        if (!ReadPoint(edge.base))
            return false;
    }
    for (fx::TrailEdge& edge : trail.history) {
        if (!ReadPoint(edge.tip))
            return false;
    }
    if (!ReadEdges(trail))
        return false;
    for (auto& sample : host.samples) {
        if (!ReadAll(sample))
            return false;
    }
    const bool kept = fx::UpdateBoneTrail(static_cast<fx::RendererCommand>(command), parameter, host);
    std::cout << ' ' << (kept ? 1 : 0) << ' ' << (host.held ? 1 : 0) << ' ' << trail.config.Pack() << ' ' << trail.newest
              << ' ' << trail.age;
    for (const fx::TrailEdge& edge : trail.history)
        PrintPoint(edge.base);
    for (const fx::TrailEdge& edge : trail.history)
        PrintPoint(edge.tip);
    for (const fx::TrailPoint point : { trail.previous.base, trail.current.base, trail.previous.tip, trail.current.tip })
        PrintPoint(point);
    host.recorded.Print();
    return true;
}

void PrintRequest(const fx::BoneTrailRequest& request)
{
    Recorded recorded;
    recorded.Add({ kBeginCall, request.effect_id, request.target_count, request.other_count, request.source_type, 0, 0, 0,
        request.target_type, 0, 0, 0, static_cast<std::int32_t>(request.parameter) });
    recorded.Print();
}

bool Spawn()
{
    std::array<std::int32_t, 4> arguments {};
    if (!ReadAll(arguments))
        return false;
    PrintRequest(fx::MakeBoneTrailRequest(arguments.at(0), arguments.at(1), arguments.at(2), arguments.at(3)));
    return true;
}

bool WeaponSpawn()
{
    std::array<std::int32_t, 2> arguments {};
    if (!ReadAll(arguments))
        return false;
    PrintRequest(fx::MakeWeaponTrailRequest(arguments.at(0), arguments.at(1)));
    return true;
}

class Chains final : public fx::BoneTrailChains {
public:
    std::array<Effects_EffectInstance, kInstances> instances { { { 0 }, { 1 }, { 2 }, { 3 } } };
    std::array<Effects_RendererSlot, kSlots> slots { { { 0 }, { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 6 }, { 7 }, { 8 }, { 9 }, { 10 }, { 11 } } };
    int first { kNull };
    std::array<int, kInstances> instance_next {};
    std::array<int, kInstances> first_slot {};
    std::array<int, kSlots> slot_next {};
    std::array<int, kSlots> trail {};
    std::array<int, kSlots> unlink {};
    Recorded recorded;

    Effects_EffectInstance* FirstInstance() override { return InstanceAt(first); }
    Effects_EffectInstance* NextInstance(Effects_EffectInstance& instance) override
    {
        return InstanceAt(instance_next.at(instance.index));
    }
    Effects_RendererSlot* FirstRenderer(Effects_EffectInstance& instance) override
    {
        return SlotAt(first_slot.at(instance.index));
    }
    Effects_RendererSlot* NextRenderer(Effects_RendererSlot& slot) override { return SlotAt(slot_next.at(slot.index)); }
    bool RunsBoneTrail(Effects_RendererSlot& slot) override { return trail.at(slot.index) != 0; }
    void Fade(Effects_RendererSlot& slot) override
    {
        recorded.Add({ kFadeCall, slot.index, std::to_underlying(fx::RendererCommand::kFade), 0 });
        // The harness's renderer may unlink the slot it fades.
        if (unlink.at(slot.index) != 0)
            slot_next.at(slot.index) = kNull;
    }

private:
    Effects_EffectInstance* InstanceAt(int index) { return index == kNull ? nullptr : &instances.at(index); }
    Effects_RendererSlot* SlotAt(int index) { return index == kNull ? nullptr : &slots.at(index); }
};

bool FadeTrails(bool weapon)
{
    std::array<long long, 2> ignored {};
    Chains chains;
    if (!ReadAll(ignored) || !Read(chains.first) || !ReadAll(chains.instance_next) || !ReadAll(chains.first_slot)
        || !ReadAll(chains.slot_next) || !ReadAll(chains.trail) || !ReadAll(chains.unlink))
        return false;
    if (weapon)
        fx::FadeWeaponTrails(chains);
    else
        fx::FadeBoneTrails(chains);
    chains.recorded.Print();
    return true;
}

unsigned PackedUv(tp::RingUv uv) { return uv.u | (static_cast<unsigned>(uv.v) << kByteBits); }

bool Clamp()
{
    std::int32_t u_offset = 0;
    std::int32_t v_offset = 0;
    std::uint32_t screen = 0;
    if (!Read(u_offset) || !Read(v_offset) || !Read(screen))
        return false;
    std::cout << ' ' << PackedUv(fx::ClampTrailTexturePoint(u_offset, v_offset, screen));
    Recorded().Print();
    return true;
}

bool Map()
{
    tp::RingQuad quad {};
    std::int32_t u_offset = 0;
    std::int32_t v_offset = 0;
    std::uint32_t flags = 0;
    std::uint32_t frame = 0;
    if (!ReadAll(quad.screens) || !Read(u_offset) || !Read(v_offset) || !Read(flags) || !Read(frame))
        return false;
    fx::MapTrailFramebufferTexture(quad, u_offset, v_offset, static_cast<std::uint16_t>(flags), frame & 1U);
    std::cout << ' ' << quad.tpage;
    for (const tp::RingUv uv : quad.uvs)
        std::cout << ' ' << PackedUv(uv);
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
        case kDraw:
            read = Draw();
            break;
        case kUpdate:
            read = Update();
            break;
        case kSpawn:
            read = Spawn();
            break;
        case kWeaponSpawn:
            read = WeaponSpawn();
            break;
        case kFade:
            read = FadeTrails(false);
            break;
        case kWeaponFade:
            read = FadeTrails(true);
            break;
        case kClamp:
            read = Clamp();
            break;
        case kMap:
            read = Map();
            break;
        default:
            return 1;
        }
        if (!read)
            return 1;
    }
    return 0;
}
