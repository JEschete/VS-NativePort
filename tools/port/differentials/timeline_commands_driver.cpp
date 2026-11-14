#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

#include "Core/Random.hpp"
#include "Effects/TimelineCommands.hpp"

namespace {
namespace fx = VS::Effects;

// The modes timeline_commands_differential.py compares.
enum Mode : int {
    kTargetImpact,
    kSourceImpact,
    kRoomTile,
    kSignal,
    kBrightness,
    kSound,
    kGimmick,
    kDefense,
};

// The call kinds the harness records.
enum Call : int {
    kCallTargetImpact = 1,
    kCallSourceImpact,
    kCallTrap,
    kCallSignal,
    kCallBrightness,
    kCallSound,
    kCallUnlock,
    kCallArm,
};

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

bool ReadEndpoint(fx::CommandEndpoint& endpoint)
{
    fx::EndpointPoint& point = endpoint.point;
    if (!Read(endpoint.type) || !Read(point.x) || !Read(point.y) || !Read(point.z) || !Read(point.pad))
        return false;
    endpoint.actor = static_cast<std::uint8_t>(point.x);
    return true;
}

bool ReadEndpoints(fx::CommandEndpoints& endpoints)
{
    return ReadEndpoint(endpoints.source) && ReadEndpoint(endpoints.emit_from) && ReadEndpoint(endpoints.target)
        && ReadEndpoint(endpoints.emit_to);
}

using Calls = std::vector<std::vector<long long>>;

void AddPoint(std::vector<long long>& call, const fx::EndpointPoint& point)
{
    call.insert(call.end(), { point.x, point.y, point.z, point.pad });
}

void AddImpact(Calls& calls, int kind, const fx::ActionImpact& impact)
{
    std::vector<long long> call { kind, impact.actor, impact.point ? 1 : 0 };
    AddPoint(call, impact.point.value_or(fx::EndpointPoint { }));
    calls.push_back(call);
}

void AddRoomPoint(Calls& calls, int kind, const std::optional<fx::EndpointPoint>& point)
{
    if (!point)
        return;
    std::vector<long long> call { kind };
    AddPoint(call, *point);
    calls.push_back(call);
}

void AddActor(Calls& calls, int kind, const std::optional<std::uint8_t>& actor)
{
    if (actor)
        calls.push_back({ kind, *actor });
}

void Print(const Calls& calls)
{
    std::cout << calls.size();
    for (const auto& call : calls) {
        for (const long long word : call)
            std::cout << ' ' << word;
    }
}

bool Sound(const fx::CommandEndpoints& endpoints, Calls& calls)
{
    std::uint8_t variant = 0;
    std::uint8_t last = 0;
    std::uint32_t seed = 0;
    if (!Read(variant) || !Read(last) || !Read(seed))
        return false;
    VS::Core::Rand15 rand(seed);
    int draws = 0;
    const auto sound = fx::SourceReactionSound(endpoints, variant, last, [&] {
        ++draws;
        return rand.Next();
    });
    if (sound) {
        calls.push_back({ kCallSound, sound->actor, sound->variant });
        last = sound->last_variant;
    }
    Print(calls);
    std::cout << ' ' << draws << ' ' << static_cast<int>(last) << '\n';
    return true;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int mode = 0;
    while (Read(mode)) {
        fx::CommandEndpoints endpoints;
        if (!ReadEndpoints(endpoints))
            return 1;
        Calls calls;
        switch (mode) {
        case kTargetImpact:
            AddImpact(calls, kCallTargetImpact, fx::TargetActionImpact(endpoints));
            break;
        case kSourceImpact:
            AddImpact(calls, kCallSourceImpact, fx::SourceActionImpact(endpoints));
            break;
        case kRoomTile:
            AddRoomPoint(calls, kCallTrap, fx::TargetRoomTile(endpoints));
            break;
        case kSignal:
            AddActor(calls, kCallSignal, fx::TimelineSourceActorToSignal(endpoints));
            break;
        case kBrightness: {
            std::uint8_t brightness = 0;
            if (!Read(brightness))
                return 1;
            if (const auto set = fx::SourceWeaponBrightness(endpoints, brightness))
                calls.push_back({ kCallBrightness, set->actor, set->enabled ? 1 : 0, set->brightness });
            break;
        }
        case kSound:
            if (!Sound(endpoints, calls))
                return 1;
            continue;
        case kGimmick:
            AddRoomPoint(calls, kCallUnlock, fx::TargetGimmick(endpoints));
            break;
        case kDefense:
            AddActor(calls, kCallArm, fx::DefenseTimingTarget(endpoints));
            break;
        default:
            return 1;
        }
        Print(calls);
        std::cout << '\n';
    }
    return 0;
}
