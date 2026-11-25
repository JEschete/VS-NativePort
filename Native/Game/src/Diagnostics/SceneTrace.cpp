#include "Diagnostics/SceneTrace.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <string_view>

#include "Core/Log.hpp"
#include "nlohmann/json.hpp"

namespace {
std::atomic<std::uint64_t> nextCommand{0};

std::string_view RegionName(VS::Event::ScriptRegion region) {
  switch (region) {
    case VS::Event::ScriptRegion::kNone:
      return "none";
    case VS::Event::ScriptRegion::kEventScript:
      return "event_script";
    case VS::Event::ScriptRegion::kDoorEvents:
      return "door_events";
    case VS::Event::ScriptRegion::kSubroutine:
      return "subroutine";
  }
  throw std::invalid_argument("Invalid scene diagnostic script region");
}

std::string_view FlowName(VS::Event::ScriptFlowKind kind) {
  switch (kind) {
    case VS::Event::ScriptFlowKind::kAdvance:
      return "advance";
    case VS::Event::ScriptFlowKind::kHold:
      return "wait";
    case VS::Event::ScriptFlowKind::kFinish:
      return "return_to_vm";
    case VS::Event::ScriptFlowKind::kStop:
      return "thread_finished";
    case VS::Event::ScriptFlowKind::kAdvanceAndYield:
      return "advance_yield";
    case VS::Event::ScriptFlowKind::kJumpBy:
      return "jump_by";
    case VS::Event::ScriptFlowKind::kJumpTo:
      return "jump_to";
  }
  throw std::invalid_argument("Invalid scene diagnostic flow");
}

void TraceFailure(std::string_view reason) noexcept {
  try {
    VS::Core::LogWrite(VS::Core::LogLevel::Error, "diagnostics", reason);
  } catch (...) {
    std::fputs("Scene diagnostic failure could not be logged\n", stderr);
  }
}
}  // namespace

std::uint64_t VS::Game::RecordSceneCommand(
    std::int32_t event_id, Event::EventSource source,
    const Event::ScriptInstruction& instruction, std::uint32_t size,
    Event::CutsceneSkip skip_state) {
  if (!VS::Core::IsLogEnabled(VS::Core::LogLevel::Debug, "event-command")) {
    return 0;
  }
  constexpr std::size_t kMaximumInstructionBytes = 256;
  if (instruction.bytes.empty() || size > instruction.bytes.size() ||
      size > kMaximumInstructionBytes) {
    TraceFailure("Invalid scene instruction span or encoded size");
    return 0;
  }
  try {
    const auto command = ++nextCommand;
    nlohmann::json record{
        {"schema", 2},
        {"command", command},
        {"event", event_id},
        {"source", static_cast<std::uint16_t>(source)},
        {"thread", instruction.thread},
        {"region", RegionName(instruction.location.region)},
        {"script_offset", instruction.location.offset},
        {"generation", instruction.location.generation},
        {"opcode", instruction.bytes.front()},
        {"encoded_size", size},
        {"skip_state", static_cast<std::uint8_t>(skip_state)},
        {"operands", instruction.bytes.subspan(1, size > 0 ? size - 1 : 0)}};
    VS::Core::LogWrite(VS::Core::LogLevel::Debug, "event-command",
                       record.dump());
    return command;
  } catch (...) {
    TraceFailure("Scene command diagnostic serialization failed");
    return 0;
  }
}

void VS::Game::RecordSceneResult(std::uint64_t command,
                                 Event::ScriptFlow result) {
  if (command == 0 ||
      !VS::Core::IsLogEnabled(VS::Core::LogLevel::Debug, "event-result")) {
    return;
  }
  try {
    nlohmann::json record{
        {"schema", 2}, {"command", command}, {"flow", FlowName(result.kind)}};
    if (result.kind == Event::ScriptFlowKind::kJumpBy) {
      record["displacement"] = result.displacement;
    } else if (result.kind == Event::ScriptFlowKind::kJumpTo) {
      record["target"] = {{"region", RegionName(result.target.region)},
                          {"offset", result.target.offset},
                          {"generation", result.target.generation}};
    } else {
      record["result"] = static_cast<std::uint8_t>(result.kind);
    }
    VS::Core::LogWrite(VS::Core::LogLevel::Debug, "event-result",
                       record.dump());
  } catch (...) {
    TraceFailure("Scene result diagnostic serialization failed");
  }
}

void VS::Game::RecordScriptFlagWrite(std::int32_t flag, std::uint8_t before,
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
