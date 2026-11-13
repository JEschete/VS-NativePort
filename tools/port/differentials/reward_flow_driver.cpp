// The shared R3000 differential runs the original reward cluster.
#include "Battle/RewardFlow.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace b = VS::Battle;
using Word = std::uint32_t;
using LootId = VS::Menu::Framework::LootListId;
constexpr Word kRoomPalette = 0x8007B10C, kClut = 0x80048FEC;
constexpr Word kUnfreeze = 0x8006C2A0, kRoom = 0x8008B4BC, kMinimap = 0x8008B4C8;
constexpr Word kAmbience = 0x8008E480, kSceneColors = 0x8007B1B8, kFreeze = 0x8006C250;
constexpr Word kIdle = 0x8009E5C4, kFlag = 0x800BEBF4, kPending = 0x8006F760;
constexpr Word kPose = 0x8009E070, kEffect = 0x8006F848, kPlatforms = 0x8008D594;
constexpr Word kNotice = 0x800CB2B8, kAbilities = 0x800CB208, kLootMenu = 0x800CB158;
constexpr Word kResults = 0x800CB23C, kUpdate = 0x800C97BC;
constexpr Word kMenuState = 0x800CB66C, kRelease = 0x800CB114;
constexpr std::array<Word, 2> kStates { 0x80500000, 0x80504000 };
constexpr std::array<Word, 2> kMenus { 0x80700000, 0x80700010 };
constexpr std::array<Word, 3> kHeads { 0, 0x80800000, 0x80800040 };
constexpr Word kMask = 0xFFFFFFFF;

struct State {
    Word selection { }, primary { }, position { }, continued { }, guard { };
};
class World final : public b::RewardWorld {
public:
    Word function { }, lane { }, mutation { }, seed { }, pending { }, changed_id { };
    Word phase { }, follow { }, paused { }, loot { }, global { }, menu { };
    std::array<Word, 3> b6 { };
    std::array<Word, 2> ab { }, menus { };
    std::array<State, 2> states { };
    std::array<Word, 6> guards { };
    Word unlocked { }, original_follow { }, original_pending { };
    std::array<Word, 256> definitions { };
    std::array<Word, 3> counts { };
    Word diagnostics { };
    std::vector<Word> calls, writes;

    int Current() const
    {
        return global == kStates[0] ? 0 : global == kStates[1] ? 1 : -1;
    }
    Word Selected() const
    {
        return Current() < 0 ? kMask : states[Current()].selection & 0xFFFF;
    }
    Word MenuWord() const
    {
        return menu == kMenus[0] ? menus[0] : menu == kMenus[1] ? menus[1] : kMask;
    }
    void Mutate(Word address)
    {
        const Word ordinal = static_cast<Word>(calls.size() / 17);
        if (mutation == 1) {
            phase = 0xA1000000 | ordinal;
            follow = (lane + ordinal) % 3 == 0 ? 0 : 0xB2000000 | ordinal;
            paused = 0xC3000000 | ordinal;
            guards[0] = 0xD4000000 | ordinal;
        }
        if (mutation == 2 && address == kAmbience)
            follow = original_follow ? 0 : 0x80000000;
        if ((mutation == 3 || mutation == 5) && address == kRoomPalette) {
            global = kStates[1];
            states[1].selection = states[1].selection & 0xFFFF0000 | changed_id;
        }
        if ((mutation == 4 || mutation == 5) && address == kNotice) {
            const int target = mutation == 5 ? 0 : 1;
            global = kStates[target];
            states[target].selection = states[target].selection & 0xFFFF0000 | changed_id;
        }
        if ((mutation == 1 || mutation == 5) && address == kRoomPalette)
            pending = original_pending ^ kMask;
        if (mutation == 6 && address == kPose) {
            follow = 0;
            phase = 0xAABBCCDD;
        }
        if (mutation == 7 && address == kLootMenu) {
            loot = loot == kHeads[2] ? kHeads[1] : kHeads[2];
            phase = 0xFACEB00C;
        }
        if (mutation == 8 && address == kUpdate)
            menu = kMenus[1];
    }
    void Event(Word address, std::initializer_list<Word> args = { }, Word result = 0)
    {
        calls.insert(calls.end(), { address, static_cast<Word>(args.size()) });
        calls.insert(calls.end(), args);
        for (std::size_t i = args.size(); i < 5; ++i)
            calls.push_back(0);
        const Word action = Selected();
        const Word status = menu == kMenus[0] || menu == kMenus[1] ? MenuWord() & 0xFFFF : kMask;
        calls.insert(calls.end(), { result, phase, follow, paused, loot, global,
                                     menu, action, status,
                                     action < definitions.size() ? definitions[action] : kMask });
        Mutate(address);
    }
    void Write(Word tag, Word index = 0)
    {
        writes.insert(writes.end(), { tag, index, 4 });
    }
    void SetPhase(b::RewardPhase value) override
    {
        phase = static_cast<Word>(value);
        Write(1);
    }
    void SetInteractionPaused(bool value) override
    {
        paused = value ? 1 : 0;
        Write(2);
    }
    std::int32_t FollowAction() override { return std::bit_cast<std::int32_t>(follow); }
    std::uint8_t ScriptFlag(int flag) override
    {
        const int slot = flag == 0xB6 ? 0 : 1;
        Word result = 0;
        if (flag == 0xB6)
            result = b6[counts[0] < 3 ? counts[0] : 2];
        else if (flag == 0xAB)
            result = ab[counts[1] < 2 ? counts[1] : 1];
        else
            ++diagnostics;
        ++counts[slot];
        Event(kFlag, { static_cast<Word>(flag) }, result);
        return static_cast<std::uint8_t>(result);
    }
    Word PendingUnlocks() override
    {
        const Word result = pending;
        ++counts[2];
        Event(kPending, { }, result);
        return result;
    }
    b::RewardSelection Selection() override
    {
        if (Current() < 0) {
            ++diagnostics;
            return { };
        }
        const auto& state = states[Current()];
        return { .action_id = static_cast<std::uint16_t>(state.selection),
            .targets_position = state.position,
            .continued = static_cast<std::uint8_t>(state.continued >> 24),
            .primary_actor = static_cast<std::uint8_t>(state.primary) };
    }
    bool DefinitionUnlocked(std::uint16_t action) override
    {
        if (action >= definitions.size()) {
            ++diagnostics;
            return false;
        }
        return (definitions[action] & 0x8000) != 0;
    }
    void MarkDefinitionUnlocked(std::uint16_t action) override
    {
        if (action >= definitions.size()) {
            ++diagnostics;
            return;
        }
        definitions[action] |= 0x8000;
        Write(3, action);
    }
    void RoomPalettes(const VS::Field::ColorTransform& value) override
    {
        Colors(kRoomPalette, value);
    }
    void SceneColors(const VS::Field::ColorTransform& value) override
    {
        Colors(kSceneColors, value);
    }
    void Colors(Word address, const VS::Field::ColorTransform& value)
    {
        Event(address, { static_cast<Word>(value.mode), static_cast<Word>(value.duration),
                           static_cast<Word>(value.red_offset), static_cast<Word>(value.green_offset),
                           static_cast<Word>(value.blue_offset) });
    }
    void EnableClutAnimation(bool value) override { Event(kClut, { value ? 1U : 0U }); }
    void FreezeActorAnimations() override { Event(kFreeze); }
    void UnfreezeActorAnimations() override { Event(kUnfreeze); }
    void PauseRoomAnimations(bool value) override { Event(kRoom, { value ? 1U : 0U }); }
    void ShowMinimap(bool value) override { Event(kMinimap, { value ? 1U : 0U }); }
    void DuckAmbience(bool value) override { Event(kAmbience, { value ? 1U : 0U }); }
    void RestoreIdlePose() override { Event(kIdle, { 0 }); }
    void PlayLearnedPose() override { Event(kPose, { 0, 0, 6 }); }
    void SpawnLearnedEffect() override { Event(kEffect); }
    void PauseMovingPlatforms() override { Event(kPlatforms, { 1 }); }
    void ShowActionLearned(std::uint16_t action) override { Event(kNotice, { 12, action, 1 }); }
    void OpenAbilityUnlocks(std::uint16_t art, std::uint16_t ability) override
    {
        Event(kAbilities, { art, ability });
    }
    LootId LootHead() override { return LootId { loot }; }
    void OpenLoot(LootId owner) override { Event(kLootMenu, { static_cast<Word>(owner) }); }
    void OpenResults() override { Event(kResults); }
    void UpdateMenu() override { Event(kUpdate); }
    std::int16_t MenuStatus() override
    {
        const Word owner = menu;
        Event(kMenuState, { }, owner);
        if (owner != kMenus[0] && owner != kMenus[1]) {
            ++diagnostics;
            return 1;
        }
        return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
            menus[owner == kMenus[0] ? 0 : 1]));
    }
    void ReleaseMenuRows() override { Event(kRelease); }
};

void Read(std::istream& input, World& world)
{
    input >> world.function >> world.lane >> world.mutation >> world.seed >> world.pending
        >> world.changed_id >> world.phase >> world.follow >> world.paused >> world.loot
        >> world.global >> world.menu;
    for (auto& value : world.b6)
        input >> value;
    for (auto& value : world.ab)
        input >> value;
    for (auto& value : world.menus)
        input >> value;
    for (auto& state : world.states)
        input >> state.selection >> state.primary >> state.position >> state.continued >> state.guard;
    for (auto& value : world.guards)
        input >> value;
    input >> world.unlocked;
    world.original_follow = world.follow;
    world.original_pending = world.pending;
    for (Word index = 0; index < world.definitions.size(); ++index) {
        Word value = world.seed ^ (index * 0x1020304);
        if (index == (world.states[0].selection & 0xFFFF))
            value = value & ~0x8000U | world.unlocked << 15;
        world.definitions[index] = value;
    }
}

void Run(World& world)
{
    switch (world.function) {
    case 0:
        b::BeginRewardPause(world);
        break;
    case 1:
        b::EndRewardPause(world);
        break;
    case 2:
        b::ResumeAfterRewards(world);
        break;
    case 3:
        b::CheckRewardUnlocks(world);
        break;
    case 4:
        b::ShowPendingRewardUnlock(world);
        break;
    case 5:
        b::OpenPendingLootRewards(world);
        break;
    case 6:
        b::OpenRewardResults(world);
        break;
    case 7:
        b::UpdateRewardResultsDialog(world);
        break;
    default:
        ++world.diagnostics;
    }
}

void Print(const World& world)
{
    std::vector<Word> output { world.diagnostics, world.phase, world.follow, world.paused,
        world.loot, world.global, world.menu, world.pending };
    output.insert(output.end(), world.counts.begin(), world.counts.end());
    output.insert(output.end(), world.menus.begin(), world.menus.end());
    output.insert(output.end(), world.guards.begin(), world.guards.end());
    for (const auto& state : world.states)
        output.insert(output.end(), { state.selection, state.primary, state.position, state.continued, state.guard });
    output.insert(output.end(), world.definitions.begin(), world.definitions.end());
    output.push_back(static_cast<Word>(world.writes.size()));
    output.insert(output.end(), world.writes.begin(), world.writes.end());
    output.push_back(static_cast<Word>(world.calls.size()));
    output.insert(output.end(), world.calls.begin(), world.calls.end());
    for (const Word value : output)
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
        Read(input, world);
        if (!input)
            return 2;
        Run(world);
        Print(world);
    }
}
