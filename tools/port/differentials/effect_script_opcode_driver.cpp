#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Effects/EffectScript.hpp"

namespace {
using VS::Effects::OpcodeStep;
using VS::Effects::ScriptCursor;

// The opcode modes effect_script_opcode_differential.py compares.
enum Mode : int {
    kSelector,
    kRenderers,
    kVariantCleared,
    kLoadVariant,
    kSetRegister,
    kRegisterEqual,
    kParticleEqual,
    kClearVariant,
};

constexpr std::size_t kProgramBytes = 0x10000;

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

template <typename Values> bool ReadAll(Values& values)
{
    for (auto& value : values) {
        if (!Read(value))
            return false;
    }
    return true;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int mode = 0;
    while (Read(mode)) {
        std::uint16_t offset = 0;
        VS::Effects::ScriptRegisters registers { };
        std::uint8_t particles = 0;
        int renderers_attached = 0;
        std::int32_t selector = 0;
        std::uint8_t variant = 0;
        VS::Effects::ActionVariantValues values { };
        std::size_t placed = 0;
        if (!Read(offset) || !ReadAll(registers) || !Read(particles) || !Read(renderers_attached)
            || !Read(selector) || !Read(variant) || !ReadAll(values) || !Read(placed))
            return 1;
        std::vector<std::byte> program(kProgramBytes);
        for (std::size_t i = 0; i < placed; ++i) {
            std::size_t at = 0;
            int byte = 0;
            if (!Read(at) || !Read(byte) || at >= program.size())
                return 1;
            program[at] = static_cast<std::byte>(byte);
        }

        ScriptCursor cursor { .program = program, .offset = offset };
        OpcodeStep step = VS::Effects::ScriptStep::kEnd;
        switch (mode) {
        case kSelector:
            step = VS::Effects::SetScriptSelector(cursor, selector);
            break;
        case kRenderers:
            step = VS::Effects::BranchIfRenderersFinished(renderers_attached != 0, cursor);
            break;
        case kVariantCleared:
            step = VS::Effects::BranchIfActionVariantCleared(variant, cursor);
            break;
        case kLoadVariant:
            step = VS::Effects::LoadActionVariantRegister(variant, values, registers);
            break;
        case kSetRegister:
            step = VS::Effects::SetScriptRegister(cursor, registers);
            break;
        case kRegisterEqual:
            step = VS::Effects::BranchIfScriptRegisterEqual(cursor, registers);
            break;
        case kParticleEqual:
            step = VS::Effects::BranchIfParticleCountEqual(particles, cursor);
            break;
        case kClearVariant:
            step = VS::Effects::ClearActionVariant(variant);
            break;
        default:
            return 1;
        }
        const int refusal = step ? 0 : 1 + static_cast<int>(step.error());
        const long long result = step ? VS::Effects::HandlerResult(*step) : -1;
        std::cout << refusal << ' ' << result << ' ' << cursor.offset;
        for (const std::uint8_t value : registers) {
            std::cout << ' ' << static_cast<int>(value);
        }
        std::cout << ' ' << selector << ' ' << static_cast<int>(variant) << '\n';
    }
    return 0;
}
