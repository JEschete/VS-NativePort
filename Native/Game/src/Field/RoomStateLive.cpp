// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomStateLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/Background.hpp"
#include "Field/BackgroundLive.hpp"
#include "Field/RoomLoadingLive.hpp"

extern "C" {
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "Field/BackgroundLiveState.h"
#include "Field/Doors.h"
#include "Field/GimmickAnimationQueueLive.h"
#include "Field/Gimmicks.h"
#include "Field/Locations.h"
#include "Field/Manual.h"
#include "Field/Minimap.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/RoomRenderingLiveState.h"
#include "Field/RoomStateLiveState.h"
#include "Field/Zone.h"
#include "Legacy/Psx.h"
#include "State/Progress.h"

// decomp: D_800F19A0 (D_800F19A0@BATTLE/BATTLE.PRG)
int Field_isRoomVisitPending;
}

namespace VS::Field {
namespace {
constexpr const char* kChannel = "field";
// The room's palette copy: 64 CLUTs of 16 colours (18.c:595 0x800).
constexpr std::size_t kRoomPaletteBytes = 0x800;
// A screen position for each of the 32 floating labels (18.c:595 0x80).
constexpr std::size_t kPlacedLabelPositionBytes = 0x80;

VisitedPlaces LoadVisitedPlaces() {
  VisitedPlaces visited;
  for (std::size_t word = 0; word < visited.rooms.size(); ++word) {
    visited.rooms.at(word) =
        static_cast<std::uint32_t>(State_roomVisitProgress.roomFlags[word]);
  }
  for (std::size_t word = 0; word < visited.areas.size(); ++word) {
    visited.areas.at(word) =
        static_cast<std::uint32_t>(State_roomVisitProgress.areaFlags[word]);
  }
  return visited;
}

void StoreVisitedPlaces(const VisitedPlaces& visited) {
  for (std::size_t word = 0; word < visited.rooms.size(); ++word) {
    State_roomVisitProgress.roomFlags[word] =
        static_cast<int>(visited.rooms.at(word));
  }
  for (std::size_t word = 0; word < visited.areas.size(); ++word) {
    State_roomVisitProgress.areaFlags[word] =
        static_cast<int>(visited.areas.at(word));
  }
}

RoomVisitState LoadRoomVisit() {
  return RoomVisitState{.pending = Field_isRoomVisitPending != 0,
                        .visited = LoadVisitedPlaces()};
}

void StoreRoomVisit(const RoomVisitState& state) {
  Field_isRoomVisitPending = state.pending ? 1 : 0;
  StoreVisitedPlaces(state.visited);
}

static_assert(sizeof(State_roomVisitProgress.roomFlags) ==
              kVisitedRoomWords * sizeof(std::uint32_t));
static_assert(sizeof(State_roomVisitProgress.areaFlags) ==
              kVisitedAreaWords * sizeof(std::uint32_t));

class LiveRoomVisitHost final : public RoomVisitHost {
 public:
  [[nodiscard]] int CurrentSceneId() const override {
    return VS::Field::LiveCurrentSceneId();
  }
  [[nodiscard]] int CurrentRoomId() const override {
    return LiveCurrentRoomId();
  }
  void ReportRefusedVisit(const char* record, int id) override {
    VS_LOG_WARN(kChannel, "visited {} {} outside the visit records", record,
                id);
  }
};

RoomDisplay LoadRoomDisplay() {
  return RoomDisplay{.loaded = Field_roomData.isLoaded == 1,
                     .displayed = Field_roomData.displayFlag != 0,
                     .owner_room_index = Field_roomData.roomIndex};
}

// The display flag is written only when the owner check passed (146C.c:15756
// stores 1 or 0 then); the room keeps its load state and owner.
void StoreRoomDisplayed(const RoomDisplay& display) {
  Field_roomData.displayFlag = static_cast<char>(display.displayed ? 1 : 0);
}

template <typename Rule>
bool RunRoomDisplay(Rule&& rule) {
  RoomDisplay display = LoadRoomDisplay();
  const bool owned = rule(display);
  if (owned) {
    StoreRoomDisplayed(display);
  }
  return owned;
}

class LiveRoomRuntimeHost final : public RoomRuntimeHost {
 public:
  void Clear(RoomRecord record) override {
    namespace Memory = Core::Memory;
    switch (record) {
      case RoomRecord::kBackgroundReferenceVectors:
        Memory::ZeroLiveAlignedWords(Field_backgroundReferenceVectors,
                                     sizeof Field_backgroundReferenceVectors);
        return;
      case RoomRecord::kRoomData:
        Memory::ZeroLiveAlignedWords(&Field_roomData, sizeof Field_roomData);
        return;
      case RoomRecord::kEntranceDoor:
        Memory::ZeroLiveAlignedWords(&Field_entranceDoorRecord,
                                     sizeof Field_entranceDoorRecord);
        return;
      case RoomRecord::kUnusedMenuCursorMemory:
        Memory::ZeroLiveAlignedWords(&Battle_unusedMenu9CursorMemory,
                                     sizeof Battle_unusedMenu9CursorMemory);
        return;
      case RoomRecord::kManualDisplay:
        Memory::ZeroLiveAlignedWords(&Field_manualDisplayState,
                                     sizeof Field_manualDisplayState);
        return;
    }
  }
  void Allocate(RoomBuffer buffer) override {
    namespace Memory = Core::Memory;
    switch (buffer) {
      case RoomBuffer::kRoomPalettes:
        Field_roomPalettes = static_cast<u_short*>(
            Memory::AllocateLiveFromLowEnd(kRoomPaletteBytes));
        return;
      case RoomBuffer::kFloatingLabels:
        Battle_floatingLabels = static_cast<decltype(Battle_floatingLabels)>(
            Memory::AllocateLiveFromLowEnd(sizeof *Battle_floatingLabels));
        return;
      case RoomBuffer::kPlacedLabelPositions:
        Battle_placedLabelPositions = static_cast<DVECTOR*>(
            Memory::AllocateLiveFromLowEnd(kPlacedLabelPositionBytes));
        return;
    }
  }
  void ApplyDefaults(const RoomRuntimeDefaults& defaults) override {
    Field_enteredDoorId = static_cast<std::int16_t>(defaults.entered_door_id);
    Field_areRoomUpdatesPaused =
        static_cast<u_short>(defaults.room_updates_paused ? 1 : 0);
    Field_roomTint.isActive =
        static_cast<std::int16_t>(defaults.room_tint_active ? 1 : 0);
    Battle_unusedSubmenuState = 0;
    Battle_unusedSceneBuffer = nullptr;
    Field_isMinimapVisible =
        static_cast<char>(defaults.minimap_visible ? 1 : 0);
    Field_minimapSlideFrame =
        static_cast<std::int16_t>(defaults.minimap_slide_frame);
  }
  void InitializeRoomBackground() override { LiveInitializeRoomBackground(); }
  void ResetGimmickAnimationQueue() override {
    Field_ResetGimmickAnimationQueueEntry();
  }
  // The original hands the cleared reference vectors' first five words to
  // the background as its gradient colours (18.c:595).
  void ApplyBackgroundReferenceColors() override {
    const auto& [first, second] = Field_backgroundReferenceVectors;
    const GradientColors colors{
        static_cast<std::int32_t>(first.vx),
        static_cast<std::int32_t>(first.vy),
        static_cast<std::int32_t>(first.vz),
        static_cast<std::int32_t>(first.pad),
        static_cast<std::int32_t>(second.vx),
    };
    LiveSetRoomBackgroundColors(colors.data());
  }
};
}  // namespace

std::array<LocationDestination, kLocationCount> LiveLocationTable() {
  std::array<LocationDestination, kLocationCount> table{};
  static_assert(sizeof(Field_locationDestinations) == kLocationCount * 2);
  for (std::size_t id = 0; id < table.size(); ++id) {
    table.at(id) = LocationDestination{
        .zone_id =
            static_cast<std::uint8_t>(Field_locationDestinations[id * 2]),
        .room_id =
            static_cast<std::uint8_t>(Field_locationDestinations[(id * 2) + 1]),
    };
  }
  return table;
}

void LiveMarkCurrentRoomVisited() {
  RoomVisitState state = LoadRoomVisit();
  LiveRoomVisitHost host;
  MarkCurrentRoomVisited(state, host);
  StoreRoomVisit(state);
}

void LiveCancelPendingRoomVisit() {
  RoomVisitState state = LoadRoomVisit();
  CancelPendingRoomVisit(state);
  StoreRoomVisit(state);
}

int LiveFindLocationId(int zone_id, int room_id) {
  const std::array<LocationDestination, kLocationCount> table =
      LiveLocationTable();
  return FindLocationId(table, zone_id, room_id);
}

int LiveCurrentRoomId() {
  const std::array<LocationDestination, kLocationCount> table =
      LiveLocationTable();
  return CurrentRoomId(table, CurrentRoom{.zone_id = Field_zoneContext.zndId,
                                          .room_id = Field_zoneContext.mapId});
}

int LiveEnableOwnedRoomRendering(int owner_room_index) {
  return RunRoomDisplay([owner_room_index](RoomDisplay& display) {
    return EnableOwnedRoomRendering(display, owner_room_index);
  })
             ? 1
             : 0;
}

int LiveDisableOwnedRoomRendering(int owner_room_index) {
  return RunRoomDisplay([owner_room_index](RoomDisplay& display) {
    return DisableOwnedRoomRendering(display, owner_room_index);
  })
             ? 1
             : 0;
}

void LiveShowPrimaryRoomGeometry() {
  static_cast<void>(RunRoomDisplay(ShowPrimaryRoomGeometry));
}

void LiveHidePrimaryRoomGeometry() {
  static_cast<void>(RunRoomDisplay(HidePrimaryRoomGeometry));
}

void LiveInitializeRoomRuntimeState() {
  LiveRoomRuntimeHost host;
  InitializeRoomRuntimeState(host);
}
}  // namespace VS::Field
