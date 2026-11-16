// A39-05 native driver: production Live entries and their owned dependencies.
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Core/Memory/HeapLive.hpp"
#include "Effects/AnalyzeGrid.hpp"
#include "Effects/AnalyzeGridLive.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/PackageExtents.hpp"

extern "C" {
#include "Core/Arena.h"
#include "Core/Diagnostics.h"
#include "Core/Gte.h"
#include "Core/Scratch.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
alignas(16) std::array<std::byte, 0x1000> scratch { };

[[noreturn]] void UnexpectedService(const char* detail) noexcept
{
    std::fputs(detail, stderr);
    std::fputc('\n', stderr);
    std::exit(2);
}
}

extern "C" {
Effects_EffectPackage* Effects_package = nullptr;
Effects_EffectInstance* Effects_currentInstance = nullptr;
Effects_ActionEffectRequest Effects_actionRequest { };
int Effects_curveSamples[211] { };
int Render_nearClip = 0;
int Render_frameBuf = 0;
void* Render_legacyOrderingTables[2] { };
void* Render_legacyPrimitiveBuffers[2] { };
void* Render_legacyCurrentOrderingTable = nullptr;
int Render_kStatGaugeColors[4] { };
void* Core_ScratchBase() { return scratch.data(); }
void* Core_GameHeapArena() { return nullptr; }
void Core_Errorf(const char*, const char*, ...) { }

// The original links physical addresses. Keep only that address-normalization
// boundary synthetic; allocation, bounds checks and packet production are real.
u_long LegacyGpu_Handle(const void* pointer)
{
    const auto base = reinterpret_cast<std::uintptr_t>(Render_legacyPrimitiveBuffers[0]);
    return static_cast<u_long>(
        0x80540000U + (reinterpret_cast<std::uintptr_t>(pointer) - base));
}

// Linked UI and segmented-curve dependencies are outside this renderer. Fail
// closed if a changed production path starts invoking one of these services.
void* LegacyGpu_Pointer(u_long) { UnexpectedService("unexpected GPU chain traversal"); }
int LoadImage(RECT*, u_long*) { UnexpectedService("unexpected image upload"); }
int StoreImage(RECT*, u_long*) { UnexpectedService("unexpected image readback"); }
void DrawPrim(void*) { UnexpectedService("unexpected immediate drawing"); }
}

namespace VS::Effects {
std::span<std::byte> LiveBoundPackageBlock(PackageBlockType)
{
    UnexpectedService("unexpected segmented curve");
}
} // namespace VS::Effects

#ifndef VS_ANALYZE_GRID_TEST_RUN
namespace {
namespace effects = VS::Effects;
constexpr std::size_t kDefinitions = 256;
constexpr std::size_t kCurveCount = 16;
constexpr std::size_t kCurveSize = 8;
constexpr std::size_t kCurveHeader = 4 + kCurveCount;
constexpr std::size_t kOrderingWords = 0x2088 / 4;
constexpr std::size_t kPacketWords = 15;
constexpr std::size_t kObservedPacketWords = 12;
constexpr std::size_t kScratchBytes = 0x18C;
constexpr std::array<Effects_EffectRenderer, 4> entries {
    effects::UpdateLivePlg005AnalyzeGrid, effects::UpdateLivePlg054AnalyzeGrid,
    effects::UpdateLivePlg055AnalyzeGrid, effects::UpdateLivePlg074AnalyzeGrid
};

struct DefinitionBlock {
    std::uint32_t reserved { };
    std::array<Effects_ParticleDefinition, kDefinitions> definitions { };
};
static_assert(offsetof(DefinitionBlock, definitions) == 4);
static_assert(sizeof(effects::AnalyzeGridState) == 28);

struct Fixture {
    std::array<std::byte, 0x10000> arena { };
    VS::Core::Memory::ScopedLiveGameHeap heap { arena };
    DefinitionBlock definitions { };
    alignas(
        4) std::array<std::byte, kCurveHeader + kCurveCount * kCurveSize> curve_bytes { };
    std::array<u_long, 0x20000 / 4> primitives { };
    std::array<u_long, kOrderingWords> ordering { };
    Effects_EffectPackage package { };
    Effects_EffectInstance instance { };
    Effects_RendererSlot slot { };

    Fixture()
    {
        Effects_package = &package;
        Effects_currentInstance = &instance;
        effects::ForgetLivePackageExtents();
        package.block5Data = reinterpret_cast<Effects_PackageBlock5*>(&definitions);
        effects::RecordLiveParticleDefinitions(
            std::as_bytes(std::span { &definitions, 1 }));
        curve_bytes[0] = static_cast<std::byte>(kCurveCount);
        curve_bytes[2] = static_cast<std::byte>(kCurveHeader);
        for (std::size_t index = 0; index < kCurveCount; ++index) {
            curve_bytes.at(4 + index) = static_cast<std::byte>(kCurveSize);
            package.curves[index] = reinterpret_cast<u_char*>(
                curve_bytes.data() + kCurveHeader + index * kCurveSize);
        }
        package.curveBlock = reinterpret_cast<Effects_CurveBlock*>(curve_bytes.data());
        effects::RecordLiveEffectCurves(curve_bytes);
        std::memset(scratch.data(), 0, scratch.size());
        std::memset(scratch.data() + 0x1D0, 0xA5, kScratchBytes);
        Render_legacyPrimitiveBuffers[0] = primitives.data();
        Render_legacyOrderingTables[0] = ordering.data();
        Render_frameScratch.primitiveCursor = primitives.data();
        Render_frameScratch.orderingTable = ordering.data() + 32;
        for (std::size_t index = 0; index < ordering.size(); ++index) {
            ordering.at(index) = static_cast<u_long>(0xA5000000U | (index * 20 + 7));
        }
        Core_Gte_Reset();
    }
    ~Fixture()
    {
        effects::ForgetLivePackageExtents();
        Effects_package = nullptr;
        Effects_currentInstance = nullptr;
    }
};

std::int64_t Read(std::istream& input)
{
    std::int64_t value = 0;
    if (!(input >> value))
        throw std::runtime_error("truncated grid input");
    return value;
}

void ReadBytes(std::istream& input, std::span<std::byte> target)
{
    for (std::byte& byte : target)
        byte = static_cast<std::byte>(Read(input));
}

void ReadMatrix(std::istream& input, MATRIX& matrix)
{
    for (auto& row : matrix.m) {
        for (auto& entry : row)
            entry = static_cast<short>(Read(input));
    }
    for (auto& component : matrix.t)
        component = static_cast<long>(Read(input));
}

void Print(const Fixture& fixture, int result, unsigned command)
{
    const auto* state =
        static_cast<const effects::AnalyzeGridState*>(fixture.slot.rendererState);
    if (state == nullptr)
        throw std::runtime_error("native refused grid state");
    const auto bytes = std::bit_cast<std::array<std::uint8_t, 28>>(*state);
    std::cout << result << ' ' << (command == 1 ? sizeof(*state) : 0);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index != 22 && index != 23) {
            std::cout << ' ' << static_cast<unsigned>(bytes.at(index));
        }
    }
    for (std::size_t index = 0; index < kScratchBytes; ++index) {
        std::cout << ' ' << std::to_integer<unsigned>(scratch.at(0x1D0 + index));
    }
    const auto* cursor = static_cast<const u_long*>(Render_frameScratch.primitiveCursor);
    const auto used = static_cast<std::size_t>(cursor - fixture.primitives.data());
    std::cout << ' ' << used * sizeof(u_long);
    for (const auto word : fixture.ordering)
        std::cout << ' ' << word;
    for (std::size_t packet = 0; packet < used / kPacketWords; ++packet) {
        for (std::size_t word = 0; word < kObservedPacketWords; ++word) {
            auto value = fixture.primitives.at(packet * kPacketWords + word);
            if (word == 8 || word == 10)
                value &= 0xFFFF;
            std::cout << ' ' << value;
        }
    }
    std::cout << '\n';
}

void Run(std::istream& input)
{
    Fixture fixture;
    const auto mode = static_cast<std::size_t>(Read(input));
    const auto command = static_cast<unsigned>(Read(input));
    const int packed = static_cast<int>(Read(input));
    std::array<std::byte, 28> raw_state { };
    ReadBytes(input, raw_state);
    const auto state = std::bit_cast<effects::AnalyzeGridState>(raw_state);
    std::array<std::byte, effects::kParticleDefinitionBytes> definition { };
    ReadBytes(input, definition);
    fixture.definitions.definitions.at(state.definition_index) =
        std::bit_cast<Effects_ParticleDefinition>(definition);
    ReadBytes(input, std::span(fixture.curve_bytes).subspan(kCurveHeader));
    auto& origin = Effects_actionRequest.position;
    origin.vx = static_cast<short>(Read(input));
    origin.vy = static_cast<short>(Read(input));
    origin.vz = static_cast<short>(Read(input));
    const auto transform =
        fixture.definitions.definitions.at(state.definition_index).transformIndex;
    auto& matrices = fixture.instance.transforms[transform];
    for (auto& component : matrices.world.t) {
        component = static_cast<long>(Read(input));
    }
    ReadMatrix(input, Render_frameScratch.viewMatrix);
    ReadMatrix(input, matrices.view);
    Render_nearClip = static_cast<int>(Read(input));
    Core_Gte_SetGeomScreen(static_cast<long>(Read(input)));
    const long ofx = static_cast<long>(Read(input));
    const long ofy = static_cast<long>(Read(input));
    Core_Gte_SetGeomOffset(ofx, ofy);
    const long dqa = static_cast<long>(Read(input));
    const long dqb = static_cast<long>(Read(input));
    Core_Gte_SetDepthCue(dqa, dqb);
    if (command != 1) {
        auto* allocation = static_cast<effects::AnalyzeGridState*>(
            VS::Core::Memory::AllocateLiveFromHighEnd(sizeof(state)));
        if (allocation == nullptr)
            throw std::runtime_error("fixture heap failed");
        fixture.slot.rendererState = std::construct_at(allocation, state);
    }
    const int result = entries.at(mode)(&fixture.slot, command, packed);
    Print(fixture, result, command);
}
} // namespace

int main()
{
    try {
        std::string line;
        while (std::getline(std::cin, line)) {
            std::istringstream input(line);
            Run(input);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
