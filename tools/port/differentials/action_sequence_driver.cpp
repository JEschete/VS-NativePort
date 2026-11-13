// Semantic service fixture; the shared differential executes the original instructions.
#include "Battle/ActionSequence.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
namespace b = VS::Battle;
using Word = std::uint32_t;
constexpr std::array<Word, 2> kStates { 0x80500000, 0x80504000 };
constexpr std::array<Word, 3> kActors { 0, 1, 40 };
constexpr Word kNotification = 0x800CB45C, kFlags = 0x800A0BE0;
constexpr Word kAsset = 0x800995B0, kCanStart = 0x800CEEBC, kAdvance = 0x800704B0;
constexpr Word kAnimation = 0x80070278, kTrack = 0x800CEF74, kPlans = 0x800DEC88;
constexpr Word kRelease = 0x80069D14, kFade = 0x8006FCBC, kBusy = 0x8009E4B0;
constexpr Word kDefeat = 0x80074A20, kRecovery = 0x8006FBCC, kProjection = 0x80041534;
constexpr Word kLook = 0x80077DF0, kResume = 0x8006C40C, kHud = 0x800CB654;
constexpr Word kIdle = 0x8009E5C4, kResolve = 0x800792E4, kEvent = 0x800BEC58;
constexpr Word kCutscene = 0x80073898, kLoot = 0x8006FB48;
int Signed(Word value) { return std::bit_cast<std::int32_t>(value); }
Word Raw(int value) { return std::bit_cast<Word>(value); }
void SetByte(Word& value, Word byte) { value = (value & 0xFFFFFF00) | (byte & 255); }

class World;
class Action final : public b::SequenceAction {
public:
    std::array<Word, 66> raw { };
    World* world { };
    Word state { }, queue { };
    Word Address() const { return kStates.at(state) + 0x854 + queue * 0x84C; }
    b::ActionTargetHit Primary() const override
    {
        return { raw[2] != 0, static_cast<int>(raw[1] & 255), 0 };
    }
    b::ActionTargetHit Target(std::size_t index) const override
    {
        return { raw.at(7 + index * 2) != 0,
            static_cast<int>(raw.at(6 + index * 2) & 255), 0 };
    }
    std::uint16_t Count() const override
    {
        return static_cast<std::uint16_t>(raw[3] >> 16);
    }
    void SetCount(Word count) { raw[3] = (raw[3] & 0xFFFF) | ((count & 0xFFFF) << 16); }
    void SetPrimary(Word actor) { SetByte(raw[1], actor); }
    void SetTarget(std::size_t index, Word actor)
    {
        SetByte(raw.at(6 + index * 2), actor);
        raw.at(7 + index * 2) = 0;
    }
    int RebuildPlans() override;
};
struct State {
    std::array<Word, 5> fields { };
};
class World final : public b::ActionSequenceWorld {
public:
    Word function { }, lane { }, mutation { }, diagnostics { };
    std::array<Word, 13> services { };
    std::array<Word, 7> globals { };
    std::array<Word, 6> guards { };
    std::array<State, 2> states { };
    std::array<Action, 8> actions { };
    Action* active { };
    std::unordered_map<Word, Word> counts;
    std::vector<Word> calls;
    World()
    {
        for (Word state = 0; state < 2; ++state) {
            for (Word queue = 0; queue < 4; ++queue) {
                auto& action = actions.at(state * 4 + queue);
                action.world = this;
                action.state = state;
                action.queue = queue;
            }
        }
    }
    std::size_t Current() const
    {
        for (std::size_t state = 0; state < 2; ++state) {
            if (globals[0] == kStates[state])
                return state;
        }
        std::_Exit(91);
    }
    Action& At(std::size_t state, std::size_t queue)
    {
        return actions.at(state * 4 + queue);
    }
    Word ActorResult(std::size_t start, int actor) const
    {
        for (std::size_t i = 0; i < kActors.size(); ++i) {
            if (static_cast<Word>(actor) == kActors[i])
                return services.at(start + i);
        }
        std::_Exit(92);
    }
    void Mutate(Word address)
    {
        const bool first = counts[address] == 1;
        if (function == 0) {
            auto& queued = At(0, initial_index & 3);
            if (address == kNotification) {
                if (mutation == 1 || mutation == 2 || mutation == 3)
                    globals[0] = kStates[1];
                if (mutation == 2 || mutation == 3)
                    queued.SetPrimary(mutation == 2 ? 1 : 40);
            }
            if (mutation == 4 && address == kFlags) {
                globals[0] = kStates[1];
                queued.SetPrimary(0);
            }
            if (mutation == 5 && address == kAsset)
                globals[0] = 0xDEAD0040;
        }
        if (function == 1 && address == kAnimation) {
            if (mutation == 1 || mutation == 2)
                globals[4] = mutation == 1 ? 2 : 0xFFFFFFFF;
            if (mutation == 3)
                globals[0] = 0xDEAD0080;
        }
        if (function == 2) {
            if (address == kPlans) {
                if (mutation == 1 || mutation == 3)
                    globals[0] = kStates[1];
                if (mutation == 2)
                    states[0].fields[0] = initial_index ^ 3;
                if (mutation == 3)
                    At(1, 0).SetPrimary(40);
                if (mutation == 5 && services[4] != 0)
                    globals[0] = 0xDEAD00C0;
            }
            if (mutation == 4 && address == kRelease)
                globals[0] = kStates[1];
        }
        if (function != 3 && function != 4)
            return;
        if (address == kBusy && first) {
            if (mutation == 1)
                globals[0] = kStates[1 - Current()];
            if (mutation == 2)
                active->SetCount(0);
            if (mutation == 3 || mutation == 10) {
                active->SetCount(mutation == 10 ? 30 : 4);
                active->SetTarget(2, 1);
                active->SetTarget(3, 40);
            }
            if (mutation == 4) {
                globals[0] = kStates[1];
                states[1].fields[1] = 0;
                active->SetPrimary(lane % 2 == 0 ? 0 : 40);
                active->raw[2] = 1;
            }
            if (mutation == 5)
                states[Current()].fields[1] = 6;
            if (mutation == 11) {
                active->SetPrimary(1);
                active->raw[2] = 1;
            }
            if (mutation == 13)
                states[Current()].fields[1] = 1;
        }
        if (address == kBusy && mutation == 9) {
            globals[0] = kStates[1 - Current()];
            active->SetTarget(1, 40);
        }
        if ((address == kDefeat || address == kResolve) && first) {
            if (mutation == 6 || mutation == 12 || mutation == 15) {
                globals[0] = kStates[1];
                active->SetCount(3);
                active->SetPrimary(1);
                active->SetTarget(1, 1);
                active->SetTarget(2, 40);
                if (mutation == 15)
                    states[1].fields[1] = 6;
            }
            if (mutation == 7) {
                active->SetCount(0);
                active->SetPrimary(40);
                active->raw[2] = 1;
                states[Current()].fields[1] = 0;
            }
            if (mutation == 13) {
                states[Current()].fields[1] = 0;
                active->SetCount(3);
                active->SetTarget(1, 1);
                active->SetTarget(2, 40);
            }
        }
        if (address == kDefeat && mutation == 8) {
            active->SetPrimary(lane % 2 == 0 ? 0 : 40);
            active->raw[2] = 1;
        }
        if (function == 3 && address == kRecovery && mutation == 14) {
            globals[0] = 0xDEAD0100;
            globals[5] = 3;
        }
        if (function == 4) {
            if (address == kProjection && mutation == 11) {
                globals[0] = kStates[1];
                globals[2] = Raw(lane % 2 ? 300 : -300);
                states[1].fields[3] = (states[1].fields[3] & 0xFFFF0000)
                                    | (static_cast<Word>(lane % 2 ? 100 : -100) & 0xFFFF);
                globals[1] = lane % 2 ? 511 : 512;
                globals[3] = 0;
            }
            if (mutation == 10 && address == kLook)
                globals[0] = kStates[1];
            if (mutation == 12 && address == kResume) {
                globals[0] = kStates[1];
                SetByte(states[1].fields[2], 40);
            }
            if (mutation == 12 && address == kHud)
                SetByte(states[Current()].fields[2], 1);
            if (mutation == 14 && address == kResolve)
                globals[5] = 3;
            if (mutation == 15 && address == kEvent)
                globals[5] = 3;
        }
    }
    Word initial_index { };
    void Event(Word address, std::initializer_list<Word> args = { }, Word result = 0)
    {
        ++counts[address];
        calls.insert(calls.end(), { address, static_cast<Word>(args.size()) });
        calls.insert(calls.end(), args);
        for (std::size_t i = args.size(); i < 4; ++i)
            calls.push_back(0);
        calls.insert(calls.end(),
            { result, globals[0], globals[1], globals[2], globals[3], globals[5],
                states[0].fields[1], states[1].fields[1], globals[4] });
        Mutate(address);
    }
    b::SequenceAction& Queued() override
    {
        active = &At(Current(), states[Current()].fields[0] & 3);
        return *active;
    }
    b::SequenceAction& Step(std::size_t index) override
    {
        active = &At(Current(), index);
        return *active;
    }
    Word ChainCount() override { return states[Current()].fields[1]; }
    int NotificationPending() override
    {
        Event(kNotification, { }, services[0]);
        return Signed(services[0]);
    }
    Word ActorFlags(int actor) override
    {
        Event(kFlags, { static_cast<Word>(actor) }, services[1]);
        return services[1];
    }
    int FreeAssetRequest() override
    {
        Event(kAsset, { }, services[2]);
        return Signed(services[2]);
    }
    int CanStartEffect() override
    {
        Event(kCanStart, { }, services[3]);
        return Signed(services[3]);
    }
    void AdvanceEffectPhase() override { Event(kAdvance); }
    Word EffectBusyFlags() override { return globals[4]; }
    void StartAnimation() override { Event(kAnimation); }
    void StartEffectTrack() override { Event(kTrack, { 0 }); }
    void ReleaseAttacks() override { Event(kRelease); }
    void FadeTrails() override { Event(kFade); }
    int AnimationBusy(int actor) override
    {
        const auto result = ActorResult(7, actor);
        Event(kBusy, { static_cast<Word>(actor) }, result);
        return Signed(result);
    }
    int BeginDefeat(int actor) override
    {
        const auto result = ActorResult(10, actor);
        Event(kDefeat, { static_cast<Word>(actor) }, result);
        return Signed(result);
    }
    void BeginRecovery(int mode) override { Event(kRecovery, { Raw(mode) }); }
    int CameraMode() override { return Signed(globals[3]); }
    int Projection() override { return Signed(globals[1]); }
    void ApplyProjection(int projection) override
    {
        globals[1] = Raw(projection);
        Event(kProjection, { Raw(projection) });
    }
    int Distance() override { return Signed(globals[2]); }
    void SetDistance(int value) override { globals[2] = Raw(value); }
    std::int16_t PreferredDistance() override
    {
        return std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(states[Current()].fields[3]));
    }
    int LookAtOnAshley() override
    {
        Event(kLook, { }, services[5]);
        return Signed(services[5]);
    }
    void ResumeField() override { Event(kResume); }
    void ShowHud() override { Event(kHud, { 0 }); }
    std::uint8_t SelectedActor() override
    {
        return static_cast<std::uint8_t>(states[Current()].fields[2]);
    }
    void RestoreIdle(int actor) override { Event(kIdle, { static_cast<Word>(actor) }); }
    void ResolveDefeat(int source, int target) override
    {
        Event(kResolve, { static_cast<Word>(source), static_cast<Word>(target), 1 });
    }
    Word ControlMode() override { return globals[5]; }
    std::int32_t EvaluateRoomEvents() override
    {
        Event(kEvent, { 12, 0, 0, 0 }, services[6]);
        return Signed(services[6]);
    }
    void BeginCutscene() override { Event(kCutscene); }
    void OpenLoot() override { Event(kLoot); }
};
int Action::RebuildPlans()
{
    const auto result = world->services[4];
    world->Event(kPlans, { Address() }, result);
    return Signed(result);
}
void Print(const World& world)
{
    std::cout << world.diagnostics;
    for (auto value : world.globals)
        std::cout << ' ' << value;
    for (auto value : world.guards)
        std::cout << ' ' << value;
    for (const auto& state : world.states) {
        for (auto value : state.fields)
            std::cout << ' ' << value;
    }
    for (const auto& action : world.actions) {
        for (auto value : action.raw)
            std::cout << ' ' << value;
    }
    std::cout << ' ' << world.calls.size();
    for (auto value : world.calls)
        std::cout << ' ' << value;
    std::cout << '\n';
}
}

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty())
            continue;
        std::istringstream input(line);
        World world;
        input >> world.function >> world.lane >> world.mutation;
        for (auto& value : world.services)
            input >> value;
        for (auto& value : world.globals)
            input >> value;
        for (auto& value : world.guards)
            input >> value;
        for (auto& state : world.states) {
            for (auto& value : state.fields)
                input >> value;
        }
        for (auto& action : world.actions) {
            for (auto& value : action.raw)
                input >> value;
        }
        if (!input)
            return 2;
        world.initial_index = world.states[0].fields[0];
        switch (world.function) {
        case 0:
            b::WaitForActionEffectReady(world);
            break;
        case 1:
            b::StartReadyActionAnimation(world);
            break;
        case 2:
            b::WaitForActionResolution(world);
            break;
        case 3:
            world.diagnostics = b::WaitForTargetRecovery(world)
                             == b::TargetRecoveryStatus::kInvalidSequence;
            break;
        case 4:
            b::FinishActionSequence(world);
            break;
        default:
            return 3;
        }
        Print(world);
    }
}
