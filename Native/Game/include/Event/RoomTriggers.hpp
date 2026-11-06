#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

#include "Event/ScriptCommand.hpp"
#include "Event/ScriptVm.hpp"

namespace VS::Event {

enum class RoomTriggerRequest : std::uint16_t {
  kRoomEntry = 1,
  kPlayerTile = 2,
  kInteraction = 4,
  kActorState = 8,
  kFollowOn = 0xA,
  kActionResult = 0xC,
  kSceneTimer = 0x10,
  kDirectEvent = 0x40,
  kPlaybackStatus = 0xFF,
};

enum class RoomTriggerResult : std::uint8_t {
  kNoMatch,
  kStarted,
  kBusy,
  kRefused,
};

struct TriggerActorPair {
  int actor = 0;
  int other = 0;
};

struct TriggerTile {
  int x = 0;
  int z = 0;
};

// clang-format's alias continuation disagrees with cpplint.
using RoomTriggerContext =  // NOLINTNEXTLINE(whitespace/indent_namespace)
    std::variant<std::monostate, TriggerActorPair, TriggerTile>;

struct RoomTriggerQuery {
  RoomTriggerRequest request = RoomTriggerRequest::kRoomEntry;
  std::int16_t event_id = 0;
  // Original mode zero with zero arguments names the player/player pair.
  RoomTriggerContext context = TriggerActorPair{};
};

struct TriggerPlacement {
  TriggerTile tile;
  int world_y = 0;
  std::uint8_t facing = 0;
};

enum class RoomTriggerRefusal : std::uint8_t {
  kMissingSection,
  kOffsetTable,
  kRecordOffset,
  kRecordHeader,
  kCondition,
  kShortCondition,
  kUnterminatedRecord,
  kMissingPlacement,
};

class RoomTriggerHost {
 public:
  RoomTriggerHost() = default;
  RoomTriggerHost(const RoomTriggerHost&) = delete;
  RoomTriggerHost& operator=(const RoomTriggerHost&) = delete;
  RoomTriggerHost(RoomTriggerHost&&) = delete;
  RoomTriggerHost& operator=(RoomTriggerHost&&) = delete;
  virtual ~RoomTriggerHost() = default;

  virtual void Refuse(RoomTriggerRefusal reason, std::size_t location) = 0;
  [[nodiscard]] virtual PlaybackPhase Phase() = 0;
  [[nodiscard]] virtual bool DirectRequest() = 0;
  [[nodiscard]] virtual bool RoomEntryPending() = 0;
  [[nodiscard]] virtual std::int16_t CurrentEventId() = 0;
  virtual void ConsumeRoomEntry() = 0;
  virtual void QueueEvent(EventSource source, std::int16_t event_id,
                          bool direct) = 0;

  [[nodiscard]] virtual std::uint8_t Flag(int flag) = 0;
  virtual void SetFlag(int flag, std::int16_t value) = 0;
  [[nodiscard]] virtual bool ActorPresent(int actor) = 0;
  [[nodiscard]] virtual std::optional<int> FindActorByModel(int model) = 0;
  // Nullopt replaces sentinel 1; resolved IDs retain signed-halfword semantics.
  [[nodiscard]] virtual std::optional<int> ResolveActor(
      std::uint16_t reference) = 0;
  [[nodiscard]] virtual std::optional<TriggerPlacement> ActorPlacement(
      int actor) = 0;
  [[nodiscard]] virtual int CountActorsByDisposition(int disposition) = 0;
  [[nodiscard]] virtual int ActorHpPercentage(int actor) = 0;
  [[nodiscard]] virtual std::uint32_t ActorStateFlags(int actor) = 0;
  [[nodiscard]] virtual bool ActorRetainedAfterDefeat(int actor) = 0;
  [[nodiscard]] virtual int CountInventoryItem(int item) = 0;
  [[nodiscard]] virtual std::optional<TriggerPlacement> GimmickPlacement(
      int gimmick) = 0;
  [[nodiscard]] virtual int GimmickSupportHeight(TriggerTile tile) = 0;
};

// decomp: BATTLE.PRG func_800BEC58 (src/BATTLE/BATTLE.PRG/4A0A8.c:4302)
// The imported offset table has at most sixteen records. Each read is bounded
// by the supplied section, including a short table with an early sentinel.
[[nodiscard]] RoomTriggerResult EvaluateRoomEventTriggers(
    const RoomTriggerQuery& query, ScriptCommand section,
    RoomTriggerHost& host);

}  // namespace VS::Event
