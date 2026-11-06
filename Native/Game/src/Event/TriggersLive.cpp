#include "Event/TriggersLive.hpp"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>

#include "Actors/ActorStateLive.hpp"
#include "Event/RoomTriggerEntryLive.h"
#include "Event/ScriptVmLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Items/InventoryLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "Field/GimmickPlacementLive.h"
#include "Field/Gimmicks.h"
}

namespace VS::Event {
namespace {
// decomp: BATTLE.PRG func_800BEC58 (src/BATTLE/BATTLE.PRG/4A0A8.c:4302).
class LiveRoomTriggerHost final : public RoomTriggerHost {
 public:
  void Refuse(RoomTriggerRefusal reason, std::size_t location) override {
    Core_Warnf("event", "Room trigger refused: reason=%d location=%zu",
               static_cast<int>(reason), location);
  }
  PlaybackPhase Phase() override { return LiveScriptVm().Phase(); }
  bool DirectRequest() override { return Event_isDirectEventRequest != 0; }
  bool RoomEntryPending() override {
    return (Event_hasRoomEntryScript & 1) != 0;
  }
  std::int16_t CurrentEventId() override { return LiveScriptVm().EventId(); }
  void ConsumeRoomEntry() override { Event_hasRoomEntryScript = 0; }
  void QueueEvent(EventSource source, std::int16_t event_id,
                  bool direct) override {
    auto& vm = LiveScriptVm();
    vm.SetEventId(event_id);
    vm.SetSource(source);
    vm.SetPhase(PlaybackPhase::kRequested);
    Event_isDirectEventRequest = static_cast<char>(direct);
  }
  std::uint8_t Flag(int flag) override { return State::LiveScriptFlag(flag); }
  void SetFlag(int flag, std::int16_t value) override {
    State::SetLiveScriptFlag(flag, value);
  }
  bool ActorPresent(int actor) override {
    return Actors::LiveActorAtSlot(actor) != nullptr;
  }
  std::optional<int> FindActorByModel(int model) override {
    const int actor = Actors::FindLiveActorByModelId(model);
    return actor == 1 ? std::nullopt : std::optional<int>{actor};
  }
  std::optional<int> ResolveActor(std::uint16_t reference) override {
    const auto actor = static_cast<std::int16_t>(
        ResolveScriptActorReference(reference, LiveScriptVm()));
    if (actor == 1) {
      return std::nullopt;
    }
    return actor;
  }
  std::optional<TriggerPlacement> ActorPlacement(int actor) override {
    const auto placement = Actors::LiveActorPlacementView(actor);
    if (!placement) {
      return std::nullopt;
    }
    return TriggerPlacement{{placement->tile.x, placement->tile.z},
                            placement->position.y,
                            placement->tile.spare};
  }
  int CountActorsByDisposition(int disposition) override {
    return Actors::CountLiveActorsByDisposition(disposition);
  }
  int ActorHpPercentage(int actor) override {
    return Actors::LiveActorHpPercentage(actor);
  }
  std::uint32_t ActorStateFlags(int actor) override {
    return Actors::LiveActorStateFlags(actor);
  }
  bool ActorRetainedAfterDefeat(int actor) override {
    return Actors::IsLiveActorRetainedAfterDefeat(actor) != 0;
  }
  int CountInventoryItem(int item) override {
    return Items::CountLiveInventoryItem(item);
  }
  std::optional<TriggerPlacement> GimmickPlacement(int gimmick) override {
    Field_GimmickSnapshot snapshot{};
    if (Field_GimmickPlacement(gimmick, &snapshot) < 0) {
      return std::nullopt;
    }
    constexpr unsigned kHalfwordBits = 16;
    const auto word = std::bit_cast<std::uint32_t>(snapshot.positionXY);
    const auto world_y = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(word >> kHalfwordBits));
    return TriggerPlacement{{snapshot.placementByte0, snapshot.placementByte2},
                            world_y,
                            snapshot.orientation};
  }
  int GimmickSupportHeight(TriggerTile tile) override {
    return Field::LiveGimmickSupportHeight(tile.x, tile.z);
  }
};

// _loadMpdClearedScriptDoorSections / func_800BEB34 (4A0A8.c:4201, 4250).
class LiveRoomSectionHost final : public RoomEventSectionHost {
 public:
  void Refuse(RoomEventLoadError error) override {
    Core_Warnf("event", "Room event import refused: reason=%d bytes=%zu",
               static_cast<int>(error.reason), error.bytes);
  }
  RoomEventCopy CopySections(const RoomEventInput& input) override {
    auto sections = RoomEventSections::Copy(input);
    if (!sections || input.entry_script.empty()) {
      return sections;
    }
    auto source = PrepareLiveRoomEntrySource(*sections);
    if (!source) {
      Core_Warnf("event", "Room entry source preparation refused: reason=%d",
                 static_cast<int>(source.error()));
      return std::unexpected(RoomEventLoadError{
          RoomEventRefusal::kSourcePreparation, input.entry_script.size()});
    }
    prepared_source_ = std::move(*source);
    prepared_room_ = *sections;
    return sections;
  }
  void PublishSections(RoomEventLease sections) override {
    PublishLiveRoomScriptSections(std::move(sections));
  }
  void BindEntryScript(RoomEventLease sections) override {
    if (sections != prepared_room_ ||
        !PublishLiveScriptSource(std::move(prepared_source_))) {
      Core_Errorf("event", "Prepared room entry publication invariant failed");
      std::abort();
    }
    prepared_room_.reset();
  }
  std::int16_t RoomChangeState() override {
    return LiveScriptVm().RoomChangeState();
  }
  void SetRoomChangeState(std::int16_t state) override {
    LiveScriptVm().SetRoomChangeState(state);
  }
  void SetPlaybackPhase(PlaybackPhase phase) override {
    LiveScriptVm().SetPhase(phase);
  }
  void SetStartConfirmed(bool confirmed) override {
    LiveScriptVm().SetStartConfirmed(confirmed);
  }
  void SetRoomEntryPending(bool pending) override {
    Event_hasRoomEntryScript = static_cast<std::int16_t>(pending);
  }
  void SetFlag(int flag, std::int16_t value) override {
    State::SetLiveScriptFlag(flag, value);
  }
  void ReleaseScriptActorControl() override { ReleaseLiveScriptActorControl(); }

 private:
  PreparedScriptSource prepared_source_;
  RoomEventLease prepared_room_;
};
}  // namespace

bool EvaluateLiveActorRoomEventTrigger(int actor_id) {
  const RoomTriggerQuery query{.request = RoomTriggerRequest::kInteraction,
                               .context = TriggerActorPair{0, actor_id}};
  return EvaluateLiveRoomEventTriggers(query) == RoomTriggerResult::kStarted;
}

RoomTriggerResult EvaluateLiveRoomEventTriggers(const RoomTriggerQuery& query) {
  const auto room = LiveRoomScriptSections();
  LiveRoomTriggerHost host;
  return EvaluateRoomEventTriggers(
      query, room ? room->Triggers() : ScriptCommand{}, host);
}

RoomEventLoad LoadLiveRoomEventSections(RoomEventInput input) {
  return ImportLiveRoomScripts(input);
}

RoomEventLoad ImportLiveRoomScripts(RoomEventInput input) {
  LiveRoomSectionHost host;
  return LoadRoomEventSections(input, host);
}

void FreeLiveRoomScripts() {
  LiveRoomSectionHost host;
  FreeRoomEventSections(host);
}
}  // namespace VS::Event
