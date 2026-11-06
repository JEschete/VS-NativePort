#include "Event/RoomTriggers.hpp"

#include <array>
#include <limits>
#include <new>

namespace VS::Event {
namespace {

constexpr std::size_t kTriggerSlots = 16;
constexpr std::size_t kOffsetBytes = 2;
constexpr std::size_t kRecordHeaderBytes = 3;
constexpr std::uint16_t kEndOfOffsets = 0xFFFF;
constexpr std::uint16_t kDoorEventBit = 0x8000;
constexpr std::uint16_t kDoorEventMask = 0xF;
constexpr std::uint16_t kDirectActorBit = 0x1000;
constexpr std::uint16_t kActorSlotMask = 0x1F;
constexpr std::uint32_t kPlayerTriggerBlockFlags = 0x104000;
constexpr std::uint8_t kNegate = 0xFE;
constexpr std::uint8_t kMatch = 0xFF;
constexpr int kWorldUnitsPerHeight = 128;
constexpr int kFacingQuarterShift = 2;
constexpr auto kConditionBytes = std::to_array<std::size_t>(
    {0, 3, 5, 4, 3, 4, 5, 5, 3, 1, 5, 6, 5, 5, 7, 4, 3});
constexpr std::size_t kFirstOperand = 1;
constexpr std::size_t kFollowingHalfwordOperand = 3;

enum class Condition : std::uint8_t {
  kPlayerTile = 1,
  kPlayerRectangle = 2,
  kFlagImmediate = 3,
  kActorPresence = 4,
  kDispositionCount = 5,
  kActorPair = 6,
  kActorHp = 7,
  kContextTile = 8,
  kPlayerState = 9,
  kInventoryCount = 10,
  kGimmickPosition = 11,
  kGimmickHeight = 12,
  kActorTile = 13,
  kActorRectangle = 14,
  kFlagPair = 15,
  kActorRetention = 16,
};

enum class Comparison : std::uint8_t {
  kLessOrEqual = 0x00,
  kGreaterOrEqual = 0x10,
  kEqual = 0x20,
  kNotEqual = 0x30,
  kLess = 0x40,
  kGreater = 0x50,
};

enum class ConditionResult {
  kNoMatch,
  kMatch,
  kUnresolvedActor,
  kRefused,
};

ConditionResult Result(bool matches) {
  return matches ? ConditionResult::kMatch : ConditionResult::kNoMatch;
}

bool AtTile(const TriggerPlacement& placement, ScriptCommand command,
            std::size_t offset) {
  return placement.tile.x == OperandByte(command, offset) &&
         placement.tile.z == OperandByte(command, offset + 1);
}

bool InRectangle(const TriggerPlacement& placement, ScriptCommand command,
                 std::size_t offset) {
  constexpr std::size_t kRectangleHeightOffset = 3;
  const int left = OperandByte(command, offset);
  const int top = OperandByte(command, offset + 1);
  return placement.tile.x >= left &&
         placement.tile.x < left + OperandByte(command, offset + 2) &&
         placement.tile.z >= top &&
         placement.tile.z <
             top + OperandByte(command, offset + kRectangleHeightOffset);
}

ConditionResult StoreOrTest(RoomTriggerHost& host, ScriptCommand command,
                            std::size_t flag_offset, int value) {
  const std::int16_t narrowed = static_cast<std::int16_t>(value);
  const std::int16_t flag = OperandHalf(command, flag_offset);
  if (flag != 0) {
    host.SetFlag(flag, narrowed);
    return ConditionResult::kMatch;
  }
  return Result(narrowed != 0);
}

ConditionResult CompareFlags(RoomTriggerHost& host, ScriptCommand command,
                             bool flag_pair) {
  constexpr int kFlagHighMask = 0xF;
  constexpr int kFlagHighShift = 6;
  constexpr int kFlagLowShift = 2;
  constexpr int kSecondFlagHighMask = 3;
  constexpr int kByteBits = 8;
  constexpr int kComparisonMask = 0xF0;
  const int left_id =
      ((OperandByte(command, 1) & kFlagHighMask) << kFlagHighShift) |
      (OperandByte(command, 2) >> kFlagLowShift);
  const int left = host.Flag(left_id);
  const int right =
      flag_pair ? host.Flag(((OperandByte(command, 2) & kSecondFlagHighMask)
                             << kByteBits) |
                            OperandByte(command, 3))
                : OperandByte(command, 3);
  switch (static_cast<Comparison>(OperandByte(command, 1) & kComparisonMask)) {
    case Comparison::kLessOrEqual:
      return Result(left <= right);
    case Comparison::kGreaterOrEqual:
      return Result(left >= right);
    case Comparison::kEqual:
      return Result(left == right);
    case Comparison::kNotEqual:
      return Result(left != right);
    case Comparison::kLess:
      return Result(left < right);
    case Comparison::kGreater:
      return Result(left > right);
    default:
      return ConditionResult::kNoMatch;
  }
}

ConditionResult ActorPosition(RoomTriggerHost& host, int actor,
                              ScriptCommand command, std::size_t offset,
                              bool rectangle, std::size_t location) {
  const auto placement = host.ActorPlacement(actor);
  if (!placement) {
    host.Refuse(RoomTriggerRefusal::kMissingPlacement, location);
    return ConditionResult::kRefused;
  }
  return Result(rectangle ? InRectangle(*placement, command, offset)
                          : AtTile(*placement, command, offset));
}

ConditionResult ReferencedActor(RoomTriggerHost& host, ScriptCommand command,
                                std::size_t location) {
  const auto actor =
      host.ResolveActor(static_cast<std::uint16_t>(OperandHalf(command, 1)));
  if (!actor) {
    return ConditionResult::kUnresolvedActor;
  }
  constexpr std::size_t kActorPositionOffset = 3;
  switch (static_cast<Condition>(command.front())) {
    case Condition::kActorHp:
      return StoreOrTest(host, command, kActorPositionOffset,
                         host.ActorHpPercentage(*actor));
    case Condition::kActorTile:
      return ActorPosition(host, *actor, command, kActorPositionOffset, false,
                           location);
    case Condition::kActorRectangle:
      return ActorPosition(host, *actor, command, kActorPositionOffset, true,
                           location);
    case Condition::kActorRetention:
      return Result(host.ActorRetainedAfterDefeat(*actor));
    default:
      return ConditionResult::kNoMatch;
  }
}

ConditionResult ActorPair(RoomTriggerHost& host, ScriptCommand command,
                          const RoomTriggerContext& context) {
  const auto* pair = std::get_if<TriggerActorPair>(&context);
  if (pair == nullptr) {
    return ConditionResult::kNoMatch;
  }
  const auto actor =
      host.ResolveActor(static_cast<std::uint16_t>(OperandHalf(command, 1)));
  constexpr std::size_t kOtherActorOffset = 3;
  const auto other = host.ResolveActor(
      static_cast<std::uint16_t>(OperandHalf(command, kOtherActorOffset)));
  return Result(actor && other && *actor == pair->actor &&
                *other == pair->other);
}

ConditionResult GimmickPosition(RoomTriggerHost& host, ScriptCommand command) {
  const auto placement = host.GimmickPlacement(OperandByte(command, 1));
  if (!placement) {
    return ConditionResult::kNoMatch;
  }
  constexpr std::size_t kTileOffset = 2;
  constexpr std::size_t kHeightOffset = 4;
  constexpr std::size_t kFacingOffset = 5;
  const int height = OperandByte(command, kHeightOffset);
  const int facing = OperandByte(command, kFacingOffset);
  return Result(AtTile(*placement, command, kTileOffset) &&
                (height == 0 ||
                 -(placement->world_y / kWorldUnitsPerHeight) + 1 == height) &&
                (facing == 0 ||
                 (placement->facing >> kFacingQuarterShift) + 1 == facing));
}

ConditionResult CheckCondition(RoomTriggerHost& host, ScriptCommand command,
                               const RoomTriggerContext& context,
                               std::size_t location) {
  const auto condition = static_cast<Condition>(command.front());
  switch (condition) {
    case Condition::kPlayerTile:
    case Condition::kPlayerRectangle:
      return ActorPosition(host, 0, command, kFirstOperand,
                           condition == Condition::kPlayerRectangle, location);
    case Condition::kFlagImmediate:
    case Condition::kFlagPair:
      return CompareFlags(host, command, condition == Condition::kFlagPair);
    case Condition::kActorPresence: {
      const auto reference =
          static_cast<std::uint16_t>(OperandHalf(command, 1));
      return Result(
          (reference & kDirectActorBit) != 0
              ? host.ActorPresent(reference & kActorSlotMask)
              : host.FindActorByModel(static_cast<std::int16_t>(reference))
                    .has_value());
    }
    case Condition::kDispositionCount:
      return StoreOrTest(
          host, command, 2,
          host.CountActorsByDisposition(OperandByte(command, 1)));
    case Condition::kActorPair:
      return ActorPair(host, command, context);
    case Condition::kActorHp:
    case Condition::kActorTile:
    case Condition::kActorRectangle:
    case Condition::kActorRetention:
      return ReferencedActor(host, command, location);
    case Condition::kContextTile: {
      const auto* tile = std::get_if<TriggerTile>(&context);
      return Result(tile != nullptr && tile->x == OperandByte(command, 1) &&
                    tile->z == OperandByte(command, 2));
    }
    case Condition::kPlayerState:
      return Result((host.ActorStateFlags(0) & kPlayerTriggerBlockFlags) == 0);
    case Condition::kInventoryCount:
      return StoreOrTest(host, command, kFollowingHalfwordOperand,
                         host.CountInventoryItem(OperandHalf(command, 1)));
    case Condition::kGimmickPosition:
      return GimmickPosition(host, command);
    case Condition::kGimmickHeight:
      // Narrow only after division; negating INT_MIN directly is undefined.
      return StoreOrTest(
          host, command, kFollowingHalfwordOperand,
          -(host.GimmickSupportHeight(
                {OperandByte(command, 1), OperandByte(command, 2)}) /
            kWorldUnitsPerHeight));
    default:
      host.Refuse(RoomTriggerRefusal::kCondition, location);
      return ConditionResult::kRefused;
  }
}

RoomTriggerResult StartRecordEvent(RoomTriggerHost& host, std::uint16_t event) {
  const bool door = (event & kDoorEventBit) != 0;
  host.QueueEvent(
      door ? EventSource::kDoorScript : EventSource::kEventFile,
      static_cast<std::int16_t>(door ? event & kDoorEventMask : event), false);
  return RoomTriggerResult::kStarted;
}

RoomTriggerResult EvaluateRecord(RoomTriggerHost& host, ScriptCommand section,
                                 std::size_t offset,
                                 const RoomTriggerContext& context) {
  if (offset >= section.size()) {
    host.Refuse(RoomTriggerRefusal::kRecordOffset, offset);
    return RoomTriggerResult::kRefused;
  }
  if (section.size() - offset < kRecordHeaderBytes) {
    host.Refuse(RoomTriggerRefusal::kRecordHeader, offset);
    return RoomTriggerResult::kRefused;
  }
  const auto event =
      static_cast<std::uint16_t>(OperandHalf(section, offset + 1));
  std::size_t cursor = offset + kRecordHeaderBytes;
  bool negate = false;
  while (cursor < section.size()) {
    const std::uint8_t opcode = section[cursor];
    if (opcode == kMatch) {
      return StartRecordEvent(host, event);
    }
    if (opcode == kNegate) {
      negate = true;
      ++cursor;
      continue;
    }
    if (opcode >= kConditionBytes.size() || kConditionBytes[opcode] == 0) {
      host.Refuse(RoomTriggerRefusal::kCondition, cursor);
      return RoomTriggerResult::kRefused;
    }
    const std::size_t size = kConditionBytes[opcode];
    if (section.size() - cursor < size) {
      host.Refuse(RoomTriggerRefusal::kShortCondition, cursor);
      return RoomTriggerResult::kRefused;
    }
    const auto result =
        CheckCondition(host, section.subspan(cursor, size), context, cursor);
    if (result == ConditionResult::kRefused) {
      return RoomTriggerResult::kRefused;
    }
    if (result == ConditionResult::kUnresolvedActor ||
        (result == ConditionResult::kMatch) == negate) {
      return RoomTriggerResult::kNoMatch;
    }
    cursor += size;
    negate = false;
  }
  host.Refuse(RoomTriggerRefusal::kUnterminatedRecord, cursor);
  return RoomTriggerResult::kRefused;
}

}  // namespace

RoomEventCopy RoomEventSections::Copy(const RoomEventInput& input) {
  // The legacy importer passed unsigned-halfword lengths, not truncated spans.
  constexpr auto kImportedSectionBytes =
      std::numeric_limits<std::uint16_t>::max();
  if (input.triggers.size() > kImportedSectionBytes) {
    return std::unexpected(RoomEventLoadError{RoomEventRefusal::kTriggerExtent,
                                              input.triggers.size()});
  }
  if (input.entry_script.size() > kEventScriptBytes) {
    return std::unexpected(RoomEventLoadError{
        RoomEventRefusal::kEntryScriptExtent, input.entry_script.size()});
  }
  if (input.doors.size() > kImportedSectionBytes) {
    return std::unexpected(
        RoomEventLoadError{RoomEventRefusal::kDoorExtent, input.doors.size()});
  }
  if (input.triggers.empty() && input.entry_script.empty() &&
      input.doors.empty()) {
    return RoomEventLease{};
  }
  try {
    return RoomEventLease(new RoomEventSections(input));
  } catch (const std::bad_alloc&) {
    return std::unexpected(RoomEventLoadError{RoomEventRefusal::kAllocation,
                                              input.triggers.size() +
                                                  input.entry_script.size() +
                                                  input.doors.size()});
  }
}

// decomp: BATTLE.PRG _loadMpdClearedScriptDoorSections (4A0A8.c:4201).
RoomEventLoad LoadRoomEventSections(RoomEventInput input,
                                    RoomEventSectionHost& host) {
  constexpr std::int16_t kWaitingForRoom = 1;
  constexpr std::int16_t kRoomReady = 2;
  constexpr std::int16_t kStopForRoom = 3;
  constexpr int kRoomLoadFlag = 0xA9;
  constexpr int kRoomFlagFirst = 32;
  constexpr int kRoomFlagEnd = 64;
  const std::int16_t transition = host.RoomChangeState();
  if (transition == kWaitingForRoom) {
    input.entry_script = {};
  }
  const auto prepared = host.CopySections(input);
  if (!prepared) {
    host.Refuse(prepared.error());
    return std::unexpected(prepared.error());
  }
  host.SetFlag(kRoomLoadFlag, 0);
  if (!input.first_entry) {
    for (int flag = kRoomFlagFirst; flag < kRoomFlagEnd; ++flag) {
      host.SetFlag(flag, 0);
    }
  }
  host.PublishSections(*prepared);
  if (transition == kWaitingForRoom) {
    host.SetRoomChangeState(kRoomReady);
    return RoomEventLoadResult::kContinuingEvent;
  }
  if (transition == kStopForRoom) {
    host.SetPlaybackPhase(PlaybackPhase::kIdle);
    host.SetRoomChangeState(0);
    host.SetStartConfirmed(false);
  }
  host.SetRoomEntryPending(!input.entry_script.empty());
  if (!input.entry_script.empty()) {
    host.BindEntryScript(*prepared);
  }
  return RoomEventLoadResult::kReady;
}

// decomp: BATTLE.PRG func_800BEB34 (4A0A8.c:4250).
void FreeRoomEventSections(RoomEventSectionHost& host) {
  constexpr std::int16_t kStopForRoom = 3;
  if (host.RoomChangeState() == kStopForRoom) {
    host.ReleaseScriptActorControl();
  }
  host.PublishSections({});
}

// decomp: BATTLE.PRG func_800BEBEC (4A0A8.c:4273).
void ReleaseZoneEventResources() {}

// decomp: BATTLE.PRG func_800BEC58 (src/BATTLE/BATTLE.PRG/4A0A8.c:4302)
RoomTriggerResult EvaluateRoomEventTriggers(const RoomTriggerQuery& query,
                                            ScriptCommand section,
                                            RoomTriggerHost& host) {
  if (query.request == RoomTriggerRequest::kPlaybackStatus) {
    return host.Phase() == PlaybackPhase::kIdle ? RoomTriggerResult::kNoMatch
                                                : RoomTriggerResult::kBusy;
  }
  if (query.request == RoomTriggerRequest::kRoomEntry &&
      host.RoomEntryPending()) {
    host.ConsumeRoomEntry();
    host.QueueEvent(EventSource::kRoomEntryScript, host.CurrentEventId(), true);
    return RoomTriggerResult::kStarted;
  }
  if (query.request == RoomTriggerRequest::kDirectEvent) {
    host.QueueEvent(EventSource::kEventFile, query.event_id, true);
    return RoomTriggerResult::kStarted;
  }
  if (host.Phase() != PlaybackPhase::kIdle && host.DirectRequest()) {
    return RoomTriggerResult::kBusy;
  }
  if (section.empty()) {
    host.Refuse(RoomTriggerRefusal::kMissingSection, 0);
    return RoomTriggerResult::kRefused;
  }
  for (std::size_t index = 0; index < kTriggerSlots; ++index) {
    const std::size_t entry = index * kOffsetBytes;
    if (entry > section.size() || section.size() - entry < kOffsetBytes) {
      host.Refuse(RoomTriggerRefusal::kOffsetTable, entry);
      return RoomTriggerResult::kRefused;
    }
    const auto offset = static_cast<std::uint16_t>(OperandHalf(section, entry));
    if (offset == kEndOfOffsets) {
      return RoomTriggerResult::kNoMatch;
    }
    if (offset >= section.size()) {
      host.Refuse(RoomTriggerRefusal::kRecordOffset, offset);
      return RoomTriggerResult::kRefused;
    }
    if ((section[offset] & static_cast<std::uint16_t>(query.request)) == 0) {
      continue;
    }
    const auto result = EvaluateRecord(host, section, offset, query.context);
    if (result != RoomTriggerResult::kNoMatch) {
      return result;
    }
  }
  return RoomTriggerResult::kNoMatch;
}

}  // namespace VS::Event
