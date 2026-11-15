// Native side of tools\port\differentials\action_callbacks_*.py (F00064, F00068, F00069,
// F00119, F00120, F00143): one vector per stdin line, one line of integers per vector, in the
// order action_callbacks_common.py records the original's calls.
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Effects/ActionEffectCallbacks.hpp"

namespace {
namespace fx = VS::Effects;

enum Mode : int { kSignal, kTint, kRestore, kBegin, kEnd, kBusy };
constexpr int kAdvanceCall = 1, kTransformCall = 2, kBeginCall = 3, kHudCall = 4,
              kMinimapCall = 5, kExploreCall = 6;
// The request's count, which the live binding writes.
constexpr int kCount = 1;

long long Next(std::istringstream& in)
{
    long long value = 0;
    if (!(in >> value))
        std::abort();
    return value;
}

class Host final : public fx::ActionEffectCallbackHost {
public:
    std::int32_t tinted = 0, control = 0, tick = 0;
    std::uint32_t busy = 0;
    std::vector<std::vector<long long>> calls;

    void Record(std::initializer_list<long long> call)
    {
        std::vector<long long> values(call);
        values.insert(values.end(), { tinted, control, tick });
        calls.push_back(values);
    }
    void BeginActionEffect(const fx::PrimaryEffectRequest& request, std::int32_t variant,
        std::int32_t argument) override
    {
        const auto& s = request.source;
        const auto& t = request.target;
        Record({ kBeginCall, variant, argument, request.effect_id, kCount, request.other_count,
            static_cast<long long>(s.kind), s.actor, s.slot, s.part,
            static_cast<long long>(t.kind), t.actor, t.slot, t.part, request.position.x,
            request.position.y, request.position.z });
    }
    void RequestActorAnimationAdvance(std::int32_t actor) override
    {
        Record({ kAdvanceCall, actor });
    }
    void TransformActorColors(const fx::ActorColorTransform& transform) override
    {
        Record({ kTransformCall, static_cast<long long>(transform.mode), transform.duration,
            transform.actor, transform.red_offset, transform.green_offset,
            transform.blue_offset });
    }
    std::int32_t TintedActor() override { return tinted; }
    void SetTintedActor(std::int32_t actor) override { tinted = actor; }
    void SetFieldControlMode(VS::Field::FieldControlMode mode) override
    {
        control = static_cast<std::int32_t>(mode);
    }
    void SetTickSpeed(std::int32_t value) override { tick = value; }
    void SetHudVisibility(VS::Battle::HudVisibility visibility) override
    {
        Record({ kHudCall, static_cast<long long>(visibility) });
    }
    void SetMinimapVisible(bool visible) override { Record({ kMinimapCall, visible ? 1 : 0 }); }
    void EnterFieldExplorationMode() override { Record({ kExploreCall }); }
    std::uint32_t EffectBusyFlags() override { return busy; }
};
} // namespace

int main(int, char**)
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        const int mode = static_cast<int>(Next(in));
        Host host;
        host.tinted = static_cast<std::int32_t>(Next(in));
        host.control = static_cast<std::int32_t>(Next(in));
        host.tick = static_cast<std::int32_t>(Next(in));
        const auto argument = static_cast<std::int32_t>(Next(in));
        host.busy = static_cast<std::uint32_t>(Next(in));
        long long result = 0;
        switch (mode) {
        case kSignal:
            fx::SignalActorEffectAnimation(argument, host);
            break;
        case kTint:
            fx::TintActionActorRed(argument, host);
            break;
        case kRestore:
            fx::RestoreActionActorTint(host);
            break;
        case kBegin:
            // The live entry narrows the effect id as the original's halfword store does.
            fx::BeginAshleyStandaloneEffect(static_cast<std::uint16_t>(argument), host);
            break;
        case kEnd:
            fx::EndAshleyStandaloneEffect(host);
            break;
        default:
            result = static_cast<std::int32_t>(fx::ActionEffectBusyFlags(host));
            break;
        }
        std::vector<long long> values { host.tinted, host.control, host.tick };
        if (mode == kBusy)
            values.push_back(result);
        values.push_back(static_cast<long long>(host.calls.size()));
        for (const auto& call : host.calls)
            values.insert(values.end(), call.begin(), call.end());
        for (std::size_t i = 0; i < values.size(); ++i)
            std::cout << (i ? " " : "") << values[i];
        std::cout << '\n';
    }
}
