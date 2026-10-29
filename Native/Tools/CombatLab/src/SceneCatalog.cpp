#include "CombatLab/SceneCatalog.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "CombatLab/DoorTests.hpp"
#include "Content/DataStore.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Event/ScriptField.h"
#include "Event/ScriptVm.h"
}

namespace VS::combat_lab {
namespace {
using Bytes = std::span<const std::uint8_t>;
constexpr std::size_t kScriptHeaderBytes = 16;
constexpr std::size_t kRoomHeaderBytes = 48;
constexpr std::size_t kLocalEventCount = 16;
constexpr std::size_t kBitsPerByte = 8;
constexpr std::size_t kRoomTriggersOffset = 8;
constexpr std::size_t kRoomEntryOffset = 16;
constexpr std::size_t kRoomLocalsOffset = 24;
constexpr std::size_t kSectionLengthOffset = 4;
constexpr std::uint8_t kFlagBranchOperand = 4;
constexpr std::uint32_t kLocalEventBit = 0x8000;
constexpr std::uint32_t kLocalEventMask = 0xF;
constexpr std::size_t kTriggerHeaderBytes = 3;
constexpr std::uint8_t kTriggerEnd = 0xFF;
constexpr std::uint8_t kTriggerNegation = 0xFE;

Bytes Section(Bytes bytes, std::size_t offset, std::size_t size) {
  if (offset > bytes.size() || size > bytes.size() - offset) {
    throw std::runtime_error(
        std::format("scene section outside asset: offset {}, size {}, asset {}",
                    offset, size, bytes.size()));
  }
  return bytes.subspan(offset, size);
}

std::uint32_t ReadInteger(Bytes bytes, std::size_t offset,
                          std::size_t width = 2) {
  const auto source = Section(bytes, offset, width);
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < width; ++index) {
    value |= static_cast<std::uint32_t>(source[index])
             << (index * kBitsPerByte);
  }
  return value;
}

SceneOpcodes ProductionOpcodes() {
  SceneOpcodes result;
  for (std::size_t index = 0; index < result.size(); ++index) {
    auto& opcode = result[index];
    opcode.size = Event_kOpcodeSizes[index];
    const auto handler = Event_kOpcodeHandlers[index];
    if (handler == Event_ScriptReportInvalidOpcode || handler == nullptr) {
      opcode.size = 0;
    } else if (handler == Event_ScriptBranchOnFlagImmediate ||
               handler == Event_ScriptBranchOnFlagPair) {
      opcode.flow = SceneFlow::kBranch;
      opcode.branch_operand = kFlagBranchOperand;
    } else if (handler == Event_ScriptBranchOnCachedFlag) {
      opcode.flow = SceneFlow::kBranch;
      opcode.branch_operand = 2;
    } else if (handler == Event_ScriptEndCutsceneAndJump) {
      opcode.flow = SceneFlow::kBranch;
      opcode.branch_operand = 1;
    } else if (handler == Event_ScriptJumpForward) {
      opcode.flow = SceneFlow::kJump;
      opcode.branch_operand = 1;
    } else if (handler == Event_ScriptLoopBackward) {
      opcode.flow = SceneFlow::kLoop;
      opcode.branch_operand = 2;
    } else if (handler == Event_CommandStartScriptContext) {
      opcode.flow = SceneFlow::kThread;
      opcode.branch_operand = 2;
    } else if (handler == Event_ScriptReturnToSavedAddress) {
      opcode.flow = SceneFlow::kReturn;
    } else if (handler == Event_CommandFinishEvent ||
               handler == Event_CommandStopScriptContext) {
      opcode.flow = SceneFlow::kFinish;
    } else if (handler == Event_CommandSpecialOperation) {
      opcode.flow = SceneFlow::kSpecial;
    }
    if (handler == Event_CommandLoadNextEvent) {
      opcode.operation = SceneOperation::kChainEvent;
    } else if (handler == Event_ScriptChangeRoom) {
      opcode.operation = SceneOperation::kChangeRoom;
    }
  }
  return result;
}

nlohmann::json DescribeCommand(Bytes instruction, std::size_t offset,
                               SceneOperation operation) {
  nlohmann::json command = {{"offset", offset},
                            {"opcode", instruction[0]},
                            {"operands", instruction.subspan(1)}};
  if (operation == SceneOperation::kChainEvent) {
    command["operation"] = "load_next_event";
    command["event_id"] =
        static_cast<std::int16_t>(ReadInteger(instruction, 1));
  } else if (operation == SceneOperation::kChangeRoom) {
    // func_800B98C0 (4A0A8.c:2018): zone 0 uses the room-only transition.
    constexpr std::size_t kRoomHandoffOperands = 9;
    std::array<std::uint8_t, kRoomHandoffOperands> operands{};
    std::ranges::copy(Section(instruction, 1, operands.size()),
                      operands.begin());
    const auto [zone, room, x, direction, z, elevation, flags, effect, speed] =
        operands;
    command["operation"] = "change_room";
    command["destination"] = {
        {"zone", zone},
        {"room", room},
        {"zone_mode", zone == 0 ? "current" : "explicit"}};
    command["entry"] = {
        {"x", x}, {"z", z}, {"direction", direction}, {"elevation", elevation}};
    command["flags"] = flags;
    command["transition"] = {{"effect", effect}, {"speed", speed}};
  }
  return command;
}

int ExternalEventId(const std::string& path) {
  const auto stem = std::filesystem::path(path).stem().string();
  int id = 0;
  const auto parsed =
      std::from_chars(stem.data(), stem.data() + stem.size(), id);
  if (parsed.ec != std::errc() || parsed.ptr != stem.data() + stem.size() ||
      id < 0 || id > Game::EncounterLimits::kLastExternalEvent) {
    throw std::runtime_error("invalid external event asset ID: " + path);
  }
  return id;
}

nlohmann::json DescribeScript(Bytes bytes, const SceneOpcodes& opcodes) {
  try {
    return DecodeSceneScript(bytes, opcodes);
  } catch (const std::runtime_error& error) {
    return {{"status", "invalid"}, {"error", error.what()}};
  }
}

nlohmann::json ReadRoomScenes(Bytes bytes, const SceneOpcodes& opcodes) {
  // _loadMpdRoom and _loadMpdClearedScriptDoorSections, 146C.c/4A0A8.c.
  Section(bytes, 0, kRoomHeaderBytes);
  nlohmann::json room = {{"scripts", nlohmann::json::array()},
                         {"triggers", nlohmann::json::array()}};
  const auto section = [bytes](std::size_t header) {
    return Section(bytes, ReadInteger(bytes, header, sizeof(std::uint32_t)),
                   ReadInteger(bytes, header + kSectionLengthOffset));
  };
  const auto triggers = section(kRoomTriggersOffset);
  if (!triggers.empty()) room["triggers"] = DecodeSceneTriggers(triggers);
  const auto entry = section(kRoomEntryOffset);
  if (!entry.empty()) {
    auto script = DescribeScript(entry, opcodes);
    script["source"] = "room_entry";
    script["offset"] =
        ReadInteger(bytes, kRoomEntryOffset, sizeof(std::uint32_t));
    room["scripts"].push_back(std::move(script));
  }
  const auto locals = section(kRoomLocalsOffset);
  if (!locals.empty()) {
    Section(locals, 0, kLocalEventCount * sizeof(std::uint16_t));
    for (std::size_t index = 0; index < kLocalEventCount; ++index) {
      const auto offset = ReadInteger(locals, index * sizeof(std::uint16_t));
      if (offset == 0) continue;
      auto script = DescribeScript(
          Section(locals, offset, locals.size() - offset), opcodes);
      script["source"] = "room_local";
      script["selector"] = index;
      script["offset"] =
          ReadInteger(bytes, kRoomLocalsOffset, sizeof(std::uint32_t)) + offset;
      room["scripts"].push_back(std::move(script));
    }
  }
  return room;
}
nlohmann::json DescribeRoomContext(int zone, const Game::EncounterRoom& room) {
  nlohmann::json context{{"zone", zone}, {"room", room.index}};
  const auto start = FindRoomFixtureStart(room, room.entrance);
  if (!start) {
    context["unavailable"] = room.unavailable_reason.empty()
                                 ? "no supported non-door fixture tile"
                                 : room.unavailable_reason;
    return context;
  }
  context["supported_start"] = {{"x", start->x},
                                {"z", start->z},
                                {"elevation", start->elevation},
                                {"direction", start->direction}};
  context["fixture_scope"] =
      "original supported non-door tile; story prerequisites not granted";
  auto alternatives = nlohmann::json::array();
  for (const int z : {0, room.depth - 1}) {
    for (const int x : {0, room.width - 1}) {
      const auto corner = FindRoomFixtureStart(room, {x, z, 0, 0});
      if (corner) {
        alternatives.push_back({{"x", corner->x},
                                {"z", corner->z},
                                {"elevation", corner->elevation},
                                {"direction", corner->direction}});
      }
    }
  }
  context["supported_alternatives"] = std::move(alternatives);
  auto supported = nlohmann::json::array();
  for (int z = 0; z < room.depth; ++z) {
    for (int x = 0; x < room.width; ++x) {
      if (IsRoomFixtureTile(room, x, z)) {
        supported.push_back({{"x", x}, {"z", z}});
      }
    }
  }
  context["supported_tiles"] = std::move(supported);
  return context;
}
}  // namespace

nlohmann::json DecodeSceneScript(Bytes bytes, const SceneOpcodes& opcodes) {
  // func_800BF5F0/func_800BFBB8, BATTLE.PRG/4A0A8.c and 573B8.c.
  if (bytes.empty() ||
      std::ranges::all_of(bytes, [](auto byte) { return byte == 0; })) {
    return {{"status", "empty"}, {"bytes", bytes.size()}};
  }
  Section(bytes, 0, kScriptHeaderBytes);
  auto code_end = bytes.size();
  for (const auto offset : {2, 4, 6}) {
    const auto section = ReadInteger(bytes, offset);
    if (section < kScriptHeaderBytes || section > bytes.size()) {
      throw std::runtime_error("invalid scene script section offset");
    }
    code_end = std::min(code_end, static_cast<std::size_t>(section));
  }
  auto commands = nlohmann::json::array();
  auto edges = nlohmann::json::array();
  auto unresolved = nlohmann::json::array();
  std::set<std::size_t> pending{kScriptHeaderBytes};
  std::map<std::size_t, std::size_t> decoded;
  while (!pending.empty()) {
    const auto offset = *pending.begin();
    pending.erase(pending.begin());
    if (decoded.contains(offset)) continue;
    if (offset < kScriptHeaderBytes || offset >= code_end) {
      throw std::runtime_error("scene branch is outside script instructions");
    }
    const auto& opcode = opcodes[bytes[offset]];
    if (opcode.size == 0 || opcode.size > code_end - offset) {
      throw std::runtime_error(
          std::format("invalid scene instruction at {}: opcode {:02X}, size {}",
                      offset, bytes[offset], opcode.size));
    }
    for (const auto& [start, size] : decoded) {
      if (offset < start + size && start < offset + opcode.size) {
        throw std::runtime_error(
            "scene branch overlaps an instruction operand");
      }
    }
    decoded.emplace(offset, opcode.size);
    commands.push_back(DescribeCommand(Section(bytes, offset, opcode.size),
                                       offset, opcode.operation));
    const auto link = [&](std::size_t target, std::string_view kind) {
      edges.push_back({{"from", offset}, {"to", target}, {"kind", kind}});
      pending.insert(target);
    };
    if (opcode.flow == SceneFlow::kReturn) {
      unresolved.push_back(
          {{"offset", offset}, {"kind", "saved_return_cursor"}});
      continue;
    }
    if (opcode.flow == SceneFlow::kFinish) continue;
    if (opcode.flow == SceneFlow::kSpecial) {
      unresolved.push_back(
          {{"offset", offset}, {"kind", "special_operation_preconditions"}});
    }
    if (opcode.flow == SceneFlow::kBranch || opcode.flow == SceneFlow::kJump ||
        opcode.flow == SceneFlow::kLoop || opcode.flow == SceneFlow::kThread) {
      const auto operand = opcode.branch_operand;
      if (operand == 0 || operand + sizeof(std::uint16_t) > opcode.size) {
        throw std::runtime_error("scene branch operand exceeds instruction");
      }
      const auto distance =
          static_cast<std::int16_t>(ReadInteger(bytes, offset + operand));
      const auto target =
          static_cast<std::int64_t>(offset) +
          (opcode.flow == SceneFlow::kLoop ? -distance : distance);
      if (target < static_cast<std::int64_t>(kScriptHeaderBytes) ||
          target >= static_cast<std::int64_t>(code_end)) {
        throw std::runtime_error("scene branch is outside script instructions");
      }
      link(static_cast<std::size_t>(target),
           opcode.flow == SceneFlow::kLoop     ? "loop"
           : opcode.flow == SceneFlow::kThread ? "parent_thread"
                                               : "branch");
      if (opcode.flow == SceneFlow::kJump) continue;
    }
    link(offset + opcode.size,
         opcode.flow == SceneFlow::kThread ? "child_thread" : "next");
  }
  return {{"status", "indexed"},
          {"bytes", bytes.size()},
          {"instruction_end", code_end},
          {"commands", commands},
          {"edges", edges},
          {"unresolved", unresolved},
          {"playback", "unverified"},
          {"preconditions", "not_established"}};
}

nlohmann::json DecodeSceneTriggers(Bytes bytes) {
  // func_800BEC58, BATTLE.PRG/4A0A8.c:4302. Conditions are not evaluated.
  constexpr std::array<std::size_t, 17> kConditionBytes{
      0, 3, 5, 4, 3, 4, 5, 5, 3, 1, 5, 6, 5, 5, 7, 4, 3};
  std::vector<std::uint32_t> offsets;
  std::size_t table_bytes = 0;
  for (std::size_t index = 0; index < kLocalEventCount; ++index) {
    const auto offset = ReadInteger(bytes, index * sizeof(std::uint16_t));
    table_bytes = (index + 1) * sizeof(std::uint16_t);
    constexpr std::uint32_t kEnd = 0xFFFF;
    if (offset == kEnd) break;
    offsets.push_back(offset);
  }
  auto result = nlohmann::json::array();
  for (std::size_t index = 0; index < offsets.size(); ++index) {
    auto offset = offsets[index];
    if (offset < table_bytes) {
      throw std::runtime_error("scene trigger overlaps its offset table");
    }
    const auto header = Section(bytes, offset, kTriggerHeaderBytes);
    const auto event = ReadInteger(header, 1);
    nlohmann::json trigger = {
        {"index", index},
        {"offset", offset},
        {"type_mask", header[0]},
        {"source", (event & kLocalEventBit) != 0 ? "room_local" : "external"},
        {"selector",
         (event & kLocalEventBit) != 0 ? event & kLocalEventMask : event},
        {"conditions", nlohmann::json::array()}};
    offset += kTriggerHeaderBytes;
    while (true) {
      const auto opcode = Section(bytes, offset, 1)[0];
      if (opcode == kTriggerEnd) break;
      const auto size = opcode == kTriggerNegation ? 1
                        : opcode < kConditionBytes.size()
                            ? kConditionBytes[opcode]
                            : 0;
      if (size == 0) {
        throw std::runtime_error(
            std::format("unknown scene trigger condition {:02X}", opcode));
      }
      trigger["conditions"].push_back(
          {{"offset", offset},
           {"opcode", opcode},
           {"operands", Section(bytes, offset + 1, size - 1)}});
      offset += static_cast<std::uint32_t>(size);
    }
    result.push_back(std::move(trigger));
  }
  return result;
}

nlohmann::json ReadSceneCatalog(
    const std::vector<Game::EncounterZone>& catalog) {
  const auto* store = Content::DataStore::Get();
  if (store == nullptr) {
    throw std::runtime_error("scene catalog has no data store");
  }
  const auto opcodes = ProductionOpcodes();
  std::map<std::string, nlohmann::json> references;
  for (const auto& zone : catalog) {
    for (const auto& room : zone.rooms) {
      if (!room.asset.empty()) {
        auto& rooms = references[room.asset];
        if (rooms.is_null()) rooms = nlohmann::json::array();
        rooms.push_back(DescribeRoomContext(zone.id, room));
      }
    }
  }
  auto assets = nlohmann::json::array();
  std::size_t errors = 0;
  for (const auto& entry : store->Entries()) {
    const bool room = entry.path.ends_with(".MPD");
    const bool event = entry.path.ends_with(".EVT");
    const bool animation =
        entry.path.ends_with(".SEQ") || entry.path.ends_with(".ESQ");
    const bool movie = entry.path.ends_with(".STR");
    if (!room && !event && !animation && !movie) continue;
    nlohmann::json asset = {{"asset", entry.path},
                            {"lba", entry.lba},
                            {"bytes", entry.size},
                            {"kind", room        ? "room_events"
                                     : event     ? "external_event"
                                     : animation ? "actor_animation"
                                                 : "movie"},
                            {"playback", "unverified"},
                            {"preconditions", "not_established"}};
    if (event) asset["event_id"] = ExternalEventId(entry.path);
    if (room || event) {
      const auto bytes = store->ReadAll(entry);
      if (!bytes) throw std::runtime_error(bytes.error());
      try {
        asset["definition"] = room ? ReadRoomScenes(*bytes, opcodes)
                                   : DecodeSceneScript(*bytes, opcodes);
      } catch (const std::runtime_error& error) {
        asset["definition"] = {{"status", "invalid"}, {"error", error.what()}};
      }
      if (asset["definition"].value("status", "") == "invalid") ++errors;
      if (room) {
        asset["rooms"] = references.contains(entry.path)
                             ? references.at(entry.path)
                             : nlohmann::json::array();
        for (const auto& script :
             asset["definition"].value("scripts", nlohmann::json::array())) {
          if (script.value("status", "") == "invalid") ++errors;
        }
      }
    }
    assets.push_back(std::move(asset));
  }
  return {{"schema", 1},
          {"coverage", "static_scene_inventory"},
          {"data_root", store->Root().string()},
          {"errors", errors},
          {"assets", assets}};
}
}  // namespace VS::combat_lab
