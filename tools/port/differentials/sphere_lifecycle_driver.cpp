// Native side of tools\port\differentials\sphere_lifecycle.py.
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <source_location>
#include <string>

#include "Battle/SphereLifecycleLive.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }

extern "C" {
#include "Battle/TargetPages.h"
#include "Battle/TargetingState.h"
}

extern "C" {
Battle_TargetSphereSetupView* Battle_targetSphere = nullptr;
u_char Battle_reuseSphereVolumes = 0;
int Input_buttonsState = 0;

void Battle_ResetTargetPages() {}
}

namespace sphere_lifecycle_test {
Battle_TargetSphereSetupView* allocation_target = nullptr;
unsigned allocations = 0;
int releases = 0;
}  // namespace sphere_lifecycle_test

namespace VS::Core::Memory {
void* AllocateLiveFromLowEnd(std::size_t bytes, std::source_location) {
    if (sphere_lifecycle_test::allocation_target == nullptr
        || bytes != sizeof(*sphere_lifecycle_test::allocation_target)) {
        std::abort();
    }
    ++sphere_lifecycle_test::allocations;
    return sphere_lifecycle_test::allocation_target;
}

void FreeLiveFromLowEnd(const void* payload, std::source_location) {
    if (payload != sphere_lifecycle_test::allocation_target) {
        std::abort();
    }
    ++sphere_lifecycle_test::releases;
}
}  // namespace VS::Core::Memory

namespace {
using Word = std::uint32_t;
constexpr std::size_t kFunctionCount = 8;

struct Vector {
    unsigned function = 0;
    unsigned sphere_exists = 0;
    unsigned reuse = 0;
    unsigned secondary_present = 0;
    unsigned preview_argument_present = 0;
    unsigned preview_already_bound = 0;
    unsigned sphere_mode = 0;
    Word buttons = 0;
    Word phase_a = 0;
    Word phase_b = 0;
    Word phase_c = 0;
    Word wire_shape = 0;
    Word target_count = 0;
    Word selected_target = 0;
    Word initial_mode = 0;
    Word reserved_38 = 0;
    Word reserved_3c = 0;
    std::array<Word, 4> primary_words { };
    std::array<Word, 4> secondary_words { };

    bool Read(std::istream& input)
    {
        if (!(input >> function >> sphere_exists >> reuse >> secondary_present
                >> preview_argument_present >> preview_already_bound >> sphere_mode
                >> buttons >> phase_a >> phase_b >> phase_c >> wire_shape >> target_count
                >> selected_target >> initial_mode >> reserved_38 >> reserved_3c)) {
            return false;
        }
        for (auto& value : primary_words) {
            if (!(input >> value))
                return false;
        }
        for (auto& value : secondary_words) {
            if (!(input >> value))
                return false;
        }
        return function < kFunctionCount;
    }
};

Battle_TargetSphereSpec VolumeFromWords(const std::array<Word, 4>& words)
{
    static_assert(sizeof(Battle_TargetSphereSpec) == sizeof(words));
    return std::bit_cast<Battle_TargetSphereSpec>(words);
}

std::array<Word, 4> WordsFromVolume(const Battle_TargetSphereSpec& volume)
{
    static_assert(sizeof(Battle_TargetSphereSpec) == sizeof(Word) * 4);
    return std::bit_cast<std::array<Word, 4>>(volume);
}

unsigned PointerCode(
    const Battle_TargetPreview* pointer, const Battle_TargetPreview* previews)
{
    if (pointer == nullptr)
        return 0;
    return pointer == previews ? 1 : 2;
}

void PrintWord(Word value) { std::printf("%u ", value); }

Word SphereWord(const Battle_TargetSphereSetupView& sphere,
                std::size_t offset)
{
    Word value = 0;
    std::memcpy(&value, reinterpret_cast<const unsigned char*>(&sphere) + offset,
        sizeof(value));
    return value;
}

void PrintState(const Vector& value, Battle_TargetSphereSetupView& sphere,
    Battle_TargetSphereSetupView* live_sphere, std::uint8_t reuse, int interaction_mode,
    const Battle_TargetPreview* previews, unsigned allocation_count, unsigned zero_count)
{
    PrintWord(live_sphere == nullptr ? 0 : 1);
    PrintWord(reuse);
    for (const Word word : WordsFromVolume(sphere.primaryVolume))
        PrintWord(word);
    for (const Word word : WordsFromVolume(sphere.secondaryVolume))
        PrintWord(word);
    PrintWord(static_cast<std::uint16_t>(sphere.wirePhaseA));
    PrintWord(static_cast<std::uint16_t>(sphere.wirePhaseB));
    PrintWord(static_cast<std::uint16_t>(sphere.wirePhaseC));
    PrintWord(static_cast<std::uint16_t>(sphere.wireShape));
    PrintWord(static_cast<std::uint8_t>(sphere.targetCount));
    PrintWord(static_cast<std::uint8_t>(sphere.selectedTarget));
    PrintWord(sphere.sphereMode);
    PrintWord(std::bit_cast<Word>(sphere.reserved38));
    PrintWord(std::bit_cast<Word>(sphere.reserved3C));
    PrintWord(PointerCode(sphere.targetList0, previews));
    PrintWord(PointerCode(sphere.targetList1, previews));
    if (value.function == 0) {
        for (const auto edge : sphere.wireEdges) {
            PrintWord(std::bit_cast<std::uint16_t>(edge));
        }
        for (const std::size_t offset : { 0x30U, 0x80U, 0x100U, 0x4B18U }) {
            PrintWord(SphereWord(sphere, offset));
        }
    }

    std::array<unsigned, 2> calls { };
    unsigned call_count = 0;
    if (value.function == 0) {
        if (allocation_count != 0)
            calls[call_count++] = 1;
        if (zero_count != 0)
            calls[call_count++] = 2;
    } else if (value.function == 1 && sphere_lifecycle_test::releases != 0) {
        calls[call_count++] = 3;
    }
    if (value.function == 7) {
        PrintWord(static_cast<Word>(interaction_mode));
    }
    PrintWord(call_count);
    for (unsigned index = 0; index < call_count; ++index) {
        PrintWord(calls[index]);
    }
    std::printf("\n");
}

void Run(const Vector& value)
{
    Battle_TargetSphereSetupView sphere { };
    if (value.function == 0) {
        std::memset(&sphere, 0xA5, sizeof(sphere));
    }
    std::array<Battle_TargetPreview, 2> previews { };
    sphere.primaryVolume = VolumeFromWords(value.primary_words);
    sphere.secondaryVolume = VolumeFromWords(value.secondary_words);
    sphere.wirePhaseA =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value.phase_a));
    sphere.wirePhaseB =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value.phase_b));
    sphere.wirePhaseC =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value.phase_c));
    sphere.wireShape =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value.wire_shape));
    sphere.targetCount =
        std::bit_cast<char>(static_cast<std::uint8_t>(value.target_count));
    sphere.selectedTarget =
        std::bit_cast<char>(static_cast<std::uint8_t>(value.selected_target));
    sphere.sphereMode = static_cast<u_char>(value.initial_mode);
    sphere.reserved38 = std::bit_cast<int>(value.reserved_38);
    sphere.reserved3C = std::bit_cast<int>(value.reserved_3c);
    if (value.preview_already_bound != 0)
        sphere.targetList1 = previews.data();

    Battle_targetSphere = value.sphere_exists != 0 ? &sphere : nullptr;
    Battle_reuseSphereVolumes = static_cast<u_char>(value.reuse);
    Input_buttonsState = value.buttons;
    sphere_lifecycle_test::allocation_target = &sphere;
    sphere_lifecycle_test::allocations = 0;
    sphere_lifecycle_test::releases = 0;
    unsigned zero_count = 0;
    int interaction_mode = 0;

    switch (value.function) {
    case 0:
        VS::Battle::InitializeSphereEntry();
        ++zero_count;
        break;
    case 1:
        VS::Battle::FreeSphereEntry();
        break;
    case 2:
        VS::Battle::SetSphereVolumesEntry(&sphere.primaryVolume,
            value.secondary_present != 0 ? &sphere.secondaryVolume : nullptr,
            static_cast<u_char>(value.sphere_mode),
            value.preview_argument_present != 0 ? previews.data() : nullptr);
        break;
    case 3:
        VS::Battle::ClearPrimarySphereVolumeEntry();
        break;
    case 4:
        VS::Battle::SetSecondarySphereVolumeEntry(&sphere.secondaryVolume);
        break;
    case 5:
        VS::Battle::ClearSecondarySphereVolumeEntry();
        break;
    case 6:
        VS::Battle::ClearSphereTargetsEntry();
        break;
    case 7:
        interaction_mode = VS::Battle::SphereInteractionModeEntry();
        break;
    default:
        std::abort();
    }

    PrintState(value, sphere, Battle_targetSphere, Battle_reuseSphereVolumes,
        interaction_mode, previews.data(), sphere_lifecycle_test::allocations,
        zero_count);
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
