// Native side of tools\port\differentials\event_camera_ownership.py.
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <source_location>
#include <sstream>
#include <string>
#include <vector>

#include "Actors/ActorStateLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Event/EventCameraOwnershipLive.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Field/CameraLive.hpp"
#include "Render/ScreenOverlayLive.hpp"

extern "C" {
#include "Event/Camera.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
}

extern "C" {
char Event_skipNextFieldCameraRestore = 0;
Event_CameraState* Event_camera = nullptr;
CVECTOR Event_kDefaultScreenTint = {0x80, 0x80, 0x80, 0};
Render_FramebufferEffectTranslation Event_kDefaultScreenTransform = {0x1000, 0x1000, 0x1000, 0};
}

namespace {
using Word = std::uint32_t;
constexpr std::size_t kFunctionCount = 6;
constexpr std::size_t kCameraWords = sizeof(Event_CameraState) / sizeof(Word);
static_assert(sizeof(Event_CameraState) == 0x28C);

// Call kinds shared with the case.
constexpr Word kCallAllocate = 1;
constexpr Word kCallFree = 2;
constexpr Word kCallPosition = 3;
constexpr Word kCallLookAt = 4;
constexpr Word kCallScreen = 10;
constexpr Word kCallResolve = 20;
constexpr Word kCallPlacement = 21;

struct Vector {
    unsigned function = 0;
    unsigned camera_code = 0;
    Word skip = 0;
    Word argument = 0;
    unsigned odd = 0;
    Word reference = 0;
    Word resolved = 0;
    unsigned actor_present = 0;
    std::array<Word, 3> position {};
    Word facing = 0;
    std::array<Word, 3> field_eye {};
    std::array<Word, 3> field_look_at {};
    std::array<Word, kCameraWords> block {};

    bool Read(std::istream& input)
    {
        if (!(input >> function >> camera_code >> skip >> argument >> odd >> reference >> resolved
                >> actor_present)) {
            return false;
        }
        for (auto* group : { &position, &field_eye }) {
            for (auto& value : *group) {
                if (!(input >> value))
                    return false;
            }
            if (group == &position && !(input >> facing))
                return false;
        }
        for (auto& value : field_look_at) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : block) {
            if (!(input >> value))
                return false;
        }
        return function < kFunctionCount && camera_code < 3;
    }
};

alignas(8) Event_CameraState camera_block {};
alignas(8) Event_CameraState previous_block {};
const Vector* current = nullptr;
std::vector<std::array<Word, 3>> calls;

Word PointerCode(const void* pointer)
{
    if (pointer == nullptr)
        return 0;
    if (pointer == &camera_block)
        return 1;
    return pointer == &previous_block ? 2 : 3;
}

Word ExpectedPointerCode(const void* pointer, const void* expected)
{
    if (pointer == nullptr)
        return 0;
    return pointer == expected ? 1 : 2;
}

Word CameraChecksum()
{
    Word value = 0x811C9DC5U;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&camera_block);
    for (std::size_t index = 0; index < sizeof(camera_block); ++index) {
        value = (value ^ bytes[index]) * 0x01000193U;
    }
    return value;
}

void RecordScreen(Word index, Word argument)
{
    calls.push_back({ kCallScreen + index, argument, CameraChecksum() });
}

void WriteFieldVector(VECTOR* out, const std::array<Word, 3>& words)
{
    out->vx = std::bit_cast<std::int32_t>(words[0]);
    out->vy = std::bit_cast<std::int32_t>(words[1]);
    out->vz = std::bit_cast<std::int32_t>(words[2]);
    out->pad = 0;
}

std::int16_t Half(Word value)
{
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
} // namespace

namespace VS::Core::Memory {
void* AllocateLiveFromHighEnd(std::size_t bytes, std::source_location)
{
    calls.push_back({ kCallAllocate, static_cast<Word>(bytes), 0 });
    return &camera_block;
}

void FreeLiveFromHighEnd(const void* payload, std::source_location)
{
    calls.push_back({ kCallFree, PointerCode(payload), 0 });
}
} // namespace VS::Core::Memory

namespace VS::Field {
void LiveCameraPosition(VECTOR* out_position)
{
    calls.push_back({ kCallPosition, 0, 0 });
    WriteFieldVector(out_position, current->field_eye);
}

void LiveCameraLookAt(VECTOR* out_look_at)
{
    calls.push_back({ kCallLookAt, 0, 0 });
    WriteFieldVector(out_look_at, current->field_look_at);
}
} // namespace VS::Field

namespace VS::Render {
void SetLiveFramebufferEffectEnabled(int enabled) { RecordScreen(0, static_cast<Word>(enabled)); }

void SetLiveFramebufferEffectRotation(int roll_angle) { RecordScreen(1, static_cast<Word>(roll_angle)); }

void SetLiveFramebufferEffectScale(const Render_FramebufferEffectTranslation* scale)
{
    RecordScreen(2, ExpectedPointerCode(scale, &Event_kDefaultScreenTransform));
}

void SetLiveFramebufferEffectTint(const CVECTOR* tint)
{
    RecordScreen(3, ExpectedPointerCode(tint, &Event_kDefaultScreenTint));
}

void SetLiveFramebufferEffectOffset(const Render_FramebufferEffectTranslation* offset)
{
    RecordScreen(4, ExpectedPointerCode(offset, &Event_kDefaultScreenTransform));
}

void SetLiveFramebufferEffectBlendMode(int blend_mode) { RecordScreen(5, static_cast<Word>(blend_mode)); }
} // namespace VS::Render

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
    calls.push_back({ kCallPlacement, static_cast<Word>(actor_id), 0 });
    if (current->actor_present == 0)
        return std::nullopt;
    return ActorPlacementView { .tile = { .x = 4, .layer = 3, .z = 2, .spare = 1 },
        .position = { .x = Half(current->position[0]),
            .y = Half(current->position[1]),
            .z = Half(current->position[2]),
            .w = Half(current->facing) } };
}
} // namespace VS::Actors

namespace {
void Run(const Vector& value)
{
    current = &value;
    calls.clear();
    static_assert(sizeof(camera_block) == sizeof(value.block));
    std::memcpy(&camera_block, value.block.data(), sizeof(camera_block));
    std::memset(&previous_block, 0, sizeof(previous_block));
    const std::array<Event_CameraState*, 3> cameras { nullptr, &camera_block, &previous_block };
    Event_camera = cameras[value.camera_code];
    Event_skipNextFieldCameraRestore = static_cast<char>(value.skip);
    std::array<std::uint8_t, 8> instruction {};
    instruction[value.odd] = 0xE8;
    instruction[value.odd + 1] = static_cast<std::uint8_t>(value.reference);
    instruction[value.odd + 2] = static_cast<std::uint8_t>(value.reference >> 8);

    Word result = 0;
    switch (value.function) {
    case 0:
        result = static_cast<Word>(Event_CommandSetCameraOriginActorOpcode(&instruction[value.odd], 0));
        break;
    case 1:
        VS::Event::InitializeEventCameraEntry();
        break;
    case 2:
        // F01215 is retired: LiveVmHost::FreeEventCamera frees the workspace directly.
        VS::Core::Memory::FreeLiveFromHighEnd(Event_camera);
        break;
    case 3:
        VS::Event::SetEventFieldCameraOverrideEntry(std::bit_cast<int>(value.argument));
        break;
    case 4:
        VS::Event::SetEventCameraOriginActorEntry(static_cast<std::uint16_t>(value.argument));
        break;
    case 5:
        VS::Event::UpdateEventCameraOriginEntry();
        break;
    default:
        std::abort();
    }

    std::printf("%u %u %u ", PointerCode(Event_camera),
        static_cast<unsigned>(static_cast<std::uint8_t>(Event_skipNextFieldCameraRestore)), result);
    std::array<Word, kCameraWords> words {};
    std::memcpy(words.data(), &camera_block, sizeof(camera_block));
    for (const Word word : words)
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
