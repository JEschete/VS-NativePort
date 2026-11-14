#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

#include "Effects/EffectScript.hpp"

namespace {
using VS::Effects::ScriptCursor;
using VS::Effects::ScriptStep;

enum Mode : int {
    kBranch,
    kYield,
    kJump,
    kEnd,
    kContinue,
    kReadByte,
    kReadHalfword,
    kRun,
};

// The handler kinds effect_script_differential.py reads from the original table.
enum Kind : int {
    kStubbed,
    kYieldHandler,
    kJumpHandler,
    kEndHandler,
    kContinueHandler,
};

constexpr std::size_t kProgramBytes = 0x10000;
constexpr std::size_t kOpcodes = VS::Effects::kEffectScriptOpcodes;

// The original table's dispatch: the typed operand handlers, and the vector's result for
// every entry the original run stubbed.
class TableMachine final : public VS::Effects::ScriptMachine {
public:
    TableMachine(ScriptCursor& cursor, const std::array<int, kOpcodes>& kinds,
        const std::array<std::int32_t, kOpcodes>& results)
        : cursor_(cursor)
        , kinds_(kinds)
        , results_(results)
    {
    }

    std::optional<std::uint8_t> NextOpcode() override
    {
        return VS::Effects::ReadScriptByte(cursor_);
    }

    std::optional<ScriptStep> Execute(std::uint8_t opcode) override
    {
        if (opcode >= kOpcodes)
            return std::nullopt;
        switch (kinds_[opcode]) {
        case kYieldHandler:
            return VS::Effects::YieldScriptAt(cursor_);
        case kJumpHandler:
            return VS::Effects::JumpScript(cursor_);
        case kEndHandler:
            return VS::Effects::EndScript();
        case kContinueHandler:
            return VS::Effects::ContinueScript();
        default:
            stubbed.push_back(opcode);
            return VS::Effects::StepFromHandlerResult(results_[opcode]);
        }
    }

    std::vector<int> stubbed;

private:
    ScriptCursor& cursor_;
    const std::array<int, kOpcodes>& kinds_;
    const std::array<std::int32_t, kOpcodes>& results_;
};

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

long long Optional(const auto& value)
{
    return value ? static_cast<long long>(*value) : -1;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int mode = 0;
    while (Read(mode)) {
        std::uint16_t offset = 0;
        std::int32_t condition = 0;
        std::array<int, kOpcodes> kinds { };
        std::array<std::int32_t, kOpcodes> results { };
        std::size_t placed = 0;
        if (!Read(offset) || !Read(condition))
            return 1;
        for (auto& kind : kinds) {
            if (!Read(kind))
                return 1;
        }
        for (auto& result : results) {
            if (!Read(result))
                return 1;
        }
        if (!Read(placed))
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
        TableMachine machine(cursor, kinds, results);
        long long result = 0;
        switch (mode) {
        case kBranch:
            result = VS::Effects::BranchScriptIf(condition != 0, cursor) ? 1 : 0;
            break;
        case kYield:
            result = VS::Effects::HandlerResult(VS::Effects::YieldScriptAt(cursor));
            break;
        case kJump:
            result = VS::Effects::HandlerResult(VS::Effects::JumpScript(cursor));
            break;
        case kEnd:
            result = VS::Effects::HandlerResult(VS::Effects::EndScript());
            break;
        case kContinue:
            result = VS::Effects::HandlerResult(VS::Effects::ContinueScript());
            break;
        case kReadByte:
            result = Optional(VS::Effects::ReadScriptByte(cursor));
            break;
        case kReadHalfword:
            result = Optional(VS::Effects::ReadScriptHalfword(cursor));
            break;
        case kRun:
            result =
                VS::Effects::RunScript(machine) == VS::Effects::ScriptRun::kEnded ? 0 : 1;
            break;
        default:
            return 1;
        }
        std::cout << result << ' ' << cursor.offset << ' ' << machine.stubbed.size();
        for (const int opcode : machine.stubbed) {
            std::cout << ' ' << opcode;
        }
        std::cout << '\n';
    }
    return 0;
}
