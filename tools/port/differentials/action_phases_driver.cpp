// Semantic service fixture; the shared differential executes the original instructions.
#include "Battle/ActionPhases.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace b = VS::Battle;
using Word = std::uint32_t;
constexpr Word kIdle = 0x8009E5C4, kFlags = 0x800A0BE0, kPalette = 0x8007B10C;
constexpr Word kFreeze = 0x8006C250, kClut = 0x80048FEC, kRoom = 0x8008B4BC;
constexpr Word kMarkers = 0x80095B70, kBone = 0x800A152C, kFade = 0x8006C4A4;
constexpr Word kWeapon = 0x8007C0AC;
constexpr std::array<Word, 2> kStates { 0x80500000, 0x80504000 };
constexpr int kSlots = 17;

struct State {
    Word index { }, primary { }, queue { }, indexed_queue { }, guard { };
};
struct Actor {
    Word present { };
    std::array<Word, 2> before { }, after { };
    VS::Actors::ActorCommand command { };
    std::array<Word, 2> flags { };
};
void Read(std::istream& in, State& state)
{
    in >> state.index >> state.primary >> state.queue >> state.indexed_queue
        >> state.guard;
}
void Read(std::istream& in, Actor& actor)
{
    Word command { };
    in >> actor.present >> actor.before[0] >> actor.before[1] >> command;
    actor.command.state = static_cast<std::uint8_t>(command);
    actor.command.opcode = static_cast<std::uint8_t>(command >> 8);
    actor.command.target = static_cast<std::uint8_t>(command >> 16);
    actor.command.flags = static_cast<std::uint8_t>(command >> 24);
    for (auto& argument : actor.command.args) {
        Word raw { };
        in >> raw;
        argument = std::bit_cast<std::int32_t>(raw);
    }
    in >> actor.after[0] >> actor.after[1] >> actor.flags[0] >> actor.flags[1];
}
void SetByte(Word& value, Word byte) { value = (value & 0xFFFFFF00) | (byte & 255); }

class World final : public b::ActionPhaseWorld {
public:
    Word function { }, lane { }, mutation { }, reward { }, bone { };
    Word phase { }, recovery_reward { }, tick { }, control { }, global { }, unrelated { };
    std::array<Word, 9> guards { };
    std::array<State, 2> states { };
    std::array<Actor, kSlots> actors { };
    std::array<Word, kSlots> flag_reads { };
    std::vector<Word> calls, writes;
    Word diagnostics { };

    int Current() const
    {
        if (global == kStates[0])
            return 0;
        if (global == kStates[1])
            return 1;
        return -1;
    }
    void ChangedGlobals()
    {
        const Word ordinal = static_cast<Word>(calls.size() / 15);
        const Word suffix = ((lane & 0xFFFF) << 8) | ordinal;
        phase = 0xA1000000 | suffix;
        recovery_reward = 0xB2000000 | suffix;
        tick = 0xC3000000 | suffix;
        control = 0xD4000000 | suffix;
        unrelated = 0xE5000000 | suffix;
    }
    void Mutate(Word address, std::initializer_list<Word> args)
    {
        const Word slot = args.size() ? *args.begin() : 0;
        if (mutation == 1)
            ChangedGlobals();
        if (function == 1 && mutation == 2) {
            ChangedGlobals();
            global = 0xDEAD0040;
        }
        if (function == 2) {
            const bool first = address == kFlags && flag_reads[slot] == 1;
            const bool second = address == kFlags && flag_reads[slot] == 2;
            const bool idle = address == kIdle;
            if ((mutation == 2 && first) || (mutation == 5 && idle)
                || (mutation == 6 && address == kFlags) || (mutation == 7 && second)) {
                global = kStates[1 - Current()];
                SetByte(states[Current()].primary, slot + 1);
            }
            if ((mutation == 3 && first) || (mutation == 4 && idle)) {
                SetByte(states[Current()].primary, slot + 1);
            }
            if (mutation == 8
                && (address == kPalette || address == kFreeze || address == kClut
                    || address == kRoom || address == kMarkers)) {
                ChangedGlobals();
                global = kStates[1];
                SetByte(states[1].primary, 255);
            }
        }
        if (function == 3) {
            const Word actor = (states_initial_queue + 73) & 255;
            if (address == kBone) {
                if (mutation == 2 || mutation == 4 || mutation == 7)
                    global = kStates[1];
                if (mutation == 3 || mutation == 4 || mutation == 7)
                    SetByte(states[0].queue, actor);
                if (mutation == 5)
                    SetByte(states[0].indexed_queue, actor);
                if (mutation == 6)
                    SetByte(states[0].queue, 255);
                if (mutation == 7) {
                    SetByte(states[1].queue, actor ^ 255);
                    SetByte(states[1].primary, actor ^ 127);
                }
            }
            if (mutation == 8 && address == kFade) {
                SetByte(states[0].queue, actor);
                global = kStates[1];
                ChangedGlobals();
            }
        }
        if (function == 6
            && ((mutation == 2 && address == kWeapon)
                || (mutation == 3 && address == kIdle && slot == 7))) {
            ChangedGlobals();
            global = 0xDEAD0080;
        }
    }
    Word states_initial_queue { };
    void Event(Word address, std::initializer_list<Word> args = { }, Word result = 0)
    {
        calls.push_back(address);
        calls.push_back(static_cast<Word>(args.size()));
        calls.insert(calls.end(), args);
        for (std::size_t i = args.size(); i < 5; ++i)
            calls.push_back(0);
        const int current = Current();
        calls.insert(
            calls.end(), { result, global, phase, recovery_reward, tick, control,
                             current < 0 ? 0xFFFFFFFF : states[current].primary & 255,
                             states[0].queue & 255 });
        Mutate(address, args);
    }
    void Write(Word tag, Word slot, Word size)
    {
        writes.insert(writes.end(), { tag, slot, size });
    }
    bool Slot(int slot)
    {
        if (slot >= 0 && slot < kSlots)
            return true;
        ++diagnostics;
        return false;
    }
    std::optional<VS::Actors::ActorCommand> Command(int slot) override
    {
        if (function != 0)
            ++diagnostics;
        if (!Slot(slot) || !actors[slot].present)
            return std::nullopt;
        return actors[slot].command;
    }
    void SetCommandState(int slot, std::uint8_t state) override
    {
        if (!Slot(slot))
            return;
        actors[slot].command.state = state;
        Write(1, static_cast<Word>(slot), 1);
    }
    std::uint8_t PrimaryActor() override
    {
        if (function != 2 || Current() < 0) {
            ++diagnostics;
            return 255;
        }
        return static_cast<std::uint8_t>(states[Current()].primary);
    }
    Word ActorFlags(int slot) override
    {
        if (function != 2)
            ++diagnostics;
        if (!Slot(slot))
            return 0;
        const Word result = actors[slot].flags[flag_reads[slot] ? 1 : 0];
        ++flag_reads[slot];
        Event(kFlags, { static_cast<Word>(slot) }, result);
        return result;
    }
    void RestoreIdle(int slot) override { Event(kIdle, { static_cast<Word>(slot) }); }
    void RestoreRoomPalettes() override { Event(kPalette, { 8, 4, 0, 0, 0 }); }
    void FreezeAnimations() override { Event(kFreeze); }
    void EnableClutAnimation() override { Event(kClut, { 1 }); }
    void ResumeRoomAnimations() override { Event(kRoom, { 0 }); }
    void ShowActorMarkers() override { Event(kMarkers, { 0 }); }
    void SetRecoveryReward(std::int32_t mode) override
    {
        recovery_reward = std::bit_cast<Word>(mode);
        Write(4, 0, 4);
    }
    void SetTickSpeed(Word speed) override
    {
        tick = speed;
        Write(5, 0, 4);
    }
    void SetPhase(b::ActionPhase value) override
    {
        phase = static_cast<Word>(value);
        Write(3, 0, 4);
    }
    void SetControlMode(VS::Field::FieldControlMode mode) override
    {
        control = static_cast<Word>(mode);
        Write(2, 0, 4);
    }
    int TrailBone(std::uint8_t actor) override
    {
        Event(kBone, { actor, 0xF0, 0 }, bone);
        return std::bit_cast<std::int32_t>(bone);
    }
    void FadeTrails(std::uint8_t actor, int part) override
    {
        Event(kFade, { actor, std::bit_cast<Word>(part) });
    }
    void ToggleAshleyWeapon() override { Event(kWeapon, { 0, 2 }); }
};

class Source final : public b::ActionPhaseSource {
public:
    explicit Source(const State& owner)
        : owner_(owner)
    {
    }
    std::uint8_t Actor() const override
    {
        return static_cast<std::uint8_t>(owner_.queue);
    }

private:
    const State& owner_;
};

void Print(const World& world)
{
    std::vector<Word> output { world.diagnostics, world.phase, world.recovery_reward,
        world.tick, world.control, world.global, world.unrelated };
    output.insert(output.end(), world.guards.begin(), world.guards.end());
    for (const auto& state : world.states) {
        output.insert(output.end(), { state.index, state.primary, state.queue,
                                        state.indexed_queue, state.guard });
    }
    for (const auto& actor : world.actors) {
        const auto& command = actor.command;
        const Word packed = command.state | static_cast<Word>(command.opcode) << 8
                          | static_cast<Word>(command.target) << 16
                          | static_cast<Word>(command.flags) << 24;
        output.insert(output.end(), { actor.before[0], actor.before[1], packed });
        for (const auto argument : command.args)
            output.push_back(std::bit_cast<Word>(argument));
        output.insert(output.end(), actor.after.begin(), actor.after.end());
    }
    output.insert(output.end(), world.flag_reads.begin(), world.flag_reads.end());
    output.push_back(static_cast<Word>(world.writes.size()));
    output.insert(output.end(), world.writes.begin(), world.writes.end());
    output.push_back(static_cast<Word>(world.calls.size()));
    output.insert(output.end(), world.calls.begin(), world.calls.end());
    for (const auto value : output)
        std::cout << value << ' ';
    std::cout << '\n';
}
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        World world;
        std::istringstream input(line);
        input >> world.function >> world.lane >> world.mutation >> world.reward
            >> world.bone;
        input >> world.phase >> world.recovery_reward >> world.tick >> world.control
            >> world.global >> world.unrelated;
        for (auto& guard : world.guards)
            input >> guard;
        for (auto& state : world.states)
            Read(input, state);
        for (auto& actor : world.actors)
            Read(input, actor);
        if (!input)
            return 2;
        world.states_initial_queue = world.states[0].queue;
        const Source source(world.states[0]);
        switch (world.function) {
        case 0:
            b::ReleaseQueuedActorAttacks(world);
            break;
        case 1:
            b::WaitForPostActionMovement(world);
            break;
        case 2:
            b::BeginPostActionRecovery(std::bit_cast<std::int32_t>(world.reward), world);
            break;
        case 3:
            b::BeginActionTrailFade(source, world);
            break;
        case 4:
            b::FinishBattleAbilitySequence(world);
            break;
        case 5:
            b::ResetActionPhase(world);
            break;
        case 6:
            b::BeginAshleyWeaponToggle(world);
            break;
        default:
            return 3;
        }
        Print(world);
    }
}
