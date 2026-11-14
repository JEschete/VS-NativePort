#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Effects/EffectScript.hpp"

namespace {
namespace fx = VS::Effects;

// The modes effect_script_particle_differential.py compares.
enum Mode : int {
    kVertical,
    kSignal,
    kCurves,
    kPaired,
    kChild,
    kTable,
    kInvalid,
    kDiagnostic,
};

constexpr std::size_t kProgramBytes = 0x10000;
constexpr std::size_t kTransforms = 16;
constexpr unsigned kChildShift = 11;

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

bool ReadCurves(std::vector<fx::ParticleColorCurves>& curves)
{
    std::size_t count = 0;
    if (!Read(count))
        return false;
    curves.assign(count, { });
    for (auto& curve : curves) {
        if (!Read(curve.red) || !Read(curve.green) || !Read(curve.blue))
            return false;
    }
    return true;
}

bool ReadPreset(fx::ParticleColorCurves& curves, std::uint8_t& fourth)
{
    return Read(curves.red) && Read(curves.green) && Read(curves.blue) && Read(fourth);
}

void PrintStep(const fx::OpcodeStep& step)
{
    const int refusal = step ? 0 : 1 + static_cast<int>(step.error());
    const long long result = step ? fx::HandlerResult(*step) : -1;
    std::cout << refusal << ' ' << result;
}

void PrintCurves(std::span<const fx::ParticleColorCurves> curves)
{
    for (const auto& curve : curves) {
        std::cout << ' ' << static_cast<int>(curve.red) << ' ' << static_cast<int>(curve.green) << ' '
                  << static_cast<int>(curve.blue);
    }
    std::cout << '\n';
}

bool Vertical()
{
    std::uint16_t effect_id = 0;
    fx::ScriptRegisters registers { };
    std::size_t count = 0;
    std::array<std::int32_t, kTransforms> heights { };
    if (!Read(effect_id) || !Read(registers[fx::kActionVariantRegister]) || !Read(count) || count > kTransforms)
        return false;
    for (auto& height : heights) {
        if (!Read(height))
            return false;
    }
    const fx::ScriptStep step
        = fx::SetActionEffectVerticalOffset(effect_id, registers, std::span { heights }.first(count));
    PrintStep(step);
    for (const std::int32_t height : heights)
        std::cout << ' ' << height;
    std::cout << '\n';
    return true;
}

bool Signal()
{
    std::uint16_t type = 0;
    std::uint8_t actor = 0;
    if (!Read(type) || !Read(actor))
        return false;
    const std::optional<std::uint8_t> signalled = fx::SourceActorToSignal(type, actor);
    PrintStep(fx::ContinueScript());
    std::cout << ' ' << (signalled ? 1 : 0);
    if (signalled)
        std::cout << ' ' << static_cast<int>(*signalled);
    std::cout << '\n';
    return true;
}

bool Curves()
{
    std::uint16_t offset = 0;
    std::vector<fx::ParticleColorCurves> curves;
    std::size_t placed = 0;
    if (!Read(offset) || !ReadCurves(curves) || !Read(placed))
        return false;
    std::vector<std::byte> program(kProgramBytes);
    for (std::size_t i = 0; i < placed; ++i) {
        std::size_t at = 0;
        int byte = 0;
        if (!Read(at) || !Read(byte) || at >= program.size())
            return false;
        program[at] = static_cast<std::byte>(byte);
    }
    fx::ScriptCursor cursor { .program = program, .offset = offset };
    PrintStep(fx::SetParticleColorCurves(cursor, curves));
    std::cout << ' ' << cursor.offset;
    PrintCurves(curves);
    return true;
}

bool Paired()
{
    std::uint16_t preset = 0;
    std::vector<fx::ParticleColorCurves> curves;
    std::array<fx::PairedColorPreset, fx::kParticleColorPresets> presets { };
    if (!Read(preset) || !ReadCurves(curves))
        return false;
    for (auto& paired : presets) {
        for (auto& entry : paired) {
            std::uint8_t padding = 0;
            if (!ReadPreset(entry, padding))
                return false;
        }
    }
    PrintStep(fx::ApplyPairedColorPreset(preset, presets, curves));
    PrintCurves(curves);
    return true;
}

bool Table()
{
    std::uint16_t preset = 0;
    std::vector<fx::ParticleColorCurves> curves;
    std::array<fx::ColorPresetTable, fx::kParticleColorPresets> tables { };
    if (!Read(preset) || !ReadCurves(curves))
        return false;
    for (auto& table : tables) {
        for (auto& entry : table) {
            if (!ReadPreset(entry.curves, entry.definition))
                return false;
        }
    }
    PrintStep(fx::ApplyColorPresetTable(preset, tables, curves));
    PrintCurves(curves);
    return true;
}

bool Child()
{
    int has_parent = 0;
    std::uint32_t word = 0;
    if (!Read(has_parent) || !Read(word))
        return false;
    const std::optional<std::uint8_t> children = has_parent
        ? std::optional<std::uint8_t> { static_cast<std::uint8_t>((word >> kChildShift) & fx::kChildCountMask) }
        : std::nullopt;
    if (const std::optional<std::uint8_t> next = fx::CompleteChildScript(children)) {
        word = (word & ~(std::uint32_t { fx::kChildCountMask } << kChildShift))
            | (std::uint32_t { *next } << kChildShift);
    }
    PrintStep(fx::ContinueScript());
    std::cout << ' ' << word << '\n';
    return true;
}

void Invalid()
{
    const fx::ScriptDiagnosticStep report = fx::ReportInvalidInstruction();
    PrintStep(report.step);
    std::cout << " 1 " << static_cast<int>(report.diagnostic) << '\n';
}

bool Diagnostic()
{
    std::int32_t code = 0;
    if (!Read(code))
        return false;
    std::cout << "0 " << (fx::DescribeEffectDiagnostic(code).empty() ? 0 : 1) << '\n';
    return true;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int mode = 0;
    while (Read(mode)) {
        bool read = true;
        switch (mode) {
        case kVertical:
            read = Vertical();
            break;
        case kSignal:
            read = Signal();
            break;
        case kCurves:
            read = Curves();
            break;
        case kPaired:
            read = Paired();
            break;
        case kChild:
            read = Child();
            break;
        case kTable:
            read = Table();
            break;
        case kInvalid:
            Invalid();
            break;
        case kDiagnostic:
            read = Diagnostic();
            break;
        default:
            return 1;
        }
        if (!read)
            return 1;
    }
    return 0;
}
