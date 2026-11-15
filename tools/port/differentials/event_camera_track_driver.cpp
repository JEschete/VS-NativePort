// Native side of tools\port\differentials\event_camera_track.py.
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
#include "Event/EventCameraTrackLive.hpp"
#include "Event/InterpolationCurveLive.h"
#include "Event/ScriptVmLive.hpp"
#include "Field/CollisionMapLive.hpp"

extern "C" {
#include "Event/Camera.h"
#include "Event/ScriptCamera.h"
#include "State/Timers.h"
}

extern "C" {
Event_CameraState* Event_camera = nullptr;
int State_tickSpeed = 0;
}

namespace {
using Word = std::uint32_t;
constexpr std::size_t kFunctionCount = 7;
constexpr std::size_t kCameraWords = sizeof(Event_CameraState) / sizeof(Word);
constexpr std::size_t kInstructionBytes = 12;
constexpr std::size_t kMatrixWords = 8;
constexpr std::size_t kDestWords = 4;
static_assert(sizeof(Event_CameraState) == 0x28C);
static_assert(sizeof(MATRIX) == kMatrixWords * sizeof(Word));
static_assert(sizeof(VECTOR) == kDestWords * sizeof(Word));

// The actor placement query's output words, which the native workspace leaves alone.
constexpr std::size_t kFirstSkippedWord = 0x1D0 / sizeof(Word);
constexpr std::size_t kSkippedWords = 3;

// Call kinds shared with the case; the bone query adds its endpoint selector.
constexpr Word kCallResolve = 20;
constexpr Word kCallPlacement = 21;
constexpr Word kCallBone = 30;
constexpr Word kCallFloor = 40;
constexpr Word kCallEase = 50;
constexpr std::array<Word, 6> kFloorEdges { 0x8000, 0x7FFF, 0x4000, 0xC000, 0x3FFF, 0 };
constexpr Word kEaseSalt = 0x5A5A;

struct Vector {
    unsigned function = 0;
    unsigned odd = 0;
    unsigned eye = 0;
    Word tick = 0;
    Word selector = 0;
    Word seed = 0;
    Word floor_mode = 0;
    Word floor_base = 0;
    Word ease_mode = 0;
    Word resolved = 0;
    Word placement_present = 0;
    std::array<Word, 4> placement { };
    Word bone_present = 0;
    std::array<Word, 3> bone { };
    std::array<Word, kInstructionBytes> instruction { };
    std::array<Word, kMatrixWords> matrix { };
    std::array<Word, kDestWords> dest { };
    std::array<Word, kCameraWords> block { };

    bool Read(std::istream& input)
    {
        if (!(input >> function >> odd >> eye >> tick >> selector >> seed >> floor_mode
                >> floor_base >> ease_mode >> resolved >> placement_present)) {
            return false;
        }
        for (auto& value : placement) {
            if (!(input >> value))
                return false;
        }
        if (!(input >> bone_present))
            return false;
        for (auto& value : bone) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : instruction) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : matrix) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : dest) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : block) {
            if (!(input >> value))
                return false;
        }
        return function < kFunctionCount && odd < 2 && eye < 2;
    }
};

alignas(8) Event_CameraState camera_block { };
const Vector* current = nullptr;
std::vector<std::array<Word, 3>> calls;

std::int16_t Half(Word value)
{
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

// The stubs' shared hash; the case's mix.
Word Mix(Word first, Word second, Word seed)
{
    Word value = first * 0x9E3779B1U + second * 0x85EBCA77U + seed;
    value ^= value >> 15;
    value *= 0x2C1B3C6DU;
    return value ^ (value >> 12);
}

Word FloorWord(Word x, Word z)
{
    const Word hashed = Mix(x, z, current->seed);
    switch (current->floor_mode) {
    case 0:
        return current->floor_base;
    case 1:
        return current->floor_base + (hashed & 0xFF) - 0x80;
    case 2:
        return hashed;
    default:
        return kFloorEdges.at(hashed % kFloorEdges.size());
    }
}

Word EaseWord(Word curve, Word frame, Word duration)
{
    const Word hashed =
        Mix(curve | frame << 8 | duration << 16, kEaseSalt, current->seed);
    switch (current->ease_mode) {
    case 0:
        return duration == 0 ? 4096 : frame * 4096 / duration;
    case 1:
        return hashed & 0x1FFF;
    case 2:
        return hashed;
    default:
        return std::array<Word, 3> { 0, 2048, 4096 }.at(hashed % 3);
    }
}
} // namespace

extern "C" int Event_EvaluateInterpolationCurveEntry(
    int16_t mode, int16_t frame, int16_t duration)
{
    const auto curve = static_cast<Word>(static_cast<std::uint16_t>(mode));
    const auto elapsed = static_cast<Word>(static_cast<std::uint16_t>(frame));
    const auto length = static_cast<Word>(static_cast<std::uint16_t>(duration));
    calls.push_back({ kCallEase, curve, elapsed | length << 16 });
    return std::bit_cast<int>(EaseWord(curve, elapsed, length));
}

namespace VS::Field {
std::int16_t LiveFloorHeightAt(int world_x, int world_z)
{
    const auto x = std::bit_cast<Word>(world_x);
    const auto z = std::bit_cast<Word>(world_z);
    calls.push_back({ kCallFloor, x, z });
    return Half(FloorWord(x, z));
}
} // namespace VS::Field

namespace VS::Event {
int ResolveLiveScriptActorReference(std::uint16_t reference)
{
    calls.push_back({ kCallResolve, reference, 0 });
    return std::bit_cast<int>(current->resolved);
}
} // namespace VS::Event

namespace VS::Actors {
std::optional<ActorPlacementView> LiveActorPlacementView(int actor_id)
{
    calls.push_back({ kCallPlacement, std::bit_cast<Word>(actor_id), 0 });
    if (current->placement_present == 0)
        return std::nullopt;
    return ActorPlacementView { .tile = { .x = 4, .layer = 3, .z = 2, .spare = 1 },
        .position = { .x = Half(current->placement[0]),
            .y = Half(current->placement[1]),
            .z = Half(current->placement[2]),
            .w = Half(current->placement[3]) } };
}

int LiveActorTaggedBonePosition(
    int actor_id, int tag, SVECTOR* position_out, int endpoint)
{
    calls.push_back({ kCallBone + std::bit_cast<Word>(endpoint),
        std::bit_cast<Word>(actor_id), std::bit_cast<Word>(tag) });
    if (current->bone_present == 0)
        return -1;
    position_out->vx = Half(current->bone[0]);
    position_out->vy = Half(current->bone[1]);
    position_out->vz = Half(current->bone[2]);
    return 0;
}
} // namespace VS::Actors

namespace {
void Run(const Vector& value)
{
    current = &value;
    calls.clear();
    static_assert(sizeof(camera_block) == sizeof(value.block));
    std::memcpy(&camera_block, value.block.data(), sizeof(camera_block));
    Event_camera = &camera_block;
    State_tickSpeed = std::bit_cast<int>(value.tick);
    std::array<std::uint8_t, kInstructionBytes + 4> instruction { };
    for (std::size_t index = 0; index < kInstructionBytes; ++index)
        instruction.at(value.odd + index) =
            static_cast<std::uint8_t>(value.instruction.at(index));
    MATRIX matrix { };
    std::memcpy(&matrix, value.matrix.data(), sizeof(matrix));
    VECTOR dest { };
    std::memcpy(&dest, value.dest.data(), sizeof(dest));
    Event_CameraTrack* track = value.eye != 0 ? &camera_block.eye : &camera_block.lookAt;
    Event_CameraTrack* other = value.eye != 0 ? &camera_block.lookAt : &camera_block.eye;

    Word result = 0;
    switch (value.function) {
    case 0:
        result = static_cast<Word>(
            VS::Event::CommandCameraMotionEntry(&instruction.at(value.odd), 0));
        break;
    case 1:
        VS::Event::UpdateEventCameraTrackEntry(track, other);
        break;
    case 2:
        VS::Event::AdvanceTerrainCameraTrackEntry(track);
        break;
    case 3:
        result = std::bit_cast<Word>(VS::Event::EventCameraGroundClearanceEntry(&matrix));
        break;
    case 4:
        VS::Event::ScaleEventCameraTrackDurationEntry(track);
        break;
    case 5:
        VS::Event::ScaleEventCameraDurationEntry(Half(value.selector));
        break;
    case 6:
        result = VS::Event::ReadScriptFixedVectorEntry(&dest, &instruction.at(value.odd))
                      == &dest
                   ? 1
                   : 2;
        break;
    default:
        std::abort();
    }

    std::printf("%u ", result);
    std::array<Word, kCameraWords> words { };
    std::memcpy(words.data(), &camera_block, sizeof(camera_block));
    for (std::size_t index = 0; index < kCameraWords; ++index) {
        if (index >= kFirstSkippedWord && index < kFirstSkippedWord + kSkippedWords)
            continue;
        std::printf("%u ", words.at(index));
    }
    std::array<Word, kDestWords> dest_words { };
    std::memcpy(dest_words.data(), &dest, sizeof(dest));
    for (const Word word : dest_words)
        std::printf("%u ", word);
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
