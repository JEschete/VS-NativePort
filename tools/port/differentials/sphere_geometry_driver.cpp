#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <type_traits>

#include "Battle/SphereGeometryLive.hpp"

namespace sphere_geometry_test {
std::array<std::byte, 0x400> scratch{};
}

extern "C" {
#include "Battle/Sphere.h"
#include "Core/Diagnostics.h"
#include "Core/Scratch.h"

short Battle_sphereArcBases[502]{};
short Battle_sphereMirroredVertices[430]{};
u_char Battle_sphereShapeCommandStarts[488]{};
u_short Battle_sphereWireframeCommands[218]{};
u_char Battle_reuseSphereVolumes = 0;
Battle_TargetSphereSetupView* Battle_targetSphere = nullptr;

void* Core_ScratchBase(void) { return sphere_geometry_test::scratch.data(); }
void Core_Errorf(const char*, const char*, ...) { std::abort(); }
}

namespace {
namespace battle = VS::Battle;
constexpr std::size_t kArcBasisCount = 12;
constexpr std::size_t kSineCount = 32;
constexpr std::size_t kArcBaseCount = 72;
constexpr std::size_t kMirroredCount = 186;
constexpr std::size_t kShapeStartCount = 52;
constexpr std::size_t kCommandCount = 218;
constexpr std::int16_t kSentinel = -1234;

struct Input {
    int operation { };
    int phase { };
    int segments { };
    int index { };
    int angle { };
    int step { };
    int radius { };
    int type { };
    std::array<std::int16_t, 4> state { };
    battle::TargetVolumeExtents extents { };
    std::array<std::int16_t, kArcBasisCount> basis { };
    std::array<std::int16_t, kSineCount> sine { };
    std::array<std::int16_t, kArcBaseCount> arc_bases { };
    std::array<std::int16_t, kMirroredCount> mirrored { };
    std::array<std::uint8_t, kShapeStartCount> starts { };
    std::array<std::uint16_t, kCommandCount> commands { };
};

std::int16_t SignedShort(int value)
{
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

bool ReadArray(auto& values)
{
    int value = 0;
    for (auto& item : values) {
        if (!(std::cin >> value))
            return false;
        item = static_cast<std::remove_reference_t<decltype(item)>>(value);
    }
    return true;
}

bool Read(Input& input)
{
    if (!(std::cin >> input.operation >> input.phase >> input.segments >> input.index
            >> input.angle >> input.step >> input.radius >> input.type)) {
        return false;
    }
    std::array<int, 4> values { };
    if (!ReadArray(values))
        return false;
    for (std::size_t index = 0; index < input.state.size(); ++index) {
        input.state[index] = SignedShort(values[index]);
    }
    std::array<int, 4> extents { };
    if (!ReadArray(extents))
        return false;
    for (std::size_t index = 0; index < input.extents.size(); ++index) {
        input.extents[index] = static_cast<std::uint8_t>(extents[index]);
    }
    if (!ReadArray(input.basis) || !ReadArray(input.sine) || !ReadArray(input.arc_bases)
        || !ReadArray(input.mirrored) || !ReadArray(input.starts)
        || !ReadArray(input.commands)) {
        return false;
    }
    return true;
}

template <typename Value> void Print(Value value)
{
    std::cout << ' ' << static_cast<int>(value);
}

void PrintVertices(const Battle_TargetSphereSetupView& sphere)
{
    for (const auto& vertex : sphere.wireVertices) {
        for (const auto component : vertex.xyz)
            Print(component);
        Print(vertex.opaque6);
    }
}

void Run(const Input& input)
{
    Battle_TargetSphereSetupView sphere{};
    Battle_targetSphere = &sphere;
    for (auto& vertex : sphere.wireVertices) {
        for (auto& component : vertex.xyz)
            component = kSentinel;
        vertex.opaque6 = kSentinel;
    }
    std::fill(sphere_geometry_test::scratch.begin(),
        sphere_geometry_test::scratch.end(), std::byte{});
    std::memcpy(sphere_geometry_test::scratch.data() + 0x3B0,
        input.sine.data(), input.sine.size() * sizeof(input.sine[0]));
    std::memcpy(sphere_geometry_test::scratch.data() + 0x398,
        input.basis.data(), input.basis.size() * sizeof(input.basis[0]));

    std::cout << 1;
    switch (input.operation) {
    case 0: {
        Print(battle::AppendSphereArcVerticesEntry(
            input.phase, input.segments, input.index));
        PrintVertices(sphere);
        break;
    }
    case 1: {
        std::array<u_char, 4> scale{};
        std::copy(input.extents.begin(), input.extents.end(), scale.begin());
        const auto* matrix =
            battle::BuildSphereVolumeTransformEntry(scale.data(), input.angle);
        const auto* values = reinterpret_cast<const std::int16_t*>(matrix);
        for (std::size_t index = 0; index < 9; ++index) Print(values[index]);
        break;
    }
    case 2: {
        Print(battle::AppendSphereCircleVerticesEntry(input.angle, input.index));
        PrintVertices(sphere);
        break;
    }
    case 3: {
        Print(battle::AppendSphereConeVerticesEntry(
            input.step, input.radius, input.index));
        const auto* basis =
            reinterpret_cast<const std::int16_t*>(
                sphere_geometry_test::scratch.data() + 0x398);
        for (std::size_t index = 0; index < 9; ++index) Print(basis[index]);
        PrintVertices(sphere);
        break;
    }
    case 4: {
        sphere.wirePhaseA = input.state[0];
        sphere.wirePhaseB = input.state[1];
        sphere.wirePhaseC = input.state[2];
        sphere.wireShape = input.state[3];
        std::copy(input.arc_bases.begin(), input.arc_bases.end(),
            Battle_sphereArcBases);
        std::copy(input.mirrored.begin(), input.mirrored.end(),
            Battle_sphereMirroredVertices);
        std::copy(input.starts.begin(), input.starts.end(),
            Battle_sphereShapeCommandStarts);
        std::copy(input.commands.begin(), input.commands.end(),
            Battle_sphereWireframeCommands);
        battle::BuildSphereWireframeEntry(input.type);
        Print(sphere.wirePhaseA);
        Print(sphere.wirePhaseB);
        Print(sphere.wirePhaseC);
        Print(sphere.wireShape);
        Print(static_cast<std::uint16_t>(sphere.reserved30 >> 16));
        PrintVertices(sphere);
        break;
    }
    default:
        std::cout << '\n';
        return;
    }
    std::cout << '\n';
}
} // namespace

int main()
{
    Input input;
    while (Read(input))
        Run(input);
}
