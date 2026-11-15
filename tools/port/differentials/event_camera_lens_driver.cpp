// Native side of tools\port\differentials\event_camera_lens.py.
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

#include "Core/Math/ScalarMath.hpp"
#include "Event/EventCameraLensLive.hpp"

extern "C" {
#include "Core/Scratch.h"
#include "Event/Camera.h"
#include "Event/InterpolationCurveLive.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
#include "State/Timers.h"
}

namespace {
using Word = std::uint32_t;
constexpr std::size_t kFunctionCount = 4;
constexpr std::size_t kCameraWords = sizeof(Event_CameraState) / sizeof(Word);
static_assert(sizeof(Event_CameraState) == 0x28C);
constexpr std::size_t kEnvelopeScratch = 0x88;
constexpr std::size_t kShakeScratch = 0xD8;
constexpr std::size_t kShakeScratchWords = 6;
constexpr std::size_t kEaseCount = 1;
constexpr std::size_t kSineCount = 4;
constexpr std::size_t kInstructionBytes = 6;
constexpr std::array<std::size_t, 3> kLensTweens { 0x180, 0x18C, 0x198 };
constexpr std::array<std::size_t, 2> kOscillators { 0x1A6, 0x1B2 };

// Call kinds shared with the case.
constexpr Word kCallEase = 1;
constexpr Word kCallSine = 2;

struct Vector {
    unsigned function = 0;
    unsigned odd = 0;
    unsigned target = 0;
    Word tick_speed = 0;
    std::array<Word, 1 + kShakeScratchWords> scratch { };
    std::array<Word, kEaseCount> eases { };
    std::array<Word, kSineCount> sines { };
    std::array<Word, kInstructionBytes> instruction { };
    std::array<Word, kCameraWords> block { };

    bool Read(std::istream& input)
    {
        if (!(input >> function >> odd >> target >> tick_speed))
            return false;
        for (auto& value : scratch) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : eases) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : sines) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : instruction) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : block) {
            if (!(input >> value))
                return false;
        }
        return function < kFunctionCount && odd < 4 && target < kLensTweens.size();
    }
};

alignas(8) Event_CameraState camera_block { };
alignas(8) std::array<unsigned char, 0x400> scratch_block { };
const Vector* current = nullptr;
std::vector<std::array<Word, 4>> calls;
std::size_t ease_index = 0;
std::size_t sine_index = 0;

Word ScratchWord(std::size_t offset)
{
    Word value = 0;
    std::memcpy(&value, scratch_block.data() + offset, sizeof(value));
    return value;
}

void SetScratchWord(std::size_t offset, Word value)
{
    std::memcpy(scratch_block.data() + offset, &value, sizeof(value));
}
} // namespace

extern "C" {
Event_CameraState* Event_camera = nullptr;
int State_tickSpeed = 0;

void* Core_ScratchBase(void) { return scratch_block.data(); }

int Event_EvaluateInterpolationCurveEntry(int16_t mode, int16_t frame, int16_t duration)
{
    calls.push_back({ kCallEase, static_cast<Word>(static_cast<int>(mode)),
        static_cast<Word>(static_cast<int>(frame)),
        static_cast<Word>(static_cast<int>(duration)) });
    const Word value = ease_index < kEaseCount ? current->eases.at(ease_index) : 0;
    ++ease_index;
    return std::bit_cast<int>(value);
}
}

namespace VS::Core::Math {
std::int32_t Sine(std::int32_t angle) noexcept
{
    calls.push_back({ kCallSine, static_cast<Word>(angle), 0, 0 });
    const Word value = sine_index < kSineCount ? current->sines.at(sine_index) : 0;
    ++sine_index;
    return std::bit_cast<std::int32_t>(value);
}
} // namespace VS::Core::Math

namespace {
void Run(const Vector& value)
{
    current = &value;
    calls.clear();
    ease_index = 0;
    sine_index = 0;
    static_assert(sizeof(camera_block) == sizeof(value.block));
    std::memcpy(&camera_block, value.block.data(), sizeof(camera_block));
    Event_camera = &camera_block;
    State_tickSpeed = std::bit_cast<int>(value.tick_speed);
    scratch_block.fill(0);
    SetScratchWord(kEnvelopeScratch, value.scratch[0]);
    for (std::size_t index = 0; index < kShakeScratchWords; ++index) {
        SetScratchWord(kShakeScratch + index * 4, value.scratch.at(index + 1));
    }
    std::array<std::uint8_t, 16> instruction { };
    for (std::size_t index = 0; index < kInstructionBytes; ++index) {
        instruction.at(value.odd + index) =
            static_cast<std::uint8_t>(value.instruction.at(index));
    }
    auto* bytes = reinterpret_cast<unsigned char*>(&camera_block);

    Word result = 0;
    switch (value.function) {
    case 0:
        result = static_cast<Word>(
            Event_CommandCameraLensOrShakeOpcode(&instruction.at(value.odd), 0));
        break;
    case 1:
        VS::Event::UpdateEventLensTweenEntry(
            reinterpret_cast<Event_LensTween*>(bytes + kLensTweens.at(value.target)));
        break;
    case 2:
        result = static_cast<Word>(VS::Event::UpdateEventShakeOscillatorEntry(
            reinterpret_cast<Event_ShakeOscillator*>(
                bytes + kOscillators.at(value.target))));
        break;
    case 3:
        VS::Event::UpdateEventCameraShakeEntry();
        break;
    default:
        std::abort();
    }

    std::printf("%u %u ", result, ScratchWord(kEnvelopeScratch));
    for (std::size_t index = 0; index < kShakeScratchWords; ++index) {
        std::printf("%u ", ScratchWord(kShakeScratch + index * 4));
    }
    std::array<Word, kCameraWords> words { };
    std::memcpy(words.data(), &camera_block, sizeof(camera_block));
    for (const Word word : words)
        std::printf("%u ", word);
    std::printf("%zu", calls.size());
    for (const auto& call : calls) {
        std::printf(" %u %u %u %u", call[0], call[1], call[2], call[3]);
    }
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
