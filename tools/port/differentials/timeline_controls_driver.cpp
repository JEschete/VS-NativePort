#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Effects/TimelineCommands.hpp"

namespace {
namespace fx = VS::Effects;

// The modes timeline_controls_differential.py compares.
enum Mode : int {
    kTrail,
    kFade,
    kResume,
    kHold,
    kPause,
    kInvalidate,
    kDispatch,
};

// The call kinds the harness records; a command records kCallCommand + its number.
enum Call : int {
    kCallSave = 1,
    kCallSpawn,
    kCallRestore,
    kCallFade,
    kCallPause,
    kCallCommand = 16,
    kCallAdvance = 32,
};

constexpr std::size_t kTransforms = 16;
// The operands the dispatcher passes each command, as the Live binding uses them.
constexpr std::array<int, fx::kActionControlCommands> kCommandOperands { 0, 0, 0, 2, 0, 1, 0, 0, 0, 1, 1, 1, 0, 2, 1, 0 };

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

using Calls = std::vector<std::vector<long long>>;

// The resource context, trail and room object calls, with the arguments the Live binding passes.
class RecordingEffectHost final : public fx::TimelineEffectHost {
public:
    void SaveResourceContext() override { calls.push_back({ kCallSave }); }
    void RestoreResourceContext() override { calls.push_back({ kCallRestore }); }
    void SpawnBoneTrail(const fx::WeaponTrail& trail) override
    {
        calls.push_back({ kCallSpawn, trail.actor, trail.bone, trail.color, trail.texture });
    }
    void FadeBoneTrails() override { calls.push_back({ kCallFade, 0, 0 }); }
    void FreezeRoomObjects() override { calls.push_back({ kCallPause }); }

    Calls calls;
};

// The commands performed, with the instance and the operands each takes, and the track advance.
class RecordingControlHost final : public fx::ActionControlHost {
public:
    explicit RecordingControlHost(bool more_keys)
        : more_keys_(more_keys)
    {
    }

    void Perform(const fx::ActionControl& control) override
    {
        const auto number = static_cast<std::size_t>(control.command);
        std::vector<long long> call { kCallCommand + static_cast<long long>(number), 1 };
        const std::array<long long, 2> operands { control.operand0, control.operand1 };
        call.insert(call.end(), operands.begin(), operands.begin() + kCommandOperands.at(number));
        calls.push_back(call);
    }
    bool AdvanceKey() override
    {
        calls.push_back({ kCallAdvance, 1 });
        return more_keys_;
    }

    Calls calls;

private:
    bool more_keys_;
};

void Print(const Calls& calls)
{
    std::cout << calls.size();
    for (const auto& call : calls) {
        for (const long long word : call)
            std::cout << ' ' << word;
    }
    std::cout << '\n';
}

bool Trail()
{
    std::uint8_t index = 0;
    std::uint8_t style = 0;
    std::array<fx::TransformAnchor, kTransforms> anchors { };
    if (!Read(index) || !Read(style))
        return false;
    for (auto& anchor : anchors) {
        if (!Read(anchor.actor) || !Read(anchor.bone))
            return false;
    }
    RecordingEffectHost host;
    if (const auto slot = fx::TransformSlotOf(index))
        fx::SpawnTransformWeaponTrail(anchors.at(*slot), style, host);
    Print(host.calls);
    return true;
}

bool TransformFlags(std::uint16_t (*command)(std::uint16_t))
{
    std::uint8_t index = 0;
    std::array<std::uint16_t, kTransforms> flags { };
    if (!Read(index))
        return false;
    for (auto& value : flags) {
        if (!Read(value))
            return false;
    }
    if (const auto slot = fx::TransformSlotOf(index))
        flags.at(*slot) = command(flags.at(*slot));
    for (const std::uint16_t value : flags)
        std::cout << value << ' ';
    Print({ });
    return true;
}

bool Dispatch()
{
    fx::ActionControlKey key;
    std::int16_t frame = 0;
    int more = 0;
    if (!Read(key.timing) || !Read(key.operand0) || !Read(key.operand1) || !Read(frame) || !Read(more))
        return false;
    RecordingControlHost host(more != 0);
    const bool running = fx::TickActionControlTrack(key, frame, host);
    std::cout << (running ? 1 : 0) << ' ';
    Print(host.calls);
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
        case kTrail:
            read = Trail();
            break;
        case kFade: {
            RecordingEffectHost host;
            fx::FadeActionWeaponTrails(host);
            Print(host.calls);
            break;
        }
        case kResume:
            read = TransformFlags(fx::ResumeTransformMotion);
            break;
        case kHold: {
            std::uint8_t flags = 0;
            read = Read(flags);
            std::cout << static_cast<int>(fx::HoldTimelineFrame(flags)) << ' ';
            Print({ });
            break;
        }
        case kPause: {
            RecordingEffectHost host;
            fx::PauseMovingRoomObjects(host);
            Print(host.calls);
            break;
        }
        case kInvalidate:
            read = TransformFlags(fx::InvalidateTransformAttachments);
            break;
        case kDispatch:
            read = Dispatch();
            break;
        default:
            return 1;
        }
        if (!read)
            return 1;
    }
    return 0;
}
