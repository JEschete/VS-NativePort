// The shared R3000 differential executes the original reward dialog routines.
#include "Battle/RewardFlow.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace b = VS::Battle;
using Word = std::uint32_t;
using LootId = VS::Menu::Framework::LootListId;
constexpr Word kUpdate = 0x800C97BC, kMenuState = 0x800CB66C, kRelease = 0x800CB114;
constexpr Word kFieldResume = 0x8006C40C, kBeginAbility = 0x8006F89C,
               kGrantAbility = 0x8006FA20;
constexpr Word kNotification = 0x800CB45C, kReleaseNotification = 0x800CB50C;
constexpr Word kClut = 0x80048FEC, kSceneColors = 0x8007B1B8, kFreeze = 0x8006C250;
constexpr Word kRoom = 0x8008B4BC, kAmbience = 0x8008E480;
constexpr Word kIdle = 0x8009E5C4, kFlag = 0x800BEBF4, kPending = 0x8006F760;
constexpr Word kPose = 0x8009E070, kEffect = 0x8006F848, kPlatforms = 0x8008D594;
constexpr std::array<Word, 2> kStates { 0x80500000, 0x80504000 };
constexpr std::array<Word, 2> kMenus { 0x80700000, 0x80700010 };
constexpr Word kMask = 0xFFFFFFFF;
constexpr Word kEventWidth = 20;

struct State {
    Word selection { }, primary { }, position { }, continued { }, guard { };
};
class World final : public b::RewardDialogWorld {
public:
    Word function { }, lane { }, mutation { }, seed { }, pending { }, notification { },
        menu { }, changed_id { };
    Word phase { }, follow { }, loot { }, paused { }, effects { }, global { };
    std::array<Word, 3> flags { };
    std::array<Word, 2> menus { };
    std::array<State, 2> states { };
    std::array<Word, 4> guards { };
    Word unlocked { };
    std::array<Word, 256> definitions { };
    std::array<Word, 2> counts { };
    std::vector<Word> calls, writes;

    int Current() const
    {
        return global == kStates[0] ? 0 : global == kStates[1] ? 1 : -1;
    }
    Word Selected() const
    {
        return Current() < 0
                 ? kMask
                 : states.at(static_cast<std::size_t>(Current())).selection & 0xFFFF;
    }
    Word MenuWord() const
    {
        return menu == kMenus[0] ? menus[0] : menu == kMenus[1] ? menus[1] : kMask;
    }
    static void Toggle(Word& value) { value = value ? 0 : 0x80000000; }
    void Mutate(Word address, Word result)
    {
        if (address == kUpdate) {
            if (mutation == 1 || mutation == 3 || mutation == 4 || mutation == 13)
                menu = kMenus[1];
            if (mutation == 1 || mutation == 2 || mutation == 13) {
                Toggle(follow);
                Toggle(loot);
            }
            if (mutation == 2)
                menus[0] = menus[0] & 0xFFFF0000 | (lane % 2 ? 1U : 0xFFFFU);
            if (mutation == 3)
                notification = notification ? 0 : 0x80000000;
            if (mutation == 4)
                notification = 0;
            if (mutation == 13)
                global = kStates[1];
        }
        if (mutation == 5 && address == kFieldResume) {
            global = kStates[1];
            states[1].selection = states[1].selection & 0xFFFF0000 | changed_id;
        }
        if ((mutation == 6 || mutation == 9 || mutation == 12) && address == kRelease) {
            if (mutation == 6) {
                global = kStates[1];
                Toggle(follow);
                Toggle(loot);
                notification ^= kMask;
            }
            if (mutation == 9)
                menu = kMenus[1];
            if (mutation == 12) {
                global = 0;
                pending = 1;
            }
        }
        if (mutation == 7 && address == kReleaseNotification) {
            global = kStates[1];
            states[1].selection = states[1].selection & 0xFFFF0000 | changed_id;
            pending ^= 0x10001;
        }
        if (mutation == 8 && address == kAmbience) {
            Toggle(follow);
            Toggle(loot);
        }
        if (mutation == 10 && address == kNotification) {
            notification ^= kMask;
            phase = 0xFACEB00C;
        }
        if (mutation == 11 && address == kMenuState) {
            menu = result == kMenus[0] ? kMenus[1] : kMenus[0];
            auto& word = menus.at(result == kMenus[0] ? 0 : 1);
            word = word & 0xFFFF0000 | (lane % 2 ? 1U : 0x101U);
        }
        if (mutation == 14) {
            const Word ordinal = static_cast<Word>(calls.size() / kEventWidth);
            phase = 0xA1000000 | ordinal;
            paused = 0xB2000000 | ordinal;
            guards[0] = 0xC3000000 | ordinal;
        }
        if (mutation == 15 && address == kPose) {
            follow = 0;
            phase = 0xAABBCCDD;
        }
    }
    void Event(Word address, std::initializer_list<Word> args = { }, Word result = 0)
    {
        calls.insert(calls.end(), { address, static_cast<Word>(args.size()) });
        calls.insert(calls.end(), args);
        for (std::size_t i = args.size(); i < 5; ++i)
            calls.push_back(0);
        const Word action = Selected();
        const Word status =
            menu == kMenus[0] || menu == kMenus[1] ? MenuWord() & 0xFFFF : kMask;
        calls.insert(calls.end(),
            { result, phase, follow, loot, paused, effects, global, menu, notification,
                pending, action, status,
                action < definitions.size() ? definitions.at(action) : kMask });
        Mutate(address, result);
    }
    void SetPhase(b::RewardPhase value) override
    {
        phase = static_cast<Word>(value);
        writes.insert(writes.end(), { 1, 4 });
    }
    void SetInteractionPaused(bool value) override
    {
        paused = value ? 1U : 0U;
        writes.insert(writes.end(), { 2, 4 });
    }
    std::int32_t FollowAction() override { return std::bit_cast<std::int32_t>(follow); }
    std::int32_t LootMenuOpened() override { return std::bit_cast<std::int32_t>(loot); }
    Word EffectsBusyFlags() override { return effects; }
    std::int32_t SceneNotificationPending() override
    {
        const Word result = notification;
        Event(kNotification, { }, result);
        return std::bit_cast<std::int32_t>(result);
    }
    std::uint8_t ScriptFlag(int flag) override
    {
        if (flag != 0xB6)
            throw std::out_of_range("unexpected story flag");
        const Word result = flags.at(counts[0] < 3 ? counts[0] : 2);
        ++counts[0];
        Event(kFlag, { static_cast<Word>(flag) }, result);
        return static_cast<std::uint8_t>(result);
    }
    Word PendingUnlocks() override
    {
        const Word result = pending;
        ++counts[1];
        Event(kPending, { }, result);
        return result;
    }
    b::RewardSelection Selection() override
    {
        if (Current() < 0)
            throw std::out_of_range("missing used action owner");
        const auto& state = states.at(static_cast<std::size_t>(Current()));
        return { .action_id = static_cast<std::uint16_t>(state.selection),
            .targets_position = state.position,
            .continued = static_cast<std::uint8_t>(state.continued >> 24),
            .primary_actor = static_cast<std::uint8_t>(state.primary) };
    }
    bool DefinitionUnlocked(std::uint16_t action) override
    {
        return (definitions.at(action) & 0x8000) != 0;
    }
    void MarkDefinitionUnlocked(std::uint16_t action) override
    {
        definitions.at(action) |= 0x8000;
    }
    void RoomPalettes(const VS::Field::ColorTransform&) override
    {
        throw std::logic_error("unexpected pause");
    }
    void SceneColors(const VS::Field::ColorTransform& value) override
    {
        Event(kSceneColors,
            { static_cast<Word>(value.mode), static_cast<Word>(value.duration),
                static_cast<Word>(value.red_offset),
                static_cast<Word>(value.green_offset),
                static_cast<Word>(value.blue_offset) });
    }
    void EnableClutAnimation(bool value) override { Event(kClut, { value ? 1U : 0U }); }
    void FreezeActorAnimations() override { Event(kFreeze); }
    void UnfreezeActorAnimations() override
    {
        throw std::logic_error("unexpected unfreeze");
    }
    void PauseRoomAnimations(bool value) override { Event(kRoom, { value ? 1U : 0U }); }
    void ShowMinimap(bool) override { throw std::logic_error("unexpected minimap"); }
    void DuckAmbience(bool value) override { Event(kAmbience, { value ? 1U : 0U }); }
    void RestoreIdlePose() override { Event(kIdle, { 0 }); }
    void PlayLearnedPose() override { Event(kPose, { 0, 0, 6 }); }
    void SpawnLearnedEffect() override { Event(kEffect); }
    void PauseMovingPlatforms() override { Event(kPlatforms, { 1 }); }
    void ShowActionLearned(std::uint16_t) override
    {
        throw std::logic_error("unexpected reward notice");
    }
    void OpenAbilityUnlocks(std::uint16_t, std::uint16_t) override
    {
        throw std::logic_error("unexpected reward unlock");
    }
    LootId LootHead() override { throw std::logic_error("unexpected loot head"); }
    void OpenLoot(LootId) override { throw std::logic_error("unexpected loot menu"); }
    void OpenResults() override { throw std::logic_error("unexpected results"); }
    void UpdateMenu() override { Event(kUpdate); }
    std::int16_t MenuStatus() override
    {
        const Word owner = menu;
        Event(kMenuState, { }, owner);
        if (owner != kMenus[0] && owner != kMenus[1])
            throw std::out_of_range("missing used menu owner");
        return std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(menus.at(owner == kMenus[0] ? 0 : 1)));
    }
    void ReleaseMenuRows() override { Event(kRelease); }
    void ResumeFieldAfterMenu() override { Event(kFieldResume); }
    void ReleaseSceneNotification() override { Event(kReleaseNotification); }
    void BeginPendingAbilityReward() override { Event(kBeginAbility); }
    void GrantPendingAbilityUnlock() override { Event(kGrantAbility); }
};

void Read(std::istream& input, World& world)
{
    input >> world.function >> world.lane >> world.mutation >> world.seed >> world.pending
        >> world.notification >> world.menu >> world.changed_id >> world.phase
        >> world.follow >> world.loot >> world.paused >> world.effects >> world.global;
    for (auto& value : world.flags)
        input >> value;
    for (auto& value : world.menus)
        input >> value;
    for (auto& state : world.states)
        input >> state.selection >> state.primary >> state.position >> state.continued
            >> state.guard;
    for (auto& value : world.guards)
        input >> value;
    input >> world.unlocked;
    for (Word index = 0; index < world.definitions.size(); ++index) {
        Word value = world.seed ^ index * 0x1020304;
        if (index == (world.states[0].selection & 0xFFFF))
            value = value & ~0x8000U | world.unlocked << 15;
        world.definitions.at(index) = value;
    }
}

void Run(World& world)
{
    switch (world.function) {
    case 0:
        b::UpdateLootResults(world);
        break;
    case 1:
        b::UpdateAbilityUnlockDelay(world);
        break;
    case 2:
        b::UpdateAbilityUnlockResults(world);
        break;
    case 3:
        b::UpdatePendingLootDialog(world);
        break;
    default:
        throw std::out_of_range("unknown continuation");
    }
}
void Print(const World& world)
{
    std::cout << 0 << ' ' << world.phase << ' ' << world.follow << ' ' << world.loot
              << ' ' << world.paused << ' ' << world.effects << ' ' << world.global << ' '
              << world.menu << ' ' << world.notification << ' ' << world.pending;
    for (Word value : world.counts)
        std::cout << ' ' << value;
    for (Word value : world.menus)
        std::cout << ' ' << value;
    for (Word value : world.guards)
        std::cout << ' ' << value;
    for (const auto& state : world.states)
        std::cout << ' ' << state.selection << ' ' << state.primary << ' '
                  << state.position << ' ' << state.continued << ' ' << state.guard;
    for (Word value : world.definitions)
        std::cout << ' ' << value;
    std::cout << ' ' << world.writes.size();
    for (Word value : world.writes)
        std::cout << ' ' << value;
    std::cout << ' ' << world.calls.size();
    for (Word value : world.calls)
        std::cout << ' ' << value;
    std::cout << '\n';
}
} // namespace

int main()
{
    for (std::string line; std::getline(std::cin, line);) {
        std::istringstream input(line);
        World world;
        Read(input, world);
        Run(world);
        Print(world);
    }
}
