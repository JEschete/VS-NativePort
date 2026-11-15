// Native side of tools\port\differentials\event_camera_control.py.
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Actors/ActorPlacement.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Event/EventCameraControlLive.hpp"
#include "Event/InterpolationCurveLive.h"
#include "Event/ScriptVmLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Event/Camera.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
#include "Event/ScriptVm.h"
#include "State/Timers.h"
}

extern "C" {
Event_CameraState* Event_camera = nullptr;
int State_tickSpeed = 0;
char Event_skipNextFieldCameraRestore = 0;
short Event_isFieldCameraRestoreScripted = 0;
}

namespace {
using Word = std::uint32_t;
constexpr std::size_t kFunctionCount = 7;
constexpr std::size_t kCameraWords = sizeof(Event_CameraState) / sizeof(Word);
constexpr std::size_t kInstructionBytes = 3;
constexpr std::size_t kPlacementWords = 12;
static_assert(sizeof(Event_CameraState) == 0x28C);

constexpr Word kFacingFlag = 0xAF;
constexpr Word kDistanceFlag = 0xB0;
// Call kinds shared with the case; the motion services the started motions never use print kind 99.
constexpr Word kCallSpherical = 60;
constexpr Word kCallFlag = 70;
constexpr Word kCallUnexpected = 99;

struct Vector {
    unsigned function = 0;
    Word argument0 = 0;
    Word argument1 = 0;
    std::array<Word, kInstructionBytes> instruction { };
    Word skip = 0;
    Word scripted = 0;
    std::array<Word, 2> flags { };
    std::array<Word, kPlacementWords> placements { };
    std::array<Word, kCameraWords> block { };

    bool Read(std::istream& input)
    {
        if (!(input >> function >> argument0 >> argument1))
            return false;
        for (auto& value : instruction) {
            if (!(input >> value))
                return false;
        }
        if (!(input >> skip >> scripted))
            return false;
        for (auto& value : flags) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : placements) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : block) {
            if (!(input >> value))
                return false;
        }
        return function < kFunctionCount;
    }
};

alignas(8) Event_CameraState camera_block { };
const Vector* current = nullptr;
std::vector<std::array<Word, 3>> calls;
std::size_t placements_made = 0;

std::int16_t Half(Word value)
{
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
} // namespace

extern "C" int Event_EvaluateInterpolationCurveEntry(int16_t /*mode*/, int16_t /*frame*/, int16_t /*duration*/)
{
    calls.push_back({ kCallUnexpected, 1, 0 });
    return 0;
}

namespace VS::Field {
std::int16_t LiveFloorHeightAt(int /*world_x*/, int /*world_z*/)
{
    calls.push_back({ kCallUnexpected, 2, 0 });
    return 0;
}

void LiveInitializeSphericalCamera(VECTOR* out_position, VECTOR* out_look_at, int facing, int mode)
{
    calls.push_back({ kCallSpherical, std::bit_cast<Word>(facing), std::bit_cast<Word>(mode) });
    std::array<Word, 6> words { };
    if (placements_made < 2) {
        for (std::size_t index = 0; index < words.size(); ++index)
            words.at(index) = current->placements.at(placements_made * words.size() + index);
    }
    ++placements_made;
    out_position->vx = std::bit_cast<std::int32_t>(words[0]);
    out_position->vy = std::bit_cast<std::int32_t>(words[1]);
    out_position->vz = std::bit_cast<std::int32_t>(words[2]);
    out_look_at->vx = std::bit_cast<std::int32_t>(words[3]);
    out_look_at->vy = std::bit_cast<std::int32_t>(words[4]);
    out_look_at->vz = std::bit_cast<std::int32_t>(words[5]);
}
} // namespace VS::Field

namespace VS::State {
std::uint8_t LiveScriptFlag(int id)
{
    const auto ident = std::bit_cast<Word>(id);
    calls.push_back({ kCallFlag, ident, 0 });
    if (ident == kFacingFlag)
        return static_cast<std::uint8_t>(current->flags[0]);
    if (ident == kDistanceFlag)
        return static_cast<std::uint8_t>(current->flags[1]);
    return 0;
}
} // namespace VS::State

namespace VS::Event {
int ResolveLiveScriptActorReference(std::uint16_t /*reference*/)
{
    calls.push_back({ kCallUnexpected, 3, 0 });
    return 0;
}
} // namespace VS::Event

namespace VS::Actors {
std::optional<ActorPlacementView> LiveActorPlacementView(int /*actor_id*/)
{
    calls.push_back({ kCallUnexpected, 4, 0 });
    return std::nullopt;
}

int LiveActorTaggedBonePosition(int /*actor_id*/, int /*tag*/, SVECTOR* /*position_out*/, int /*endpoint*/)
{
    calls.push_back({ kCallUnexpected, 5, 0 });
    return -1;
}
} // namespace VS::Actors

namespace {
void Run(const Vector& value)
{
    current = &value;
    calls.clear();
    placements_made = 0;
    static_assert(sizeof(camera_block) == sizeof(value.block));
    std::memcpy(&camera_block, value.block.data(), sizeof(camera_block));
    Event_camera = &camera_block;
    Event_skipNextFieldCameraRestore = static_cast<char>(value.skip);
    Event_isFieldCameraRestoreScripted = Half(value.scripted);
    std::array<std::uint8_t, kInstructionBytes + 1> instruction { };
    for (std::size_t index = 0; index < kInstructionBytes; ++index)
        instruction.at(index) = static_cast<std::uint8_t>(value.instruction.at(index));

    Word result = 0;
    switch (value.function) {
    case 0:
        result = static_cast<Word>(Event_CommandStopCameraMotionOpcode(instruction.data(), 0));
        break;
    case 1:
        result = static_cast<Word>(Event_CommandSetCameraFollowingOpcode(instruction.data(), 0));
        break;
    case 2:
        result = static_cast<Word>(Event_CommandRestoreFieldCameraOpcode(instruction.data(), 0));
        break;
    case 3:
        VS::Event::RestoreEventFieldCameraEntry(Half(value.argument0), std::bit_cast<int>(value.argument1));
        break;
    case 4:
        VS::Event::SetEventCameraFollowingEntry(
            std::bit_cast<int>(value.argument0), std::bit_cast<int>(value.argument1));
        break;
    case 5:
        VS::Event::StopEventCameraMotionEntry();
        break;
    case 6:
        VS::Event::StartEventCameraOrbitReturnEntry(static_cast<char>(value.argument0 & 0xFF));
        break;
    default:
        std::abort();
    }

    std::printf("%u ", result);
    std::array<Word, kCameraWords> words { };
    std::memcpy(words.data(), &camera_block, sizeof(camera_block));
    for (const Word word : words)
        std::printf("%u ", word);
    std::printf("%u %u ", static_cast<Word>(static_cast<std::uint8_t>(Event_skipNextFieldCameraRestore)),
        static_cast<Word>(static_cast<std::uint16_t>(Event_isFieldCameraRestoreScripted)));
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
