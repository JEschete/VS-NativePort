// Native side of tools\port\differentials\event_scene_transition.py.
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Actors/RoomEnemyMemoryLive.hpp"
#include "Audio/MusicLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Battle/RewardFlowLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Event/ScriptAudio.h"
#include "Event/ScriptControlOpcodes.h"
#include "Event/ScriptVm.h"
#include "Field/Mpd.h"
}

extern "C" {
char Event_isMenuTransitionActive = 0;
char Event_musicSlot = 0;
short Event_roomChangeState = 0;
}

namespace {
using Word = std::uint32_t;
constexpr std::size_t kFunctionCount = 4;
constexpr std::size_t kInstructionBytes = 10;

// Call kinds shared with the case; a transition is followed by its door's three words.
constexpr Word kCallForget = 10;
constexpr Word kCallHud = 20;
constexpr Word kCallGetFlag = 30;
constexpr Word kCallSetFlag = 31;
constexpr Word kCallEventRewards = 40;
constexpr Word kCallPostActionRewards = 41;
constexpr Word kCallUpdateRewards = 42;
constexpr Word kCallOpenMenu = 50;
constexpr Word kCallUpdateOverlay = 51;
constexpr Word kCallSelection = 52;
constexpr Word kCallReleaseRows = 53;
constexpr Word kCallMusicSwitch = 60;
constexpr Word kCallRoom = 70;
constexpr Word kCallZone = 80;

struct Vector {
    unsigned function = 0;
    std::array<Word, kInstructionBytes> instruction { };
    Word active = 0;
    Word music_slot = 0;
    Word room_state = 0;
    Word flag = 0;
    Word rewards_running = 0;
    Word menu_status = 0;

    bool Read(std::istream& input)
    {
        if (!(input >> function))
            return false;
        for (auto& value : instruction) {
            if (!(input >> value))
                return false;
        }
        if (!(input >> active >> music_slot >> room_state >> flag >> rewards_running
                >> menu_status))
            return false;
        return function < kFunctionCount;
    }
};

const Vector* current = nullptr;
std::vector<std::array<Word, 3>> calls;
Menu_Framework_ActionSelection selection { };

Word Bits(int value) { return std::bit_cast<Word>(value); }

void RecordTransition(Word kind, const Field_MpdDoorRecord* door, int effect, int speed)
{
    static_assert(sizeof(Field_MpdDoorRecord) == 3 * sizeof(Word));
    std::array<Word, 3> words { };
    std::memcpy(words.data(), door, sizeof(words));
    calls.push_back({ kind, Bits(effect), Bits(speed) });
    calls.push_back({ kind + 1, words[0], words[1] });
    calls.push_back({ kind + 2, words[2], 0 });
}
} // namespace

namespace VS::Actors {
void ForgetLiveRoomEnemies(int map, int zone)
{
    calls.push_back({ kCallForget, Bits(map), Bits(zone) });
}
} // namespace VS::Actors

namespace VS::Battle {
int SetLiveGameplayHudHidden(int command)
{
    calls.push_back({ kCallHud, Bits(command), 0 });
    return 0;
}

void BeginLiveEventRewardResults() { calls.push_back({ kCallEventRewards, 0, 0 }); }

void BeginLivePostActionRewards() { calls.push_back({ kCallPostActionRewards, 0, 0 }); }

int UpdateLiveRewardSequence()
{
    calls.push_back({ kCallUpdateRewards, 0, 0 });
    return std::bit_cast<int>(current->rewards_running);
}
} // namespace VS::Battle

namespace VS::State {
std::uint8_t LiveScriptFlag(int id)
{
    calls.push_back({ kCallGetFlag, Bits(id), 0 });
    return static_cast<std::uint8_t>(current->flag);
}

void SetLiveScriptFlag(int id, int value)
{
    calls.push_back({ kCallSetFlag, Bits(id), Bits(value) });
}
} // namespace VS::State

namespace VS::Menu::Framework {
int OpenLiveGameMenu(int menu_state, int mode)
{
    calls.push_back({ kCallOpenMenu, Bits(menu_state), Bits(mode) });
    return 0;
}

void UpdateLiveMenuOverlay() { calls.push_back({ kCallUpdateOverlay, 0, 0 }); }

Menu_Framework_ActionSelection* LiveActionSelectionState()
{
    calls.push_back({ kCallSelection, 0, 0 });
    selection.menuStatus =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(current->menu_status));
    return &selection;
}

void ReleaseLiveMenuRows() { calls.push_back({ kCallReleaseRows, 0, 0 }); }
} // namespace VS::Menu::Framework

namespace VS::Audio {
int LiveSetZoneMusicSwitchEnabled(int enabled)
{
    calls.push_back({ kCallMusicSwitch, Bits(enabled), 0 });
    return 0;
}
} // namespace VS::Audio

namespace VS::Field {
void LiveBeginScriptedRoomTransition(
    const Field_MpdDoorRecord* door, int effect, int speed)
{
    RecordTransition(kCallRoom, door, effect, speed);
}

void LiveBeginScriptedZoneTransition(
    const Field_MpdDoorRecord* door, int effect, int speed)
{
    RecordTransition(kCallZone, door, effect, speed);
}
} // namespace VS::Field

namespace {
void Run(const Vector& value)
{
    current = &value;
    calls.clear();
    Event_isMenuTransitionActive = static_cast<char>(value.active);
    Event_musicSlot = static_cast<char>(value.music_slot);
    Event_roomChangeState =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value.room_state));
    std::array<std::uint8_t, kInstructionBytes> instruction { };
    for (std::size_t index = 0; index < kInstructionBytes; ++index)
        instruction.at(index) = static_cast<std::uint8_t>(value.instruction.at(index));

    std::intptr_t result = 0;
    switch (value.function) {
    case 0:
        result = Event_ScriptClearRoomActorCacheOpcode(instruction.data(), 0);
        break;
    case 1:
        result = Event_ScriptSetHudVisibleOpcode(instruction.data(), 0);
        break;
    case 2:
        result = Event_ScriptRunMenuTransitionOpcode(instruction.data(), 0);
        break;
    case 3:
        result = Event_ScriptChangeRoomOpcode(instruction.data(), 0);
        break;
    default:
        std::abort();
    }

    std::printf("%u %u %u %u ", static_cast<Word>(result),
        static_cast<Word>(static_cast<std::uint8_t>(Event_isMenuTransitionActive)),
        static_cast<Word>(static_cast<std::uint8_t>(Event_musicSlot)),
        static_cast<Word>(static_cast<std::uint16_t>(Event_roomChangeState)));
    std::printf("%zu", calls.size());
    for (const auto& call : calls)
        std::printf(" %u %u %u", call[0], call[1], call[2]);
    std::printf("\n");
}
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty())
            continue;
        std::istringstream input(line);
        Vector value;
        if (!value.Read(input))
            return 2;
        Run(value);
    }
}
