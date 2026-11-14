#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

#include "Effects/EffectScript.hpp"

namespace {
namespace fx = VS::Effects;

// The modes effect_script_timeline_differential.py compares.
enum Mode : int {
    kImmediate,
    kRegister,
    kTick,
    kFinished,
    kFree,
    kUpdate,
    kInitialize,
    kEnvironment,
};

// The call kinds the harness's stubs record.
enum Call : int {
    kLoaded = 1,
    kTicked,
    kFreed,
    kUpdated,
    kInitialized,
    kRefreshed,
};

constexpr std::size_t kProgramBytes = 0x10000;

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

// Records the calls the core makes; a freed timeline clears `timeline`.
class RecordingHost final : public fx::ScriptEffectHost {
public:
    void LoadTimeline(std::uint8_t index) override { calls.emplace_back(kLoaded, index); }
    void TickTimeline(std::uint16_t child_offset) override { calls.emplace_back(kTicked, child_offset); }
    void FreeTimeline() override
    {
        calls.emplace_back(kFreed, 0);
        timeline = 0;
    }
    void UpdateTransforms() override { calls.emplace_back(kUpdated, 0); }
    void InitializeTransforms() override { calls.emplace_back(kInitialized, 0); }
    void UpdateParticleEnvironment() override { calls.emplace_back(kRefreshed, 0); }

    std::vector<std::pair<int, long long>> calls;
    std::uint32_t timeline = 0;
};

bool ReadProgram(std::vector<std::byte>& program, std::uint16_t& offset)
{
    std::size_t placed = 0;
    if (!Read(offset) || !Read(placed))
        return false;
    program.assign(kProgramBytes, std::byte { });
    for (std::size_t i = 0; i < placed; ++i) {
        std::size_t at = 0;
        int value = 0;
        if (!Read(at) || !Read(value) || at >= program.size())
            return false;
        program[at] = static_cast<std::byte>(value);
    }
    return true;
}

void PrintStep(const fx::OpcodeStep& step)
{
    const int refusal = step ? 0 : 1 + static_cast<int>(step.error());
    const long long result = step ? fx::HandlerResult(*step) : -1;
    std::cout << refusal << ' ' << result;
}

void PrintCalls(const RecordingHost& host)
{
    std::cout << ' ' << host.calls.size();
    for (const auto& [kind, argument] : host.calls)
        std::cout << ' ' << kind << ' ' << argument;
}

bool Program(int mode)
{
    std::vector<std::byte> program;
    std::uint16_t offset = 0;
    if (!ReadProgram(program, offset))
        return false;
    fx::ScriptCursor cursor { .program = program, .offset = offset };
    RecordingHost host;
    fx::OpcodeStep step;
    if (mode == kImmediate) {
        step = fx::LoadTimelineImmediate(cursor, host);
    } else if (mode == kRegister) {
        fx::ScriptRegisters registers { };
        int has_parent = 0;
        fx::TimelineRemap remap { };
        int has_count = 0;
        std::int32_t count = 0;
        for (auto& value : registers) {
            if (!Read(value))
                return false;
        }
        if (!Read(has_parent))
            return false;
        for (auto& id : remap.ids) {
            if (!Read(id))
                return false;
        }
        if (!Read(has_count) || !Read(count))
            return false;
        if (has_count)
            remap.timeline_count = count;
        step = fx::LoadRegisterTimeline(cursor, registers, has_parent != 0, remap, host);
    } else if (mode == kTick) {
        step = fx::TickTimelineFromScript(cursor, true, host);
    } else {
        std::uint32_t active = 0;
        if (!Read(active))
            return false;
        PrintStep(fx::BranchIfTimelineFinished(active, cursor));
        std::cout << ' ' << cursor.offset << '\n';
        return true;
    }
    PrintStep(step);
    std::cout << ' ' << cursor.offset;
    PrintCalls(host);
    std::cout << '\n';
    return true;
}

bool Free()
{
    RecordingHost host;
    if (!Read(host.timeline))
        return false;
    PrintStep(fx::FreeScriptTimeline(host.timeline != 0, host));
    PrintCalls(host);
    std::cout << ' ' << host.timeline << '\n';
    return true;
}

void Service(int mode)
{
    RecordingHost host;
    fx::ScriptStep step = fx::ScriptStep::kEnd;
    if (mode == kUpdate)
        step = fx::TickScriptTransforms(host);
    else if (mode == kInitialize)
        step = fx::InitializeScriptTransforms(host);
    else
        step = fx::RefreshParticleEnvironment(host);
    PrintStep(step);
    PrintCalls(host);
    std::cout << '\n';
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
        case kImmediate:
        case kRegister:
        case kTick:
        case kFinished:
            read = Program(mode);
            break;
        case kFree:
            read = Free();
            break;
        case kUpdate:
        case kInitialize:
        case kEnvironment:
            Service(mode);
            break;
        default:
            return 1;
        }
        if (!read)
            return 1;
    }
    return 0;
}
