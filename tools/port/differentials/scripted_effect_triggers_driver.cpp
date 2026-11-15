// Native side of tools\port\differentials\scripted_effect_triggers_*.py (F00238, F00239,
// F00240, F00242): one vector per stdin line, one line of integers per vector, in the order
// scripted_effect_triggers_common.py records the original's calls.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Effects/ScriptedEffectTriggers.hpp"

namespace {
namespace fx = VS::Effects;

enum Mode : int { kStart, kRelease, kPhase, kReady };
constexpr int kBeginCall = 1, kTrackCall = 2, kPhaseCall = 3, kCanStartCall = 4;
// The request's count, which the live binding writes.
constexpr int kCount = 1;

long long Next(std::istringstream& in)
{
    long long value = 0;
    if (!(in >> value))
        std::abort();
    return value;
}

class Host final : public fx::ScriptedEffectTriggerHost {
public:
    std::uint32_t loaded = 0;
    std::int32_t phase = 0, can_start = 0;
    std::vector<std::vector<long long>> calls;

    void BeginActionEffect(const fx::PrimaryEffectRequest& request, std::int32_t variant,
        std::int32_t argument) override
    {
        const auto& s = request.source;
        const auto& t = request.target;
        std::vector<long long> call { kBeginCall, variant, argument, request.effect_id, kCount,
            request.other_count, static_cast<long long>(s.kind), s.position.x, s.position.y,
            s.position.z, static_cast<long long>(t.kind), t.actor, t.part };
        for (std::size_t i = 0; i < request.other_count && i < request.others.size(); ++i)
            call.push_back(request.others[i]);
        calls.push_back(call);
    }
    bool ActorLoaded(std::size_t slot) override
    {
        return slot < fx::kScriptedEffectActorSlots && ((loaded >> slot) & 1U) != 0;
    }
    void StartEffectTrack(std::int32_t track) override { calls.push_back({ kTrackCall, track }); }
    std::int32_t PrimaryEffectPhase() override
    {
        calls.push_back({ kPhaseCall });
        return phase;
    }
    std::int32_t CanStartEffectTrack() override
    {
        calls.push_back({ kCanStartCall });
        return can_start;
    }
};
} // namespace

int main(int, char**)
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        const int mode = static_cast<int>(Next(in));
        const auto effect = static_cast<std::int32_t>(Next(in));
        const auto target = static_cast<std::int32_t>(Next(in));
        Host host;
        host.loaded = static_cast<std::uint32_t>(Next(in));
        host.phase = static_cast<std::int32_t>(Next(in));
        host.can_start = static_cast<std::int32_t>(Next(in));
        std::vector<long long> values;
        switch (mode) {
        case kStart:
            // The live entry narrows the effect id as the original's halfword store does.
            fx::StartScriptedActorEffect(static_cast<std::uint16_t>(effect), target, host);
            break;
        case kRelease:
            fx::ReleaseScriptedEffectTrigger(effect, host);
            break;
        case kPhase:
            values.push_back(fx::ScriptedEffectPlaybackState(host));
            break;
        default:
            values.push_back(fx::IsScriptedEffectTriggerReady(host) ? 1 : 0);
            break;
        }
        values.push_back(static_cast<long long>(host.calls.size()));
        for (const auto& call : host.calls)
            values.insert(values.end(), call.begin(), call.end());
        for (std::size_t i = 0; i < values.size(); ++i)
            std::cout << (i ? " " : "") << values[i];
        std::cout << '\n';
    }
}
