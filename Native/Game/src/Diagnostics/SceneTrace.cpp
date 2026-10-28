#include "Diagnostics/SceneTrace.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

#include "Core/Log.hpp"
#include "nlohmann/json.hpp"

namespace {
std::atomic<std::uint64_t> next_command{0};

void TraceFailure(std::string_view reason) noexcept {
  try {
    VS::Core::LogWrite(VS::Core::LogLevel::Error, "diagnostics", reason);
  } catch (...) {
    std::fputs("Scene diagnostic failure could not be logged\n", stderr);
  }
}
}  // namespace

extern "C" std::uint64_t Diagnostics_RecordSceneCommand(
    std::int32_t event_id, std::int32_t source, std::int32_t thread,
    const void* instruction, const void* script_base, std::uint32_t size,
    std::int32_t skip_state) {
  if (!VS::Core::IsLogEnabled(VS::Core::LogLevel::Debug, "event-command")) {
    return 0;
  }
  constexpr std::size_t kMaximumInstructionBytes = 256;
  if (instruction == nullptr || size > kMaximumInstructionBytes) {
    TraceFailure("Invalid scene instruction pointer or encoded size");
    return 0;
  }
  try {
    const auto* bytes = static_cast<const std::uint8_t*>(instruction);
    const auto command = ++next_command;
    const auto pc = reinterpret_cast<std::uintptr_t>(instruction);
    const auto base = reinterpret_cast<std::uintptr_t>(script_base);
    nlohmann::json record{
        {"command", command},
        {"event", event_id},
        {"source", source},
        {"thread", thread},
        {"pc", pc},
        {"script_base", base},
        {"opcode", bytes[0]},
        {"encoded_size", size},
        {"skip_state", skip_state},
        {"operands", std::span(bytes + 1, size > 0 ? size - 1 : 0)}};
    if (base != 0 && pc >= base) record["script_offset"] = pc - base;
    VS::Core::LogWrite(VS::Core::LogLevel::Debug, "event-command",
                       record.dump());
    return command;
  } catch (...) {
    TraceFailure("Scene command diagnostic serialization failed");
    return 0;
  }
}

extern "C" void Diagnostics_RecordSceneResult(std::uint64_t command,
                                              std::int64_t result) {
  if (command == 0 ||
      !VS::Core::IsLogEnabled(VS::Core::LogLevel::Debug, "event-result")) {
    return;
  }
  try {
    const auto flow = result == 0   ? "advance"
                      : result == 1 ? "wait"
                      : result == 2 ? "return_to_vm"
                      : result == 3 ? "thread_finished"
                      : result == 4 ? "advance_yield"
                                    : "branch";
    VS::Core::LogWrite(
        VS::Core::LogLevel::Debug, "event-result",
        nlohmann::json{{"command", command}, {"result", result}, {"flow", flow}}
            .dump());
  } catch (...) {
    TraceFailure("Scene result diagnostic serialization failed");
  }
}

extern "C" void Diagnostics_RecordScriptFlagWrite(std::int32_t flag,
                                                  std::uint8_t before,
                                                  std::uint8_t after) {
  if (!VS::Core::IsLogEnabled(VS::Core::LogLevel::Debug, "state-flag")) return;
  try {
    VS::Core::LogWrite(VS::Core::LogLevel::Debug, "state-flag",
                       nlohmann::json{{"flag", flag},
                                      {"before", before},
                                      {"after", after},
                                      {"changed", before != after}}
                           .dump());
  } catch (...) {
    TraceFailure("Script flag diagnostic serialization failed");
  }
}
